from tqsdk import TqApi, TqBacktest,TqAccount, TargetPosTask, TqAuth
import time
import threading
import platform
import datetime
import os
import sys

if platform.system() == "Linux":
    import readline

def cls():
    if platform.system() == "Linux":
        print("\033[2J")
    else:
        os.system("cls")

plat = sys.argv[1]
account = sys.argv[2]
password = sys.argv[3]
auth = sys.argv[4]
api = TqApi(TqAccount(plat, account, password),auth=auth)
#api = TqApi(TqAccount(plat, account, password),auth=auth)
l = []
mquote = {}
mposition = {}
schdules = []

today = datetime.date.today()
current_year = today.year
current_month = today.month
m2name = {}
name2m = {}
month2long = {}
wxy_tbl = {}
wxy_time_tbl = {}

df_map = {
        "03-05":0,
        "04-05":0,
        "05-06":0,
        "06-07":0,
        "05-09":0
}

def get_conbine_quote(str):
    return ""

def get_quote_jd(strstr):
    if len(str)==4:
        return "DCE.jd" + strstr
    name = "DCE.jd" + str(current_year-2000) + strstr
    quote = api.get_quote(name)
    if quote.expired == True:
        name = "DCE.jd" + str(current_year-1999) + strstr
        quote = api.get_quote(name)
    return quote

def get_quote(strstr):
    if strstr in mquote:
        return mquote[strstr]
    if strstr.find("&") >= 0:
        return get_conbine_quote(strstr)
    if strstr.isdigit():
        return get_quote_jd(strstr)
    quote = None
    try:
        quote = api.get_quote("DCE." + strstr)
        print(quote)
    except BaseException as e:
        print(e)
        try:
            quote = api.get_quote("CZCE." + strstr)
        except BaseException:
            quote = api.get_quote("SHFE." + strstr)
    return quote

for j in range(current_month,current_month+12):
    i = j
    if i > 12:
        i = i - 12
    month = "{0:02d}".format(i)
    name = "DCE.jd" + str(current_year-2000) + month
    month2long[month] = str(current_year-2000)+month
    quote = api.get_quote(name)
    if quote.expired == True:
        name = "DCE.jd" + str(current_year-1999) + "{0:02d}".format(i)
        quote = api.get_quote(name)
        month2long[month] = str(current_year-1999)+month
    mquote[month] = quote
    position = api.get_position(name)
    mposition[month] = position
    l.append(month)
    m2name[month] = name
    name2m[name] = month

def sub(str_list):
    for s in str_list:
        quote = get_quote(s)
        if quote is not None:
            mquote[s] = quote
            l.append(s)

def ask_price(quote):
    try:
        return float(quote.ask_price1)
    except:
        return float(quote.upper_limit)

def bid_price(quote):
    try:
        return float(quote.bid_price1)
    except:
        return float(quote.lower_limit)

def print_tl_price(name1,name2):
    price1 = str(ask_price(mquote[name1]) - bid_price(mquote[name2]))
    price2 = str(bid_price(mquote[name1]) - ask_price(mquote[name2]))
    print(name1+"-"+name2,"\t",price2,"\t\t",price1)

def show():
    print("==============show==============")
    for i in l:
        if i in mquote:
            quote = mquote[i]
            print(i,"\t",bid_price(quote),"\t",quote.bid_volume1,"\t",ask_price(quote),"\t",quote.ask_volume1,"\t",quote.highest,quote.lowest,"\t",quote.open_interest - quote.pre_open_interest,"\t",quote.open_interest)
            m2name[i] = quote.instrument_id
            if i not in mposition:
                mposition[i] = api.get_position(quote.instrument_id)

    print("")
    keys = list(df_map.keys())
    keys.sort()
    for k in keys:
        args = k.split("-")
        print_tl_price(args[0],args[1])

def help():
    print("============help================")
    print("help")
    print("show")
    print("position")
    print("order")
    print("kd 09 1 4100")
    print("kk 05 1 3100")
    print("pd 09 1 4300")
    print("pk 05 1 2800")
    print("zt 09 01 1")
    print("pzt 09 01 1")
    print("ft 05 06 1")
    print("pft 05 06 1")
    print("wxy set 01 10")
    print("wxy c 01")
    print("wxy show")
    print("cancel order_id(get from order)")
    print("")

def position():
    print("============position================")
    for i in l:
        if i in mposition:
            p = mposition[i]
            if p.pos_long > 0 or p.pos_short > 0:
                print(i,"long:",p.pos_long,"\t",p.open_price_long,"\t  short:",p.pos_short,"\t",p.open_price_short)

order_cache = []
def order():
    print("============order================")
    order_cache.clear()
    orders = api.get_order()
    for name in orders:
        o = api.get_order(name)
#        print(o)
        if o.status == "ALIVE":
            print(len(order_cache)+1,o.order_id,o.instrument_id,o.direction,o.offset,o.limit_price,o.volume_orign,o.volume_left)
            order_cache.append(o.order_id)

def insert_order(month,direction,offset,volume,price=None):
    print("============insert_order================")
    if month in m2name:
        symbol = m2name[month]
    elif month.find("&")>=0:
        ss = month.split("&")
        symbol = "DCE.SP jd" + month2long[ss[0]] + "&jd" + month2long[ss[1]]
    else:
        return
    if price is None:
        return api.insert_order(symbol, direction=direction.upper(), offset=offset.upper(), volume=volume)
    return api.insert_order(symbol, direction=direction.upper(), offset=offset.upper(), volume=volume,limit_price=price)

def zt(month1,month2,volume):
    order1 = api.insert_order(symbol=m2name[month1], direction="BUY",  offset="OPEN", volume=volume)
    order2 = api.insert_order(symbol=m2name[month2], direction="SELL", offset="OPEN", volume=volume)
    return (order1,order2)

def pzt(month1,month2,volume):
    order1 = api.insert_order(symbol=m2name[month1], direction="SELL",offset="CLOSE", volume=volume)
    order2 = api.insert_order(symbol=m2name[month2], direction="BUY", offset="CLOSE", volume=volume)
    return (order1,order2)

def ft(month1,month2,volume):
    order1 = api.insert_order(symbol=m2name[month1], direction="SELL", offset="OPEN", volume=volume)
    order2 = api.insert_order(symbol=m2name[month2], direction="BUY",  offset="OPEN", volume=volume)
    return (order1,order2)

def pft(month1,month2,volume):
    order1 = api.insert_order(symbol=m2name[month1], direction="BUY", offset="CLOSE", volume=volume)
    order2 = api.insert_order(symbol=m2name[month2], direction="SELL",  offset="CLOSE", volume=volume)
    return (order1,order2)

def cancel_order(id):
    print("============cancel_order================")
    if len(id) <= 5:
        api.cancel_order(order_cache[int(id)-1])
    else:
        api.cancel_order(id)

trade_cache = {}

def init_trade_cache():
    global trade_cache
    orders = api.get_order()
    for id in orders:
        o = api.get_order(id)
        for k,trade in o.trade_records.items():
            if o.direction == trade.direction:
                trade_cache[trade.exchange_trade_id] = trade

def update_trade_cache():
    global gride_tbl
    global trade_cache
    new_trades = []
    orders = api.get_order()
    for id in orders:
        o = api.get_order(id)
        name = o.exchange_id + "." + o.instrument_id
        if o.exchange_id + "." + o.instrument_id != name or o.volume_orign == o.volume_left:
            continue
        if name not in name2m:
            continue
        month = name2m[name]
        if month not in wxy_tbl:
            continue
        gride = wxy_tbl[month]
        gride_time = wxy_time_tbl[month]
        for k,trade in o.trade_records.items():
            if o.direction != trade.direction:
                continue
            if trade.trade_date_time/1000000000 < gride_time - 2:
                #print("time old")
                continue
            if trade.exchange_trade_id in trade_cache:
                continue
            print("k 2",trade.exchange_trade_id)
            trade_cache[trade.exchange_trade_id] = trade
            price = o.limit_price
            if o.price_type == 'ANY':
                price = trade.price
            new_trades.append([name,gride,o.limit_price,o.direction,trade.volume,o.offset])

    for kv in new_trades:
        name = kv[0]
        gride = kv[1]
        price = kv[2]
        odirection = kv[3]
        volume = kv[4]
        ooffset = kv[5]
        direction = "SELL"
        offset = "CLOSE"
        new_price = price + gride
        if odirection == "SELL":
            direction = "BUY"
            new_price = price - gride
        if new_price == price:
            continue
        if ooffset != "OPEN":
            offset = "OPEN"
        print(name, direction, offset, volume, new_price)
        api.insert_order(name, direction=direction, offset=offset, volume=volume, limit_price=new_price)

close = False
class WorkingThread(threading.Thread):
    def run(self):
        init_trade_cache()
        while True:
            api.wait_update()
            if close:
               api.close()
               break
            update_trade_cache()

wt = WorkingThread()
wt.start()

help()

while True:
    line = input("command:")
    args = line.split(" ")
    cmd = args[0]
    if cmd == "help" or cmd == "h":
        help()
    elif cmd == "show" or cmd == "s":
        show()
    elif cmd == "position" or cmd == "p":
        position()
    elif cmd == "order" or cmd == "o":
        order()
    elif cmd == "sub":
        if len(args) <= 1:
            pass
        sub(args[1:])
    elif cmd == "df":
        if len(args) < 3:
            pass
        df_map[args[1] + "-" + args[2]] = 0
    elif cmd == "kd":
        if len(args) >= 4:
            insert_order(args[1],"buy","open",int(args[2]),int(args[3]))
        else:
            insert_order(args[1],"buy","open",int(args[2]))
    elif cmd == "kk":
        if len(args) >= 4:
            insert_order(args[1],"sell","open",int(args[2]),int(args[3]))
        else:
            insert_order(args[1],"sell","open",int(args[2]))
    elif cmd == "pd":
        if len(args) >= 4:
            insert_order(args[1],"sell","close",int(args[2]),int(args[3]))
        else:
            insert_order(args[1],"sell","close",int(args[2]))
    elif cmd == "pk":
        if len(args) >= 4:
            insert_order(args[1],"buy","close",int(args[2]),int(args[3]))
        else:
            insert_order(args[1],"buy","close",int(args[2])) 
    elif cmd == "zt":
        zt(args[1],args[2],int(args[3]))
    elif cmd == "pzt":
        pzt(args[1],args[2],int(args[3]))
    elif cmd == "ft":
        ft(args[1],args[2],int(args[3]))
    elif cmd == "pft":
        pft(args[1],args[2],int(args[3]))
    elif cmd == "cancel" or cmd == "c":
        cancel_order(str(line[len(cmd)+1:]))
    elif cmd == "wxy":
        if args[1] == "set":
            if len(args) == 4:
                if args[1] not in wxy_tbl:
                    wxy_time_tbl[args[2]] = time.time()
                wxy_tbl[args[2]] = int(args[3])
        elif args[1] == "c":
            if len(args) == 3 and (args[2] in wxy_tbl):
                del wxy_tbl[args[2]]
                del wxy_time_tbl[args[2]]
        elif args[1] == "show":
            for (k,v) in wxy_tbl.items():
                print(k,v)
    elif cmd == "cls":
        cls()
    elif cmd == "quit":
        close = True
        break

time.sleep(1)
