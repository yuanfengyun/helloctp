#!/usr/bin/env python
#  -*- coding: utf-8 -*-
import asyncio
import copy
import functools
import logging
import os
import platform
import re
import sys
import time
from datetime import datetime, date
from typing import Union, List, Any, Optional

import numpy as np
import psutil
from shinny_structlog import ShinnyLoggerAdapter, JSONFormatter

try:
    import pandas as pd
except ImportError as e:
    err_msg = f"执行 import pandas 时发生错误： {e}。\n"
    err_msg += """
    当遇到此问题时，如果您是 windows 用户，并且安装的 pandas 版本大于等于 1.0.2，可以尝试以下解决方案之一，再重新运行程序即可。
    （您使用的机器缺少 pandas 需要的运行时环境）
    1. 到微软官网下载您机器上安装 python 对应版本的 vc_redist 文件运行安装即可。https://www.microsoft.com/en-us/download/details.aspx?id=48145 
       vc_redist.x64.exe（64 位 python）、 vc_redist.x86.exe（32 位 python）
    2. 卸载当前的 pandas (pip uninstall pandas)
       安装 pandas 1.0.1 版本 (pip install pandas==1.0.1)
    """
    raise Exception(err_msg)
import requests
from pandas import RangeIndex, Index
from pandas.core.internals import FloatBlock

from tqsdk.auth import TqAuth
from tqsdk.account import TqAccount, TqKq
from tqsdk.multiaccount import TqMultiAccount
from tqsdk.backtest import TqBacktest, TqReplay
from tqsdk.channel import TqChan
from tqsdk.connect import TqConnect, MdReconnectHandler, TdReconnectHandler, ReconnectTimer
from tqsdk.data_series import DataSeries
from tqsdk.datetime import _get_trading_day_start_time, _get_trading_day_end_time, _get_trading_calendar
from tqsdk.diff import _merge_diff, _get_obj, _is_key_exist, _register_update_chan
from tqsdk.entity import Entity
from tqsdk.log import _get_log_name, _clear_logs
from tqsdk.objs import Quote, Kline, Tick, Account, Position, Order, Trade, QuotesEntity, RiskManagementRule, RiskManagementData
from tqsdk.objs import SecurityAccount, SecurityOrder, SecurityTrade, SecurityPosition
from tqsdk.tqwebhelper import TqWebHelper
from tqsdk.stockprofit import TqStockProfit
from tqsdk.utils import _generate_uuid, _query_for_quote, _symbols_to_quotes, _query_for_init, \
    BlockManagerUnconsolidated, _quotes_add_night
from tqsdk.tafunc import get_dividend_df, get_dividend_factor
from .__version__ import __version__


class TqApi(object):
    """
    天勤接口及数据管理类

    通常情况下, 一个线程中 **应该只有一个** TqApi的实例, 它负责维护网络连接, 接收行情及账户数据, 并在内存中维护业务数据截面
    """

    def __init__(self, account: Union[TqAccount,None] = None) -> None:
        # 初始化 logger
        self._logger = logging.getLogger("TqApi")
        self._logger.setLevel(logging.DEBUG)
        self.disable_print = False

        self._debug = debug  # 日志选项
        self._ins_url = "https://openmd.shinnytech.com/t/md/symbols/latest.json"
        self._md_url = "https://openmd.shinnytech.com/t/md/symbols/latest.json"
        self._td_url = "https://openmd.shinnytech.com/t/md/symbols/latest.json"
        self._loop = asyncio.SelectorEventLoop() if loop is None else loop  # 创建一个新的 ioloop, 避免和其他框架/环境产生干扰

        # 初始化loop
        self._send_chan, self._recv_chan = TqChan(self), TqChan(self)  # 消息收发队列
        self._tasks = set()  # 由api维护的所有根task，不包含子task，子task由其父task维护
        self._exceptions = []  # 由api维护的所有task抛出的例外
        # 回测需要行情和交易 lockstep, 而 asyncio 没有将内部的 _ready 队列暴露出来,
        # 因此 monkey patch call_soon 函数用来判断是否有任务等待执行
        self._loop.call_soon = functools.partial(self._call_soon, self._loop.call_soon)
        self._event_rev, self._check_rev = 0, 0
        self._latest_order_id = {}

        # 内部关键数据
        self._requests = {
            "quotes": set(),
            "klines": {},
            "ticks": {},
        }  # 记录已发出的请求
        self._serials = {}  # 记录所有数据序列
        # 记录所有(若有多个serial 则仅data_length不同, right_id相同)合约、周期相同的多合约K线中最大的更新数据范围
        # key:(主合约,(所有副合约),duration), value:(K线的新数据中主合约最小的id, 主合约的right_id)。用于is_changing()中新K线生成的判定
        self._klines_update_range = {}
        self._data = Entity()  # 数据存储
        self._data._instance_entity([])
        self._data["quotes"] = QuotesEntity(self)
        self._data["quotes"]._instance_entity(["quotes"])
        self._diffs = []  # 自上次wait_update返回后收到更新数据的数组
        self._pending_diffs = []  # 从网络上收到的待处理的 diffs, 只在 wait_update 函数执行过程中才可能为非空
        self._prototype = self._gen_prototype()  # 各业务数据的原型, 用于决定默认值及将收到的数据转为特定的类型
        self._security_prototype = self._gen_security_prototype() # 股票业务数据原型
        self._wait_timeout = False  # wait_update 是否触发超时
        self._dividend_cache = {}  # 缓存合约对应的复权系数矩阵，每个合约只计算一次
        # 初始化
        if sys.platform.startswith("win"):
            self.create_task(self._windows_patch())  # Windows系统下asyncio不支持KeyboardInterrupt的临时补丁
        self.create_task(self._notify_watcher())  # 监控服务器发送的通知
        self._reconnect_timer = ReconnectTimer()  # 管理 ws 连接重连时间
        self._setup_connection()  # 初始化通讯连接

        # 等待初始化完成
        deadline = time.time() + 60
        try:
            # 多账户时，所有账户需要初始化完成
            trade_more_data = True
            while self._data.get("mdhis_more_data", True) or trade_more_data:
                if not self.wait_update(deadline=deadline):  # 等待连接成功并收取截面数据
                    raise Exception("接收数据超时，请检查客户端及网络是否正常")

                trade_more_data = self._account._get_trade_more_data_and_order_id(self, self._data)
        except:
            self.close()
            raise
        # 使用非空list,使得wait_update()能正确发送peek_message; 使用空dict, 使得is_changing()返回false, 因为截面数据不算做更新数据.
        self._diffs = [{}]

    def _print(self, msg: str = "", level: str = "INFO"):
        if self.disable_print:
            return
        dt = "" if self._backtest else datetime.now().strftime('%Y-%m-%d %H:%M:%S')
        level = level if isinstance(level, str) else logging.getLevelName(level)
        print(f"{(dt + ' - ') if dt else ''}{level:>8} - {msg}")

    def close(self) -> None:
        """
        关闭天勤接口实例并释放相应资源

        Example::

            # m1901开多3手
            from tqsdk import TqApi, TqAuth
            from contextlib import closing

            with closing(TqApi(auth=TqAuth("信易账户", "账户密码")) as api:
                api.insert_order(symbol="DCE.m1901", direction="BUY", offset="OPEN", volume=3)
        """
        if self._loop.is_closed():
            return
        if self._loop.is_running():
            raise Exception("不能在协程中调用 close, 如需关闭 api 实例需在 wait_update 返回后再关闭")
        elif asyncio._get_running_loop():
            raise Exception(
                "TqSdk 使用了 python3 的原生协程和异步通讯库 asyncio，您所使用的 IDE 不支持 asyncio, 请使用 pycharm 或其它支持 asyncio 的 IDE")

        # 总会发送 serial_extra_array 数据，由 TqWebHelper 处理
        for _, serial in self._serials.items():
            self._process_serial_extra_array(serial)
        self._run_until_idle()  # 由于有的处于 ready 状态 task 可能需要报撤单, 因此一直运行到没有 ready 状态的 task
        for task in self._tasks:
            task.cancel()
        while self._tasks:  # 等待 task 执行完成
            self._run_once()
        self._loop.run_until_complete(self._loop.shutdown_asyncgens())
        self._loop.close()
        mem = psutil.virtual_memory()
        self._logger.debug("process end", mem_total=mem.total, mem_free=mem.free)
        _clear_logs()  # 清除过期日志文件

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    # ----------------------------------------------------------------------
    def get_quote(self, symbol: str) -> Quote:
        """
        获取指定合约的盘口行情.

        Args:
            symbol (str): 指定合约代码。注意：天勤接口从0.8版本开始，合约代码格式变更为 交易所代码.合约代码 的格式. 可用的交易所代码如下：
                         * CFFEX: 中金所
                         * SHFE: 上期所
                         * DCE: 大商所
                         * CZCE: 郑商所
                         * INE: 能源交易所(原油)
                         * SSE: 上交所
                         * SZSE: 深交所

        Returns:
            :py:class:`~tqsdk.objs.Quote`: 返回一个盘口行情引用. 其内容将在 :py:meth:`~tqsdk.api.TqApi.wait_update` 时更新.

            注意: 在 tqsdk 还没有收到行情数据包时, 此对象中各项内容为 NaN 或 0

        Example::

            # 获取 SHFE.cu1812 合约的报价
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            quote = api.get_quote("SHFE.cu1812")
            print(quote.last_price)
            while api.wait_update():
                print(quote.last_price)

            # 预计的输出是这样的:
            nan
            24575.0
            24575.0
            ...
        """
        if self._loop.is_running():
            self.create_task(self._get_quote_async(symbol))  # 协程中需要等待合约信息，然后发送订阅请求
            return _get_obj(self._data, ["quotes", symbol], self._prototype["quotes"]["#"])
        else:
            self._ensure_symbol(symbol)
            quote = _get_obj(self._data, ["quotes", symbol], self._prototype["quotes"]["#"])
            if symbol not in self._requests["quotes"]:
                self._requests["quotes"].add(symbol)
                self._send_pack({
                    "aid": "subscribe_quote",
                    "ins_list": ",".join(self._requests["quotes"]),
                })
                deadline = time.time() + 30
                while quote["datetime"] == "":
                    # @todo: merge diffs
                    if not self.wait_update(deadline=deadline):
                        raise Exception(f"获取 {symbol} 的行情信息超时，请检查客户端及网络是否正常")
            return quote

    def _ensure_symbol(self, symbol):
        # 已经收到收到合约信息之后返回，同步
        if symbol in self._data.get("quotes", {}):
            return True
        if self._stock is False:
            raise Exception("代码 %s 不存在, 请检查合约代码是否填写正确" % (symbol))
        elif self._loop.is_running():
            self.create_task(self._ensure_symbol_async(symbol))
        else:
            query_pack = _query_for_quote(symbol)
            self._send_pack(query_pack)
            deadline = time.time() + 30
            while query_pack["query_id"] not in self._data.get("symbols", {}):
                if not self.wait_update(deadline=deadline):
                    raise Exception(f"获取 {symbol} 的合约信息超时，请检查客户端及网络是否正常，且合约代码填写正确")

    async def _ensure_symbol_async(self, symbol):
        # 已经收到收到合约信息之后返回，异步
        if symbol in self._data.get("quotes", {}):
            return True
        if self._stock is False:
            raise Exception("代码 %s 不存在, 请检查合约代码是否填写正确" % (symbol))
        else:
            query_pack = _query_for_quote(symbol)
            self._send_pack(query_pack)
            async with self.register_update_notify() as update_chan:
                async for _ in update_chan:
                    if query_pack["query_id"] in self._data.get("symbols", {}):
                        break

    async def _get_quote_async(self, symbol):
        # 协程中, 在收到合约信息之后再发送订阅行情请求，来保证，不会发送订阅请求去订阅不存在的合约
        await self._ensure_symbol_async(symbol)
        self._auth._has_md_grants(symbol)
        if symbol not in self._requests["quotes"]:
            self._requests["quotes"].add(symbol)
            self._send_pack({
                "aid": "subscribe_quote",
                "ins_list": ",".join(self._requests["quotes"]),
            })

    # ----------------------------------------------------------------------
    def get_kline_serial(self, symbol: Union[str, List[str]], duration_seconds: int, data_length: int = 200,
                         chart_id: Optional[str] = None, adj_type: Union[str, None] = None) -> pd.DataFrame:
        """
        获取k线序列数据

        请求指定合约及周期的K线数据. 序列数据会随着时间推进自动更新

        Args:
            symbol (str/list of str): 指定合约代码或合约代码列表.
                * str: 一个合约代码
                * list of str: 合约代码列表 （一次提取多个合约的K线并根据相同的时间向第一个合约（主合约）对齐)

            duration_seconds (int): K线数据周期, 以秒为单位。例如: 1分钟线为60,1小时线为3600,日线为86400。\
            注意: 周期在日线以内时此参数可以任意填写, 在日线以上时只能是日线(86400)的整数倍, 最大为28天 (86400*28)。

            data_length (int): 需要获取的序列长度。默认200根, 返回的K线序列数据是从当前最新一根K线开始往回取data_length根。\
            每个序列最大支持请求 8964 个数据

            chart_id (str): [可选]指定序列id, 默认由 api 自动生成

            adj_type (str/None): [可选]指定复权类型，默认为 None。adj_type 参数只对股票和基金类型合约有效。\
            "F" 表示前复权；"B" 表示后复权；None 表示不做处理。

            **注：关于传入合约代码列表 获取多合约K线的说明：**

                1 主合约的字段名为原始K线数据字段，从第一个副合约开始，字段名在原始字段后加数字，如第一个副合约的开盘价为 "open1" , 第二个副合约的收盘价为 "close2"。

                2 每条K线都包含了订阅的所有合约数据，即：如果任意一个合约（无论主、副）在某个时刻没有数据（即使其他合约在此时有数据）,\
                    则不能对齐，此多合约K线在该时刻那条数据被跳过，现象表现为K线不连续（如主合约有夜盘，而副合约无夜盘，则生成的多合约K线无夜盘时间的数据）。

                3 若设置了较大的序列长度参数，而所有可对齐的数据并没有这么多，则序列前面部分数据为NaN（这与获取单合约K线且数据不足序列长度时情况相似）。

                4 若主合约与副合约的交易时间在所有合约数据中最晚一根K线时间开始往回的 8964*周期 时间段内完全不重合，则无法生成多合约K线，程序会报出获取数据超时异常。

                5 datetime、duration是所有合约公用的字段，则未单独为每个副合约增加一份副本，这两个字段使用原始字段名（即没有数字后缀）。

                6 **暂不支持复权** 获取多合约K线，若填入 adj_type，程序会报参数错误。

        Returns:
            pandas.DataFrame: 本函数总是返回一个 pandas.DataFrame 实例. 行数=data_length, 包含以下列:

            * id: 1234 (k线序列号)
            * datetime: 1501080715000000000 (K线起点时间(按北京时间)，自unix epoch(1970-01-01 00:00:00 GMT)以来的纳秒数)
            * open: 51450.0 (K线起始时刻的最新价)
            * high: 51450.0 (K线时间范围内的最高价)
            * low: 51450.0 (K线时间范围内的最低价)
            * close: 51450.0 (K线结束时刻的最新价)
            * volume: 11 (K线时间范围内的成交量)
            * open_oi: 27354 (K线起始时刻的持仓量)
            * close_oi: 27355 (K线结束时刻的持仓量)

        Example1::

            # 获取 SHFE.cu1812 的1分钟线
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            klines = api.get_kline_serial("SHFE.cu1812", 60)
            print(klines.iloc[-1].close)
            while True:
                api.wait_update()
                print(klines.iloc[-1].close)

            # 预计的输出是这样的:
            50970.0
            50970.0
            50960.0
            ...

        Example2::

            # 获取按时间对齐的多合约K线
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            # 获取 CFFEX.IF1912 按照K线时间向 SHFE.au2006 对齐的K线
            klines = api.get_kline_serial(["SHFE.au2006", "CFFEX.IF2006"], 5, data_length=10)
            print("多合约K线：", klines.iloc[-1])
            while True:
                api.wait_update()
                if api.is_changing(klines.iloc[-1], ["close1", "close"]):  # 判断任何一个收盘价是否有更新
                    dif = klines.close1 - klines.close  # 使用对齐的K线直接计算价差等数据
                    print("价差序列：", dif)

        Example3::

            # 使用tqsdk自带的时间转换函数, 将最后一根K线的纳秒时间转换为 datetime.datetime 类型
            from tqsdk import tafunc
            ...

            klines = api.get_kline_serial("DCE.jd2001", 10)
            kline_time = tafunc.time_to_datetime(klines.iloc[-1]["datetime"])  # datetime.datetime 类型值
            print(type(kline_time), kline_time)
            print(kline_time.year, kline_time.month, kline_time.day, kline_time.hour, kline_time.minute, kline_time.second)
            ...
        """
        if not isinstance(symbol, list):
            symbol = [symbol]
        duration_seconds = int(duration_seconds)  # 转成整数
        if duration_seconds <= 0 or duration_seconds > 86400 and duration_seconds % 86400 != 0:
            raise Exception("K线数据周期 %d 错误, 请检查K线数据周期值是否填写正确" % (duration_seconds))
        data_length = int(data_length)
        if data_length <= 0:
            raise Exception("K线数据序列长度 %d 错误, 请检查序列长度是否填写正确" % (data_length))
        if adj_type not in [None, "F", "B", "FORWARD", "BACK"]:
            raise Exception("adj_type 参数只支持 None (不复权) ｜ 'F' (前复权) ｜ 'B' (后复权)")
        adj_type = adj_type[0] if adj_type else adj_type
        if adj_type and len(symbol) > 1:
            raise Exception("参数错误，多合约 K 线序列不支持复权。")
        if data_length > 8964:
            data_length = 8964
        dur_id = duration_seconds * 1000000000
        request = (tuple(symbol), duration_seconds, data_length, adj_type, chart_id)  # request 中 symbols 为 tuple 序列
        serial = self._requests["klines"].get(request, None)
        pack = {
            "aid": "set_chart",
            "chart_id": chart_id if chart_id is not None else _generate_uuid("PYSDK_realtime"),
            "ins_list": ",".join(symbol),
            "duration": dur_id,
            "view_width": data_length if len(symbol) == 1 else 8964,
            # 如果同时订阅了两个以上合约K线，初始化数据时默认获取 1w 根K线(初始化完成后修改指令为设定长度)
        }
        if self._loop.is_running():
            self.create_task(self._get_kline_serial_async(symbol, chart_id, serial, pack.copy()))
        else:
            for s in symbol:
                self._ensure_symbol(s)
                self._auth._has_md_grants(s)
            if serial is None or chart_id is not None:  # 判断用户是否指定了 chart_id（参数）, 如果指定了，则一定会发送新的请求。
                self._send_pack(pack.copy())  # 注意：将数据权转移给TqChan时其所有权也随之转移，因pack还需要被用到，所以传入副本
        if serial is None:
            serial = self._init_serial([_get_obj(self._data, ["klines", s, str(dur_id)]) for s in symbol],
                                       data_length, self._prototype["klines"]["*"]["*"]["data"]["@"], adj_type)
            serial["chart"] = _get_obj(self._data, ["charts", pack["chart_id"]])  # 保存chart信息
            serial["chart"].update(pack)
            self._requests["klines"][request] = serial
            self._serials[id(serial["df"])] = serial
        # 对于多合约Kline，超时的等待时间应该和需要下载的数据量成正比，根据合约数量判断下载的数据量
        deadline = time.time() + 25 + 5 * len(symbol)
        while not self._loop.is_running() and not serial["init"]:
            # @todo: merge diffs
            if not self.wait_update(deadline=deadline):
                if len(symbol) > 1:
                    raise Exception("获取 %s (%d) 的K线超时，请检查客户端及网络是否正常，或任一副合约在主合约行情的最后 %d 秒内无可对齐的K线" % (
                        symbol, duration_seconds, 8964 * duration_seconds))
                else:
                    raise Exception("获取 %s (%d) 的K线超时，请检查客户端及网络是否正常" % (symbol, duration_seconds))
        return serial["df"]

    async def _get_kline_serial_async(self, symbol, chart_id, serial, pack):
        for s in symbol:
            await self._ensure_symbol_async(s)
            self._auth._has_md_grants(s)
        if serial is None or chart_id is not None:
            self._send_pack(pack)

    # ----------------------------------------------------------------------
    def get_tick_serial(self, symbol: str, data_length: int = 200, chart_id: Optional[str] = None,
                        adj_type: Union[str, None] = None) -> pd.DataFrame:
        """
        获取tick序列数据

        请求指定合约的Tick序列数据. 序列数据会随着时间推进自动更新

        Args:
            symbol (str): 指定合约代码.

            data_length (int): 需要获取的序列长度。每个序列最大支持请求 8964 个数据

            chart_id (str): [可选]指定序列id, 默认由 api 自动生成

            adj_type (str/None): [可选]指定复权类型，默认为 None。adj_type 参数只对股票和基金类型合约有效。\
            "F" 表示前复权；"B" 表示后复权；None 表示不做处理。

        Returns:
            pandas.DataFrame: 本函数总是返回一个 pandas.DataFrame 实例. 行数=data_length, 包含以下列:

            * id: 12345 tick序列号
            * datetime: 1501074872000000000 (tick从交易所发出的时间(按北京时间)，自unix epoch(1970-01-01 00:00:00 GMT)以来的纳秒数)
            * last_price: 3887.0 (最新价)
            * average: 3820.0 (当日均价)
            * highest: 3897.0 (当日最高价)
            * lowest: 3806.0 (当日最低价)
            * ask_price1: 3886.0 (卖一价)
            * ask_volume1: 3 (卖一量)
            * bid_price1: 3881.0 (买一价)
            * bid_volume1: 18 (买一量)
            * volume: 7823 (当日成交量)
            * amount: 19237841.0 (成交额)
            * open_interest: 1941 (持仓量)

        Example::

            # 获取 SHFE.cu1812 的Tick序列
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            serial = api.get_tick_serial("SHFE.cu1812")
            while True:
                api.wait_update()
                print(serial.iloc[-1].bid_price1, serial.iloc[-1].ask_price1)

            # 预计的输出是这样的:
            50860.0 51580.0
            50860.0 51580.0
            50820.0 51580.0
            ...
        """
        data_length = int(data_length)
        if data_length <= 0:
            raise Exception("K线数据序列长度 %d 错误, 请检查序列长度是否填写正确" % (data_length))
        if adj_type not in [None, "F", "B", "FORWARD", "BACK"]:
            raise Exception("adj_type 参数只支持 None (不复权) ｜ 'F' (前复权) ｜ 'B' (后复权)")
        adj_type = adj_type[0] if adj_type else adj_type
        if data_length > 8964:
            data_length = 8964
        request = (symbol, data_length, adj_type, chart_id)
        serial = self._requests["ticks"].get(request, None)
        pack = {
            "aid": "set_chart",
            "chart_id": chart_id if chart_id is not None else _generate_uuid("PYSDK_realtime"),
            "ins_list": symbol,
            "duration": 0,
            "view_width": data_length,
        }
        if self._loop.is_running():
            self.create_task(self._get_tick_serial_async(symbol, chart_id, serial, pack.copy()))
        else:
            self._ensure_symbol(symbol)
            self._auth._has_md_grants(symbol)
            if serial is None or chart_id is not None:  # 判断用户是否指定了 chart_id（参数）, 如果指定了，则一定会发送新的请求。
                self._send_pack(pack.copy())  # pack 的副本数据和所有权转移给TqChan
        if serial is None:
            serial = self._init_serial([_get_obj(self._data, ["ticks", symbol])], data_length,
                                       self._prototype["ticks"]["*"]["data"]["@"], adj_type)
            serial["chart"] = _get_obj(self._data, ["charts", pack["chart_id"]])
            serial["chart"].update(pack)
            self._requests["ticks"][request] = serial
            self._serials[id(serial["df"])] = serial
        deadline = time.time() + 30
        while not self._loop.is_running() and not serial["init"]:
            # @todo: merge diffs
            if not self.wait_update(deadline=deadline):
                raise Exception("获取 %s 的Tick超时，请检查客户端及网络是否正常，且合约代码填写正确" % (symbol))
        return serial["df"]

    async def _get_tick_serial_async(self, symbol, chart_id, serial, pack):
        await self._ensure_symbol_async(symbol)
        self._auth._has_md_grants(symbol)
        if serial is None or chart_id is not None:
            self._send_pack(pack)

    # ----------------------------------------------------------------------
    def get_kline_data_series(self, symbol: Union[str, List[str]], duration_seconds: int,
                              start_dt: Union[date, datetime], end_dt: Union[date, datetime],
                              adj_type: Union[str, None] = None) -> pd.DataFrame:
        """
        获取指定时间段内的 K 线序列，TqSdk 会缓存已经下载过的合约，提升代码执行效率、节约请求流量。

        本接口仅限专业版用户使用，如需购买专业版或者申请试用，请访问 https://www.shinnytech.com/tqsdk_professional/。

        该函数返回的对象不会更新，不建议在循环内调用该方法。

        Args:
            symbol (str): 指定合约代码。当前只支持单个合约。

            duration_seconds (int): K 线数据周期, 以秒为单位。例如: 1 分钟线为 60，1 小时线为 3600，日线为 86400。\
            注意: 周期在日线以内时此参数可以任意填写, 在日线以上时只能是日线(86400)的整数倍

            start_dt (date/datetime): 起始时间, 如果类型为 date 则指的是交易日, 如果为 datetime 则指的是具体时间点

            end_dt (date/datetime): 结束时间, 如果类型为 date 则指的是交易日, 如果为 datetime 则指的是具体时间点

            adj_type (str/None): [可选]指定复权类型，默认为 None。adj_type 参数只对股票和基金类型合约有效。\
            "F" 表示前复权；"B" 表示后复权；None 表示不做处理。

        Returns:
            pandas.DataFrame: 本函数总是返回一个 pandas.DataFrame 实例。包含以下列:

            * id: 1234 (k线序列号)
            * datetime: 1501080715000000000 (K线起点时间(按北京时间)，自unix epoch(1970-01-01 00:00:00 GMT)以来的纳秒数)
            * open: 51450.0 (K线起始时刻的最新价)
            * high: 51450.0 (K线时间范围内的最高价)
            * low: 51450.0 (K线时间范围内的最低价)
            * close: 51450.0 (K线结束时刻的最新价)
            * volume: 11 (K线时间范围内的成交量)
            * open_oi: 27354 (K线起始时刻的持仓量)
            * close_oi: 27355 (K线结束时刻的持仓量)

        Example::

            # 获取 SHFE.cu1805 合约 20180101-06:00:00 ~ 20180601-16:00:00 的 1 分钟线
            from tqsdk import TqApi, TqAuth
            from tqsdk.ta import MA, MACD

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))

            kline_data = api.get_kline_data_series(symbol = "SHFE.cu1805", duration_seconds=60,
                        start_dt = datetime(2018, 1, 1, 6, 0, 0), end_dt = datetime(2018, 6, 1, 16, 0, 0))
            print(kline_data)

            ma = MA(kline_data, 30)  # 计算 MA 指标
            print(list(ma["ma"]))

            macd = MACD(kline_data, 12, 26, 9)  # 计算 MACD 指标
            print(list(macd["diff"]))
            print(list(macd["dea"]))
            print(list(macd["bar"]))

            api.close()

        """
        if duration_seconds <= 0 or duration_seconds > 86400 and duration_seconds % 86400 != 0:
            raise Exception("K线数据周期 %d 错误, 请检查K线数据周期值是否填写正确" % duration_seconds)
        return self._get_data_series("get_kline_data_series", symbol, duration_seconds, start_dt, end_dt, adj_type)

    # ----------------------------------------------------------------------
    def get_tick_data_series(self, symbol: Union[str, List[str]], start_dt: Union[date, datetime],
                             end_dt: Union[date, datetime], adj_type: Union[str, None] = None) -> pd.DataFrame:
        """
        获取指定时间段内的 tick 序列，TqSdk 会缓存已经下载过的合约，提升代码执行效率、节约请求流量。

        本接口仅限专业版用户使用，如需购买专业版或者申请试用，请访问 https://www.shinnytech.com/tqsdk_professional/。

        该函数返回的对象不会更新，不建议在循环内调用该方法。

        Args:
            symbol (str): 指定合约代码。当前只支持单个合约。

            start_dt (date/datetime): 起始时间, 如果类型为 date 则指的是交易日, 如果为 datetime 则指的是具体时间点

            end_dt (date/datetime): 结束时间, 如果类型为 date 则指的是交易日, 如果为 datetime 则指的是具体时间点

            adj_type (str/None): [可选]指定复权类型，默认为 None。adj_type 参数只对股票和基金类型合约有效。\
            "F" 表示前复权；"B" 表示后复权；None 表示不做处理。

        Returns:
            pandas.DataFrame: 本函数总是返回一个 pandas.DataFrame 实例。包含以下列:

            * id: 12345 tick序列号
            * datetime: 1501074872000000000 (tick从交易所发出的时间(按北京时间)，自unix epoch(1970-01-01 00:00:00 GMT)以来的纳秒数)
            * last_price: 3887.0 (最新价)
            * average: 3820.0 (当日均价)
            * highest: 3897.0 (当日最高价)
            * lowest: 3806.0 (当日最低价)
            * ask_price1: 3886.0 (卖一价)
            * ask_volume1: 3 (卖一量)
            * bid_price1: 3881.0 (买一价)
            * bid_volume1: 18 (买一量)
            * volume: 7823 (当日成交量)
            * amount: 19237841.0 (成交额)
            * open_interest: 1941 (持仓量)

        Example::

            # 获取 SHFE.cu1805 合约 20180201-06:00:00 ~ 20180301-16:00:00 的 tick 数据
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))

            tick_data = api.get_tick_data_series(symbol = "SHFE.cu1805",
                        start_dt = datetime(2018, 2, 1, 6, 0, 0), end_dt = datetime(2018, 3, 1, 16, 0, 0))
            print(tick_data)

            ma = MA(tick_data, 30)  # 计算 MA 指标
            print(list(ma["ma"]))

            api.close()

        """
        return self._get_data_series("get_tick_data_series", symbol, 0, start_dt, end_dt, adj_type)

    def _get_data_series(self, call_func: str, symbol_list: Union[str, List[str]], duration_seconds: int,
                         start_dt: Union[date, datetime], end_dt: Union[date, datetime],
                         adj_type: Union[str, None] = None) -> pd.DataFrame:
        if self._loop.is_running():
            raise Exception(f"不支持在协程中调用 {call_func} 接口")
        if not self._auth._has_feature("tq_dl"):
            raise Exception(
                f"{call_func} 数据获取方式仅限专业版用户使用，如需购买专业版或者申请试用，请访问 https://www.shinnytech.com/tqsdk_professional/")
        dur_nano = duration_seconds * 1000000000
        symbol_list = symbol_list if isinstance(symbol_list, list) else [symbol_list]
        if len(symbol_list) != 1:
            raise Exception(f"{call_func} 数据获取方式暂不支持多合约请求")
        self._ensure_symbol(symbol_list[0])  # 检查合约代码是否存在
        if isinstance(start_dt, datetime):
            start_dt_nano = int(start_dt.timestamp() * 1e9)
        elif isinstance(start_dt, date):
            start_dt_nano = _get_trading_day_start_time(
                int(datetime(start_dt.year, start_dt.month, start_dt.day).timestamp() * 1e9))
        else:
            raise Exception(f"start_dt 参数类型 {type(start_dt)} 错误, 只支持 datetime / date 类型，请检查是否正确")
        if isinstance(end_dt, datetime):
            end_dt_nano = int(end_dt.timestamp() * 1e9)
        elif isinstance(end_dt, date):
            end_dt_nano = _get_trading_day_end_time(
                int(datetime(end_dt.year, end_dt.month, end_dt.day).timestamp() * 1e9))
        else:
            raise Exception(f"end_dt 参数类型 {type(end_dt)} 错误, 只支持 datetime / date 类型，请检查是否正确")
        if adj_type not in [None, "F", "B"]:
            raise Exception("adj_type 参数只支持 None (不复权) ｜ 'F' (前复权) ｜ 'B' (后复权) ")
        ds = DataSeries(self, symbol_list, dur_nano, start_dt_nano, end_dt_nano, adj_type)
        while not self._loop.is_running() and not ds.is_ready:
            deadline = time.time() + 30
            if not self.wait_update(deadline=deadline):
                raise Exception(
                    f"{call_func} 获取数据 ({symbol_list, duration_seconds, start_dt, end_dt}) 超时，请检查客户端及网络是否正常。")
        return ds.df

    # ----------------------------------------------------------------------
    def get_trading_calendar(self, start_dt: Union[date, datetime], end_dt: Union[date, datetime]):
        """
        获取一段时间内的交易日历信息，交易日历可以处理的范围为 2003-01-01 ～ 2021-12-31。

        Args:
            start_dt (date/datetime): 起始时间，如果类型为 date 则指的是该日期；如果为 datetime 则指的是该时间点所在日期

            end_dt (date/datetime): 结束时间，如果类型为 date 则指的是该日期；如果为 datetime 则指的是该时间点所在日期

        Returns:
            pandas.DataFrame: 包含以下列:

            *    date: (datetime64[ns]) 日期
            * trading:           (bool) 是否是交易日

        Example::

            from datetime import date
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            cld = api.get_trading_calendar(start_dt=date(2021,2,1), end_dt=date(2021,3,1))
            print(cld)

            # 预期输出如下
            #         date  trading
            # 0  2021-02-01    True
            # 1  2021-02-02    True
            # 2  2021-02-03    True
            # ......
            # 26 2021-02-27   False
            # 27 2021-02-28   False
            # 28 2021-03-01    True

            api.close()
        """
        if isinstance(start_dt, datetime):
            start_dt = date(year=start_dt.year, month=start_dt.month, day=start_dt.day)
        elif not isinstance(start_dt, date):
            raise Exception(f"start_dt 参数类型 {type(start_dt)} 错误, 只支持 datetime / date 类型，请检查是否正确")
        if isinstance(end_dt, datetime):
            end_dt = date(year=end_dt.year, month=end_dt.month, day=end_dt.day)
        elif not isinstance(end_dt, date):
            raise Exception(f"end_dt 参数类型 {type(end_dt)} 错误, 只支持 datetime / date 类型，请检查是否正确")
        if start_dt < date(2003, 1, 1) or end_dt > date(2021, 12, 31):
            raise Exception(f"交易日历可以处理的范围为 2003-01-01 ～ 2021-12-31，请修改参数")
        return _get_trading_calendar(start_dt, end_dt)

    # ----------------------------------------------------------------------
    def insert_order(self, symbol: str, direction: str, offset: str = "", volume: int = 0,
                     limit_price: Union[str, float, None] = None,
                     advanced: Optional[str] = None, order_id: Optional[str] = None, account: Optional[Union[
                TqAccount, TqKq, TqSim]] = None) -> Order:
        """
        发送下单指令. **注意: 指令将在下次调用** :py:meth:`~tqsdk.api.TqApi.wait_update` **时发出**

        Args:
            symbol (str): 拟下单的合约symbol, 格式为 交易所代码.合约代码,  例如 "SHFE.cu1801"

            direction (str): "BUY" 或 "SELL"

            offset (str): "OPEN", "CLOSE" 或 "CLOSETODAY"  \
            (上期所和原油分平今/平昨, 平今用"CLOSETODAY", 平昨用"CLOSE"; 其他交易所直接用"CLOSE" 按照交易所的规则平仓), \
            股票交易中该参数无需填写

            volume (int): 下单交易数量, 期货为下单手数, A 股股票为股数

            limit_price (float | str): [可选] 下单价格, 默认为 None, 股票交易目前仅支持限价单, 该字段必须指定。
                * 数字类型: 限价单，按照限定价格或者更优价格成交
                * None: 市价单，默认值就是市价单 (郑商所期货/期权、大商所期货支持)
                * "BEST": 最优一档，以对手方实时最优一档价格为成交价格成交（仅中金所支持）
                * "FIVELEVEL": 最优五档，在对手方实时最优五个价位内以对手方价格为成交价格依次成交（仅中金所支持）

            advanced (str): [可选] "FAK", "FOK"。默认为 None, 股票交易中该参数不支持。
                * None: 对于限价单，任意手数成交，委托单当日有效；对于市价单、最优一档、最优五档(与 FAK 指令一致)，任意手数成交，剩余撤单。
                * "FAK": 剩余即撤销，指在指定价位成交，剩余委托自动被系统撤销。(限价单、市价单、最优一档、最优五档有效)
                * "FOK": 全成或全撤，指在指定价位要么全部成交，否则全部自动被系统撤销。(限价单、市价单有效，郑商所期货品种不支持 FOK)

            order_id (str): [可选]指定下单单号, 默认由 api 自动生成, 股票交易下单时, 无需指定

            account (TqAccount/TqKq/TqSim): [可选]指定发送下单指令的账户实例, 多账户模式下，该参数必须指定

        Returns:
            :py:class:`~tqsdk.objs.Order`: 返回一个委托单对象引用. 其内容将在 :py:meth:`~tqsdk.api.TqApi.wait_update` 时更新.

        Example1::

            # 市价开3手 DCE.m1809 多仓
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            order = api.insert_order(symbol="DCE.m1809", direction="BUY", offset="OPEN", volume=3)
            while True:
                api.wait_update()
                print("单状态: %s, 已成交: %d 手" % (order.status, order.volume_orign - order.volume_left))

            # 预计的输出是这样的:
            单状态: ALIVE, 已成交: 0 手
            单状态: ALIVE, 已成交: 0 手
            单状态: FINISHED, 已成交: 3 手
            ...

        Example2::

            # 限价开多3手 DCE.m1901
            from tqsdk import TqApi, TqAuth
            with TqApi(auth=TqAuth("信易账户", "账户密码")) as api:
                order = api.insert_order(symbol="DCE.m2009", direction="BUY", offset="OPEN", volume=3, limit_price=3000)
                while True:
                    api.wait_update()
                    print("单状态: %s, 已成交: %d 手" % (order.status, order.volume_orign - order.volume_left))

            # 预计的输出是这样的:
            单状态: ALIVE, 已成交: 0 手
            单状态: ALIVE, 已成交: 0 手
            单状态: FINISHED, 已成交: 3 手
            ...

        Example3::

            # 市价开多3手 DCE.m1901 FAK
            from tqsdk import TqApi, TqAuth
            with TqApi(auth=TqAuth("信易账户", "账户密码")) as api:
                order = api.insert_order(symbol="DCE.m2009", direction="BUY", offset="OPEN", volume=3, advanced="FAK")
                while True:
                    api.wait_update()
                    print("单状态: %s, 已成交: %d 手" % (order.status, order.volume_orign - order.volume_left))

            # 预计的输出是这样的:
            单状态: ALIVE, 已成交: 0 手
            单状态: ALIVE, 已成交: 0 手
            单状态: FINISHED, 已成交: 3 手
            ...

        Example4::

            # 多账户模式下, 使用不同期货公司交易账户进行下单操作
            from tqsdk import TqApi, TqAuth, TqMultiAccount

            account1 = TqAccount("H海通期货", "123456", "123456")
            account2 = TqAccount("H宏源期货", "123456", "123456")
            with TqApi(TqMultiAccount([account1, account2]), auth=TqAuth("信易账户", "账户密码")) as api:
                order1 = api.insert_order(symbol="DCE.m2101", direction="BUY", offset="OPEN", volume=3, account=account1)
                order2 = api.insert_order(symbol="DCE.m2103", direction="BUY", offset="OPEN", volume=3, account=account2)
                while order1.status != "FINISHED" or order2.status != "FINISHED":
                    api.wait_update()
                    print("委托单1已成交: %d 手, 委托单2已成交: %d 手", order1.volume_orign - order1.volume_left,
                    order2.volume_orign - order2.volume_left)

            # 预计的输出是这样的:
            委托单1已成交: 0 手, 委托单2已成交: 3 手
            ...

        """
        (exchange_id, instrument_id) = symbol.split(".", 1)
        if not self._account._check_valid(account):
            raise Exception(f"多账户模式下, 需要指定账户实例 account")
        if direction not in ("BUY", "SELL"):
            raise Exception("下单方向(direction) %s 错误, 请检查 direction 参数是否填写正确" % (direction))
        volume = int(volume)
        if volume <= 0:
            raise Exception("下单数量(volume) %s 错误, 请检查 volume 是否填写正确" % (volume))
        # 股票下单时, 不支持 order_id 和 offset 参数
        if exchange_id in ["SSE", "SZSE"] and self._account._is_stock_type(account):
            if order_id:
                raise Exception("沪深A股下单委托单号不支持自定义")
            if offset:
                raise Exception("股票交易无需指定开平标志" % (offset))
            order_id = self._account._get_order_id(account)
            offset = "UNKNOWN"
        else:
            if offset not in ("OPEN", "CLOSE", "CLOSETODAY"):
                raise Exception("开平标志(offset) %s 错误, 请检查 offset 是否填写正确" % (offset))
            order_id = order_id if order_id else _generate_uuid("PYSDK_insert")

        if self._loop.is_running():
            # 需要在异步代码中发送合约信息请求和下单请求
            self.create_task(
                self._insert_order_async(symbol, direction, offset, volume, limit_price, advanced, order_id, account))
            order = self.get_order(order_id, account=account)
            order.update({
                "order_id": order_id,
                "exchange_id": exchange_id,
                "instrument_id": instrument_id,
                "direction": direction,
                "offset": offset,
                "volume_orign": volume,
                "volume_left": volume,
                "status": "ALIVE",
                "_this_session": True
            })
            return order
        else:
            self._ensure_symbol(symbol)  # 合约是否存在
            self._auth._has_td_grants(symbol)  # 用户是否有该合约交易权限
            if isinstance(account, TqKq) and exchange_id in ["SSE", "SZSE"]:  # 快期模拟暂不支持，提前抛错
                raise Exception(f"快期模拟暂不支持股票及 ETF 期权交易。")
            pack = self._get_insert_order_pack(symbol, direction, offset, volume, limit_price, advanced, order_id, account)
            self._send_pack(pack)
            order = self.get_order(order_id, account=account)
            order.update({
                "order_id": order_id,
                "exchange_id": exchange_id,
                "instrument_id": instrument_id,
                "direction": direction,
                "offset": offset,
                "volume_orign": volume,
                "volume_left": volume,
                "status": "ALIVE",
                "_this_session": True,
                "limit_price": pack.get("limit_price", float("nan")),
                "price_type": pack["price_type"],
                "volume_condition": pack["volume_condition"],
                "time_condition": pack["time_condition"]
            })
            return order

    def _get_insert_order_pack(self, symbol, direction, offset, volume, limit_price, advanced, order_id,
                               account: Optional[Union[TqAccount, TqKq, TqSim]] = None):
        quote = self._data["quotes"][symbol]
        (exchange_id, instrument_id) = symbol.split(".", 1)
        msg = {
            "aid": "insert_order",
            "account_key": self._account._get_account_key(account),
            "user_id": self._account._get_account_id(account),
            "order_id": order_id,
            "exchange_id": exchange_id,
            "instrument_id": instrument_id,
            "direction": direction,
            "offset": offset,
            "volume": volume
        }
        if limit_price == "BEST" or limit_price == "FIVELEVEL":
            if exchange_id != "CFFEX":
                raise Exception(f"{symbol} 不支持 {limit_price} 市价单，请修改 limit_price 参数。仅中金所支持 BESE / FIVELEVEL")
            if exchange_id in ["CFFEX"] and advanced == "FOK":
                raise Exception(f"{symbol} 不支持 advanced 为 \"FOK\"。中金所不支持 FOK.")
            msg["price_type"] = limit_price
            msg["time_condition"] = "IOC"
        elif limit_price is None:
            if exchange_id in ["CFFEX", "SHFE", "INE", "SSE", "SZSE"]:
                raise Exception(f"{symbol} 不支持市价单，请使用 limit_price 参数指定价格。中金所、上期所、原油交易所、上交所、深交所不支持市价单。")
            if exchange_id == "DCE" and quote.ins_class in ["OPTION", "FUTURE_OPTION"]:
                raise Exception(f"{symbol} 不支持市价单，请使用 limit_price 参数指定价格。大商所期权不支持市价单。")
            if advanced == "FOK" and exchange_id == "CZCE" and quote.ins_class == "FUTURE":
                raise Exception(f"{symbol} 不支持 advanced 为 \"FOK\"。郑商所期货品种不支持 FOK。")
            msg["price_type"] = "ANY"
            msg["time_condition"] = "IOC"
        else:
            if advanced == "FOK" and exchange_id == "CZCE" and quote.ins_class == "FUTURE":
                raise Exception(f"{symbol} 不支持 advanced 为 \"FOK\"。郑商所期货品种不支持 FOK。")
            if advanced == "FAK" and exchange_id in ["SZSE", "SSE"] and quote.ins_class.endswith("OPTION"):
                raise Exception(f"{symbol} 不支持 advanced 为 \"FAK\"。上交所、深交所限价单不支持 FAK。")
            msg["price_type"] = "LIMIT"
            msg["limit_price"] = float(limit_price)
            msg["time_condition"] = "IOC" if advanced else "GFD"
        msg["volume_condition"] = "ALL" if advanced == "FOK" else "ANY"

        return msg

    async def _insert_order_async(self, symbol, direction, offset, volume, limit_price, advanced, order_id,
                                  account: Optional[Union[TqAccount, TqKq, TqSim]] = None):
        await self._ensure_symbol_async(symbol)  # 合约是否存在
        self._auth._has_td_grants(symbol)  # 用户是否有该合约交易权限
        (exchange_id, instrument_id) = symbol.split(".", 1)
        if isinstance(account, TqKq) and exchange_id in ["SSE", "SZSE"]:  # 快期模拟暂不支持，提前抛错
            raise Exception(f"快期模拟暂不支持股票及 ETF 期权交易。")
        pack = self._get_insert_order_pack(symbol, direction, offset, volume, limit_price, advanced, order_id, account)
        self._send_pack(pack)

    # ----------------------------------------------------------------------
    def cancel_order(self, order_or_order_id: Union[str, Order],
                     account: Optional[Union[TqAccount, TqKq, TqSim]] = None) -> None:
        """
        发送撤单指令. **注意: 指令将在下次调用** :py:meth:`~tqsdk.api.TqApi.wait_update` **时发出**

        Args:
            order_or_order_id (str/ :py:class:`~tqsdk.objs.Order` ): 拟撤委托单或单号

            account (TqAccount/TqKq/TqSim): [可选]指定发送撤单指令的账户实例, 多账户模式下, 该参数必须指定

        Example1::

            # 挂价开3手 DCE.m1809 多仓, 如果价格变化则撤单重下，直到全部成交
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            quote = api.get_quote("DCE.m1809")
            order = {}

            while True:
                api.wait_update()
                # 当行情有变化且当前挂单价格不优时，则撤单
                if order and api.is_changing(quote) and order.status == "ALIVE" and quote.bid_price1 > order.limit_price:
                    print("价格改变，撤单重下")
                    api.cancel_order(order)
                # 当委托单已撤或还没有下单时则下单
                if (not order and api.is_changing(quote)) or (api.is_changing(order) and order.volume_left != 0 and order.status == "FINISHED"):
                    print("下单: 价格 %f" % quote.bid_price1)
                    order = api.insert_order(symbol="DCE.m1809", direction="BUY", offset="OPEN", volume=order.get("volume_left", 3), limit_price=quote.bid_price1)
                if api.is_changing(order):
                    print("单状态: %s, 已成交: %d 手" % (order.status, order.volume_orign - order.volume_left))


            # 预计的输出是这样的:
            下单: 价格 3117.000000
            单状态: ALIVE, 已成交: 0 手
            价格改变，撤单重下
            下单: 价格 3118.000000
            单状态: ALIVE, 已成交: 0 手
            单状态: FINISHED, 已成交: 3 手
            ...

        Example2::

            # 多账户条件下, 股票账户依据期货账户下单结果进行操作
            from tqsdk import TqApi, TqAuth, TqMultiAccount

            future_account = TqAccount("N南华期货", "123456", "123456")
            stock_account = TqAccount("N南华期货_股票", "88888888", "123456")
            api = TqApi(TqMultiAccount([account1, account2]), auth=TqAuth("信易账户", "账户密码"))
            quote = api.get_quote("CFFEX.IF2011")
            order1 = api.insert_order(symbol="CFFEX.IF2011", direction="SELL", offset="OPEN", volume=3, account=future_account)
            while True:
                api.wait_update()
                # 当行情有变化且当前挂单价格不优时，则撤单
                if order1.status == "ALIVE" and api.is_changing(quote) and quote.bid_price1 > order.limit_price:
                    api.cancel_order(order1, future_account)
                # 当期货账户下单成功后, 操作股票账户进行下单
                if order1.status == "FINISHED" and order1.volume_left == 0:
                    order2 = api.insert_order(symbol="SSE.10002504", direction="BUY", volume=volume, account=stock_account)

            api.close()
            ...

        """
        if not self._account._check_valid(account):
            raise Exception(f"多账户模式下, 需要指定账户实例 account")

        if isinstance(order_or_order_id, (Order, SecurityOrder)):
            order_id = order_or_order_id.order_id
        else:
            order_id = order_or_order_id
        msg = {
            "aid": "cancel_order",
            "account_key": self._account._get_account_key(account),
            "user_id": self._account._get_account_id(account),
            "order_id": order_id,
        }
        self._send_pack(msg)

    # ----------------------------------------------------------------------
    def get_account(self, account: Optional[Union[TqAccount, TqKq, TqSim]] = None) -> Account:
        """
        获取用户账户资金信息
        Args:
            account (TqAccount/TqKq/TqSim): [可选]指定获取账户资金信息的账户实例, 多账户模式下, 该参数必须指定

        Returns:
            :py:class:`~tqsdk.objs.Account`: 返回一个账户对象引用. 其内容将在 :py:meth:`~tqsdk.api.TqApi.wait_update` 时更新.

        Example1::

            # 获取当前浮动盈亏
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            account = api.get_account()
            print(account.float_profit)

            # 预计的输出是这样的:
            2180.0
            2080.0
            2080.0
            ...

        Example2::

            # 多账户模式下, 分别获取各账户浮动盈亏
            from tqsdk import TqApi, TqAuth, TqMultiAccount

            account1 = TqAccount("N南华期货", "123456", "123456")
            account2 = TqAccount("H宏源期货", "111111", "123456")
            api = TqApi(TqMultiAccount([account1, account2]), auth=TqAuth("信易账户", "账户密码"))
            account_info1 = api.get_account(account=account1)
            account_info2 = api.get_account(account=account2)
            print("账户 1 浮动盈亏 %f, 账户 2 浮动盈亏 %f", account_info1.float_profit, account_info2.float_profit)
            api.close()

            # 预计的输出是这样的:
            账户 1 浮动盈亏 20580.0, 账户 2 浮动盈亏 -7390.0
            ...

        """
        if not self._account._check_valid(account):
            raise Exception(f"多账户模式下, 需要指定账户实例 account")
        prototype = self._security_prototype if self._account._is_stock_type(account) else self._prototype
        return _get_obj(self._data, ["trade", self._account._get_account_key(account), "accounts", "CNY"],
                        prototype["trade"]["*"]["accounts"]["@"])

    # ----------------------------------------------------------------------
    def get_position(self, symbol: Optional[str] = None, account: Optional[Union[TqAccount, TqKq, TqSim]] = None) -> \
            Union[Position, Entity]:
        """
        获取用户持仓信息

        Args:
            symbol (str): [可选]合约代码, 不填则返回所有持仓

            account (TqAccount/TqKq/TqSim): [可选]指定获取持仓信息的账户实例, 多账户模式下, 必须指定

        Returns:
            :py:class:`~tqsdk.objs.Position`: 当指定了 symbol 时, 返回一个持仓对象引用.
            其内容将在 :py:meth:`~tqsdk.api.TqApi.wait_update` 时更新.

            不填 symbol 参数调用本函数, 将返回包含用户所有持仓的一个tqsdk.objs.Entity对象引用, 使用方法与dict一致, \
            其中每个元素的key为合约代码, value为 :py:class:`~tqsdk.objs.Position`。

            注意: 为保留一些可供用户查询的历史信息, 如 volume_long_yd(本交易日开盘前的多头持仓手数) 等字段, 因此服务器会返回当天已平仓合约( pos_long 和 pos_short 等字段为0)的持仓信息

        Example1::

            # 获取 DCE.m1809 当前浮动盈亏
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            position = api.get_position("DCE.m1809")
            print(position.float_profit_long + position.float_profit_short)
            while api.wait_update():
                print(position.float_profit_long + position.float_profit_short)

            # 预计的输出是这样的:
            300.0
            330.0
            ...

        Example2::

            # 多账户模式下, 分别获取各账户浮动盈亏
            from tqsdk import TqApi, TqAuth, TqMultiAccount

            account1 = TqAccount("N南华期货", "123456", "123456")
            account2 = TqAccount("N宏源期货", "123456", "123456")
            api = TqApi(TqMultiAccount([account1, account2]), auth=TqAuth("信易账户", "账户密码"))
            position1 = api.get_position("DCE.m2101", account=account1)
            position2 = api.get_position("DCE.m2101", account=account2)
            print("账户 1 浮动盈亏 %f, 账户 2 浮动盈亏 %f", position1.float_profit_long + position2.float_profit_short,
                  position1.float_profit_long + position2.float_profit_short)
            api.close()

            # 预计的输出是这样的:
            账户 1 浮动盈亏 2140.0, 账户 2 浮动盈亏 0.00
            ...

        """
        if not self._account._check_valid(account):
            raise Exception(f"多账户模式下, 需要指定账户实例 account")
        prototype = self._security_prototype if self._account._is_stock_type(account) else self._prototype
        if symbol:
            if self._stock and self._loop.is_running():
                self.create_task(self._ensure_symbol_async(symbol))
            else:
                self._ensure_symbol(symbol)
            return _get_obj(self._data, ["trade", self._account._get_account_key(account), "positions", symbol],
                            prototype["trade"]["*"]["positions"]["@"])
        return _get_obj(self._data, ["trade", self._account._get_account_key(account), "positions"])

    # ----------------------------------------------------------------------
    def get_order(self, order_id: Optional[str] = None, account: Optional[Union[TqAccount, TqKq, TqSim]] = None) -> \
            Union[Order, Entity]:
        """
        获取用户委托单信息

        Args:
            order_id (str): [可选]单号, 不填单号则返回所有委托单

            account (TqAccount/TqKq/TqSim): [可选]指定获取委托单号的账户实例, 多账户模式下, 该参数必须指定

        Returns:
            :py:class:`~tqsdk.objs.Order`: 当指定了order_id时, 返回一个委托单对象引用. \
            其内容将在 :py:meth:`~tqsdk.api.TqApi.wait_update` 时更新.

            不填order_id参数调用本函数, 将返回包含用户所有委托单的一个tqsdk.objs.Entity对象引用, \
            使用方法与dict一致, 其中每个元素的key为委托单号, value为 :py:class:`~tqsdk.objs.Order`

            注意: 在刚下单后, tqsdk 还没有收到回单信息时, 此对象中各项内容为空

        Example1::

            # 获取当前总挂单手数
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            orders = api.get_order()
            while True:
                api.wait_update()
                print(sum(order.volume_left for oid, order in orders.items() if order.status == "ALIVE"))

            # 预计的输出是这样的:
            3
            3
            0
            ...

        Example2::

            # 多账户模式下, 分别获取各账户浮动盈亏
            from tqsdk import TqApi, TqAuth, TqMultiAccount

            account1 = TqAccount("N南华期货", "123456", "123456")
            account2 = TqAccount("N宏源期货", "123456", "123456")
            api = TqApi(TqMultiAccount([account1, account2]), auth=TqAuth("信易账户", "账户密码"))
            orders1 = api.get_order(account=account1))
            order2 = api.get_order(order_id="订单号", account=account2)
            print(len(orders1), order2.volume_left)
            api.close()

            # 预计的输出是这样的:
            2 0
            ...

        """
        prototype = self._security_prototype if self._account._is_stock_type(account) else self._prototype
        if order_id:
            return _get_obj(self._data, ["trade", self._account._get_account_key(account), "orders", order_id],
                            prototype["trade"]["*"]["orders"]["@"])
        return _get_obj(self._data, ["trade", self._account._get_account_key(account), "orders"])

    # ----------------------------------------------------------------------
    def get_trade(self, trade_id: Optional[str] = None, account: Optional[Union[TqAccount, TqKq, TqSim]] = None) -> \
            Union[Trade, Entity]:
        """
        获取用户成交信息

        Args:
            trade_id (str): [可选]成交号, 不填成交号则返回所有委托单

            account (TqAccount/TqKq/TqSim): [可选]指定获取用户成交信息的账户实例, 多账户模式下, 该参数必须指定

        Returns:
            :py:class:`~tqsdk.objs.Trade`: 当指定了trade_id时, 返回一个成交对象引用. \
            其内容将在 :py:meth:`~tqsdk.api.TqApi.wait_update` 时更新.

            不填trade_id参数调用本函数, 将返回包含用户当前交易日所有成交记录的一个tqsdk.objs.Entity对象引用, 使用方法与dict一致, \
            其中每个元素的key为成交号, value为 :py:class:`~tqsdk.objs.Trade`

            推荐优先使用 :py:meth:`~tqsdk.objs.Order.trade_records` 获取某个委托单的相应成交记录, 仅当确有需要时才使用本函数.

        Example::

            # 多账户模式下, 分别获取各账户浮动盈亏
            from tqsdk import TqApi, TqAuth, TqMultiAccount

            account1 = TqAccount("N南华期货", "123456", "123456")
            account2 = TqAccount("N宏源期货", "123456", "123456")
            api = TqApi(TqMultiAccount([account1, account2]), auth=TqAuth("信易账户", "账户密码"))
            orders1 = api.get_trade(account=account1))
            orders2 = api.get_trade(account=account2)
            print(len(orders1), len(orders2))
            api.close()

            # 预计的输出是这样的:
            20 55
            ...

        """
        prototype = self._security_prototype if self._account._is_stock_type(account) else self._prototype
        if trade_id:
            return _get_obj(self._data, ["trade", self._account._get_account_key(account), "trades", trade_id],
                            prototype["trade"]["*"]["trades"]["@"])
        return _get_obj(self._data, ["trade", self._account._get_account_key(account), "trades"])

    # ----------------------------------------------------------------------
    def get_risk_management_rule(self, exchange_id: Optional[str] = None,
                                 account: Optional[Union[TqAccount, TqKq, TqSim]] = None) -> Union[RiskManagementRule, Entity]:
        """
        获取账户风控统计规则

        Args:
            exchange_id (str): [可选] 交易所代码, 不填交易所代码则返回所有交易所风控规则
            目前支持设置风控规则的交易所 SSE（上海证券交易所）、SZSE（深圳证券交易所）

        Returns:
            :py:class:`~tqsdk.objs.RiskManagementRule`: 当指定了 exchange_id 时, 返回该交易所的风控统计规则对象的引用.

            不填 exchange_id 参数调用本函数, 将返回包含所有交易所风控规则的一个 tqsdk.objs.Entity 对象引用, 使用方法与dict一致, \
            其中每个元素的 key 为交易所代码, value为 :py:class:`~tqsdk.objs.RiskManagementRule`。

        Example::

            from tqsdk import TqApi, TqAuth, TqAccount
            api = TqApi(TqAccount("H海通期货", "022631", "123456"), auth=TqAuth("信易账户", "账户密码"))
            rule = api.get_risk_management_rule(exchange_id="SSE")
            print(exchange_id, rule['enable']")
            print("自成交限制:", rule.self_trade)
            print("频繁报撤单限制:", rule.frequent_cancellation)
            print("成交持仓比限制:", rule.trade_position_ratio)
            api.close()
        """
        if exchange_id:
            if exchange_id not in ["SSE", "SZSE"]:
                raise Exception(f"{exchange_id} 不支持设置风控规则，只有 SSE，SZSE 支持")
            return _get_obj(self._data, ["trade", self._account._get_account_key(account), "risk_management_rule", exchange_id],
                            RiskManagementRule(self))
        else:
            return _get_obj(self._data, ["trade", self._account._get_account_key(account), "risk_management_rule"])

    # ----------------------------------------------------------------------
    def set_risk_management_rule(self, exchange_id: str, enable: bool, count_limit: int = None, insert_order_count_limit: Optional[int] = None,
                                 cancel_order_count_limit: Optional[int] = None, cancel_order_percent_limit: Optional[float] = None,
                                 trade_units_limit: Optional[int] = None, trade_position_ratio_limit: Optional[float] = None,
                                 account: Optional[Union[TqAccount, TqKq, TqSim]] = None):
        """
        设置交易所风控规则. **注意: 指令将在下次调用** :py:meth:`~tqsdk.api.TqApi.wait_update` **时发出**
        调用本函数时，没有填写的可选参数会被服务器设置为默认值。

        Args:
            exchange_id (str): 交易所代码, "SSE" 或者 "SZSE"

            enable (bool): 是否启用该规则

            count_limit (int): [可选]最大自成交次数限制，如果未填写，服务器将根据交易所不同赋不同的默认值。

            insert_order_count_limit (int): [可选]频繁报撤单起算报单次数，如果未填写，服务器将根据交易所不同赋不同的默认值。

            cancel_order_count_limit (int): [可选]频繁报撤单起算撤单次数，如果未填写，服务器将根据交易所不同赋不同的默认值。

            cancel_order_percent_limit (float): [可选]频繁报撤单撤单比例限额，为百分比，如果未填写，服务器将根据交易所不同赋不同的默认值。

            trade_units_limit (int): [可选]成交持仓比起算成交手数，如果未填写，服务器将根据交易所不同赋不同的默认值。

            trade_position_ratio_limit (float): [可选]成交持仓比例限额，为百分比，如果未填写，服务器将根据交易所不同赋不同的默认值。

        Returns:
            :py:class:`~tqsdk.objs.RiskManagementRule`: 返回一个风控规则对象引用. 其内容将在 :py:meth:`~tqsdk.api.TqApi.wait_update` 时更新.

        Example::

            from tqsdk import TqApi, TqAuth, TqAccount
            api = TqApi(TqAccount("H海通期货", "022631", "123456"), auth=TqAuth("信易账户", "账户密码"))
            # 开启 SSE 风控限制
            api.set_risk_management_rule(exchange_id="SSE", enable=True)
            api.wait_update()  # 真正把设置规则数据包发送到服务器
            rule = api.get_risk_management_rule(exchange_id="SSE")
            print(rule)
            api.close()
        """
        if account is not None and type(account) is not TqAccount:
            self._print("模拟账户不支持设置风控规则", "WARNING")
            return None
        if exchange_id not in ["SSE", "SZSE"]:
            raise Exception(f"{exchange_id} 不支持设置风控规则，只有 SSE，SZSE 支持")
        rule_pack = {
            "aid": "set_risk_management_rule",
            "user_id": self._account._get_account_id(account),
            "exchange_id": exchange_id,
            "enable": enable,
            "self_trade": {},
            "frequent_cancellation": {},
            "trade_position_ratio": {}
        }
        if count_limit is not None:  # 最大自成交次数限制
            rule_pack["self_trade"]["count_limit"] = int(count_limit)
        if insert_order_count_limit is not None:  # 频繁报撤单起算报单次数
            rule_pack["frequent_cancellation"]["insert_order_count_limit"] = int(insert_order_count_limit)
        if cancel_order_count_limit is not None:  # 频繁报撤单起算撤单次数
            rule_pack["frequent_cancellation"]["cancel_order_count_limit"] = int(cancel_order_count_limit)
        if cancel_order_percent_limit is not None:  # 频繁报撤单撤单比例限额，为百分比
            rule_pack["frequent_cancellation"]["cancel_order_percent_limit"] = float(cancel_order_percent_limit)
        if trade_units_limit is not None:  # 成交持仓比起算成交手数
            rule_pack["trade_position_ratio"]["trade_units_limit"] = int(trade_units_limit)
        if trade_position_ratio_limit is not None:  # 成交持仓比例限额，为百分比
            rule_pack["trade_position_ratio"]["trade_position_ratio_limit"] = float(trade_position_ratio_limit)
        self._send_pack(copy.deepcopy(rule_pack))
        del rule_pack["aid"]
        rule = _get_obj(self._data, ["trade", self._account._get_account_key(account), "risk_management_rule", exchange_id], RiskManagementRule(self))
        if not self._loop.is_running():
            deadline = time.time() + 30
            while not (rule_pack['enable'] == rule['enable']
                       and rule_pack['self_trade'].items() <= rule['self_trade'].items()
                       and rule_pack['frequent_cancellation'].items() <= rule['frequent_cancellation'].items()
                       and rule_pack['trade_position_ratio'].items() <= rule['trade_position_ratio'].items()):
                # @todo: merge diffs
                if not self.wait_update(deadline=deadline):
                    raise Exception("设置风控规则超时请检查客户端及网络是否正常")
        return rule

    # ----------------------------------------------------------------------
    def get_risk_management_data(self, symbol: Optional[str] = None, account: Optional[Union[TqAccount, TqKq, TqSim]] = None
                                 ) -> Union[RiskManagementData, Entity]:
        """
        获取账户风控统计数据

        Args:
            symbol (str): [可选]合约代码, 不填合约代码则返回账户下所有持仓合约的风控统计数据

        Returns:
            :py:class:`~tqsdk.objs.RiskManagementData`: 当指定了 symbol 时, 返回该合约下的风控统计数据对象引用.
            其内容将在 :py:meth:`~tqsdk.api.TqApi.wait_update` 时更新.

            不填 symbol 参数调用本函数, 将返回包含用户所有持仓合约的一个 tqsdk.objs.Entity 对象引用, 使用方法与 dict 一致, \
            其中每个元素的 key 为合约代码, value为 :py:class:`~tqsdk.objs.RiskManagementData`。
        """
        if symbol:
            return _get_obj(self._data, ["trade", self._account._get_account_key(account), "risk_management_data", symbol], RiskManagementData(self))
        else:
            return _get_obj(self._data, ["trade", self._account._get_account_key(account), "risk_management_data"])

    # ----------------------------------------------------------------------
    def wait_update(self, deadline: Optional[float] = None) -> None:
        """
        等待业务数据更新

        调用此函数将阻塞当前线程, 等待天勤主进程发送业务数据更新并返回

        注: 它是TqApi中最重要的一个函数, 每次调用它时都会发生这些事:
            * 实际发出网络数据包(如行情订阅指令或交易指令等).
            * 尝试从服务器接收一个数据包, 并用收到的数据包更新内存中的业务数据截面.
            * 让正在运行中的后台任务获得动作机会(如策略程序创建的后台调仓任务只会在wait_update()时发出交易指令).
            * 如果没有收到数据包，则挂起等待.

        Args:
            deadline (float): [可选]指定截止时间，自unix epoch(1970-01-01 00:00:00 GMT)以来的秒数(time.time())。默认没有超时(无限等待)

        Returns:
            bool: 如果收到业务数据更新则返回 True, 如果到截止时间依然没有收到业务数据更新则返回 False

        注意:
            * 由于存在网络延迟, 因此有数据更新不代表之前发出的所有请求都被处理了, 例如::

                from tqsdk import TqApi, TqAuth

                api = TqApi(auth=TqAuth("信易账户", "账户密码"))
                quote = api.get_quote("SHFE.cu1812")
                api.wait_update()
                print(quote.datetime)

            可能输出 ""(空字符串), 表示还没有收到该合约的行情
        """
        if self._loop.is_running():
            raise Exception("不能在协程中调用 wait_update, 如需在协程中等待业务数据更新请使用 register_update_notify")
        elif asyncio._get_running_loop():
            raise Exception(
                "TqSdk 使用了 python3 的原生协程和异步通讯库 asyncio，您所使用的 IDE 不支持 asyncio, 请使用 pycharm 或其它支持 asyncio 的 IDE")
        self._wait_timeout = False
        # 先尝试执行各个task,再请求下个业务数据
        self._run_until_idle()

        # 总会发送 serial_extra_array 数据，由 TqWebHelper 处理
        for _, serial in self._serials.items():
            self._process_serial_extra_array(serial)
        # 先尝试执行各个task,再请求下个业务数据
        self._run_until_idle()
        if not self._is_slave and self._diffs:
            self._send_chan.send_nowait({
                "aid": "peek_message"
            })

        # 先 _fetch_msg 再判断 deadline, 避免当 deadline 立即触发时无法接收数据
        update_task = self.create_task(self._fetch_msg())
        deadline_handle = None if deadline is None else self._loop.call_later(max(0, deadline - time.time()),
                                                                              self._set_wait_timeout)
        try:
            while not self._wait_timeout and not self._pending_diffs:
                self._run_once()
            return len(self._pending_diffs) != 0
        finally:
            self._diffs = self._pending_diffs
            self._pending_diffs = []
            # 清空K线更新范围，避免在 wait_update 未更新K线时仍通过 is_changing 的判断
            self._klines_update_range = {}
            for d in self._diffs:
                # 判断账户类别, 对股票和期货的 trade 数据分别进行处理
                if "trade" in d:
                    for k, v in d.get('trade').items():
                        prototype = self._security_prototype if self._account._is_stock_type(k) else self._prototype
                        _merge_diff(self._data, {'trade': {k: v} }, prototype, False)
                # 非交易数据均按照期货处理逻辑
                diff_without_trade = {k : v for k, v in d.items() if k != "trade"}
                if diff_without_trade:
                    _merge_diff(self._data, diff_without_trade, self._prototype, False)

                for query_id, query_result in d.get("symbols", {}).items():
                    if query_id.startswith("PYSDK_quote") and query_result.get("error", None) is None:
                        quotes = self._symbols_to_quotes(query_result)
                        _merge_diff(self._data, {"quotes": quotes}, self._prototype, False)
            for _, serial in self._serials.items():
                # K线df的更新与原始数据、left_id、right_id、more_data、last_id相关，其中任何一个发生改变都应重新计算df
                # 注：订阅某K线后再订阅合约代码、周期相同但长度更短的K线时, 服务器不会再发送已有数据到客户端，即chart发生改变但内存中原始数据未改变。
                # 检测到K线数据或chart的任何字段发生改变则更新serial的数据
                if self.is_changing(serial["df"]) or self.is_changing(serial["chart"]):
                    if len(serial["root"]) == 1:  # 订阅单个合约
                        self._update_serial_single(serial)
                    else:  # 订阅多个合约
                        self._update_serial_multi(serial)
            if deadline_handle:
                deadline_handle.cancel()
            update_task.cancel()
            # 最后处理 raise Exception，保证不会因为抛错导致后面的代码没有执行
            for d in self._diffs:
                for query_id, query_result in d.get("symbols", {}).items():
                    if query_result.get("error", None):
                        raise Exception(f"查询合约服务报错 {query_result['error']}")

    # ----------------------------------------------------------------------
    def is_changing(self, obj: Any, key: Union[str, List[str], None] = None) -> bool:
        """
        判定obj最近是否有更新

        当业务数据更新导致 wait_update 返回后可以使用该函数判断 **本次业务数据更新是否包含特定obj或其中某个字段** 。

        关于判断K线更新的说明：
        当生成新K线时，其所有字段都算作有更新，若此时执行 api.is_changing(klines.iloc[-1]) 则一定返回True。

        Args:
            obj (any): 任意业务对象, 包括 get_quote 返回的 quote, get_kline_serial 返回的 k_serial, get_account 返回的 account 等

            key (str/list of str): [可选]需要判断的字段，默认不指定
                                  * 不指定: 当该obj下的任意字段有更新时返回True, 否则返回 False.
                                  * str: 当该obj下的指定字段有更新时返回True, 否则返回 False.
                                  * list of str: 当该obj下的指定字段中的任何一个字段有更新时返回True, 否则返回 False.

        Returns:
            bool: 如果本次业务数据更新包含了待判定的数据则返回 True, 否则返回 False.

        Example::

            # 追踪 SHFE.cu1812 的最新价更新
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            quote = api.get_quote("SHFE.cu1812")
            print(quote.last_price)
            while True:
                api.wait_update()
                if api.is_changing(quote, "last_price"):
                    print(quote.last_price)

            # 以上代码运行后的输出是这样的:
            51800.0
            51810.0
            51800.0
            ...
        """
        if obj is None:
            return False
        if not isinstance(key, list):
            key = [key] if key else []
        try:
            if isinstance(obj, pd.DataFrame):
                if id(obj) in self._serials:
                    paths = []
                    for root in self._serials[id(obj)]["root"]:
                        paths.append(root["_path"])
                elif len(obj) == 0:
                    return False
                else:  # 处理传入的为一个 copy 出的 DataFrame (与原 DataFrame 数据相同的另一个object)
                    duration = int(obj["duration"].iloc[0]) * 1000000000
                    paths = [
                        ["klines", obj[k].iloc[0], str(duration)] if duration != 0 else ["ticks", obj["symbol"].iloc[0]]
                        for k in obj.keys() if k.startswith("symbol")]
            elif isinstance(obj, pd.Series):
                ins_list = [v for k, v in obj.items() if k.startswith("symbol")]
                if len(ins_list) > 1:  # 如果是K线的Series
                    # 处理：一根新K线的数据被拆分到多个数据包中时 diff 中只有最后一个包的数据，
                    # 则 is_changing 无法根据前面数据包中的字段来判断K线有更新(仅在生成多合约新K线时产生此问题),因此：用_klines_update_range记录/判定更新
                    new_kline_range = self._klines_update_range.get(
                        (ins_list[0], tuple(ins_list[1:]), obj["duration"] * 1000000000), (0, 0))
                    if obj["id"] >= new_kline_range[0] and obj["id"] < new_kline_range[1]:  # 保持左闭右开规范
                        return True
                duration = int(obj["duration"]) * 1000000000
                paths = []
                if duration != 0:
                    for i in range(0, len(ins_list)):
                        # pandas的key值序列会保持固定顺序, 则ins_list[i]对应的是"id"+str(i)，否则不成立。 todo:增加测试用例以保证其顺序一致，若不再顺序一致则修改此处用法
                        key_id = "id" + str(i) if i != 0 else "id"
                        if key_id in obj.keys():
                            paths.append(["klines", ins_list[i], str(duration), "data", str(int(obj[key_id]))])
                        else:
                            paths.append(["klines", ins_list[i], str(duration), "data", str(-1)])
                else:
                    paths.append(["ticks", obj["symbol"], "data", str(int(obj["id"]))])

            else:
                paths = [obj["_path"]]
        except (KeyError, IndexError):
            return False
        for diff in self._diffs:
            # 如果传入key：生成一个dict（key:序号，value: 字段）, 遍历这个dict并在_is_key_exist()判断key是否存在
            if (isinstance(obj, pd.DataFrame) or isinstance(obj, pd.Series)) and len(key) != 0:
                k_dict = {}
                for k in key:
                    if k not in obj.index:
                        continue
                    m = re.match(r'(.*?)(\d+)$', k)  # 匹配key中的数字
                    if m is None:  # 无数字
                        k_dict.setdefault(0, []).append(k)
                    elif int(m.group(2)) < len(paths):
                        m_k = int(m.group(2))
                        k_dict.setdefault(m_k, []).append(m.group(1))
                for k_id, v in k_dict.items():
                    if _is_key_exist(diff, paths[k_id], v):
                        return True
            else:  # 如果没有传入key：遍历所有path
                for path in paths:
                    if _is_key_exist(diff, path, key):
                        return True
        return False

    # ----------------------------------------------------------------------
    def is_serial_ready(self, obj: pd.DataFrame) -> bool:
        """
        判断是否已经从服务器收到了所有订阅的数据

        Args:
            obj (pandas.Dataframe): K线数据

        Returns:
            bool: 返回 True 表示已经从服务器收到了所有订阅的数据

        Example::

            # 判断是否已经从服务器收到了最后 3000 根 SHFE.cu1812 的分钟线数据
            from tqsdk import TqApi, TqAuth

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            klines = api.get_kline_serial("SHFE.cu1812", 60, data_length=3000)
            while True:
                api.wait_update()
                print(api.is_serial_ready(klines))

            # 预计的输出是这样的:
            False
            False
            True
            True
            ...
        """
        return self._serials[id(obj)]["init"]

    # ----------------------------------------------------------------------
    def create_task(self, coro: asyncio.coroutine) -> asyncio.Task:
        """
        创建一个task

        一个task就是一个协程，task的调度是在 wait_update 函数中完成的，如果代码从来没有调用 wait_update，则task也得不到执行

        Args:
            coro (coroutine):  需要创建的协程

        Example::

            # 一个简单的task
            import asyncio
            from tqsdk import TqApi, TqAuth

            async def hello():
                await asyncio.sleep(3)
                print("hello world")

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            api.create_task(hello())
            while True:
                api.wait_update()

            #以上代码将在3秒后输出
            hello world
        """
        task = self._loop.create_task(coro)
        current_task = asyncio.Task.current_task(loop=self._loop)\
            if (sys.version_info[0] == 3 and sys.version_info[1] < 7) else asyncio.current_task(loop=self._loop)
        if current_task is None:
            self._tasks.add(task)
            task.add_done_callback(self._on_task_done)
        return task

    # ----------------------------------------------------------------------
    def register_update_notify(self, obj: Optional[Any] = None, chan: Optional[TqChan] = None) -> TqChan:
        """
        注册一个channel以便接受业务数据更新通知

        调用此函数将返回一个channel, 当obj更新时会通知该channel

        推荐使用 async with api.register_update_notify() as update_chan 来注册更新通知

        如果直接调用 update_chan = api.register_update_notify() 则使用完成后需要调用 await update_chan.close() 避免资源泄漏

        Args:
            obj (any/list of any): [可选]任意业务对象, 包括 get_quote 返回的 quote, get_kline_serial 返回的 k_serial, \
            get_account 返回的 account 等。默认不指定，监控所有业务对象

            chan (TqChan): [可选]指定需要注册的channel。默认不指定，由本函数创建

        Example::

            # 获取 SHFE.cu1812 合约的报价
            from tqsdk import TqApi, TqAuth

            async def demo():
                quote = api.get_quote("SHFE.cu1812")
                async with api.register_update_notify(quote) as update_chan:
                    async for _ in update_chan:
                        print(quote.last_price)

            api = TqApi(auth=TqAuth("信易账户", "账户密码"))
            api.create_task(demo())
            while True:
                api.wait_update()

            #以上代码将输出
            nan
            51850.0
            51850.0
            51690.0
            ...
        """
        if chan is None:
            chan = TqChan(self, last_only=True)
        if not isinstance(obj, list):
            obj = [obj] if obj is not None else [self._data]
        registing_objs = []
        for o in obj:
            if isinstance(o, pd.DataFrame):
                for root in self._serials[id(o)]["root"]:
                    registing_objs.append(root)
            else:
                registing_objs.append(o)
        return _register_update_chan(registing_objs, chan)

    def query_quotes(self, ins_class: str = None, exchange_id: str = None, product_id: str = None, expired: bool = None,
                     has_night: bool = None) -> List[str]:
        """
        根据相应的参数发送合约服务请求查询，并返回查询结果

        Args:
            ins_class (str): [可选] 合约类型
                * FUTURE: 期货
                * CONT: 主连
                * COMBINE: 组合
                * INDEX: 指数
                * OPTION: 期权
                * STOCK: 股票

            exchange_id (str): [可选] 交易所
                * CFFEX: 中金所
                * SHFE: 上期所
                * DCE: 大商所
                * CZCE: 郑商所
                * INE: 能源交易所(原油)
                * SSE: 上交所
                * SZSE: 深交所

            product_id (str): [可选] 品种（股票、期权不能通过 product_id 筛选查询）

            expired (bool): [可选] 是否已下市

            has_night (bool): [可选] 是否有夜盘

        Returns:
            list: 符合筛选条件的合约代码的列表，例如: ["SHFE.cu2012", "SHFE.au2012", "SHFE.wr2012"]

        Example::

            from tqsdk import TqApi, TqAuth
            api = TqApi(auth=TqAuth("信易账户", "账户密码"))

            # 不推荐使用以下方式获取符合某种条件的合约列表，推荐使用接口来完成此功能。
            # ls = [k for k,v in api._data["quotes"].items() if k.startswith("KQ.m")]
            # print(ls)

            ls = api.query_quotes(ins_class="FUTURE", product_id="au")
            print(ls)  # au 品种的全部合约，包括已下市以及未下市合约

            ls = api.query_quotes(ins_class="INDEX", product_id="au")
            print(ls)  # au 品种指数合约

            ls = api.query_quotes(ins_class="CONT")
            print(ls)  # 全部主连合约

            ls = api.query_quotes(ins_class="CONT", product_id="au")
            print(ls)  # au 品种主连合约

            ls = api.query_quotes(ins_class="FUTURE", exchange_id="SHFE", has_night=True)
            print(ls)  # 上期所带夜盘的期货合约列表

            ls = api.query_quotes(product_id="au", expired=False)
            print(ls)  # au 品种的全部未下市合约、指数、主连

            ls = api.query_quotes(ins_class="STOCK", exchange_id="SSE", expired=False)
            print(ls)  # 上海交易所股票代码列表

            ls = api.query_quotes(ins_class="FUND", exchange_id="SSE", expired=False)
            print(ls)  # 上海交易所基金代码列表

        """
        if self._stock is False or self._loop.is_running():
            raise Exception("不支持（_stock is False 或者在协程中）当前接口调用")
        variables = {}
        if ins_class is not None:
            variables["class"] = ins_class
        if exchange_id is not None:
            # 如果是主连和指数，请求全部，在客户端区分交易所
            if ins_class not in ["INDEX", "CONT"] or exchange_id not in ["CFFEX", "SHFE", "DCE", "CZCE", "INE"]:
                variables["exchange_id"] = exchange_id
        if product_id is not None:
            variables["product_id"] = product_id
        if expired is not None:
            variables["expired"] = expired
        if has_night is not None:
            variables["has_night"] = has_night
        types = {
            "class": "Class",
            "exchange_id": "String",
            "product_id": "String",
            "expired": "Boolean",
            "has_night": "Boolean"
        }
        query_cond = ",".join([f"${v}:{types[v]}" for v in variables])
        query_cond = f"({query_cond})" if query_cond else ''
        symbol_info_cond = ",".join([f"{v}:${v}" for v in variables])
        symbol_info_cond = f"({symbol_info_cond})" if symbol_info_cond else ''
        query = f"query{query_cond}{{symbol_info{symbol_info_cond}{{ ... on basic {{instrument_id }} }}}}"
        query_result = self.query_graphql(query, variables)
        result = []
        for quote in query_result.get("result", {}).get("symbol_info", []):
            if ins_class in ["INDEX", "CONT"] and exchange_id in ["CFFEX", "SHFE", "DCE", "CZCE", "INE"]:
                if exchange_id in quote["instrument_id"]:
                    result.append(quote["instrument_id"])
            else:
                result.append(quote["instrument_id"])
        return result

    def query_cont_quotes(self, exchange_id: str = None, product_id: str = None) -> List[str]:
        """
        根据填写的参数筛选，返回主力连续合约对应的标的合约列表

        Args:
            exchange_id (str): [可选] 交易所
                * CFFEX: 中金所
                * SHFE: 上期所
                * DCE: 大商所
                * CZCE: 郑商所
                * INE: 能源交易所(原油)

            product_id (str): [可选] 品种

        Returns:
            list: 符合筛选条件的合约代码的列表，例如: ["SHFE.cu2012", "SHFE.au2012", "SHFE.wr2012"]

        Example::

            from tqsdk import TqApi, TqAuth
            api = TqApi(auth=TqAuth("信易账户", "账户密码"))

            ls = api.query_cont_quotes()
            print(ls)  # 全部主连合约对应的标的合约

            ls = api.query_cont_quotes(exchange_id="DCE")
            print(ls)  # 大商所主连合约对应的标的合约

            ls = api.query_cont_quotes(product_id="jd")
            print(ls)  # jd 品种主连合约对应的标的合约
        """
        if self._stock is False or self._loop.is_running():
            raise Exception("不支持（_stock is False 或者在协程中）当前接口调用")
        variables = {"class": "CONT"}
        on_cont = "... on derivative{ underlying { edges {node{ ... on basic{ instrument_id exchange_id } ... on future {product_id} }}}}"
        query = f"query($class:Class){{symbol_info(class:$class){{ ... on basic {{instrument_id }} {on_cont} }}}}"
        query_result = self.query_graphql(query, variables)
        result = []
        for quote in query_result.get("result", {}).get("symbol_info", []):
            if quote.get("underlying"):
                for edge in quote["underlying"]["edges"]:
                    underlying_quote = edge["node"]
                    if (exchange_id and underlying_quote["exchange_id"] != exchange_id) \
                            or (product_id and underlying_quote["product_id"] != product_id):
                        continue
                    result.append(underlying_quote["instrument_id"])
        return result

    def query_options(self, underlying_symbol: str, option_class: str = None, exercise_year: int = None,
                      exercise_month: int = None, strike_price: float = None, expired: bool = None, has_A: bool = None,
                      **kwargs) -> List[str]:
        """
        发送合约服务请求查询，查询符合条件的期权列表，并返回查询结果

        Args:
            underlying_symbol (str): 标的合约

            option_class (str): [可选] 期权类型
                * CALL: 看涨期权
                * PUT: 看跌期权

            exercise_year (int): [可选] 最后行权日年份

            exercise_month (int): [可选] 最后行权日月份

            strike_price (float): [可选] 行权价格

            expired (bool): [可选] 是否下市

            has_A (bool): [可选] 是否含有A

        Returns:
            list: 符合筛选条件的合约代码的列表，例如: ["SHFE.cu2012C24000", "SHFE.cu2012P24000"]

        Example::

            from tqsdk import TqApi, TqAuth
            api = TqApi(auth=TqAuth("信易账户", "账户密码"))

            ls = api.query_options("SHFE.au2012")
            print(ls)  # 标的为 "SHFE.au2012" 的所有期权

            ls = api.query_options("SHFE.au2012", option_class="PUT")
            print(ls)  # 标的为 "SHFE.au2012" 的看跌期权

            ls = api.query_options("SHFE.au2012", option_class="PUT", expired=False)
            print(ls)  # 标的为 "SHFE.au2012" 的看跌期权, 未下市的

            ls = api.query_options("SHFE.au2012", strike_price=340)
            print(ls)  # 标的为 "SHFE.au2012" 、行权价为 340 的期权

            ls = api.query_options("SSE.000300")
            print(ls)  # 中金所沪深300股指期权

            ls = api.query_options("SSE.510300")
            print(ls)  # 上交所沪深300etf期权

            ls = api.query_options("SSE.510300", exercise_year=2020, exercise_month=12)
            print(ls)  # 上交所沪深300etf期权, 限制条件 2020 年 12 月份行权
        """
        if self._stock is False or self._loop.is_running():
            raise Exception("不支持（_stock is False 或者在协程中）当前接口调用")
        variables = {
            "derivative_class": "OPTION",
            "underlying_symbol": underlying_symbol
        }
        query = """
                query($derivative_class:Class, $underlying_symbol:String){
                    symbol_info(instrument_id:$underlying_symbol){ 
                        ... on basic { instrument_id
                            derivative (class: $derivative_class) { 
                                edges {
                                    node {
                                        ... on basic{ class instrument_id exchange_id english_name}
                                        ... on option{ expired expire_datetime last_exercise_datetime strike_price call_or_put}
                                    }
                                }
                            } 
                        } 
                    } 
                }
                """
        query_result = self.query_graphql(query, variables)
        options = []
        exercise_year = exercise_year if exercise_year else kwargs.get("delivery_year")
        exercise_month = exercise_month if exercise_month else kwargs.get("delivery_month")
        for quote in query_result.get("result", {}).get("symbol_info", []):
            if quote.get("derivative"):
                for edge in quote["derivative"]["edges"]:
                    option = edge["node"]
                    if (option_class and option["call_or_put"] != option_class) \
                            or (exercise_year and datetime.fromtimestamp(option["last_exercise_datetime"] / 1e9).year != exercise_year) \
                            or (exercise_month and datetime.fromtimestamp(option["last_exercise_datetime"] / 1e9).month != exercise_month) \
                            or (strike_price and option["strike_price"] != strike_price) \
                            or (expired is not None and option["expired"] != expired) \
                            or (has_A is True and option["english_name"].count('A') == 0) \
                            or (has_A is False and option["english_name"].count('A') > 0):
                        continue
                    options.append(option["instrument_id"])
        return options

    # ----------------------------------------------------------------------
    def _call_soon(self, org_call_soon, callback, *args, **kargs):
        """ioloop.call_soon的补丁, 用来追踪是否有任务完成并等待执行"""
        self._event_rev += 1
        return org_call_soon(callback, *args, **kargs)

    def _setup_connection(self):
        """初始化"""
        tq_web_helper = TqWebHelper(self)

        self._account = self._account if isinstance(self._account, TqMultiAccount) else TqMultiAccount([self._account])

        # TqWebHelper 初始化可能会修改 self._account、self._backtest，所以在这里才初始化 logger
        # 在此之前使用 self._logger 不会打印日志
        if not self._logger.handlers and (self._debug or
                                          (self._account._has_tq_account and self._debug is not False )):
            log_name = self._debug if isinstance(self._debug, str) else _get_log_name()
            fh = logging.FileHandler(filename=log_name)
            fh.setFormatter(JSONFormatter())
            fh.setLevel(logging.DEBUG)
            self._logger.addHandler(fh)
        mem = psutil.virtual_memory()
        self._logger.debug("process start", product="tqsdk-python", version=__version__, os=platform.platform(),
                           py_version=platform.python_version(), py_arch=platform.architecture()[0],
                           cmd=sys.argv, mem_total=mem.total, mem_free=mem.free)

        # 连接合约和行情服务器
        if self._md_url is None:
            self._md_url = self._auth._get_md_url(self._stock)  # 如果用户未指定行情地址，则使用名称服务获取行情地址
        md_logger = ShinnyLoggerAdapter(self._logger.getChild("TqConnect"), url=self._md_url)
        ws_md_send_chan = TqChan(self, chan_name="send to md", logger=md_logger)
        ws_md_recv_chan = TqChan(self, chan_name="recv from md", logger=md_logger)
        quotes = self._fetch_symbol_info(os.getenv("TQ_INS_URL", "https://openmd.shinnytech.com/t/md/symbols/2020-09-15.json"))
        ws_md_recv_chan.send_nowait({
            "aid": "rtn_data",
            "data": [{
                "quotes": {k: v for k, v in quotes.items() if v["expired"] is True}
            }]
        })  # 获取合约信息

        conn = TqConnect(md_logger)
        self.create_task(conn._run(self, self._md_url, ws_md_send_chan, ws_md_recv_chan))

        md_handler_logger = ShinnyLoggerAdapter(self._logger.getChild("MdReconnect"), url=self._md_url)
        md_reconnect = MdReconnectHandler(md_handler_logger)
        api_send_chan = TqChan(self, chan_name="send to md reconn", logger=md_handler_logger)
        api_recv_chan = TqChan(self, chan_name="recv from md reconn", logger=md_handler_logger)
        self.create_task(md_reconnect._run(self, api_send_chan, api_recv_chan, ws_md_send_chan, ws_md_recv_chan))
        ws_md_send_chan, ws_md_recv_chan = api_send_chan, api_recv_chan

        # 启动账户实例并连接交易服务器
        self._account._run(self, self._send_chan, self._recv_chan, ws_md_send_chan, ws_md_recv_chan)

        # 发送第一个peek_message,因为只有当收到上游数据包时wait_update()才会发送peek_message
        self._send_chan.send_nowait({
            "aid": "peek_message"
        })

    def _fetch_symbol_info(self, url):
        """获取合约信息"""
        rsp = requests.get(url, headers=self._base_headers, timeout=30)
        rsp.raise_for_status()
        quotes = {
            k: {
                "ins_class": v.get("class", ""),
                "instrument_id": v.get("instrument_id", ""),
                "exchange_id": v.get("exchange_id", ""),
                "margin": v.get("margin"),  # 用于内部实现模拟交易, 不作为 api 对外可用数据（即 Quote 类中无此字段）
                "commission": v.get("commission"),  # 用于内部实现模拟交易, 不作为 api 对外可用数据（即 Quote 类中无此字段）
                "price_tick": v["price_tick"],
                "price_decs": v["price_decs"],
                "volume_multiple": v["volume_multiple"],
                "max_limit_order_volume": v.get("max_limit_order_volume", 0),
                "max_market_order_volume": v.get("max_market_order_volume", 0),
                "min_limit_order_volume": v.get("min_limit_order_volume", 0),
                "min_market_order_volume": v.get("min_market_order_volume", 0),
                "underlying_symbol": v.get("underlying_symbol", ""),
                "strike_price": v.get("strike_price", float("nan")),
                "expired": v["expired"],
                "trading_time": v.get("trading_time"),
                "expire_datetime": v.get("expire_datetime"),
                "delivery_month": v.get("delivery_month"),
                "delivery_year": v.get("delivery_year"),
                "option_class": v.get("option_class", ""),
                "product_id": v.get("product_id", ""),
            } for k, v in rsp.json().items()
        }
        # 补丁：将旧版合约服务中 "CSI.000300" 全部修改成 "SSE.000300"
        quotes["SSE.000300"] = quotes.pop("CSI.000300", {})
        quotes["SSE.000300"]["exchange_id"] = "SSE"
        for k, v in quotes.items():
            if k.startswith("CFFEX.IO") and v["ins_class"] == "OPTION":
                v["underlying_symbol"] = "SSE.000300"
        return quotes

    def _init_serial(self, root_list, width, default, adj_type):
        last_id_list = [root.get("last_id", -1) for root in root_list]
        # 主合约的array字段
        array = [[default["datetime"]] + [i] + [default[k] for k in default if k != "datetime"] for i in
                 range(last_id_list[0] + 1 - width, last_id_list[0] + 1)]
        for last_id in last_id_list[1:]:
            # 将多个列横向合并
            array = np.hstack((array, [[i] + [default[k] for k in default if k != "datetime"] for i in
                                       range(last_id + 1 - width, last_id + 1)]))

        default_keys = ["id"] + [k for k in default.keys() if k != "datetime"]
        columns = ["datetime"] + default_keys
        for i in range(1, len(root_list)):
            columns += [k + str(i) for k in default_keys]
        serial = {
            "root": root_list,
            "width": width,
            "default": default,
            "array": np.array(array, order="F"),
            "init": False,  # 是否初始化完成. 完成状态: 订阅K线后已获取所有主、副合约的数据并填满df序列.
            "adj_type": adj_type,
            "update_row": 0,  # 起始更新数据行
            "all_attr": set(columns) | {"symbol" + str(i) for i in range(1, len(root_list))} | {"symbol", "duration"},
            "extra_array": {},
        }
        columns = Index(columns)
        index = RangeIndex(0, serial["array"].shape[0])
        values = serial["array"].T
        block = FloatBlock(values=values, placement=slice(0, len(columns)), ndim=2)
        bm = BlockManagerUnconsolidated(blocks=[block], axes=[columns, index])
        serial["df"] = pd.DataFrame(bm, copy=False)
        serial["df"]["symbol"] = root_list[0]["_path"][1]
        for i in range(1, len(root_list)):
            serial["df"]["symbol" + str(i)] = root_list[i]["_path"][1]

        serial["df"]["duration"] = 0 if root_list[0]["_path"][0] == "ticks" else int(
            root_list[0]["_path"][-1]) // 1000000000
        return serial

    def _update_serial_single(self, serial):
        """处理订阅单个合约时K线的数据更新"""
        last_id = serial["root"][0].get("last_id", -1)
        array = serial["array"]
        serial["update_row"] = 0
        if serial["init"]:  # 已经初始化完成
            shift = min(last_id - int(array[-1, 1]), serial["width"])  # array[-1, 1]: 已有数据的last_id
            if shift != 0:
                array[0:serial["width"] - shift] = array[shift:serial["width"]]
                for ext in serial["extra_array"].values():
                    ext[0:serial["width"] - shift] = ext[shift:serial["width"]]
                    if np.issubdtype(ext.dtype, np.floating):
                        ext[serial["width"] - shift:] = np.nan
                    elif np.issubdtype(ext.dtype, np.object_):
                        ext[serial["width"] - shift:] = None
                    elif np.issubdtype(ext.dtype, np.integer):
                        ext[serial["width"] - shift:] = 0
                    elif np.issubdtype(ext.dtype, np.bool_):
                        ext[serial["width"] - shift:] = False
                    elif np.issubdtype(ext.dtype, np.datetime64):
                        ext[serial["width"] - shift:] = np.datetime64('nat')
                    elif np.issubdtype(ext.dtype, np.timedelta64):
                        ext[serial["width"] - shift:] = np.timedelta64('nat')
                    else:
                        ext[serial["width"] - shift:] = np.nan
            serial["update_row"] = max(serial["width"] - shift - 1, 0)
        else:
            left_id = serial["chart"].get("left_id", -1)
            right_id = serial["chart"].get("right_id", -1)
            if (left_id != -1 or right_id != -1) and not serial["chart"].get("more_data", True) and serial["root"][
                0].get("last_id", -1) != -1:
                serial["init"] = True
        symbol = serial["chart"]["ins_list"].split(",")[0]  # 合约列表
        quote = self._data.quotes.get(symbol, {})
        duration = serial["chart"]["duration"]  # 周期
        keys = list(serial["default"].keys())
        keys.remove('datetime')
        if duration != 0:
            cols = ["open", "high", "low", "close"]
        else:
            cols = ["last_price", "highest", "lowest"] + [f"{x}{i}" for x in
                                                          ["bid_price", "ask_price"] for i in
                                                          range(1, 6)]
        for i in range(serial["update_row"], serial["width"]):
            index = last_id - serial["width"] + 1 + i
            item = serial["default"] if index < 0 else _get_obj(serial["root"][0], ["data", str(index)], serial["default"])
            # 如果需要复权，计算复权
            if index > 0 and serial["adj_type"] in ["B", "F"] and quote.ins_class in ["STOCK", "FUND"]:
                self._ensure_dividend_factor(symbol)
                last_index = index - 1
                last_item = _get_obj(serial["root"][0], ["data", str(last_index)], serial["default"])
                factor = get_dividend_factor(self._dividend_cache[symbol]["df"], last_item, item)
                if serial["adj_type"] == "B":
                    self._dividend_cache[symbol]["back_factor"] = self._dividend_cache[symbol]["back_factor"] * (1 / factor)
                    if self._dividend_cache[symbol]["back_factor"] != 1.0:
                        item = item.copy()
                        for c in cols:
                            item[c] = item[c] * self._dividend_cache[symbol]["back_factor"]
                elif serial["adj_type"] == "F" and factor != 1.0:
                    for c in cols:
                        col_index = keys.index(c) + 2
                        array[:i, col_index] = array[:i, col_index] * factor
            array[i] = [item["datetime"]] + [index] + [item[k] for k in keys if k != "datetime"]

    def _ensure_dividend_factor(self, symbol):
        quote = self._data.quotes.get(symbol, {})
        if self._dividend_cache.get(symbol, None) is None:
            dividend_df = get_dividend_df(quote.get("stock_dividend_ratio", []), quote.get("cash_dividend_ratio", []))
            self._dividend_cache[symbol] = {
                "df": dividend_df,
                "back_factor": 1.0
            }

    def _update_serial_multi(self, serial):
        """处理订阅多个合约时K线的数据更新"""
        # 判断初始化数据是否接收完全, 否: 则返回
        left_id = serial["chart"].get("left_id", -1)  # 主合约的left_id
        right_id = serial["chart"].get("right_id", -1)  # 主合约的right_id
        if (left_id == -1 and right_id == -1) or serial["chart"].get("more_data", True):
            return
        for root in serial["root"]:
            if root.get("last_id", -1) == -1:
                return

        array = serial["array"]
        ins_list = serial["chart"]["ins_list"].split(",")  # 合约列表

        if not serial["init"]:  # 未初始化完成则进行初始化处理. init完成状态: 订阅K线后获取所有数据并填满df序列.
            update_row = serial["width"] - 1  # 起始更新数据行,局部变量
            current_id = right_id  # 当前数据指针
            while current_id >= left_id and current_id >= 0 and update_row >= 0:  # 如果当前id >= left_id 且 数据尚未填满width长度
                master_item = serial["root"][0]["data"][str(current_id)]  # 主合约中 current_id 对应的数据
                # 此次更新的一行array初始化填入主合约数据
                row_data = [master_item["datetime"]] + [current_id] + [master_item[col] for col in
                                                                       serial["default"].keys() if col != "datetime"]
                tid = -1
                for symbol in ins_list[1:]:  # 遍历副合约
                    # 从binding中取出与symbol对应的last_id
                    tid = serial["root"][0].get("binding", {}).get(symbol, {}).get(str(current_id), -1)
                    if tid == -1:
                        break
                    other_item = serial["root"][ins_list.index(symbol)]["data"].get(str(tid))  # 取出tid对应的副合约数据
                    if other_item is None:
                        # 1 避免初始化时主合约和binding都收到但副合约数据尚未收到报错
                        # 2 使用break而非return: 避免夜盘时有binding数据但其对应的副合约需到白盘才有数据（即等待时间过长导致报超时错误）
                        tid = -1
                        break
                    row_data += [tid] + [other_item[col] for col in serial["default"].keys() if col != "datetime"]
                if tid != -1:
                    # 数据更新
                    array[update_row] = row_data
                    update_row -= 1
                current_id -= 1
            # 当主合约与某副合约的交易时间完全无重合时不会更新数据。当 update_row 发生了改变，表示数据有更新，则将序列就绪标志转为 True
            if update_row != serial["width"] - 1:
                serial["init"] = True
                serial["update_row"] = 0  # 若需发送数据给天勤，则发送所有数据

            # 修改行情订阅指令的 view_width
            self._send_pack({
                "aid": "set_chart",
                "chart_id": serial["chart"]["chart_id"],
                "ins_list": serial["chart"]["ins_list"],
                "duration": serial["chart"]["duration"],
                # 如果长度小于30,则默认请求30,以保证能包含到所有在向上处理数据时收到的新数据,30:get_klines_serial()中等待超时的秒数,最小K线序列为1s线
                "view_width": len(array) if len(array) >= 30 else 30,
            })
        else:  # 正常行情更新处理
            serial["update_row"] = serial["width"] - 1
            new_kline_range = None
            new_data_index = serial["width"] - 1  # 记录更新数据位置
            # 从 left_id 或 已有数据的最后一个 last_id 到服务器发回的最新数据的 last_id: 每次循环更新一行。max: 避免数据更新过多时产生大量多余循环判断
            for i in range(max(serial["chart"].get("left_id", -1), int(array[-1, 1])),
                           serial["root"][0].get("last_id", -1) + 1):
                # 如果某条主K线和某条副K线之间的 binding 映射数据存在: 则对应副合约数据也存在; 遍历主合约与所有副合约的binding信息, 如果都存在, 则将此K线填入array保存.
                master_item = serial["root"][0]["data"][str(i)]  # 主合约数据
                # array更新的一行数据: 初始化填入主合约数据
                row_data = [master_item["datetime"]] + [i] + [master_item[col] for col in serial["default"].keys() if
                                                              col != "datetime"]
                tid = -1
                for symbol in ins_list[1:]:  # 遍历副合约
                    # 从binding中取出与symbol对应的last_id
                    tid = (serial["root"][0].get("binding", {}).get(symbol, {}).get(str(i), -1))
                    if tid == -1:
                        break
                    # 取出tid对应的副合约数据
                    other_item = serial["root"][ins_list.index(symbol)]["data"].get(str(tid))
                    if other_item is None:
                        return
                    row_data += [tid] + [other_item[col] for col in serial["default"].keys() if col != "datetime"]
                # 如果有新增K线, 则向上移动一行；循环的第一条数据为原序列最后一条数据, 只更新不shift
                if tid != -1:
                    if i != array[-1, 1]:  # 如果不是已有数据的最后一行，表示生成新K线，则向上移动一行
                        new_data_index = new_data_index + 1
                        # 修改 serial["update_row"] 以保证发送正确数序列给天勤
                        serial["update_row"] = serial["update_row"] - 1 if serial["update_row"] > 0 else 0
                        if new_kline_range is None:  # 记录第一条新K线的id
                            new_kline_range = i
                    # 数据更新
                    array[new_data_index % serial["width"]] = row_data
            if new_kline_range is not None:  # 有新K线生成
                array[:] = np.roll(array, serial["width"] - (new_data_index % serial["width"]) - 1, axis=0)[:]

                remain = max(2 * serial["width"] - 1 - new_data_index, 0)
                for ext in serial["extra_array"].values():
                    ext[:remain] = ext[serial["width"] - remain:]
                    if ext.dtype == np.float:
                        ext[remain:] = np.nan
                    elif ext.dtype == np.object:
                        ext[remain:] = None
                    elif ext.dtype == np.int:
                        ext[remain:] = 0
                    elif ext.dtype == np.bool:
                        ext[remain:] = False
                    elif ext.dtype == np.datetime64:
                        ext[remain:] = np.datetime64('nat')
                    elif ext.dtype == np.timedelta64:
                        ext[remain:] = np.timedelta64('nat')
                    else:
                        ext[remain:] = np.nan

                k = (ins_list[0], tuple(ins_list[1:]), serial["df"]["duration"][0] * 1000000000)
                # 注: i 从 left_id 开始，shift最大长度为width，则必有：new_kline_range >= array[0,1]
                self._klines_update_range[k] = (
                    new_kline_range if not self._klines_update_range.get(k) else min(self._klines_update_range[k][0],
                                                                                     new_kline_range),
                    array[-1, 1] + 1)  # array[-1, 1] + 1： 保持左闭右开规范

    def _process_serial_extra_array(self, serial):
        for col in set(serial["df"].columns.values) - serial["all_attr"]:
            serial["update_row"] = 0
            serial["extra_array"][col] = serial["df"][col].to_numpy()
        # 如果策略中删除了之前添加到 df 中的序列，则 extra_array 中也将其删除
        for col in serial["all_attr"] - set(serial["df"].columns.values):
            del serial["extra_array"][col]
        serial["all_attr"] = set(serial["df"].columns.values)
        if serial["update_row"] == serial["width"]:
            return
        symbol = serial["root"][0]["_path"][1]  # 主合约的symbol，标志绘图的主合约
        duration = 0 if serial["root"][0]["_path"][0] == "ticks" else int(serial["root"][0]["_path"][-1])
        cols = list(serial["extra_array"].keys())
        # 归并数据序列
        while len(cols) != 0:
            col = cols[0].split(".")[0]
            # 找相关序列，首先查找以col开头的序列
            group = [c for c in cols if c.startswith(col + ".") or c == col]
            cols = [c for c in cols if c not in group]
            data = {c[len(col):]: serial["extra_array"][c][serial["update_row"]:] for c in group}
            self._process_chart_data_for_web(serial, symbol, duration, col, serial["width"] - serial["update_row"],
                                             int(serial["array"][-1, 1]) + 1, data)
        serial["update_row"] = serial["width"]

    def _send_series_data(self, symbol, duration, serial_id, serial_data, aid="set_chart_data"):
        pack = {
            "aid": aid,
            "symbol": symbol,
            "dur_nano": duration,
            "datas": {
                serial_id: serial_data,
            }
        }
        self._send_pack(pack)

    def _run_once(self):
        """执行 ioloop 直到 ioloop.stop 被调用"""
        if not self._exceptions:
            self._loop.run_forever()
        if self._exceptions:
            raise self._exceptions.pop(0)

    def _run_until_idle(self):
        """执行 ioloop 直到没有待执行任务"""
        while self._check_rev != self._event_rev:
            check_handle = self._loop.call_soon(self._check_event, self._event_rev + 1)
            try:
                self._run_once()
            finally:
                check_handle.cancel()

    def _check_event(self, rev):
        self._check_rev = rev
        self._loop.stop()

    def _set_wait_timeout(self):
        self._wait_timeout = True
        self._loop.stop()

    def _on_task_done(self, task):
        """当由 api 维护的 task 执行完成后取出运行中遇到的例外并停止 ioloop"""
        try:
            exception = task.exception()
            if exception:
                self._exceptions.append(exception)
        except asyncio.CancelledError:
            pass
        finally:
            self._tasks.remove(task)
            self._loop.stop()

    async def _windows_patch(self):
        """Windows系统下asyncio不支持KeyboardInterrupt的临时补丁, 详见 https://bugs.python.org/issue23057"""
        while True:
            await asyncio.sleep(1)

    async def _notify_watcher(self):
        """将从服务器收到的通知打印出来"""
        notify_logger = self._logger.getChild("Notify")
        processed_notify = set()
        notify = _get_obj(self._data, ["notify"])
        async with self.register_update_notify(notify) as update_chan:
            async for _ in update_chan:
                all_notifies = {k for k in notify if not k.startswith("_")}
                notifies = all_notifies - processed_notify
                processed_notify = all_notifies
                for n in notifies:
                    try:
                        level = getattr(logging, notify[n]["level"])
                    except (AttributeError, KeyError):
                        level = logging.INFO
                    self._print(f"通知: {notify[n]['content']}", level=level)

    async def _fetch_msg(self):
        while not self._pending_diffs:
            pack = await self._recv_chan.recv()
            if pack is None:
                return
            if not self._is_slave:
                for slave in self._slaves:
                    slave._slave_recv_pack(copy.deepcopy(pack))
            self._pending_diffs.extend(pack.get("data", []))

    def _gen_prototype(self):
        """所有业务数据的原型"""
        return {
            "quotes": {
                "#": Quote(self),  # 行情的数据原型
            },
            "klines": {
                "*": {
                    "*": {
                        "data": {
                            "@": Kline(self),  # K线的数据原型
                        }
                    }
                }
            },
            "ticks": {
                "*": {
                    "data": {
                        "@": Tick(self),  # Tick的数据原型
                    }
                }
            },
            "trade": {
                "*": {
                    "accounts": {
                        "@": Account(self),  # 账户的数据原型
                    },
                    "orders": {
                        "@": Order(self),  # 委托单的数据原型
                    },
                    "trades": {
                        "@": Trade(self),  # 成交的数据原型
                    },
                    "positions": {
                        "@": Position(self),  # 持仓的数据原型
                    },
                    "risk_management_rule": {
                        "@": RiskManagementRule(self)
                    },
                    "risk_management_data": {
                        "@": RiskManagementData(self)
                    }
                }
            },
        }

    def _gen_security_prototype(self):
        """所有业务数据的原型"""
        return {
            "quotes": {
                "#": Quote(self),  # 行情的数据原型
            },
            "klines": {
                "*": {
                    "*": {
                        "data": {
                            "@": Kline(self),  # K线的数据原型
                        }
                    }
                }
            },
            "ticks": {
                "*": {
                    "data": {
                        "@": Tick(self),  # Tick的数据原型
                    }
                }
            },
            "trade": {
                "*": {
                    "accounts": {
                        "@": SecurityAccount(self),  # 账户的数据原型
                    },
                    "orders": {
                        "@": SecurityOrder(self),  # 委托单的数据原型
                    },
                    "trades": {
                        "@": SecurityTrade(self),  # 成交的数据原型
                    },
                    "positions": {
                        "@": SecurityPosition(self),  # 持仓的数据原型
                    }
                }
            },
        }

    @staticmethod
    def _deep_copy_dict(source, dest):
        for key, value in source.__dict__.items():
            if isinstance(value, Entity):
                dest[key] = {}
                TqApi._deep_copy_dict(value, dest[key])
            else:
                dest[key] = value

    def _slave_send_pack(self, pack):
        if pack.get("aid", None) == "subscribe_quote":
            self._loop.call_soon_threadsafe(lambda: self._send_subscribe_quote(pack))
            return
        self._loop.call_soon_threadsafe(lambda: self._send_pack(pack))

    def _slave_recv_pack(self, pack):
        self._loop.call_soon_threadsafe(lambda: self._recv_chan.send_nowait(pack))

    def _send_subscribe_quote(self, pack):
        new_subscribe_set = self._requests["quotes"] | set(pack["ins_list"].split(","))
        if new_subscribe_set != self._requests["quotes"]:
            self._requests["quotes"] = new_subscribe_set
            self._send_pack({
                "aid": "subscribe_quote",
                "ins_list": ",".join(self._requests["quotes"])
            })

    def _send_pack(self, pack):
        if not self._is_slave:
            self._send_chan.send_nowait(pack)
        else:
            self._master._slave_send_pack(pack)

    def _send_chart_data(self, base_kserial_frame, serial_id, serial_data):
        s = self._serials[id(base_kserial_frame)]
        p = s["root"][0]["_path"]
        symbol = p[-2]
        dur_nano = int(p[-1])
        pack = {
            "aid": "set_chart_data",
            "symbol": symbol,
            "dur_nano": dur_nano,
            "datas": {
                serial_id: serial_data,
            }
        }
        self._send_pack(pack)

    def _offset_to_x(self, base_k_dataframe, x):
        if x is None:
            return int(base_k_dataframe["id"].iloc[-1])
        elif x < 0:
            return int(base_k_dataframe["id"].iloc[-1]) + 1 + x
        elif x >= 0:
            return int(base_k_dataframe["id"].iloc[0]) + x

    def _symbols_to_quotes(self, symbols, keys=set(Quote(None).keys())):
        quotes = _symbols_to_quotes(symbols, keys)
        if isinstance(self._backtest, TqBacktest):
            _quotes_add_night(quotes)
        return quotes