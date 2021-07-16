#!usr/bin/env python3
# -*- coding:utf-8 -*-
__author__ = 'yanqiong'

import base64
import random
import secrets
from datetime import datetime

from pandas.core.internals import BlockManager

from tqsdk.objs import Quote

RD = random.Random(secrets.randbits(128))  # 初始化随机数引擎，使用随机数作为seed，防止用户同时拉起多个策略，产生同样的 seed


def _generate_uuid(prefix=''):
    return f"{prefix + '_' if prefix else ''}{RD.getrandbits(128):032x}"


def _quote_query_id(symbol):
    """返回请求合约信息的 query_id, 每个合约生成的 query_id 是唯一的"""
    return "PYSDK_quote_" + base64.urlsafe_b64encode(symbol.encode()).decode().replace('=', '')


def _query_for_quote(symbol):
    """返回请求某个合约的合约信息的 query_pack"""
    query = """
        query ($instrument_id: String){symbol_info(instrument_id: $instrument_id){
        ... on basic{ class trading_time{day night} trading_day instrument_id instrument_name price_tick price_decs exchange_id english_name}
        ... on stock{ stock_dividend_ratio cash_dividend_ratio}
        ... on fund{ cash_dividend_ratio}
        ... on bond{ maturity_datetime }
        ... on tradeable{ volume_multiple quote_multiple}
        ... on index{ index_multiple}
        ... on securities{ currency face_value first_trading_datetime buy_volume_unit sell_volume_unit status public_float_share_quantity}
        ... on future{ expired product_id product_short_name delivery_year delivery_month expire_datetime settlement_price max_market_order_volume max_limit_order_volume margin commission mmsa}
        ... on option{ expired product_short_name expire_datetime last_exercise_datetime settlement_price max_market_order_volume max_limit_order_volume strike_price call_or_put exercise_type}
        ... on combine{ expired product_id expire_datetime max_market_order_volume max_limit_order_volume leg1{ ... on basic{instrument_id}} leg2{ ... on basic{instrument_id}} }
        ... on derivative{ 
            underlying{ 
                count edges{ underlying_multiple node{
                    ... on basic{ class trading_time{day night} trading_day instrument_id instrument_name price_tick price_decs exchange_id english_name }
                    ... on stock{ stock_dividend_ratio cash_dividend_ratio }
                    ... on fund{ cash_dividend_ratio }
                    ... on bond{ maturity_datetime }
                    ... on tradeable{ volume_multiple quote_multiple}
                    ... on index{ index_multiple}
                    ... on securities{ currency face_value first_trading_datetime buy_volume_unit sell_volume_unit status public_float_share_quantity }
                    ... on future{ expired product_id product_short_name delivery_year delivery_month expire_datetime settlement_price max_market_order_volume max_limit_order_volume margin commission mmsa}
                    }}
                }
            }
        }}
    """
    return {
        "aid": "ins_query",
        "query_id": _quote_query_id(symbol),
        "query": query,
        "variables": {"instrument_id": symbol}
    }


def _query_for_init():
    """
    返回某些类型合约的 query 和 variables
    todo: 为了兼容旧版提供给用户的 api._data["quote"].items() 类似用法，应该限制交易所 ["SHFE", "DCE", "CZCE", "INE", "CFFEX", "KQ"]
    """
    query = "query ($future:Class,$index:Class,$option:Class,$combine:Class,$cont:Class,$exSHFE:String,$exDCE:String,$exCZCE:String,$exINE:String,$exCFFEX:String,$exKQ:String){"
    for ex in ["SHFE", "DCE", "CZCE", "INE", "CFFEX", "KQ"]:
        for ins_class in ["future", "index", "option", "combine", "cont"]:
            query += ex + ins_class + ":symbol_info(class:$" + ins_class + ",exchange_id:$ex" + ex + "){" +\
                 """
                    ... on basic{ class trading_time{day night} trading_day instrument_id instrument_name price_tick price_decs exchange_id english_name}
                    ... on stock{ stock_dividend_ratio cash_dividend_ratio}
                    ... on fund{ cash_dividend_ratio}
                    ... on bond{ maturity_datetime }
                    ... on tradeable{ volume_multiple quote_multiple}
                    ... on index{ index_multiple}
                    ... on securities{ currency face_value first_trading_datetime buy_volume_unit sell_volume_unit status public_float_share_quantity}
                    ... on future{ expired product_id product_short_name delivery_year delivery_month expire_datetime settlement_price max_market_order_volume max_limit_order_volume margin commission mmsa}
                    ... on option{ expired product_short_name expire_datetime last_exercise_datetime settlement_price max_market_order_volume max_limit_order_volume strike_price call_or_put exercise_type}
                    ... on combine{ expired product_id expire_datetime max_market_order_volume max_limit_order_volume leg1{ ... on basic{instrument_id}} leg2{ ... on basic{instrument_id}} }
                    ... on derivative{ 
                        underlying{ 
                            count edges{ underlying_multiple node{
                                ... on basic{ class trading_time{day night} trading_day instrument_id instrument_name price_tick price_decs exchange_id english_name }
                                ... on stock{ stock_dividend_ratio cash_dividend_ratio }
                                ... on fund{ cash_dividend_ratio }
                                ... on bond{ maturity_datetime }
                                ... on tradeable{ volume_multiple quote_multiple}
                                ... on index{ index_multiple}
                                ... on securities{ currency face_value first_trading_datetime buy_volume_unit sell_volume_unit public_float_share_quantity }
                                ... on future{ expired product_id product_short_name delivery_year delivery_month expire_datetime settlement_price max_market_order_volume max_limit_order_volume margin commission mmsa}
                                }}
                            }
                        }
                    }
                """
    query += "}"
    return query, {
        "future": "FUTURE",
        "index": "INDEX",
        "option": "OPTION",
        "combine": "COMBINE",
        "cont": "CONT",
        "exSHFE": "SHFE",
        "exDCE": "DCE",
        "exCZCE": "CZCE",
        "exINE": "INE",
        "exCFFEX": "CFFEX",
        "exKQ": "KQ"
    }


def _symbols_to_quotes(symbols, keys=set(Quote(None).keys())):
    """将 symbols 转为 quotes，只输出 keys 包括的字段"""
    result = symbols.get("result", {})
    quotes = {}
    for k in result:
        for symbol in result[k]:
            quote = quotes.setdefault(symbol["instrument_id"], {})
            quote.update(_convert_symbol_to_quote(symbol, keys))
            if symbol.get("underlying"):
                for edge in symbol["underlying"]["edges"]:
                    underlying_symbol = edge["node"]
                    if "underlying_symbol" in keys:
                        quote["underlying_symbol"] = underlying_symbol["instrument_id"]
                    underlying_quote = quotes.setdefault(underlying_symbol["instrument_id"], {})
                    underlying_quote.update(_convert_symbol_to_quote(underlying_symbol, keys))
                    # 为期权合约补充 delivery_year delivery_month 商品期权根据标的赋值；金融期权与 exercise_year exercise_month 相同
                    # 为期权补充 delivery_year delivery_month 完全是为了兼容旧版合约服务
                    for key in ["delivery_year", "delivery_month"]:
                        if key in keys and symbol["class"] == "OPTION":
                            if symbol["exchange_id"] in ["DCE", "CZCE", "SHFE"]:
                                quote[key] = underlying_quote[key]
                            if symbol["exchange_id"] == "CFFEX" and "last_exercise_datetime" in symbol:
                                if key == "delivery_year":
                                    quote[key] = datetime.fromtimestamp(symbol["last_exercise_datetime"] / 1e9).year
                                else:
                                    quote[key] = datetime.fromtimestamp(symbol["last_exercise_datetime"] / 1e9).month
    for k in quotes:
        if quotes[k].get("ins_class", "") == "COMBINE":
            # 为组合合约补充 volume_multiple
            leg1_quote = quotes.get(quotes[k].get("leg1_symbol", ""), {})
            if leg1_quote:
                if leg1_quote.get("volume_multiple"):
                    quotes[k]["volume_multiple"] = leg1_quote["volume_multiple"]
    return quotes


def _convert_symbol_to_quote(symbol, keys):
    quote = {}
    for key in keys:
        if key == "leg1_symbol" and "leg1" in symbol:
            quote["leg1_symbol"] = symbol["leg1"]["instrument_id"]
        elif key == "leg2_symbol" and "leg2" in symbol:
            quote["leg2_symbol"] = symbol["leg2"]["instrument_id"]
        elif key == "ins_class" and "class" in symbol:
            quote["ins_class"] = symbol["class"]
        elif key == "option_class" and "call_or_put" in symbol:
            quote["option_class"] = symbol["call_or_put"]
        elif key == "volume_multiple" and "index_multiple" in symbol:
            quote["volume_multiple"] = symbol["index_multiple"]
        elif key == "expire_datetime" and symbol.get("expire_datetime"):
            quote["expire_datetime"] = symbol["expire_datetime"] / 1e9
        elif key == "last_exercise_datetime" and symbol.get("last_exercise_datetime"):
            quote["last_exercise_datetime"] = symbol["last_exercise_datetime"] / 1e9
        elif key == "exercise_year" and symbol.get("last_exercise_datetime"):
            quote["exercise_year"] = datetime.fromtimestamp(symbol["last_exercise_datetime"] / 1e9).year
        elif key == "exercise_month" and symbol.get("last_exercise_datetime"):
            quote["exercise_month"] = datetime.fromtimestamp(symbol["last_exercise_datetime"] / 1e9).month
        elif key in symbol:
            quote[key] = symbol[key]
    return quote


night_trading_table = {
    "DCE.a": ["21:00:00", "23:00:00"],
    "DCE.b": ["21:00:00", "23:00:00"],
    "DCE.c": ["21:00:00", "23:00:00"],
    "DCE.cs": ["21:00:00", "23:00:00"],
    "DCE.m": ["21:00:00", "23:00:00"],
    "DCE.y": ["21:00:00", "23:00:00"],
    "DCE.p": ["21:00:00", "23:00:00"],
    "DCE.l": ["21:00:00", "23:00:00"],
    "DCE.v": ["21:00:00", "23:00:00"],
    "DCE.pp": ["21:00:00", "23:00:00"],
    "DCE.j": ["21:00:00", "23:00:00"],
    "DCE.jm": ["21:00:00", "23:00:00"],
    "DCE.i": ["21:00:00", "23:00:00"],
    "DCE.eg": ["21:00:00", "23:00:00"],
    "DCE.eb": ["21:00:00", "23:00:00"],
    "DCE.rr": ["21:00:00", "23:00:00"],
    "DCE.pg": ["21:00:00", "23:00:00"],
    "CZCE.CF": ["21:00:00", "23:00:00"],
    "CZCE.CY": ["21:00:00", "23:00:00"],
    "CZCE.SA": ["21:00:00", "23:00:00"],
    "CZCE.SR": ["21:00:00", "23:00:00"],
    "CZCE.TA": ["21:00:00", "23:00:00"],
    "CZCE.OI": ["21:00:00", "23:00:00"],
    "CZCE.MA": ["21:00:00", "23:00:00"],
    "CZCE.FG": ["21:00:00", "23:00:00"],
    "CZCE.RM": ["21:00:00", "23:00:00"],
    "CZCE.ZC": ["21:00:00", "23:00:00"],
    "CZCE.TC": ["21:00:00", "23:00:00"],
    "SHFE.rb": ["21:00:00", "23:00:00"],
    "SHFE.hc": ["21:00:00", "23:00:00"],
    "SHFE.fu": ["21:00:00", "23:00:00"],
    "SHFE.bu": ["21:00:00", "23:00:00"],
    "SHFE.ru": ["21:00:00", "23:00:00"],
    "SHFE.sp": ["21:00:00", "23:00:00"],
    "INE.nr": ["21:00:00", "23:00:00"],
    "SHFE.cu": ["21:00:00", "25:00:00"],
    "SHFE.al": ["21:00:00", "25:00:00"],
    "SHFE.zn": ["21:00:00", "25:00:00"],
    "SHFE.pb": ["21:00:00", "25:00:00"],
    "SHFE.ni": ["21:00:00", "25:00:00"],
    "SHFE.sn": ["21:00:00", "25:00:00"],
    "SHFE.ss": ["21:00:00", "25:00:00"],
    "SHFE.au": ["21:00:00", "26:30:00"],
    "SHFE.ag": ["21:00:00", "26:30:00"],
    "INE.sc": ["21:00:00", "26:30:00"],
}


def _quotes_add_night(quotes):
    """为 quotes 中应该有夜盘但是市价合约文件中没有夜盘的品种，添加夜盘时间"""
    for symbol in quotes:
        product_id = quotes[symbol].get("product_id")
        if quotes[symbol].get("trading_time") and product_id and product_id in night_trading_table:
            quotes[symbol]["trading_time"].setdefault("night", [night_trading_table[product_id]])


class BlockManagerUnconsolidated(BlockManager):
    """mock BlockManager for unconsolidated, 不会因为自动合并同类型的 blocks 而导致 k 线数据不更新"""
    def __init__(self, *args, **kwargs):
        BlockManager.__init__(self, *args, **kwargs)
        self._is_consolidated = False
        self._known_consolidated = False

    def _consolidate_inplace(self): pass
