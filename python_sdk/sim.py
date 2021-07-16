#!/usr/bin/env python
#  -*- coding: utf-8 -*-
__author__ = 'chengzhi'

import asyncio
import statistics
import time
from datetime import datetime

from tqsdk.channel import TqChan
from tqsdk.datetime import _get_trading_day_from_timestamp, _get_trading_day_end_time, _get_trade_timestamp, \
    _is_in_trading_time, _format_from_timestamp_nano
from tqsdk.diff import _get_obj, _register_update_chan, _simple_merge_diff, _merge_diff
from tqsdk.entity import Entity
from tqsdk.objs import Quote
from tqsdk.utils import _query_for_quote


def _get_price_range(quote):
    """ 返回合约对应的买一卖一盘口价格"""
    ask_price = quote["ask_price1"]
    bid_price = quote["bid_price1"]
    if quote["ins_class"].endswith("INDEX"):
        # 在指数交易时，使用 tick 进行回测时，backtest 发的 quote 没有买一卖一价；或者在实时行情中，指数的 quote 也没有买一卖一价
        if ask_price != ask_price:
            ask_price = quote["last_price"] + quote["price_tick"]
        if bid_price != bid_price:
            bid_price = quote["last_price"] - quote["price_tick"]
    return ask_price, bid_price


def _get_option_margin(quote, last_price, underlying_last_price):
    """返回每张期权占用保证金，只有空头持仓占用保证金"""
    # 期权保证金计算公式参考深交所文档 http://docs.static.szse.cn/www/disclosure/notice/general/W020191207433397366259.pdf
    if quote["option_class"] == "CALL":
        # 认购期权义务仓开仓保证金＝[合约最新价 + Max（12% × 合约标的最新价 - 认购期权虚值， 7% × 合约标的最新价）] × 合约单位
        # 认购期权虚值＝Max（行权价 - 合约标的最新价，0）
        out_value = max(quote["strike_price"] - underlying_last_price, 0)
        return (last_price + max(0.12 * underlying_last_price - out_value,
                                 0.07 * underlying_last_price)) * quote["volume_multiple"]
    else:
        # 认沽期权义务仓开仓保证金＝Min[合约最新价+ Max（12% × 合约标的最新价 - 认沽期权虚值，7% × 行权价），行权价] × 合约单位
        # 认沽期权虚值＝Max（合约标的最新价 - 行权价，0）
        out_value = max(underlying_last_price - quote["strike_price"], 0)
        return min(quote["last_price"] + max(0.12 * underlying_last_price - out_value,
                                             0.07 * quote["strike_price"]),
                   quote["strike_price"]) * quote["volume_multiple"]


class TqSim(object):
    """
    天勤模拟交易类

    该类实现了一个本地的模拟账户，并且在内部完成撮合交易，在回测和复盘模式下，只能使用 TqSim 账户来交易。

    限价单要求报单价格达到或超过对手盘价格才能成交, 成交价为报单价格, 如果没有对手盘(涨跌停)则无法成交

    市价单使用对手盘价格成交, 如果没有对手盘(涨跌停)则自动撤单

    模拟交易不会有部分成交的情况, 要成交就是全部成交
    """

    def __init__(self, init_balance: float = 10000000.0, account_id: str = None) -> None:
        """
        Args:
            init_balance (float): [可选]初始资金, 默认为一千万

            account_id (str): [可选]帐号, 默认为 TQSIM

        Example::

            # 修改TqSim模拟帐号的初始资金为100000
            from tqsdk import TqApi, TqSim
            api = TqApi(TqSim(init_balance=100000), auth=TqAuth("信易账户", "账户密码"))

        """
        self.trade_log = {}  # 日期->交易记录及收盘时的权益及持仓
        self.tqsdk_stat = {}  # 回测结束后储存回测报告信息
        self._account_id = "TQSIM" if account_id is None else account_id
        self._account_type = "FUTURE"
        self._account_key = str(id(self))
        self._init_balance = float(init_balance)
        if self._init_balance <= 0:
            raise Exception("初始资金(init_balance) %s 错误, 请检查 init_balance 是否填写正确" % (init_balance))
        self._current_datetime = "1990-01-01 00:00:00.000000"  # 当前行情时间（最新的 quote 时间）
        self._trading_day_end = "1990-01-01 18:00:00.000000"
        self._local_time_record = float("nan")  # 记录获取最新行情时的本地时间

    async def _run(self, api, api_send_chan, api_recv_chan, md_send_chan, md_recv_chan):
        """模拟交易task"""
        self._api = api
        self._tqsdk_backtest = {}  # 储存可能的回测信息
        self._logger = api._logger.getChild("TqSim")  # 调试信息输出
        self._api_send_chan = api_send_chan
        self._api_recv_chan = api_recv_chan
        self._md_send_chan = md_send_chan
        self._md_recv_chan = md_recv_chan
        self._pending_peek = False
        self._diffs = []
        self._account = {
            "currency": "CNY",
            "pre_balance": self._init_balance,
            "static_balance": self._init_balance,
            "balance": self._init_balance,
            "available": self._init_balance,
            "float_profit": 0.0,
            "position_profit": 0.0,  # 期权没有持仓盈亏
            "close_profit": 0.0,
            "frozen_margin": 0.0,
            "margin": 0.0,
            "frozen_commission": 0.0,
            "commission": 0.0,
            "frozen_premium": 0.0,
            "premium": 0.0,
            "deposit": 0.0,
            "withdraw": 0.0,
            "risk_ratio": 0.0,
            "market_value": 0.0,
            "ctp_balance": float("nan"),
            "ctp_available": float("nan"),
        }
        self._positions = {}
        self._orders = {}  # 结构 {symbol: {order_id1: {}, order_id2: {}}}
        self._data = Entity()
        self._data._instance_entity([])
        self._prototype = {
            "quotes": {
                "#": Quote(self),  # 行情的数据原型
            }
        }
        self._quote_tasks = {}
        self._all_subscribe = set()  # 客户端+模拟交易模块订阅的合约集合
        # 是否已经发送初始账户信息
        self._has_send_init_account = False
        md_task = self._api.create_task(self._md_handler())  # 将所有 md_recv_chan 上收到的包投递到 api_send_chan 上
        try:
            async for pack in self._api_send_chan:
                if "_md_recv" in pack:
                    if pack["aid"] == "rtn_data":
                        self._md_recv(pack)  # md_recv 中会发送 wait_count 个 quotes 包给各个 quote_chan
                        await asyncio.gather(*[quote_task["quote_chan"].join() for quote_task in self._quote_tasks.values()])
                        await self._send_diff()
                elif pack["aid"] == "subscribe_quote":
                    await self._subscribe_quote(set(pack["ins_list"].split(",")))
                elif pack["aid"] == "peek_message":
                    self._pending_peek = True
                    await self._send_diff()
                    if self._pending_peek:  # 控制"peek_message"发送: 当没有新的事件需要用户处理时才推进到下一个行情
                        await self._md_send_chan.send(pack)
                elif pack["aid"] == "insert_order":
                    # 非该账户的消息包发送至下一个账户
                    if pack["account_key"] != self._account_key:
                        await self._md_send_chan.send(pack)
                    else:
                        symbol = pack["exchange_id"] + "." + pack["instrument_id"]
                        if symbol not in self._quote_tasks:
                            quote_chan = TqChan(self._api)
                            order_chan = TqChan(self._api)
                            self._quote_tasks[symbol] = {
                                "quote_chan": quote_chan,
                                "order_chan": order_chan,
                                "task": self._api.create_task(self._quote_handler(symbol, quote_chan, order_chan))
                            }
                        if "account_key" in pack:
                            pack.pop("account_key", None)
                        await self._quote_tasks[symbol]["order_chan"].send(pack)
                elif pack["aid"] == "cancel_order":
                    # 非该账户的消息包发送至下一个账户
                    if pack["account_key"] != self._account_key:
                        await self._md_send_chan.send(pack)
                    else:
                        # 发送至服务器的包需要去除 account_key 信息
                        if "account_key" in pack:
                            pack.pop("account_key", None)
                        # pack 里只有 order_id 信息，发送到每一个合约的 order_chan, 交由 quote_task 判断是不是当前合约下的委托单
                        for symbol in self._quote_tasks:
                            await self._quote_tasks[symbol]["order_chan"].send(pack)
                else:
                    await self._md_send_chan.send(pack)
                if self._tqsdk_backtest != {} and self._tqsdk_backtest["current_dt"] >= self._tqsdk_backtest["end_dt"] \
                        and not self.tqsdk_stat:
                    # 回测情况下，把 _send_stat_report 在循环中回测结束时执行
                    await self._send_stat_report()
        finally:
            if not self.tqsdk_stat:
                await self._send_stat_report()
            md_task.cancel()
            tasks = [md_task]
            for symbol in self._quote_tasks:
                self._quote_tasks[symbol]["task"].cancel()
                tasks.append(self._quote_tasks[symbol]["task"])
            await asyncio.gather(*tasks, return_exceptions=True)

    async def _md_handler(self):
        async for pack in self._md_recv_chan:
            pack["_md_recv"] = True
            await self._api_send_chan.send(pack)

    async def _send_diff(self):
        if self._pending_peek and self._diffs:
            rtn_data = {
                "aid": "rtn_data",
                "data": self._diffs,
            }
            self._diffs = []
            self._pending_peek = False
            await self._api_recv_chan.send(rtn_data)

    async def _subscribe_quote(self, symbols: [set, str]):
        """这里只会增加订阅合约，不会退订合约"""
        symbols = symbols if isinstance(symbols, set) else {symbols}
        if symbols - self._all_subscribe:
            self._all_subscribe |= symbols
            await self._md_send_chan.send({
                "aid": "subscribe_quote",
                "ins_list": ",".join(self._all_subscribe)
            })

    async def _send_stat_report(self):
        self._settle()
        self._report()
        await self._api_recv_chan.send({
            "aid": "rtn_data",
            "data": [{
                "trade": {
                    self._account_key: {
                        "accounts": {
                            "CNY": {
                                "_tqsdk_stat": self.tqsdk_stat
                            }
                        }
                    }
                }
            }]
        })

    async def _ensure_quote_info(self, symbol, quote_chan):
        """quote收到合约信息后返回"""
        quote = _get_obj(self._data, ["quotes", symbol], Quote(self._api))
        if quote.get("price_tick") == quote.get("price_tick"):
            return quote.copy()
        if quote.get("price_tick") != quote.get("price_tick"):
            await self._md_send_chan.send(_query_for_quote(symbol))
        async for _ in quote_chan:
            quote_chan.task_done()
            if quote.get("price_tick") == quote.get("price_tick"):
                return quote.copy()

    async def _ensure_quote(self, symbol, quote_chan):
        """quote收到行情以及合约信息后返回"""
        quote = _get_obj(self._data, ["quotes", symbol], Quote(self._api))
        _register_update_chan(quote, quote_chan)
        if quote.get("datetime", "") and quote.get("price_tick") == quote.get("price_tick"):
            return quote.copy()
        if quote.get("price_tick") != quote.get("price_tick"):
            # 对于没有合约信息的 quote，发送查询合约信息的请求
            await self._md_send_chan.send(_query_for_quote(symbol))
        async for _ in quote_chan:
            quote_chan.task_done()
            if quote.get("datetime", "") and quote.get("price_tick") == quote.get("price_tick"):
                return quote.copy()

    async def _quote_handler(self, symbol, quote_chan, order_chan):
        try:
            orders = self._orders.setdefault(symbol, {})
            position = self._normalize_position(symbol)
            self._positions[symbol] = position
            await self._subscribe_quote(symbol)
            quote = await self._ensure_quote(symbol, quote_chan)
            if quote["ins_class"].endswith("INDEX") and quote["exchange_id"] == "KQ":
                # 指数可以交易，需要补充 margin commission
                if "margin" not in quote:
                    quote_m = await self._ensure_quote_info(symbol.replace("KQ.i", "KQ.m"), quote_chan)
                    self._data["quotes"][symbol]["margin"] = self._data["quotes"][quote_m["underlying_symbol"]][
                        "margin"]
                    self._data["quotes"][symbol]["commission"] = self._data["quotes"][quote_m["underlying_symbol"]][
                        "commission"]
                    quote.update(self._data["quotes"][symbol])
            underlying_quote = None
            if quote["ins_class"].endswith("OPTION"):
                # 如果是期权，订阅标的合约行情，确定收到期权标的合约行情
                underlying_symbol = quote["underlying_symbol"]
                await self._subscribe_quote(underlying_symbol)
                underlying_quote = await self._ensure_quote(underlying_symbol, quote_chan)  # 订阅合约
            # 在等待标的行情的过程中，quote_chan 可能有期权行情，把 quote_chan 清空，并用最新行情更新 quote
            while not quote_chan.empty():
                quote_chan.recv_nowait()
                quote_chan.task_done()
            quote.update(self._data["quotes"][symbol])
            if underlying_quote:
                underlying_quote.update(self._data["quotes"][underlying_symbol])
            task = self._api.create_task(self._forward_chan_handler(order_chan, quote_chan))
            position["last_price"] = quote["last_price"]
            position["underlying_last_price"] = underlying_quote["last_price"] if underlying_quote else float('nan')
            async for pack in quote_chan:
                if "aid" not in pack:
                    _simple_merge_diff(quote, pack.get("quotes", {}).get(symbol, {}))
                    if underlying_quote:
                        _simple_merge_diff(underlying_quote, pack.get("quotes", {}).get(underlying_symbol, {}))
                    for order_id in list(orders.keys()):  # match_order 过程中可能会删除 orders 下对象
                        self._match_order(orders[order_id], symbol, quote, underlying_quote)
                    if position["volume_long"] > 0 or position["volume_short"] > 0:
                        self._adjust_position_account(symbol, quote, underlying_quote,
                                                      pre_last_price=position["last_price"],
                                                      last_price=quote["last_price"],
                                                      pre_underlying_last_price=position["underlying_last_price"],
                                                      underlying_last_price=underlying_quote["last_price"] if underlying_quote else float('nan'))
                        position["last_price"] = quote["last_price"]
                        position["underlying_last_price"] = underlying_quote["last_price"] if underlying_quote else float('nan')
                elif pack["aid"] == "insert_order":
                    order = self._pre_insert_order(pack, symbol)
                    self._insert_order(order, symbol, quote, underlying_quote)
                    if order["status"] == "ALIVE":
                        self._match_order(order, symbol, quote, underlying_quote)
                    await self._send_diff()
                elif pack["aid"] == "cancel_order":
                    if pack["order_id"] in orders:
                        self._cancel_order(symbol, orders[pack["order_id"]])
                    await self._send_diff()
                quote_chan.task_done()
        finally:
            await quote_chan.close()
            await order_chan.close()
            task.cancel()
            await asyncio.gather(task, return_exceptions=True)

    async def _forward_chan_handler(self, chan_from, chan_to):
        async for pack in chan_from:
            await chan_to.send(pack)

    def _md_recv(self, pack):
        for d in pack["data"]:
            self._diffs.append(d)
            # 在第一次收到 mdhis_more_data 为 False 的时候，发送账户初始截面信息，这样回测模式下，往后的模块才有正确的时间顺序
            if not self._has_send_init_account and not d.get("mdhis_more_data", True):
                self._send_account()
                self._diffs.append({
                    "trade": {
                        self._account_key: {
                            "orders": {},
                            "positions": {},
                            "trade_more_data": False
                        }
                    }
                })
                self._has_send_init_account = True
            _tqsdk_backtest = d.get("_tqsdk_backtest", {})
            if _tqsdk_backtest:
                # 回测时，用 _tqsdk_backtest 对象中 current_dt 作为 TqSim 的 _current_datetime
                self._tqsdk_backtest.update(_tqsdk_backtest)
                self._current_datetime = datetime.fromtimestamp(
                    self._tqsdk_backtest["current_dt"] / 1e9).strftime("%Y-%m-%d %H:%M:%S.%f")
                self._local_time_record = float("nan")
                # 1. 回测时不使用时间差来模拟交易所时间的原因(_local_time_record始终为初始值nan)：
                #   在sim收到行情后记录_local_time_record，然后下发行情到api进行merge_diff(),api需要处理完k线和quote才能结束wait_update(),
                #   若处理时间过长，此时下单则在判断下单时间时与测试用例中的预期时间相差较大，导致测试用例无法通过。
                # 2. 回测不使用时间差的方法来判断下单时间仍是可行的: 与使用了时间差的方法相比, 只对在每个交易时间段最后一笔行情时的下单时间判断有差异,
                #   若不使用时间差, 则在最后一笔行情时下单仍判断为在可交易时间段内, 且可成交.
            quotes_diff = d.get("quotes", {})
            # 先根据 quotes_diff 里的 datetime, 确定出 _current_datetime，再 _merge_diff(同时会发送行情到 quote_chan)
            for symbol, quote_diff in quotes_diff.items():
                if quote_diff is None:
                    continue
                # 若直接使用本地时间来判断下单时间是否在可交易时间段内 可能有较大误差,因此判断的方案为:(在接收到下单指令时判断 估计的交易所时间 是否在交易时间段内)
                # 在更新最新行情时间(即self._current_datetime)时，记录当前本地时间(self._local_time_record)，
                # 在这之后若收到下单指令，则获取当前本地时间,判 "最新行情时间 + (当前本地时间 - 记录的本地时间)" 是否在交易时间段内。
                # 另外, 若在盘后下单且下单前未订阅此合约：
                # 因为从_md_recv()中获取数据后立即判断下单时间则速度过快(两次time.time()的时间差小于最后一笔行情(14:59:9995)到15点的时间差),
                # 则会立即成交,为处理此情况则将当前时间减去5毫秒（模拟发生5毫秒网络延迟，则两次time.time()的时间差增加了5毫秒）。
                # todo: 按交易所来存储 _current_datetime(issue： #277)
                if quote_diff.get("datetime", "") > self._current_datetime:
                    # 回测时，当前时间更新即可以由 quote 行情更新，也可以由 _tqsdk_backtest.current_dt 更新，
                    # 在最外层的循环里，_tqsdk_backtest.current_dt 是在 rtn_data.data 中数组位置中的最后一个，会在循环最后一个才更新 self.current_datetime
                    # 导致前面处理 order 时的 _current_datetime 还是旧的行情时间
                    self._current_datetime = quote_diff["datetime"]  # 最新行情时间
                    # 更新最新行情时间时的本地时间，回测时不使用时间差
                    self._local_time_record = (time.time() - 0.005) if not self._tqsdk_backtest else float("nan")
                if self._current_datetime > self._trading_day_end:  # 结算
                    self._settle()
                    # 若当前行情时间大于交易日的结束时间(切换交易日)，则根据此行情时间更新交易日及交易日结束时间
                    trading_day = _get_trading_day_from_timestamp(self._get_current_timestamp())
                    self._trading_day_end = datetime.fromtimestamp(
                        (_get_trading_day_end_time(trading_day) - 999) / 1e9).strftime("%Y-%m-%d %H:%M:%S.%f")
            if quotes_diff:
                _merge_diff(self._data, {"quotes": quotes_diff}, self._prototype, False, True)
            # 处理 symbols 返回的合约信息
            symbols_diff = d.get("symbols", {})
            for query_id, query_result in symbols_diff.items() :
                if query_id.startswith("PYSDK_quote") and query_result.get("error", None) is None:
                    quotes = self._api._symbols_to_quotes(query_result,
                                                {"instrument_id", "ins_class", "price_tick", "price_decs", "margin", "commission",
                                                     "strike_price", "volume_multiple", "underlying_symbol", "exchange_id",
                                                     "trading_time", "option_class"})
                    _merge_diff(self._data, {"quotes": quotes}, self._prototype, False, True)

    def _normalize_position(self, symbol):
        return {
            "exchange_id": symbol.split(".", maxsplit=1)[0],
            "instrument_id": symbol.split(".", maxsplit=1)[1],
            "pos_long_his": 0,
            "pos_long_today": 0,
            "pos_short_his": 0,
            "pos_short_today": 0,
            "volume_long_today": 0,
            "volume_long_his": 0,
            "volume_long": 0,
            "volume_long_frozen_today": 0,
            "volume_long_frozen_his": 0,
            "volume_long_frozen": 0,
            "volume_short_today": 0,
            "volume_short_his": 0,
            "volume_short": 0,
            "volume_short_frozen_today": 0,
            "volume_short_frozen_his": 0,
            "volume_short_frozen": 0,
            "open_price_long": float("nan"),
            "open_price_short": float("nan"),
            "open_cost_long": 0.0,
            "open_cost_short": 0.0,
            "position_price_long": float("nan"),
            "position_price_short": float("nan"),
            "position_cost_long": 0.0,
            "position_cost_short": 0.0,
            "float_profit_long": 0.0,
            "float_profit_short": 0.0,
            "float_profit": 0.0,
            "position_profit_long": 0.0,
            "position_profit_short": 0.0,
            "position_profit": 0.0,
            "margin_long": 0.0,
            "margin_short": 0.0,
            "margin": 0.0,
            "last_price": float('nan'),
            "underlying_last_price": float('nan'),
            "market_value_long": 0.0,  # 权利方市值(始终 >= 0)
            "market_value_short": 0.0,  # 义务方市值(始终 <= 0)
            "market_value": 0.0,
        }

    def _handle_on_finished(self, msg, symbol, order):
        """在 order 状态变为 FINISHED 调用，负责发送 order 到下游，屏幕输出信息，打印日志，删除本模块记录 order 实例"""
        self._send_order(order)
        self._api._print(f"模拟交易委托单 {order['order_id']}: {order['last_msg']}")
        self._logger.debug(msg, order_id=order["order_id"], last_msg=order["last_msg"], status=order["status"],
                           volume_orign=order["volume_orign"], volume_left=order["volume_left"])
        del self._orders[symbol][order["order_id"]]  # 删除 order

    def _pre_insert_order(self, pack, symbol):
        """order 对象预处理"""
        order = pack.copy()
        order["exchange_order_id"] = order["order_id"]
        order["volume_orign"] = order["volume"]
        order["volume_left"] = order["volume"]
        order["frozen_margin"] = 0.0
        order["frozen_premium"] = 0.0
        order["last_msg"] = "报单成功"
        order["status"] = "ALIVE"
        order["insert_date_time"] = _get_trade_timestamp(self._current_datetime, self._local_time_record)
        del order["aid"]
        del order["volume"]

        self._orders[symbol][order["order_id"]] = order  # 记录 order 在 orders 里
        self._send_order(order)
        self._api._print(
            f"模拟交易下单 {order['order_id']}: 时间: {_format_from_timestamp_nano(order['insert_date_time'])}, "
            f"合约: {symbol}, 开平: {order['offset']}, 方向: {order['direction']}, 手数: {order['volume_left']}, "
            f"价格: {order.get('limit_price', '市价')}")
        self._logger.debug("insert order", order_id=order["order_id"], datetime=order["insert_date_time"],
                           symbol=symbol, offset=order["offset"], direction=order["direction"],
                           volume_left=order["volume_left"], limit_price=order.get("limit_price", "市价"))
        return order

    def _insert_order(self, order, symbol, quote, underlying_quote=None):
        """判断 order 是否可以记录在 orderbook"""
        if ("commission" not in quote or "margin" not in quote) and not quote["ins_class"].endswith("OPTION"):
            order["last_msg"] = "不支持的合约类型，TqSim 目前不支持组合，股票，etf期权模拟交易"
            order["status"] = "FINISHED"

        if order["status"] == "ALIVE" and not _is_in_trading_time(quote, self._current_datetime, self._local_time_record):
            order["last_msg"] = "下单失败, 不在可交易时间段内"
            order["status"] = "FINISHED"

        position = self._positions[symbol]
        if order["status"] == "ALIVE" and order["offset"].startswith('CLOSE'):
            if order["exchange_id"] in ["SHFE", "INE"]:
                if order["offset"] == "CLOSETODAY":
                    if order["direction"] == "BUY" and position["volume_short_today"] - position["volume_long_frozen_today"] < order["volume_orign"]:
                        order["last_msg"] = "平仓手数不足"
                    elif order["direction"] == "SELL" and position["volume_long_today"] - position["volume_long_frozen_today"] < order["volume_orign"]:
                        order["last_msg"] = "平仓手数不足"
                if order["offset"] == "CLOSE":
                    if order["direction"] == "BUY" and position["volume_short_his"] - position["volume_short_frozen_his"] < order["volume_orign"]:
                        order["last_msg"] = "平仓手数不足"
                    elif order["direction"] == "SELL" and position["volume_long_his"] - position["volume_long_frozen_his"] < order["volume_orign"]:
                        order["last_msg"] = "平仓手数不足"
            else:
                if order["direction"] == "BUY" and position["volume_short"] - position["volume_short_frozen"] < order["volume_orign"]:
                    order["last_msg"] = "平仓手数不足"
                elif order["direction"] == "SELL" and position["volume_long"] - position["volume_long_frozen"] < order["volume_orign"]:
                    order["last_msg"] = "平仓手数不足"
            if order["last_msg"] == "平仓手数不足":
                order["status"] = "FINISHED"

        if order["status"] == "ALIVE" and order["offset"] == "OPEN":
            # 计算冻结保证金，冻结权利金
            if quote["ins_class"].endswith("OPTION"):
                if order["direction"] == "SELL":  # 期权的SELL义务仓，开仓需要冻结保证金
                    order["frozen_margin"] = order["volume_orign"] * _get_option_margin(quote, quote["last_price"], underlying_quote["last_price"])
                else:  # 期权的BUY权利仓（市价单使用 last_price 计算需要冻结的权利金）
                    price = quote["last_price"] if order["price_type"] == "ANY" else order["limit_price"]
                    order["frozen_premium"] = order["volume_orign"] * quote["volume_multiple"] * price
            else:
                order["frozen_margin"] = order["volume_orign"] * quote["margin"]
            if order["frozen_margin"] + order["frozen_premium"] > self._account["available"]:
                order["frozen_margin"] = 0
                order["frozen_premium"] = 0
                order["last_msg"] = '开仓资金不足'
                order["status"] = "FINISHED"

        if order["status"] == "FINISHED":
            self._handle_on_finished("insert order", symbol, order)

        if order["status"] == "ALIVE" and order["offset"] == "OPEN":
            # 修改 account 计算字段
            self._adjust_account_by_order(frozen_margin=order["frozen_margin"], frozen_premium=order["frozen_premium"])
            self._send_account()

        if order["status"] == 'ALIVE' and order["offset"].startswith('CLOSE'):
            # 修改 position 原始字段
            if order["exchange_id"] in ["SHFE", "INE"]:
                if order["direction"] == "BUY":
                    position[f"volume_short_frozen_{'today' if order['offset'] == 'CLOSETODAY' else 'his'}"] += order["volume_orign"]
                else:
                    position[f"volume_long_frozen_{'today' if order['offset'] == 'CLOSETODAY' else 'his'}"] += order["volume_orign"]
            elif order["direction"] == "BUY":
                volume_short_his_available = position["volume_short_his"] - position["volume_short_frozen_his"]
                if volume_short_his_available < order["volume_orign"]:
                    position["volume_short_frozen_his"] += volume_short_his_available
                    position["volume_short_frozen_today"] += order["volume_orign"] - volume_short_his_available
                else:
                    position["volume_short_frozen_his"] += order["volume_orign"]
            else:
                volume_long_his_available = position["volume_long_his"] - position["volume_long_frozen_his"]
                if volume_long_his_available < order["volume_orign"]:
                    position["volume_long_frozen_his"] += volume_long_his_available
                    position["volume_long_frozen_today"] += order["volume_orign"] - volume_long_his_available
                else:
                    position["volume_long_frozen_his"] += order["volume_orign"]
            # 修改 position 计算字段
            self._adjust_position_volume_frozen(position)
            self._send_position(position)

    def _match_order(self, order, symbol, quote, underlying_quote=None):
        assert order["status"] == "ALIVE"

        ask_price, bid_price = _get_price_range(quote)
        # order 预期成交价格
        price = (ask_price if order["direction"] == "BUY" else bid_price) if order["price_type"] == "ANY" else order["limit_price"]

        if order["price_type"] == "ANY":
            if price != price:
                order["last_msg"] = "市价指令剩余撤销"
                order["status"] = "FINISHED"

        if order["time_condition"] == "IOC":  # IOC 立即成交，限价下单且不能成交的价格，直接撤单
            if order["direction"] == "BUY" and price < ask_price or order["direction"] == "SELL" and price > bid_price:
                order["last_msg"] = "已撤单报单已提交"
                order["status"] = "FINISHED"

        if order["status"] == 'ALIVE':
            self._match_alive_order(order, symbol, quote, underlying_quote)
        else:
            origin_frozen_margin = order["frozen_margin"]
            origin_frozen_premium = order["frozen_premium"]
            order["frozen_margin"] = 0
            order["frozen_premium"] = 0
            self._handle_on_finished("match order", symbol, order)

            if order["offset"] == 'OPEN':
                # 修改 account 计算字段
                self._adjust_account_by_order(frozen_margin=-origin_frozen_margin, frozen_premium=-origin_frozen_premium)
                self._send_account()

            position = self._positions[symbol]
            if order["offset"].startswith('CLOSE'):
                # 修改 position 原始字段
                if order["exchange_id"] in ["SHFE", "INE"]:
                    if order["direction"] == "BUY":
                        position[f"volume_short_frozen_{'today' if order['offset'] == 'CLOSETODAY' else 'his'}"] -= order["volume_orign"]
                    else:
                        position[f"volume_long_frozen_{'today' if order['offset'] == 'CLOSETODAY' else 'his'}"] -= order["volume_orign"]
                elif order["direction"] == "BUY":
                    if position["volume_short_frozen_today"] >= order["volume_orign"]:
                        position["volume_short_frozen_today"] -= order["volume_orign"]
                    else:
                        position["volume_short_frozen_his"] -= order["volume_orign"] - position["volume_short_frozen_today"]
                        position["volume_short_frozen_today"] = 0
                else:
                    if position["volume_long_frozen_today"] >= order["volume_orign"]:
                        position["volume_long_frozen_today"] -= order["volume_orign"]
                    else:
                        position["volume_long_frozen_his"] -= order["volume_orign"] - position["volume_long_frozen_today"]
                        position["volume_long_frozen_today"] = 0
                # 修改 position 计算字段
                self._adjust_position_volume_frozen(position)
                self._send_position(position)

    def _match_alive_order(self, order, symbol, quote, underlying_quote):
        ask_price, bid_price = _get_price_range(quote)
        price = (ask_price if order["direction"] == "BUY" else bid_price) if order["price_type"] == "ANY" else order["limit_price"]

        position = self._positions[symbol]
        if order["direction"] == "BUY" and price >= ask_price or order["direction"] == "SELL" and price <= bid_price:
            trade = {
                "user_id": order["user_id"],
                "order_id": order["order_id"],
                "trade_id": order["order_id"] + "|" + str(order["volume_left"]),
                "exchange_trade_id": order["order_id"] + "|" + str(order["volume_left"]),
                "exchange_id": order["exchange_id"],
                "instrument_id": order["instrument_id"],
                "direction": order["direction"],
                "offset": order["offset"],
                "price": price,
                "volume": order["volume_left"],
                # todo: 可能导致测试结果不确定
                "trade_date_time": _get_trade_timestamp(self._current_datetime, self._local_time_record),
                # 期权quote没有commission字段, 设为固定10元一张
                "commission": (10 if quote["ins_class"].endswith("OPTION") else quote["commission"]) * order["volume_left"]
            }
            trade_log = self._ensure_trade_log()
            trade_log["trades"].append(trade)
            self._send_trade(trade)

            origin_frozen_margin = order["frozen_margin"]
            origin_frozen_premium = order["frozen_premium"]
            order["frozen_margin"] = 0
            order["frozen_premium"] = 0
            order["volume_left"] = 0
            order["last_msg"] = "全部成交"
            order["status"] = "FINISHED"
            self._handle_on_finished("match order", symbol, order)

            if order["offset"] == 'OPEN':
                if order["direction"] == "BUY":
                    # 修改 position 原始字段
                    position["volume_long_today"] += order["volume_orign"]
                    position["open_cost_long"] += price * order["volume_orign"] * quote["volume_multiple"]  # 多头开仓成本
                    position["position_cost_long"] += price * order["volume_orign"] * quote["volume_multiple"]  # 多头持仓成本
                else:
                    # 修改 position 原始字段
                    position["volume_short_today"] += order["volume_orign"]
                    position["open_cost_short"] += price * order["volume_orign"] * quote["volume_multiple"]  # 空头开仓成本
                    position["position_cost_short"] += price * order["volume_orign"] * quote["volume_multiple"]  # 空头持仓成本

                # 由 order 变化，account 需要更新的计算字段
                self._adjust_account_by_order(frozen_margin=-origin_frozen_margin, frozen_premium=-origin_frozen_premium)

                # 由 trade 引起的 account 原始字段变化，account 需要更新的计算字段
                premium = 0
                if quote["ins_class"].endswith("OPTION"):
                    premium = trade["price"] * trade["volume"] * quote["volume_multiple"]
                    if order["direction"] == "BUY":
                        premium = -premium
                self._adjust_account_by_trade(commission=trade["commission"], premium=premium)

                # 由 position 字段变化，同时 account 需要更新的计算字段
                buy_open = order["volume_orign"] if order["direction"] == "BUY" else 0
                sell_open = 0 if order["direction"] == "BUY" else order["volume_orign"]
                self._adjust_position_account(symbol, quote, underlying_quote,
                                              pre_last_price=price,
                                              last_price=position["last_price"],
                                              pre_underlying_last_price=underlying_quote["last_price"] if underlying_quote else float('nan'),
                                              underlying_last_price=position["underlying_last_price"],
                                              buy_open=buy_open, sell_open=sell_open)

            else:  # order["offset"].startswith('CLOSE')
                # 修改 position 原始字段
                if order["exchange_id"] in ["SHFE", "INE"]:
                    if order["offset"] == "CLOSETODAY":
                        if order["direction"] == "BUY":
                            position["volume_short_frozen_today"] -= order["volume_orign"]
                            position["volume_short_today"] -= order["volume_orign"]
                        elif order["direction"] == "SELL":
                            position["volume_long_frozen_today"] -= order["volume_orign"]
                            position["volume_long_today"] -= order["volume_orign"]
                    if order["offset"] == "CLOSE":
                        if order["direction"] == "BUY":
                            position["volume_short_frozen_his"] -= order["volume_orign"]
                            position["volume_short_his"] -= order["volume_orign"]
                        elif order["direction"] == "SELL":
                            position["volume_long_frozen_his"] -= order["volume_orign"]
                            position["volume_long_his"] -= order["volume_orign"]
                elif order["direction"] == "BUY":
                    if position["volume_short_frozen_his"] >= order["volume_orign"]:
                        position["volume_short_frozen_his"] -= order["volume_orign"]
                        position["volume_short_his"] -= order["volume_orign"]
                    else:
                        position["volume_short_frozen_today"] -= order["volume_orign"] - position["volume_short_frozen_his"]
                        position["volume_short_today"] -= order["volume_orign"] - position["volume_short_frozen_his"]
                        position["volume_short_his"] -= position["volume_short_frozen_his"]
                        position["volume_short_frozen_his"] = 0
                else:
                    if position["volume_long_frozen_his"] >= order["volume_orign"]:
                        position["volume_long_frozen_his"] -= order["volume_orign"]
                        position["volume_long_his"] -= order["volume_orign"]
                    else:
                        position["volume_long_frozen_today"] -= order["volume_orign"] - position["volume_long_frozen_his"]
                        position["volume_long_today"] -= order["volume_orign"] - position["volume_long_frozen_his"]
                        position["volume_long_his"] -= position["volume_long_frozen_his"]
                        position["volume_long_frozen_his"] = 0

                # 修改 position 原始字段
                if order["direction"] == "SELL":
                    position["open_cost_long"] -= position["open_price_long"] * order["volume_orign"] * quote["volume_multiple"]  # 多头开仓成本
                    position["position_cost_long"] -= position["position_price_long"] * order["volume_orign"] * quote["volume_multiple"]  # 多头持仓成本
                else:
                    position["open_cost_short"] -= position["open_price_short"] * order["volume_orign"] * quote["volume_multiple"]  # 空头开仓成本
                    position["position_cost_short"] -= position["position_price_short"] * order["volume_orign"] * quote["volume_multiple"]  # 空头持仓成本

                # 由 order 变化，account 需要更新的计算字段
                self._adjust_account_by_order(frozen_margin=-origin_frozen_margin, frozen_premium=-origin_frozen_premium)

                # 由 trade 引起的 account 原始字段变化，account 需要更新的计算字段
                premium = 0
                if quote["ins_class"].endswith("OPTION"):
                    premium = trade["price"] * trade["volume"] * (-1 if order["direction"] == "BUY" else 1)
                if order["direction"] == "SELL":
                    close_profit = (price - position["position_price_long"]) * order["volume_orign"] * quote["volume_multiple"]
                else:
                    close_profit = (position["position_price_short"] - price) * order["volume_orign"] * quote["volume_multiple"]
                self._adjust_account_by_trade(commission=trade["commission"], premium=premium, close_profit=close_profit)

                # 由 position 字段变化，同时 account 需要更新的计算字段
                buy_close = order["volume_orign"] if order["direction"] == "BUY" else 0
                sell_close = 0 if order["direction"] == "BUY" else order["volume_orign"]
                self._adjust_position_account(symbol, quote, underlying_quote, pre_last_price=position["last_price"],
                                              last_price=0, pre_underlying_last_price=position["underlying_last_price"],
                                              underlying_last_price=0, buy_close=buy_close, sell_close=sell_close)
            self._send_position(position)
            self._send_account()

    def _cancel_order(self, symbol, order):
        if order["status"] == "ALIVE":
            origin_frozen_margin = order["frozen_margin"]
            origin_frozen_premium = order["frozen_premium"]
            order["frozen_margin"] = 0
            order["frozen_premium"] = 0
            order["last_msg"] = "已撤单"
            order["status"] = "FINISHED"
            self._handle_on_finished("cancel order", symbol, order)

            # 反向调整账户和持仓
            if order["offset"] == 'OPEN':
                self._adjust_account_by_order(frozen_margin=-origin_frozen_margin, frozen_premium=-origin_frozen_premium)
                self._send_account()
            else:
                position = self._positions[symbol]
                if order["exchange_id"] in ["SHFE", "INE"]:
                    if order["offset"] == "CLOSETODAY":
                        if order["direction"] == "BUY":
                            position["volume_short_frozen_today"] -= order["volume_orign"]
                        else:
                            position["volume_long_frozen_today"] -= order["volume_orign"]
                    if order.offset == "CLOSE":
                        if order["direction"] == "BUY":
                            position["volume_short_frozen_his"] -= order["volume_orign"]
                        else:
                            position["volume_long_frozen_his"] -= order["volume_orign"]
                else:
                    if order["direction"] == "BUY":
                        if position["volume_short_frozen_today"] >= order["volume_orign"]:
                            position["volume_short_frozen_today"] -= order["volume_orign"]
                        else:
                            position["volume_short_frozen_his"] -= order["volume_orign"] - position["volume_short_frozen_today"]
                            position["volume_short_frozen_today"] = 0
                    else:
                        if position["volume_long_frozen_today"] >= order["volume_orign"]:
                            position["volume_long_frozen_today"] -= order["volume_orign"]
                        else:
                            position["volume_long_frozen_his"] -= order["volume_orign"] - position["volume_long_frozen_today"]
                            position["volume_long_frozen_today"] = 0
                # 修改 position 计算字段
                self._adjust_position_volume_frozen(position)
                self._send_position(position)

    def _settle(self):
        if self._trading_day_end[:10] == "1990-01-01":
            return
        trade_log = self._ensure_trade_log()
        # 记录账户截面
        trade_log["account"] = self._account.copy()
        trade_log["positions"] = {k: v.copy() for k, v in self._positions.items()}

        for symbol in self._orders.keys():
            order_ids = list(self._orders[symbol].keys()).copy()
            for order_id in order_ids:
                self._orders[symbol][order_id]["frozen_margin"] = 0.0
                self._orders[symbol][order_id]["frozen_premium"] = 0.0
                self._orders[symbol][order_id]["last_msg"] = "交易日结束，自动撤销当日有效的委托单（GFD）"
                self._orders[symbol][order_id]["status"] = "FINISHED"
                self._handle_on_finished("settle", symbol, self._orders[symbol][order_id])

        # 为下一交易日调整账户
        # account 原始字段
        self._account["pre_balance"] = self._account["balance"]
        self._account["close_profit"] = 0.0
        self._account["commission"] = 0.0
        self._account["premium"] = 0.0
        self._account["frozen_margin"] = 0.0
        self._account["frozen_premium"] = 0.0
        # account 计算字段
        self._account["static_balance"] = self._account["balance"]
        self._account["position_profit"] = 0.0
        self._account["balance"] = self._account["static_balance"] + self._account["market_value"]
        self._account["risk_ratio"] = self._account["margin"] / self._account["balance"]
        self._account["available"] = self._account["static_balance"] - self._account["margin"]
        self._send_account()

        # 对于持仓的结算放在这里，没有放在 quote_handler 里的原因：
        # 1. 异步发送的话，会造成如果此时 sim 未收到 pending_peek, 就没法把结算的账户信息发送出去，此时用户代码中 api.get_postion 得到的持仓和 sim 里面的持仓是不一致的
        # set_target_pos 下单时就会产生错单。而且结算时一定是已经收到过行情的数据包，在同步代码的最后一步，会发送出去这个行情包 peeding_peek，
        # quote_handler 处理 settle 的时候, 所以在结算的时候 pending_peek 一定是 False, 要 api 处理过之后，才会收到 peek_message
        # 2. 同步发送的话，就可以和产生切换交易日的数据包同时发送出去
        # 对 order 的处理发生在下一次回复 peek_message
        for position in self._positions.values():
            # position 原始字段
            position["volume_long_frozen_today"] = 0
            position["volume_long_frozen_his"] = 0
            position["volume_short_frozen_today"] = 0
            position["volume_short_frozen_his"] = 0
            position["volume_long_today"] = 0
            position["volume_long_his"] = position["volume_long"]
            position["volume_short_today"] = 0
            position["volume_short_his"] = position["volume_short"]
            # position 计算字段
            position["pos_long_his"] = position["volume_long_his"]
            position["pos_long_today"] = 0
            position["pos_short_his"] = position["volume_short_his"]
            position["pos_short_today"] = 0
            position["volume_long_frozen"] = 0
            position["volume_short_frozen"] = 0
            position["position_price_long"] = position["last_price"]
            position["position_price_short"] = position["last_price"]
            quote = self._data["quotes"][f"{position['exchange_id']}.{position['instrument_id']}"]
            position["position_cost_long"] = position["last_price"] * position["volume_long"] * quote["volume_multiple"]  # position 原始字段
            position["position_cost_short"] = position["last_price"] * position["volume_short"] * quote["volume_multiple"]  # position 原始字段
            position["position_profit_long"] = 0
            position["position_profit_short"] = 0
            position["position_profit"] = 0
            self._send_position(position)

    def _report(self):
        if not self.trade_log:
            return
        self._api._print("模拟交易成交记录")
        self.tqsdk_stat["init_balance"] = self._init_balance  # 起始资金
        self.tqsdk_stat["balance"] = self._account["balance"]  # 结束资金
        self.tqsdk_stat["max_drawdown"] = 0  # 最大回撤
        max_balance = 0
        daily_yield = []
        # 胜率 盈亏额比例
        trades_logs = {}
        profit_logs = []  # 盈利记录
        loss_logs = []  # 亏损记录
        for d in sorted(self.trade_log.keys()):
            balance = self.trade_log[d]["account"]["balance"]
            if balance > max_balance:
                max_balance = balance
            drawdown = (max_balance - balance) / max_balance
            if drawdown > self.tqsdk_stat["max_drawdown"]:
                self.tqsdk_stat["max_drawdown"] = drawdown
            daily_yield.append(
                self.trade_log[d]["account"]["balance"] / self.trade_log[d]["account"]["pre_balance"] - 1)
            for t in self.trade_log[d]["trades"]:
                symbol = t["exchange_id"] + "." + t["instrument_id"]
                self._api._print(f"时间: {_format_from_timestamp_nano(t['trade_date_time'])}, 合约: {symbol}, "
                                 f"开平: {t['offset']}, 方向: {t['direction']}, 手数: {t['volume']}, 价格: {t['price']:.3f},"
                                 f"手续费: {t['commission']:.2f}")
                if symbol not in trades_logs:
                    trades_logs[symbol] = {
                        "BUY": [],
                        "SELL": [],
                    }
                if t["offset"] == "OPEN":
                    # 开仓成交 记录下买卖方向、价格、手数
                    trades_logs[symbol][t["direction"]].append({
                        "volume": t["volume"],
                        "price": t["price"]
                    })
                else:
                    opposite_dir = "BUY" if t["direction"] == "SELL" else "SELL"  # 开仓时的方向
                    opposite_list = trades_logs[symbol][opposite_dir]  # 开仓方向对应 trade log
                    cur_close_volume = t["volume"]
                    cur_close_price = t["price"]
                    cur_close_dir = 1 if t["direction"] == "SELL" else -1
                    while cur_close_volume > 0 and opposite_list[0]:
                        volume = min(cur_close_volume, opposite_list[0]["volume"])
                        profit = (cur_close_price - opposite_list[0]["price"]) * cur_close_dir
                        if profit >= 0:
                            profit_logs.append({
                                "symbol": symbol,
                                "profit": profit,
                                "volume": volume
                            })
                        else:
                            loss_logs.append({
                                "symbol": symbol,
                                "profit": profit,
                                "volume": volume
                            })
                        cur_close_volume -= volume
                        opposite_list[0]["volume"] -= volume
                        if opposite_list[0]["volume"] == 0:
                            opposite_list.pop(0)

        self.tqsdk_stat["profit_volumes"] = sum(p["volume"] for p in profit_logs)  # 盈利手数
        self.tqsdk_stat["loss_volumes"] = sum(l["volume"] for l in loss_logs)  # 亏损手数
        self.tqsdk_stat["profit_value"] = sum(
            p["profit"] * p["volume"] * self._data["quotes"][p["symbol"]]["volume_multiple"] for p in profit_logs)  # 盈利额
        self.tqsdk_stat["loss_value"] = sum(
            l["profit"] * l["volume"] * self._data["quotes"][l["symbol"]]["volume_multiple"] for l in loss_logs)  # 亏损额

        mean = statistics.mean(daily_yield)
        rf = 0.0001
        stddev = statistics.pstdev(daily_yield, mu=mean)
        self.tqsdk_stat["sharpe_ratio"] = 250 ** (1 / 2) * (mean - rf) / stddev if stddev else float("inf")  # 年化夏普率

        _ror = self.tqsdk_stat["balance"] / self.tqsdk_stat["init_balance"]
        self.tqsdk_stat["ror"] = _ror - 1  # 收益率
        self.tqsdk_stat["annual_yield"] = _ror ** (250 / len(self.trade_log)) - 1  # 年化收益率

        self._api._print("模拟交易账户资金")
        for d in sorted(self.trade_log.keys()):
            account = self.trade_log[d]["account"]
            self._api._print(
                f"日期: {d}, 账户权益: {account['balance']:.2f}, 可用资金: {account['available']:.2f}, "
                f"浮动盈亏: {account['float_profit']:.2f}, 持仓盈亏: {account['position_profit']:.2f}, "
                f"平仓盈亏: {account['close_profit']:.2f}, 市值: {account['market_value']:.2f}, "
                f"保证金: {account['margin']:.2f}, 手续费: {account['commission']:.2f}, "
                f"风险度: {account['risk_ratio'] * 100:.2f}%")
        self.tqsdk_stat["winning_rate"] = (self.tqsdk_stat["profit_volumes"] / (
                self.tqsdk_stat["profit_volumes"] + self.tqsdk_stat["loss_volumes"])) \
            if self.tqsdk_stat["profit_volumes"] + self.tqsdk_stat["loss_volumes"] else 0
        profit_pre_volume = self.tqsdk_stat["profit_value"] / self.tqsdk_stat["profit_volumes"] if self.tqsdk_stat[
            "profit_volumes"] else 0
        loss_pre_volume = self.tqsdk_stat["loss_value"] / self.tqsdk_stat["loss_volumes"] if self.tqsdk_stat[
            "loss_volumes"] else 0
        self.tqsdk_stat["profit_loss_ratio"] = abs(profit_pre_volume / loss_pre_volume) if loss_pre_volume else float(
            "inf")
        tqsdk_punchlines = [
            '幸好是模拟账户，不然你就亏完啦',
            '触底反弹,与其执迷修改参数，不如改变策略思路去天勤官网策略库进修',
            '越挫越勇，不如去天勤量化官网策略库进修',
            '不要灰心，少侠重新来过',
            '策略看来小有所成',
            '策略看来的得心应手',
            '策略看来春风得意，堪比当代索罗斯',
            '策略看来独孤求败，小心过拟合噢'
        ]
        ror_level = [i for i, k in enumerate([-1, -0.5, -0.2, 0, 0.2, 0.5, 1]) if self.tqsdk_stat["ror"] < k]
        if len(ror_level) > 0:
            self.tqsdk_stat["tqsdk_punchline"] = tqsdk_punchlines[ror_level[0]]
        else:
            self.tqsdk_stat["tqsdk_punchline"] = tqsdk_punchlines[-1]
        self._api._print(
            f"胜率: {self.tqsdk_stat['winning_rate'] * 100:.2f}%, 盈亏额比例: {self.tqsdk_stat['profit_loss_ratio']:.2f}, "
            f"收益率: {self.tqsdk_stat['ror'] * 100:.2f}%, 年化收益率: {self.tqsdk_stat['annual_yield'] * 100:.2f}%, "
            f"最大回撤: {self.tqsdk_stat['max_drawdown'] * 100:.2f}%, 年化夏普率: {self.tqsdk_stat['sharpe_ratio']:.4f}")

    def _ensure_trade_log(self):
        return self.trade_log.setdefault(self._trading_day_end[:10], {
            "trades": []
        })

    def _adjust_position_account(self, symbol, quote, underlying_quote=None, pre_last_price=float('nan'), last_price=float('nan'),
                                 pre_underlying_last_price=float('nan'), underlying_last_price=float('nan'),
                                 buy_open=0, buy_close=0, sell_open=0, sell_close=0):
        """价格变化，使得 position 中的以下计算字段需要修改，这个函数计算出需要修改的差值部分，计算出差值部分修改 position、account
        有两种情况下调用
        1. 委托单 FINISHED，且全部成交，分为4种：buy_open, buy_close, sell_open, sell_close
        2. 行情跳动
        """
        position = self._positions[symbol]
        float_profit_long = 0  # 多头浮动盈亏
        float_profit_short = 0  # 空头浮动盈亏
        position_profit_long = 0  # 多头持仓盈亏，期权持仓盈亏为0
        position_profit_short = 0  # 空头持仓盈亏，期权持仓盈亏为0
        margin_long = 0  # 多头占用保证金
        margin_short = 0  # 空头占用保证金
        market_value_long = 0  # 期权权利方市值(始终 >= 0)
        market_value_short = 0  # 期权义务方市值(始终 <= 0)
        assert [buy_open, buy_close, sell_open, sell_close].count(0) >= 3  # 只有一个大于0, 或者都是0，表示价格变化导致的字段修改
        if buy_open > 0:
            # 买开，pre_last_price 应该是成交价格，last_price 应该是 position['last_price']
            float_profit_long = (last_price - pre_last_price) * buy_open * quote["volume_multiple"]
            if quote["ins_class"].endswith("OPTION"):
                market_value_long = last_price * buy_open * quote["volume_multiple"]
            else:
                margin_long = buy_open * quote["margin"]
                position_profit_long = (last_price - pre_last_price) * buy_open * quote["volume_multiple"]
        elif sell_close > 0:
            # 卖平，pre_last_price 应该是 position['last_price']，last_price 应该是 0
            float_profit_long = -position["float_profit_long"] / position["volume_long"] * sell_close
            if quote["ins_class"].endswith("OPTION"):
                market_value_long = -pre_last_price * sell_close * quote["volume_multiple"]
            else:
                margin_long = -sell_close * quote["margin"]
                position_profit_long = -position["position_profit_long"] / position["volume_long"] * sell_close
        elif sell_open > 0:
            # 卖开
            float_profit_short = (pre_last_price - last_price) * sell_open * quote["volume_multiple"]
            if quote["ins_class"].endswith("OPTION"):
                market_value_short = -last_price * sell_open * quote["volume_multiple"]
                margin_short = sell_open * _get_option_margin(quote, last_price, underlying_last_price)
            else:
                margin_short = sell_open * quote["margin"]
                position_profit_short = (pre_last_price - last_price) * sell_open * quote["volume_multiple"]
        elif buy_close > 0:
            # 买平
            float_profit_short = -position["float_profit_short"] / position["volume_short"] * buy_close
            if quote["ins_class"].endswith("OPTION"):
                market_value_short = pre_last_price * buy_close * quote["volume_multiple"]
                margin_short = -buy_close * _get_option_margin(quote, pre_last_price, pre_underlying_last_price)
            else:
                margin_short = -buy_close * quote["margin"]
                position_profit_short = -position["position_profit_short"] / position["volume_short"] * buy_close
        else:
            # 不涉及成交，仅仅价格变化
            if last_price == pre_last_price:
                return
            float_profit_long = (last_price - pre_last_price) * position["volume_long"] * quote["volume_multiple"]  # 多头浮动盈亏
            float_profit_short = (pre_last_price - last_price) * position["volume_short"] * quote["volume_multiple"]  # 空头浮动盈亏
            if quote["ins_class"].endswith("OPTION"):
                margin_short = _get_option_margin(quote, last_price, underlying_last_price) * position["volume_short"] - position["margin_short"]
                market_value_long = (last_price - pre_last_price) * position["volume_long"] * quote["volume_multiple"]
                market_value_short = (pre_last_price - last_price) * position["volume_short"] * quote["volume_multiple"]
            else:
                position_profit_long = float_profit_long  # 多头持仓盈亏
                position_profit_short = float_profit_short  # 空头持仓盈亏

        if any([buy_open, buy_close, sell_open, sell_close]):
            # 修改 position volume 相关的计算字段
            # 在上面 sell_close buy_close 两种情况，计算浮动盈亏时，用到了修改前的手数，所以需改手数字段的代码放在这个位置
            self._adjust_position_volume(position)

        self._adjust_position(quote, position, float_profit_long, float_profit_short, position_profit_long,
                              position_profit_short, margin_long, margin_short, market_value_long, market_value_short)
        self._adjust_account_by_position(float_profit=float_profit_long + float_profit_short,
                                         position_profit=position_profit_long + position_profit_short,
                                         margin=margin_long + margin_short,
                                         market_value=market_value_long + market_value_short)

    # -------- 对于 position 的计算字段修改分为两类：
    # 1. 针对手数相关的修改，在下单、成交时会修改
    # 2. 针对盈亏、保证金、市值的修改，由于参考合约最新价，在成交、行情跳动时会修改

    def _adjust_position_volume_frozen(self, position):
        """position 原始字段修改后，只有冻结手数需要重新计算，有两种情况需要调用
        1. 下平仓单 2. 平仓单 FINISHED, 但没有成交
        """
        position["volume_long_frozen"] = position["volume_long_frozen_today"] + position["volume_long_frozen_his"]
        position["volume_short_frozen"] = position["volume_short_frozen_today"] + position["volume_short_frozen_his"]

    def _adjust_position_volume(self, position):
        """position 原始字段修改后，手数之后需要重新计算
        1. 委托单 FINISHED，且全部成交
        """
        position["pos_long_today"] = position["volume_long_today"]
        position["pos_long_his"] = position["volume_long_his"]
        position["pos_short_today"] = position["volume_short_today"]
        position["pos_short_his"] = position["volume_short_his"]
        position["volume_long"] = position["volume_long_today"] + position["volume_long_his"]
        position["volume_long_frozen"] = position["volume_long_frozen_today"] + position["volume_long_frozen_his"]
        position["volume_short"] = position["volume_short_today"] + position["volume_short_his"]
        position["volume_short_frozen"] = position["volume_short_frozen_today"] + position["volume_short_frozen_his"]

    def _adjust_position(self, quote, position, float_profit_long=0, float_profit_short=0, position_profit_long=0,
                                   position_profit_short=0, margin_long=0, margin_short=0, market_value_long=0,
                                   market_value_short=0):
        # 更新 position 计算字段，根据差值更新的字段
        position["float_profit_long"] += float_profit_long
        position["float_profit_short"] += float_profit_short
        position["position_profit_long"] += position_profit_long
        position["position_profit_short"] += position_profit_short
        position["margin_long"] += margin_long
        position["margin_short"] += margin_short
        position["market_value_long"] += market_value_long
        position["market_value_short"] += market_value_short

        # 更新 position 计算字段，原地重新计算的字段
        if position["volume_long"] > 0:
            position["open_price_long"] = position["open_cost_long"] / position["volume_long"] / quote["volume_multiple"]
            position["position_price_long"] = position["position_cost_long"] / position["volume_long"] / quote["volume_multiple"]
        else:
            position["open_price_long"] = float("nan")
            position["position_price_long"] = float("nan")
        if position["volume_short"] > 0:
            position["open_price_short"] = position["open_cost_short"] / position["volume_short"] / quote["volume_multiple"]
            position["position_price_short"] = position["position_cost_short"] / position["volume_short"] / quote["volume_multiple"]
        else:
            position["open_price_short"] = float("nan")
            position["position_price_short"] = float("nan")
        position["float_profit"] = position["float_profit_long"] + position["float_profit_short"]
        position["position_profit"] = position["position_profit_long"] + position["position_profit_short"]
        position["margin"] = position["margin_long"] + position["margin_short"]
        position["market_value"] = position["market_value_long"] + position["market_value_short"]

    # -------- 对于 account 的修改分为以下三类

    def _adjust_account_by_trade(self, commission=0, close_profit=0, premium=0):
        """由成交引起的 account 原始字段变化，account 需要更新的计算字段"""
        # account 原始字段
        self._account["close_profit"] += close_profit
        self._account["commission"] += commission
        self._account["premium"] += premium  # premium变量的值有正负，正数表示收入的权利金，负数表示付出的权利金
        # account 计算字段
        self._account["balance"] += close_profit - commission + premium
        self._account["available"] += close_profit - commission + premium
        self._account["risk_ratio"] = self._account["margin"] / self._account["balance"]

    def _adjust_account_by_position(self, float_profit=0, position_profit=0, margin=0, market_value=0):
        """由 position 变化，account 需要更新的计算字段"""
        # account 计算字段，持仓字段求和的字段
        self._account["float_profit"] += float_profit
        self._account["position_profit"] += position_profit
        self._account["margin"] += margin
        self._account["market_value"] += market_value
        # account 计算字段
        self._account["balance"] += position_profit + market_value
        self._account["available"] += position_profit - margin
        self._account["risk_ratio"] = self._account["margin"] / self._account["balance"]

    def _adjust_account_by_order(self, frozen_margin=0, frozen_premium=0):
        """由 order 变化，account 需要更新的计算字段"""
        self._account["frozen_margin"] += frozen_margin
        self._account["frozen_premium"] += frozen_premium
        self._account["available"] -= (frozen_margin + frozen_premium)

    def _send_trade(self, trade):
        self._diffs.append({
            "trade": {
                self._account_key: {
                    "trades": {
                        trade["trade_id"]: trade.copy()
                    }
                }
            }
        })

    def _send_order(self, order):
        self._diffs.append({
            "trade": {
                self._account_key: {
                    "orders": {
                        order["order_id"]: order.copy()
                    }
                }
            }
        })

    def _send_position(self, position):
        self._diffs.append({
            "trade": {
                self._account_key: {
                    "positions": {
                        position["exchange_id"] + "." + position["instrument_id"]: position.copy()
                    }
                }
            }
        })

    def _send_account(self):
        self._diffs.append({
            "trade": {
                self._account_key: {
                    "accounts": {
                        "CNY": self._account.copy()
                    }
                }
            }
        })

    def _get_current_timestamp(self):
        return int(datetime.strptime(self._current_datetime, "%Y-%m-%d %H:%M:%S.%f").timestamp() * 1e6) * 1000
