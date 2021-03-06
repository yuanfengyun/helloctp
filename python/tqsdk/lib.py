#!/usr/bin/env python
#  -*- coding: utf-8 -*-
__author__ = 'chengzhi'

import asyncio
import time
from inspect import isfunction
from datetime import datetime
from asyncio import gather
from typing import Optional, Union, Callable

from tqsdk import utils
from tqsdk.api import TqApi, TqAccount
from tqsdk.channel import TqChan
from tqsdk.datetime import _is_in_trading_time
from tqsdk.diff import _get_obj


class TargetPosTaskSingleton(type):
    """
    TargetPosTask 需要保证在每个账户下每个合约只有一个 TargetPosTask 实例。

    当用户多次调用时，应该保证对于同一账户同一合约使用相同的参数构造，否则抛错。

    在修改 TargetPosTask 构造参数时，同时应该修改 TargetPosTaskSingleton.__call__ 方法的参数，要确保其个数、名称、默认值和文档描述一致，\
    这些参数才是实际传给 TargetPosTask.__init__ 方法的参数。
    """

    # key 为 id(account) + '#' + symbol， 值为 TargetPosTask 实例。
    _instances = {}

    def __call__(cls, api, symbol, price="ACTIVE", offset_priority="今昨,开", min_volume=None, max_volume=None,
                 trade_chan=None, *args, **kwargs):
        if symbol not in TargetPosTaskSingleton._instances:
            TargetPosTaskSingleton._instances[symbol] = super(TargetPosTaskSingleton, cls).__call__(api, symbol, price,
                                                                                                    offset_priority,
                                                                                                    min_volume,
                                                                                                    max_volume,
                                                                                                    trade_chan,
                                                                                                    *args, **kwargs)
        else:
            instance = TargetPosTaskSingleton._instances[symbol]
            if instance._offset_priority != offset_priority:
                raise Exception("您试图用不同的 offset_priority 参数创建两个 %s 调仓任务, offset_priority参数原为 %s, 现为 %s" % (
                    symbol, instance._offset_priority, offset_priority))
            if instance._price != price:
                raise Exception("您试图用不同的 price 参数创建两个 %s 调仓任务, price参数原为 %s, 现为 %s" % (symbol, instance._price, price))
            if instance._min_volume != min_volume:
                raise Exception(f"您试图用不同的 min_volume 参数创建两个 {symbol} 调仓任务, min_volume 参数原为 {instance._min_volume}, 现为 {min_volume}")
            if instance._max_volume != max_volume:
                raise Exception(f"您试图用不同的 max_volume 参数创建两个 {symbol} 调仓任务, max_volume 参数原为 {instance._max_volume}, 现为 {max_volume}")
        return TargetPosTaskSingleton._instances[symbol]


class TargetPosTask(object, metaclass=TargetPosTaskSingleton):
    """目标持仓 task, 该 task 可以将指定合约调整到目标头寸"""

    def __init__(self, api: TqApi, symbol: str, price: Union[str, Callable[[str], Union[float, int]]] = "ACTIVE",
                 offset_priority: str = "今昨,开", min_volume: Optional[int] = None, max_volume: Optional[int] = None,
                 trade_chan: Optional[TqChan] = None,
                 account: Optional[Union[TqAccount]] = None) -> None:
        """
        创建目标持仓task实例，负责调整归属于该task的持仓 **(默认为整个账户的该合约净持仓)**.

        **注意:**
            1. TargetPosTask 在 set_target_volume 时并不下单或撤单, 它的下单和撤单动作, 是在之后的每次 wait_update 时执行的. 因此, **需保证 set_target_volume 后还会继续调用wait_update()** 。

            2. 请勿在使用 TargetPosTask 的同时使用 insert_order() 函数, 否则将导致 TargetPosTask 报错或错误下单。

            3. TargetPosTask 如果同时设置 min_volume（每笔最小下单手数），max_volume（每笔最大下单的手数）两个参数，表示采用 **大单拆分模式** 下单。

                在 **大单拆分模式** 下，每次下单的手数为随机生成的正整数，值介于 min_volume、max_volume 之间。

                具体说明：调用 set_target_volume 后，首先会根据目标持仓手数、开平仓顺序计算出，需要平今、平昨、开仓的目标下单手数及顺序。

                + 如果在调整持仓的目标下单手数小于 max_volume，则直接以目标下单手数下单。

                + 如果在调整持仓的目标下单手数大于等于 max_volume，则会以 min_volume、max_volume 之间的随机手数下一笔委托单，手数全部成交后，会接着处理剩余的手数；\
                继续以随机手数下一笔委托单，全部成交后，继续处理剩余的手数，直至剩余手数小于 max_volume 时，直接以剩余手数下单。

                当使用大单拆分模式下单时，必须同时填写 min_volume、max_volume，且需要满足 max_volume >= min_volume > 0。

        Args:
            api (TqApi): TqApi实例，该task依托于指定api下单/撤单

            symbol (str): 负责调整的合约代码

            price (str / Callable): [可选]下单方式, 默认为 "ACTIVE"。
                * "ACTIVE"：对价下单，在持仓调整过程中，若下单方向为买，对价为卖一价；若下单方向为卖，对价为买一价。
                * "PASSIVE"：对价下单，在持仓调整过程中，若下单方向为买，对价为买一价；若下单方向为卖，对价为卖一价。
                * Callable[[str], Union[float, int]]: 函数参数为下单方向，函数返回值是下单价格。如果返回 nan，程序会抛错。

            offset_priority (str): [可选]开平仓顺序，昨=平昨仓，今=平今仓，开=开仓，逗号=等待之前操作完成

                                   对于下单指令区分平今/昨的交易所(如上期所)，按照今/昨仓的数量计算是否能平今/昨仓
                                   对于下单指令不区分平今/昨的交易所(如中金所)，按照“先平当日新开仓，再平历史仓”的规则计算是否能平今/昨仓

                                   * "今昨,开" 表示先平今仓，再平昨仓，等待平仓完成后开仓，对于没有单向大边的品种避免了开仓保证金不足
                                   * "今昨开" 表示先平今仓，再平昨仓，并开仓，所有指令同时发出，适合有单向大边的品种
                                   * "昨开" 表示先平昨仓，再开仓，禁止平今仓，适合股指这样平今手续费较高的品种
                                   * "开" 表示只开仓，不平仓，适合需要进行锁仓操作的品种

            min_volume (int): [可选] **大单拆分模式下** 每笔最小下单的手数，默认不启用 **大单拆分模式**

            max_volume (int): [可选] **大单拆分模式下** 每笔最大下单的手数，默认不启用 **大单拆分模式**

            trade_chan (TqChan): [可选]成交通知channel, 当有成交发生时会将成交手数(多头为正数，空头为负数)发到该channel上

            account (TqAccount/TqKq/TqSim): [可选]指定发送下单指令的账户实例, 多账户模式下，该参数必须指定

        **注意**

        当 price 参数为函数类型时，该函数应该返回一个有效的价格值，应该避免返回 nan。以下为 price 参数是函数类型时的示例。

        Example1::

            # ... 用户代码 ...
            quote = api.get("SHFE.cu2012")
            def get_price(direction):
                # 在 BUY 时使用买一价加一档价格，SELL 时使用卖一价减一档价格
                if direction == "BUY":
                    price = quote.bid_price1 + quote.price_tick
                else:
                    price = quote.ask_price1 - quote.price_tick
                # 如果 price 价格是 nan，使用最新价报单
                if price != price:
                    price = quote.last_price
                return price

            target_pos = TargetPosTask(api, "SHFE.cu2012", price=get_price)
            # ... 用户代码 ...


        Example2::

            # ... 用户代码 ...
            quote1 = api.get("SHFE.cu2012")
            quote2 = api.get("SHFE.au2012")

            def get_price(direction, quote):
                # 在 BUY 时使用买一价加一档价格，SELL 时使用卖一价减一档价格
                if direction == "BUY":
                    price = quote.bid_price1 + quote.price_tick
                else:
                    price = quote.ask_price1 - quote.price_tick
                # 如果 price 价格是 nan，使用最新价报单
                if price != price:
                    price = quote.last_price
                return price

            target_pos1 = TargetPosTask(api, "SHFE.cu2012", price=lambda direction: get_price(direction, quote1))
            target_pos2 = TargetPosTask(api, "SHFE.au2012", price=lambda direction: get_price(direction, quote2))
            # ... 用户代码 ...

        """
        super(TargetPosTask, self).__init__()
        self._api = api
        self._account = account
        self._api._ensure_symbol(symbol)  # 检查合约代码是否存在
        self._symbol = symbol
        self._exchange = symbol.split(".")[0]
        if not (price in ("ACTIVE", "PASSIVE") or isfunction(price)):
            raise Exception("下单方式(price) %s 错误, 请检查 price 参数是否填写正确" % (price))
        self._price = price
        if len(offset_priority.replace(",", "").replace("今", "", 1).replace("昨", "", 1).replace("开", "", 1)) > 0:
            raise Exception("开平仓顺序(offset_priority) %s 错误, 请检查 offset_priority 参数是否填写正确" % (offset_priority))
        self._offset_priority = offset_priority
        self._pos = self._api.get_position(self._symbol, account)
        self._pos_chan = TqChan(self._api, last_only=True)
        self._trade_chan = trade_chan if trade_chan is not None else TqChan(self._api)
        self._task = self._api.create_task(self._target_pos_task())

        if min_volume is not None and min_volume <= 0:
            raise Exception("最小下单手数(min_volume) %s 错误, 请检查 min_volume 是否填写正确" % (min_volume))
        if max_volume is not None and max_volume <= 0:
            raise Exception("最大下单手数(max_volume) %s 错误, 请检查 max_volume 是否填写正确" % (max_volume))
        if (min_volume is None and max_volume) or (max_volume is None and min_volume):
            raise Exception("最小下单手数(min_volume) %s 和 最大下单手数(max_volume) %s 必须用时填写" % (min_volume, max_volume))
        if min_volume and max_volume and min_volume > max_volume:
            raise Exception("最小下单手数(min_volume) %s ，最大下单手数(max_volume) %s 错误, 请检查 min_volume, max_volume 是否填写正确" % (min_volume, max_volume))
        self._min_volume = int(min_volume) if min_volume else None
        self._max_volume = int(max_volume) if max_volume else None

        self._quote = self._api.get_quote(self._symbol)
        self._time_update_task = self._api.create_task(self._update_time_from_md())  # 监听行情更新并记录当时本地时间的task
        self._local_time_record = time.time() - 0.005  # 更新最新行情时间时的本地时间
        self._local_time_record_update_chan = TqChan(self._api, last_only=True)  # 监听 self._local_time_record 更新

    def set_target_volume(self, volume: int) -> None:
        """
        设置目标持仓手数

        Args:
            volume (int): 目标持仓手数，正数表示多头，负数表示空头，0表示空仓

        Example1::

            # 设置 rb1810 持仓为多头5手
            from tqsdk import TqApi, TqAuth, TargetPosTask

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            target_pos = TargetPosTask(api, "SHFE.rb1810")
            target_pos.set_target_volume(5)
            while True:
                # 需在 set_target_volume 后调用wait_update()以发出指令
                api.wait_update()

        Example2::

            # 多账户模式下使用 TargetPosTask
            from tqsdk import TqApi, TqMultiAccount, TqAuth, TargetPosTask

            account1 = TqAccount("H海通期货", "123456", "123456")
            account2 = TqAccount("H宏源期货", "654321", "123456")
            api = TqApi(TqMultiAccount([account1, account2]), auth=TqAuth("信易账户", "账户密码"))
            symbol1 = "DCE.m2105"
            symbol2 = "DCE.i2101"
            # 多账户模式下, 调仓工具需要指定账户实例
            target_pos1 = TargetPosTask(api, symbol1, account=account1)
            target_pos2 = TargetPosTask(api, symbol2, account=account2)
            target_pos1.set_target_volume(30)
            target_pos2.set_target_volume(80)
            while True:
                api.wait_update()

            api.close()

        """
        self._pos_chan.send_nowait(int(volume))

    def _get_order(self, offset, vol, pending_frozen):
        """
        根据指定的offset和预期下单手数vol, 返回符合要求的委托单最大报单手数
        :param offset: "昨" / "今" / "开"
        :param vol: int, <0表示SELL, >0表示BUY
        :return: order_offset: "CLOSE"/"CLOSETODAY"/"OPEN"; order_dir: "BUY"/"SELL"; "order_volume": >=0, 报单手数
        """
        if vol > 0:  # 买单(增加净持仓)
            order_dir = "BUY"
            pos_all = self._pos.pos_short
        else:  # 卖单
            order_dir = "SELL"
            pos_all = self._pos.pos_long
        if offset == "昨":
            order_offset = "CLOSE"
            if self._exchange == "SHFE" or self._exchange == "INE":
                if vol > 0:
                    pos_all = self._pos.pos_short_his
                else:
                    pos_all = self._pos.pos_long_his
                frozen_volume = sum([order.volume_left for order in self._pos.orders.values() if
                                     not order.is_dead and order.offset == order_offset and order.direction == order_dir])
            else:
                frozen_volume = pending_frozen + sum([order.volume_left for order in self._pos.orders.values() if
                                                      not order.is_dead and order.offset != "OPEN" and order.direction == order_dir])
                # 判断是否有未冻结的今仓手数: 若有则不平昨仓
                if (self._pos.pos_short_today if vol > 0 else self._pos.pos_long_today) - frozen_volume > 0:
                    pos_all = frozen_volume
            order_volume = min(abs(vol), max(0, pos_all - frozen_volume))
        elif offset == "今":
            if self._exchange == "SHFE" or self._exchange == "INE":
                order_offset = "CLOSETODAY"
                if vol > 0:
                    pos_all = self._pos.pos_short_today
                else:
                    pos_all = self._pos.pos_long_today
                frozen_volume = sum([order.volume_left for order in self._pos.orders.values() if
                                     not order.is_dead and order.offset == order_offset and order.direction == order_dir])
            else:
                order_offset = "CLOSE"
                frozen_volume = pending_frozen + sum([order.volume_left for order in self._pos.orders.values() if
                                                      not order.is_dead and order.offset != "OPEN" and order.direction == order_dir])
                pos_all = self._pos.pos_short_today if vol > 0 else self._pos.pos_long_today
            order_volume = min(abs(vol), max(0, pos_all - frozen_volume))
        elif offset == "开":
            order_offset = "OPEN"
            order_volume = abs(vol)
        else:
            order_offset = ""
            order_volume = 0
        return order_offset, order_dir, order_volume

    async def _update_time_from_md(self):
        """监听行情更新并记录当时本地时间的task"""
        try:
            chan = TqChan(self._api, last_only=True)
            self._api.register_update_notify(self._quote, chan)  # quote有更新时: 更新记录的时间
            if isinstance(self._api._backtest, TqBacktest):
                # 回测情况下，在收到回测时间有更新的时候，也需要更新记录的时间
                self._api.register_update_notify(_get_obj(self._api._data, ["_tqsdk_backtest"]), chan)
            async for _ in chan:
                self._local_time_record = time.time() - 0.005  # 更新最新行情时间时的本地时间
                self._local_time_record_update_chan.send_nowait(True)  # 通知记录的时间有更新
        finally:
            await chan.close()

    async def _target_pos_task(self):
        """负责调整目标持仓的task"""
        try:
            async for target_pos in self._pos_chan:
                # lib 中对于时间判断的方案:
                #   如果当前时间（模拟交易所时间）不在交易时间段内，则：等待直到行情更新
                #   行情更新（即下一交易时段开始）后：获取target_pos最新的目标仓位, 开始调整仓位

                # 如果不在可交易时间段内（回测时用 backtest 下发的时间判断，实盘使用 quote 行情判断）: 等待更新
                while True:
                    if isinstance(self._api._backtest, TqBacktest):
                        cur_timestamp = self._api._data.get("_tqsdk_backtest", {}).get("current_dt", float("nan"))
                        cur_dt = datetime.fromtimestamp(cur_timestamp / 1e9).strftime("%Y-%m-%d %H:%M:%S.%f")
                        time_record = float("nan")
                    else:
                        cur_dt = self._quote["datetime"]
                        time_record = self._local_time_record
                    if _is_in_trading_time(self._quote, cur_dt, time_record):
                        break
                    await self._local_time_record_update_chan.recv()

                target_pos = self._pos_chan.recv_latest(target_pos)  # 获取最后一个target_pos目标仓位
                # 确定调仓增减方向
                delta_volume = target_pos - self._pos.pos
                pending_forzen = 0
                all_tasks = []
                for each_priority in self._offset_priority + ",":  # 按不同模式的优先级顺序报出不同的offset单，股指(“昨开”)平昨优先从不平今就先报平昨，原油平今优先("今昨开")就报平今
                    if each_priority == ",":
                        await gather(*[each._task for each in all_tasks])
                        pending_forzen = 0
                        all_tasks = []
                        continue
                    order_offset, order_dir, order_volume = self._get_order(each_priority, delta_volume, pending_forzen)
                    if order_volume == 0:  # 如果没有则直接到下一种offset
                        continue
                    elif order_offset != "OPEN":
                        pending_forzen += order_volume
                    order_task = InsertOrderUntilAllTradedTask(self._api, self._symbol, order_dir, offset=order_offset,
                                                               volume=order_volume, min_volume=self._min_volume,
                                                               max_volume=self._max_volume, price=self._price,
                                                               trade_chan=self._trade_chan, account=self._account)
                    all_tasks.append(order_task)
                    delta_volume -= order_volume if order_dir == "BUY" else -order_volume
        finally:
            # 执行 task.cancel() 时, 删除掉该 symbol 对应的 TargetPosTask 实例
            TargetPosTaskSingleton._instances.pop(self._symbol, None)


class InsertOrderUntilAllTradedTask(object):
    """追价下单task, 该task会在行情变化后自动撤单重下，直到全部成交
     （注：此类主要在tqsdk内部使用，并非简单用法，不建议用户使用）"""

    def __init__(self, api, symbol, direction, offset, volume, min_volume: Optional[int] = None,
                 max_volume: Optional[int] = None, price: Union[str, Callable[[str], Union[float, int]]] = "ACTIVE",
                 trade_chan=None, account: Optional[Union[TqAccount]] = None):
        """
        创建追价下单task实例

        Args:
            api (TqApi): TqApi实例，该task依托于指定api下单/撤单

            symbol (str): 拟下单的合约symbol, 格式为 交易所代码.合约代码,  例如 "SHFE.cu1801"

            direction (str): "BUY" 或 "SELL"

            offset (str): "OPEN", "CLOSE" 或 "CLOSETODAY"

            volume (int): 需要下单的手数

            min_volume (int): [可选] **大单拆分模式下** 每笔最小下单的手数，默认不启用 **大单拆分模式**

            max_volume (int): [可选] **大单拆分模式下** 每笔最大下单的手数，默认不启用 **大单拆分模式**

            price (str / Callable): [可选]下单方式, 默认为 "ACTIVE"。
                * "ACTIVE"：对价下单，在持仓调整过程中，若下单方向为买，对价为卖一价；若下单方向为卖，对价为买一价。
                * "PASSIVE"：对价下单，在持仓调整过程中，若下单方向为买，对价为买一价；若下单方向为卖，对价为卖一价。
                * Callable[[str], Union[float, int]]: 函数参数为下单方向，函数返回值是下单价格。如果返回 nan，程序会抛错。

            trade_chan (TqChan): [可选]成交通知channel, 当有成交发生时会将成交手数(多头为正数，空头为负数)发到该channel上

            account (TqAccount/TqKq/TqSim): [可选]指定发送下单指令的账户实例, 多账户模式下，该参数必须指定
        """
        self._api = api
        self._account = account
        self._api._ensure_symbol(symbol)  # 检查合约代码是否存在
        self._symbol = symbol
        if direction not in ("BUY", "SELL"):
            raise Exception("下单方向(direction) %s 错误, 请检查 direction 参数是否填写正确" % (direction))
        self._direction = direction
        if offset not in ("OPEN", "CLOSE", "CLOSETODAY"):
            raise Exception("开平标志(offset) %s 错误, 请检查 offset 是否填写正确" % (offset))
        self._offset = offset
        self._volume = int(volume)
        if self._volume <= 0:
            raise Exception("下单手数(volume) %s 错误, 请检查 volume 是否填写正确" % (volume))

        if min_volume is not None and min_volume <= 0:
            raise Exception("最小下单手数(min_volume) %s 错误, 请检查 min_volume 是否填写正确" % (min_volume))
        if max_volume is not None and max_volume <= 0:
            raise Exception("最大下单手数(max_volume) %s 错误, 请检查 max_volume 是否填写正确" % (max_volume))
        if (min_volume is None and max_volume) or (max_volume is None and min_volume):
            raise Exception("最小下单手数(min_volume) %s 和 最大下单手数(max_volume) %s 必须用时填写" % (min_volume, max_volume))
        if min_volume and max_volume and min_volume > max_volume:
            raise Exception("最小下单手数(min_volume) %s ，最大下单手数(max_volume) %s 错误, 请检查 min_volume, max_volume 是否填写正确" % (min_volume, max_volume))
        self._min_volume = int(min_volume) if min_volume else None
        self._max_volume = int(max_volume) if max_volume else None

        if price in ("ACTIVE", "PASSIVE"):
            self._price = lambda direction: self._get_price(direction, price)
        elif isfunction(price):
            self._price = price
        else:
            raise Exception("下单方式(price) %s 错误, 请检查 price 参数是否填写正确" % (price))
        self._trade_chan = trade_chan if trade_chan is not None else TqChan(self._api)
        self._quote = self._api.get_quote(self._symbol)
        self._task = self._api.create_task(self._run())

    async def _run(self):
        """负责追价下单的task"""
        async with self._api.register_update_notify() as update_chan:
            # 确保获得初始行情
            while self._quote.datetime == "":
                await update_chan.recv()
            while self._volume != 0:
                limit_price = self._price(self._direction)
                if limit_price != limit_price:
                    raise Exception("设置价格函数返回 nan，无法处理。请检查后重试。")
                # 当前下单手数
                if self._min_volume and self._max_volume and self._volume >= self._max_volume:
                    this_volume = utils.RD.randint(self._min_volume, self._max_volume)
                else:
                    this_volume = self._volume
                insert_order_task = InsertOrderTask(self._api, self._symbol, self._direction, self._offset,
                                                    this_volume, limit_price=limit_price, trade_chan=self._trade_chan,
                                                    account=self._account)
                order = await insert_order_task._order_chan.recv()
                check_chan = TqChan(self._api, last_only=True)
                check_task = self._api.create_task(self._check_price(check_chan, limit_price, order))
                try:
                    # 当父 task 被 cancel，子 task 如果正在执行，也会捕获 CancelError
                    # 添加 asyncio.shield 后，如果父 task 被 cancel，asyncio.shield 也会被 cancel，但是子 task 不会收到 CancelError
                    # 这里需要 asyncio.shield，是因为 insert_order_task._task 预期不会被 cancel， 应该等待到 order 状态是 FINISHED 才返回
                    await asyncio.shield(insert_order_task._task)
                    order = insert_order_task._order_chan.recv_latest(order)
                    self._volume -= (this_volume - order.volume_left)
                    if order.volume_left != 0 and not check_task.done():
                        raise Exception("遇到错单: %s %s %s %d手 %f %s" % (
                            self._symbol, self._direction, self._offset, this_volume, limit_price, order.last_msg))
                finally:
                    if self._api.get_order(order.order_id, account=self._account).status == "ALIVE":
                        # 当 task 被 cancel 时，主动撤掉未成交的挂单
                        self._api.cancel_order(order.order_id, account=self._account)
                    await check_chan.close()
                    await check_task
                    # 在每次退出时，都等到 insert_order_task 执行完，此时 order 状态一定是 FINISHED；self._trade_chan 也一定会收到全部的成交手数
                    try:
                        # 当用户调用 api.close(), 会主动 cancel 所有由 api 创建的 task，包括 TargetPosTask._target_pos_task，
                        # 此时，insert_order_task._task 如果有未完成委托单，会永远等待下去（因为网络连接已经断开），所以这里增加超时机制。
                        await asyncio.wait_for(insert_order_task._task, timeout=30)
                    except asyncio.TimeoutError:
                        raise Exception('TargetPosTask 撤单超时，30s 撤单未完成。')

    def _get_price(self, direction, price_mode):
        """根据最新行情和下单方式计算出最优的下单价格"""
        # 主动买的价格序列(优先判断卖价，如果没有则用买价)
        price_list = [self._quote.ask_price1, self._quote.bid_price1]
        if direction == "SELL":
            price_list.reverse()
        if price_mode == "PASSIVE":
            price_list.reverse()
        limit_price = price_list[0]
        if limit_price != limit_price:
            limit_price = price_list[1]
        if limit_price != limit_price:
            limit_price = self._quote.last_price
        if limit_price != limit_price:
            limit_price = self._quote.pre_close
        return limit_price

    async def _check_price(self, update_chan, order_price, order):
        """判断价格是否变化的task"""
        async with self._api.register_update_notify(chan=update_chan):
            async for _ in update_chan:
                new_price = self._price(self._direction)
                if (self._direction == "BUY" and new_price > order_price) or (
                        self._direction == "SELL" and new_price < order_price):
                    self._api.cancel_order(order, account=self._account)
                    break


class InsertOrderTask(object):
    """下单task （注：此类主要在tqsdk内部使用，并非简单用法，不建议用户使用）"""

    def __init__(self, api, symbol, direction, offset, volume, limit_price=None, order_chan=None, trade_chan=None,
                 account: Optional[Union[TqAccount]] = None):
        """
        创建下单task实例

        Args:
            api (TqApi): TqApi实例，该task依托于指定api下单/撤单

            symbol (str): 拟下单的合约symbol, 格式为 交易所代码.合约代码,  例如 "SHFE.cu1801"

            direction (str): "BUY" 或 "SELL"

            offset (str): "OPEN", "CLOSE" 或 "CLOSETODAY"

            volume (int): 需要下单的手数

            limit_price (float): [可选]下单价格, 默认市价单

            order_chan (TqChan): [可选]委托单通知channel, 当委托单状态发生时会将委托单信息发到该channel上

            trade_chan (TqChan): [可选]成交通知channel, 当有成交发生时会将成交手数(多头为正数，空头为负数)发到该channel上

            account (TqAccount/TqKq/TqSim): [可选]指定发送下单指令的账户实例, 多账户模式下，该参数必须指定
        """
        self._api = api
        self._account = account
        self._api._ensure_symbol(symbol)  # 检查合约代码是否存在
        self._symbol = symbol
        if direction not in ("BUY", "SELL"):
            raise Exception("下单方向(direction) %s 错误, 请检查 direction 参数是否填写正确" % (direction))
        self._direction = direction
        if offset not in ("OPEN", "CLOSE", "CLOSETODAY"):
            raise Exception("开平标志(offset) %s 错误, 请检查 offset 是否填写正确" % (offset))
        self._offset = offset
        self._volume = int(volume)
        self._limit_price = float(limit_price) if limit_price is not None else None
        self._order_chan = order_chan if order_chan is not None else TqChan(self._api)
        self._trade_chan = trade_chan if trade_chan is not None else TqChan(self._api)
        self._task = self._api.create_task(self._run())

    async def _run(self):
        """负责下单的task"""
        order_id = utils._generate_uuid("PYSDK_target")
        order = self._api.insert_order(self._symbol, self._direction, self._offset, self._volume, self._limit_price,
                                       order_id=order_id, account=self._account)
        last_order = order.copy()  # 保存当前 order 的状态
        last_left = self._volume
        async with self._api.register_update_notify() as update_chan:
            await self._order_chan.send(last_order.copy())  # 将副本的数据及所有权转移
            while order.status != "FINISHED" or (order.volume_orign - order.volume_left) != sum(
                    [trade.volume for trade in order.trade_records.values()]):
                await update_chan.recv()
                if order.volume_left != last_left:
                    vol = last_left - order.volume_left
                    last_left = order.volume_left
                    await self._trade_chan.send(vol if order.direction == "BUY" else -vol)
                if order != last_order:
                    last_order = order.copy()
                    await self._order_chan.send(last_order.copy())
