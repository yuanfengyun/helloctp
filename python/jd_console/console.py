import time
import threading
import platform
import datetime
import os
import sys

sys.path.insert(0,"../")

from tqsdk import TqApi, TqAccount

if platform.system() == "Linux":
    import readline

def cls():
    if platform.system() == "Linux":
        print("\033[2J")
    else:
        os.system("cls")

account = ""
password = ""

if len(sys.argv) >= 3:
    account = sys.argv[1]
    password = sys.argv[2]
else:
    import login
    account = login.account
    password = login.password

api = TqApi(TqAccount(account, password))

l = []
mquote = {}
mposition = {}
schdules = []

today = datetime.date.today()
current_year = today.year
current_month = today.month
m2name = {}
month2long = {}

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

def ask_price(quote):
    try:
        return int(quote.ask_price1)
    except:
        return int(quote.upper_limit)

def bid_price(quote):
    try:
        return int(quote.bid_price1)
    except:
        return int(quote.lower_limit)

def print_tl_price(name1,name2):
    price1 = str(ask_price(mquote[name1]) - bid_price(mquote[name2]))
    price2 = str(bid_price(mquote[name1]) - ask_price(mquote[name2]))
    print(name1+"-"+name2+"\t"+price2+"\t"+price1)

def show():
    print("==============show==============")
    for i in l:
        quote = mquote[i]
        print(i,"\t",bid_price(quote),"\t",ask_price(quote),"\t",quote.highest,"\t",quote.lowest,"\t",quote.open_interest-quote.pre_open_interest)
    print("")
    print_tl_price("04","05")
    print_tl_price("05","06")
    print_tl_price("05","07")
    print_tl_price("06","07")
    print_tl_price("06","08")
    print_tl_price("07","08")
    print_tl_price("08","09")
    print_tl_price("09","10")
    print_tl_price("09","01")

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
    print("auto zt 07 08 -700 -650 1")
    print("auto ft 12 01 0 200 1")
    print("auto kd 09 4100 4300 1")
    print("auto kk 09 4300 4100 1")
    print("auto show")
    print("cancel order_id(get from order)")
    print("")

def position():
    print("============position================")
    for i in l:
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
    else:
        ss = month.split("&")
        symbol = "DCE.SP jd" + month2long[ss[0]] + "&jd" + month2long[ss[1]]
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

def tl_auto(type, month1,month2,open_price,close_price,volume,max_times=1):
    schdules.append({
        "type" : type,
        "name1" : month1,
        "name2" : month2,
        "open_price" : open_price,
        "close_price" : close_price,
        "volume" : volume,
        "status" : "ready",
        "times" : 0,
        "max_times" : max_times
    })

def db_auto(type, month,open_price,close_price,volume,max_times=1):
    schdules.append({
        "type" : type,
        "name" : month,
        "open_price" : open_price,
        "close_price" : close_price,
        "volume" : volume,
        "status" : "ready",
        "times" : 0,
        "max_times" : max_times
    })

def show_auto():
    for s in schdules:
        print("---------------")
        print("type:",s["type"])
        if "name" in s:
            print("name:",s["name"])
        else:
            print("name1:",s["name1"])
            print("name2:",s["name2"])
        print("open_price:",s["open_price"])
        print("close_price:",s["close_price"])
        print("volume:",s["volume"])
        print("status:",s["status"])
        if "open_order1" in s:
            print("open_order1:",s["open_order1"])
        if "open_order2" in s:
            print("open_order2:",s["open_order2"])
        if "close_order1" in s:
            print("close_order1:",s["close_order1"])
        if "close_order2" in s:
            print("close_order2:",s["close_order2"])

close = False

class WorkingThread(threading.Thread):
    def run(self):
        while True:
            api.wait_update()
            if close:
               api.close()
               break
            for v in schdules:
                if v["status"] == "ready":
                    if v["type"] == "zt" and ask_price(mquote[v["name1"]]) - bid_price(mquote[v["name2"]]) <= v["open_price"]:
                        o = zt(v["name1"],v["name2"],v["volume"])
                        v["open_order1"] = o[0]
                        v["open_order2"] = o[1]
                        v["status"] = "opening"
                    
                    if v["type"] == "ft" and bid_price(mquote[v["name1"]]) - ask_price(mquote[v["name2"]]) >= v["open_price"]:
                        o = ft(v["name1"],v["name2"],v["volume"])
                        v["open_order1"] = o[0]
                        v["open_order2"] = o[1]
                        v["status"] = "opening"

                    if v["type"] == "kd":
                        o = insert_order(v["name"],"buy","open",v["volume"],v["open_price"])
                        v["open_order1"] = o
                        v["status"] = "opening"
                
                    if v["type"] == "kk":
                        o = insert_order(v["name"],"sell","open",v["volume"],v["open_price"])
                        v["open_order1"] = o
                        v["status"] = "opening"

                elif v["status"] == "opening":
                    open_order1 = v["open_order1"]
                    if open_order1.volume_left > 0:
                        continue
                    if "open_order2" in v:
                        open_order2 = v["open_order2"]
                        if open_order2.volume_left > 0:
                            continue
                    v["status"] = "opened"
                elif v["status"] == "opened":
                    if v["type"] == "zt" and bid_price(mquote[v["name1"]]) - ask_price(mquote[v["name2"]]) >= v["close_price"]:
                        o = pzt(v["name1"],v["name2"],v["volume"])
                        v["close_order1"] = o[0]
                        v["close_order2"] = o[1]
                        v["status"] = "closing"

                    if v["type"] == "ft" and ask_price(mquote[v["name1"]]) - bid_price(mquote[v["name2"]]) <= v["close_price"]:
                        o = pft(v["name1"],v["name2"],v["volume"])
                        v["close_order1"] = o[0]
                        v["close_order2"] = o[1]
                        v["status"] = "closing"
                    if v["type"] == "kd":
                        o = insert_order(v["name"],"sell","close",v["volume"],v["close_price"])
                        v["close_order1"] = o
                        v["status"] = "closing"
                    if v["type"] == "kk":
                        o = insert_order(v["name"],"buy","close",v["volume"],v["close_price"])
                        v["close_order1"] = o
                        v["status"] = "closing"
                elif v["status"] == "closing":
                    close_order1 = v["close_order1"]
                    if close_order1.volume_left > 0:
                        continue
                    if "close_order2" in v:
                        close_order2 = v["close_order2"]
                        if close_order2.volume_left > 0:
                            continue
                    v ["times"] = v["times"] + 1
                    if v["times"] < v["max_times"]:
                        v["status"] = "ready"
                    else:
                        v["status"] = "done"

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
    elif cmd == "auto":
        if args[1] == "zt":
            tl_auto("zt",args[2],args[3],int(args[4]),int(args[5]),int(args[6]))
        elif args[1] == "ft":
            tl_auto("ft",args[2],args[3],int(args[4]),int(args[5]),int(args[6]))
        elif args[1] == "kd":
            if len(args) >= 7:
                db_auto("kd",args[2],int(args[3]),int(args[4]),int(args[5]),int(args[6]))
            else:
                db_auto("kd",args[2],int(args[3]),int(args[4]),int(args[5]))
        elif args[1] == "kk":
            if len(args) >= 7:
                db_auto("kk",args[2],int(args[3]),int(args[4]),int(args[5]),int(args[6]))
            else:
                db_auto("kk",args[2],int(args[3]),int(args[4]),int(args[5]))
        elif args[1] == "show":
            show_auto()
    elif cmd == "cls":
        cls()
    elif cmd == "quit":
        close = True
        break

time.sleep(1)

