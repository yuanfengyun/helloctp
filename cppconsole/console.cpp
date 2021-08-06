#include<string>
#include<cstring>
#include<stdio.h>
#include<map>
#include<vector>
#include<iostream>
#include <readline/history.h>
#include "ThostFtdcUserApiStruct.h"
#include "console.h"
#include "util.h"
#include "msg.h"
#include "td_op.h"
#include "md_op.h"
#include "position.h"
#include "wxy_schedule.h"

using namespace std;

bool position_inited = false;
map<string,CThostFtdcDepthMarketDataField*> market_datas;
map<string,CThostFtdcInvestorPositionField*> position_datas;
map<string,CThostFtdcTradeField*> trade_datas;
map<string,CThostFtdcOrderField*> order_datas;
map<string,Position*> positions;
map<string,int> wxy_long_datas;
map<string,int> wxy_short_datas;
map<string,string> diff_map;

WxySchedule* schedule = NULL;

int n_2_order_n = 0;
map<int,string> n_2_order;
map<string,int> order_2_n;
map<string,int> ordersysid_2_key;
map<string,string> ordersysid_2_order;

void init_wxy_schedule(char* filename){
   schedule = new WxySchedule;
   schedule->init(filename);
}

int get_order_id(CThostFtdcOrderField* p){
    char key_buff[256]={0};
    sprintf(key_buff,"%d-%d-%s",p->FrontID,p->SessionID,p->OrderRef);
    string key = string(key_buff);
    auto it = order_2_n.find(key);
    if(it ==order_2_n.end()) return 0;
    return it->second;
}

void update_position_with_trade(CThostFtdcTradeField* t)
{
    string key = t->InstrumentID;
    auto itp = positions.find(key);
    Position* p = NULL;
    if(itp == positions.end()){
        p = new Position(t->ExchangeID,t->InstrumentID);
        positions[key] = p;
    }else{
        p = itp->second;
    }
    // 开仓
    if(t->OffsetFlag==THOST_FTDC_OF_Open){
        if(t->Direction == THOST_FTDC_D_Buy){
            p->Long += t->Volume;
        }
        else if(t->Direction == THOST_FTDC_D_Sell){
            p->Short += t->Volume;
        }
    }else{
        if(t->Direction == THOST_FTDC_D_Buy){
            p->Short -= t->Volume;
        }
        else if(t->Direction == THOST_FTDC_D_Sell){
            p->Long -= t->Volume;
        }
    }
}



// 开仓锁定（money)
int get_open_frozen(){

}

void update_position_with_order(){
}

float GetOrderPrice1(CThostFtdcTradeField* p){
    auto it = ordersysid_2_order.find(string(p->OrderSysID));
    if(it != ordersysid_2_order.end()){
        auto it_order = order_datas.find(it->second);
        if(it_order != order_datas.end() && it_order->second->OrderPriceType==THOST_FTDC_OPT_LimitPrice){
            return it_order->second->LimitPrice;
        }
    }

return p->Price;
}

void* handle_msg(Msg* msg)
{
    char key_buff[512];
    memset(key_buff,0,sizeof(key_buff));
    if(msg->id == msg_market_data){
        auto* p = (CThostFtdcDepthMarketDataField*)msg->ptr;
        string ins = string(p->InstrumentID);
        auto it = market_datas.find(ins);
        if(it!=market_datas.end()){
            delete it->second;
        }else{
            if(schedule !=NULL && schedule->m_name == ins && position_inited){
                schedule->run();
            }
        }
        market_datas[ins] = p;
        //printf("data haha\n");
    }
    if(msg->id == msg_position_data){
        //printf("position data\n");
        auto* p = (CThostFtdcInvestorPositionField*)msg->ptr;
        string ins = string(p->InstrumentID);
        auto itp = positions.find(ins);
        if(itp == positions.end()){
            positions[ins] = new Position(p);
        }else{
            if(p->PosiDirection == THOST_FTDC_PD_Long){
                itp->second->LastLong = p->YdPosition;
                itp->second->Long = p->Position;
            }
            else if(p->PosiDirection == THOST_FTDC_PD_Short){
                itp->second->LastShort = p->YdPosition;
                itp->second->Short = p->Position;
            }
        }

        //  根据成交记录整理出当前持仓
        if(msg->isLast){
            for(auto it = trade_datas.begin();it!=trade_datas.end();it++){
                //dpdate_position_with_trade(it->second);
            }
            position_inited = true;
            if(schedule !=NULL && market_datas.find(schedule->m_name)!=market_datas.end()) schedule->run();
        }
        ins += " ";
        ins += getPositionDir(p->PosiDirection);
        auto it = position_datas.find(ins);
        if(it!=position_datas.end()){
            delete it->second;
        }
        position_datas[ins] = p;
        //printf("position msg:%s\n",ins.c_str());
    }
    if(msg->id == msg_trade_data){
        auto* p = (CThostFtdcTradeField*)msg->ptr;
        sprintf(key_buff,"%s-%s-%s-%s-%c",p->TradeDate,p->TradeTime,p->InstrumentID,p->TradeID,p->Direction);
        string key = string(key_buff);
        auto it = trade_datas.find(key);
        if(it!=trade_datas.end()){
            delete it->second;
            return 0;
        }
        trade_datas[key] = p;
        int id = ordersysid_2_key[string(p->OrderSysID)];
        if(position_inited){
            printf("[notify] %s 成交: %4d  %s\t%s %s %6.0f %3d手\n",p->TradeTime,id,p->InstrumentID,
                    getDir(p->Direction),getOffset(p->OffsetFlag),p->Volume,p->Price);
            update_position_with_trade(p);

            auto it_long = wxy_long_datas.find(p->InstrumentID);
            auto it_short = wxy_short_datas.find(p->InstrumentID);

            if(it_long != wxy_long_datas.end()){
                if(p->Direction==THOST_FTDC_D_Buy && p->OffsetFlag==THOST_FTDC_OF_Open){
                    TdOp::ReqOrderInsert(p->InstrumentID,"sell","close",GetOrderPrice1(p)+it_long->second,p->Volume);
                }else if(p->Direction==THOST_FTDC_D_Sell && p->OffsetFlag!=THOST_FTDC_OF_Open){
                    TdOp::ReqOrderInsert(p->InstrumentID,"buy","open",GetOrderPrice1(p)-it_long->second,p->Volume);
                }
            }
            if(it_short != wxy_short_datas.end()){
                if(p->Direction==THOST_FTDC_D_Buy && p->OffsetFlag!=THOST_FTDC_OF_Open){
                    TdOp::ReqOrderInsert(p->InstrumentID,"sell","open",GetOrderPrice1(p)+it_short->second,p->Volume);
                }else if(p->Direction==THOST_FTDC_D_Sell && p->OffsetFlag==THOST_FTDC_OF_Open){
                    TdOp::ReqOrderInsert(p->InstrumentID,"buy","close",GetOrderPrice1(p)-it_short->second,p->Volume);
                }
            }
         }
       }
       if(msg->id == msg_order_data){
          auto* p = (CThostFtdcOrderField*)msg->ptr;
          sprintf(key_buff,"%d-%d-%s",p->FrontID,p->SessionID,p->OrderRef);
          string key = string(key_buff);
          auto it = order_datas.find(key);
          int id = 0;
          bool show = true;
          if(it!=order_datas.end()){
              id = order_2_n[key];
              show = it->second->OrderStatus != p->OrderStatus;
              delete it->second;
          }else{
              id = ++n_2_order_n;
              order_2_n[key] = id;
          }
          order_datas[key] = p;
          n_2_order[id] = key;

          if(p->OrderSysID[0] > 0){
              ordersysid_2_key[string(p->OrderSysID)] = id;
              ordersysid_2_order[string(p->OrderSysID)] = key;
          }
          if(position_inited)
              if(p->OrderStatus != THOST_FTDC_OST_AllTraded &&
                 p->OrderStatus != THOST_FTDC_OST_PartTradedQueueing && 
                 p->OrderStatus != THOST_FTDC_OST_Unknown && show)
              printf("[notify] %s 订单: %4d  %s\t%s %s %6.0lf %3d/%d\t 状态:%c %s\n",
                      p->InsertTime, id, p->InstrumentID,
                      getDir(p->Direction),getOffset(p->CombOffsetFlag[0]),
                      p->LimitPrice,p->VolumeTraded,p->VolumeTotalOriginal,p->OrderStatus,
                      getOrderStatus(p->OrderStatus,p->OrderSubmitStatus,p->StatusMsg).c_str());
          }
}

void handle_cmd(char* cmd)
{
    if(strlen(cmd)==0) return;
    string scmd(cmd);
    vector<std::string> array = splitWithStl(scmd," ");
    string c = array[0];

    if(c == ("help")){
        printf("=============================\n");
        printf("s or show (行情)\n");
        printf("o (有效委托)\n");
        printf("oo (所有委托)\n");
        printf("r or trade (成交记录)\n");
        printf("p (持仓)\n");
        printf("kk 09 4 4600 (空4手09)\n");
        printf("pk 09 4 4600 (平4手09空单)\n");
        printf("kd 09 4 4600 (多4手09)\n");
        printf("pd 09 4 4600 (平4手09多单)\n");
        printf("zt 09 10 (多09空10各1手)\n");
        printf("ft 09 10 (空09多10各1手)\n");
        printf("pzt 09 10 (平多09空10各1手)\n");
        printf("pft 09 10 (平空09多10各1手)\n");
        printf("wxy kd 09 10 (10个点网格刷09多单)\n");
        printf("wxy kk 09 10 (10个点网格刷09空单)\n");
        printf("wxy cd 09 (取消刷09多单)\n");
        printf("wxy ck 09 (取消刷09空单)\n");
    }
    if(c==string("cls") || c == string("clear")){
        printf("\033[2J");
    }
    else if(c == string("s") || c == string("show")){
        printf("========\n");
        for(auto it=market_datas.begin();it!=market_datas.end();++it)
        {
            auto p = it->second;
            printf("%s\t%5d %5d %5d %5d %d\t%d\n",
                    p->InstrumentID,
                    int(p->BidPrice1),
                    int(p->AskPrice1),
                    int(p->HighestPrice),
                    int(p->LowestPrice),
                    int(p->OpenInterest - p->PreOpenInterest),
                    int(p->OpenInterest));
        }

        for(auto it=diff_map.begin();it!=diff_map.end();++it){
            string first = getFullName(it->first);
            string second = getFullName(it->second);
            auto it_first = market_datas.find(first);
            auto it_second = market_datas.find(second);
            if(it_first != market_datas.end() && it_second != market_datas.end()){
                auto p1 = it_first->second;
                auto p2 = it_second ->second;
                printf("%s-%s\t%5d %5d\n",
                    it->first.c_str(),
                    it->second.c_str(),
                    int(p1->BidPrice1 - p2->AskPrice1),
                    int(p1->AskPrice1 - p2->BidPrice1));
            }
        }
    }
    else if(c == string("kd")){
        if(array.size() < 3){
            return;
        }
        string price = "";
        if(array.size()>3)
            price = array[3];
        TdOp::ReqOrderInsert(array[1],"buy","open",price,array[2]);
    }
    else if(c == string("kk")){
        if(array.size() < 3){
            return;
        }
        string price = "";
        if(array.size()>3)
            price = array[3];
        TdOp::ReqOrderInsert(array[1],"sell","open",price,array[2]);
    }
    else if(c == string("pd")){
        if(array.size() < 3){
            return;
        }
        string price = "";
        if(array.size()>3)
            price = array[3];
        TdOp::ReqOrderInsert(array[1],"sell","close",price,array[2]);
    }
    else if(c == string("pk")){
        if(array.size() < 3){
            return;
        }
        string price = "";
        if(array.size()>3)
            price = array[3];
        TdOp::ReqOrderInsert(array[1],"buy","close",price,array[2]);
    }
    else if(c == string("zt")){
        if(array.size() < 3){
            return;
        }
        TdOp::ReqOrderInsert(array[1],"buy","open","","1");
        TdOp::ReqOrderInsert(array[2],"sell","open","","1");
    }
    else if(c == string("ft")){
        if(array.size() < 3){
            return;
        }
        TdOp::ReqOrderInsert(array[1],"sell","open","","1");
        TdOp::ReqOrderInsert(array[2],"buy","open","","1");
    }
    else if(c == string("pzt")){
        if(array.size() < 3){
            return;
        }
        TdOp::ReqOrderInsert(array[1],"sell","close","","1");
        TdOp::ReqOrderInsert(array[2],"buy","close","","1");
    }
    else if(c == string("pft")){
        if(array.size() < 3){
            return;
        }
        TdOp::ReqOrderInsert(array[1],"buy","close","","1");
        TdOp::ReqOrderInsert(array[2],"sell","close","","1");
    }
    else if(c == string("p")){
        printf("====position====\n");
        for(auto it=positions.begin();it!=positions.end();++it)
        {
            if(it->second->Long == 0 && it->second->Short == 0) continue;
            printf("%s\t long:%3d  frozen:%3d\tshort:%3d frozen:%3d\n",
                    it->second->InstrumentID,
                    it->second->Long,
                    get_close_frozen(it->second->InstrumentID,"sell"),
                    it->second->Short,
                    get_close_frozen(it->second->InstrumentID,"buy")
                  );
        }
    }
    else if(c==string("t") || c == string("trade")){
        printf("====trade====\n");
        for(auto it=trade_datas.begin();it!=trade_datas.end();++it)
        {
            printf("%s %s  %s %s%4d手   %.0lf\n",
                    it->second->TradeTime,
                    it->second->InstrumentID,
                    getDir(it->second->Direction),
                    getOffset(it->second->OffsetFlag),
                    it->second->Volume,
                    it->second->Price);
        }
    }
    else if(c == string("o")){
        printf("====valid order====\n");
        for(auto it=order_datas.begin();it!=order_datas.end();++it)
        {
            if(it->second->OrderStatus != THOST_FTDC_OST_PartTradedQueueing and it->second->OrderStatus != THOST_FTDC_OST_NoTradeQueueing) continue;
            string msg = GbkToUtf8(it->second->StatusMsg);
            printf("%s %d\t%s\t%s  %s %6.0lf  %2d/%d\t %s\n",
                    it->second->InsertTime, get_order_id(it->second), it->second->InstrumentID,
                    getDir(it->second->Direction),getOffset(it->second->CombOffsetFlag[0]),
                    it->second->LimitPrice,it->second->VolumeTraded,it->second->VolumeTotalOriginal,msg.c_str());
        }
    }
    else if(c == string("oo")){
        printf("====all order====\n");
        for(auto it=order_datas.begin();it!=order_datas.end();++it)
        {
            string msg = GbkToUtf8(it->second->StatusMsg);
            printf("%s %4d\t%s\t%s  %s %6.0lf  %2d/%d\t %c %s\n",
                    it->second->InsertTime, get_order_id(it->second), it->second->InstrumentID,
                    getDir(it->second->Direction),getOffset(it->second->CombOffsetFlag[0]),
                    it->second->LimitPrice,it->second->VolumeTraded,it->second->VolumeTotalOriginal,
                    it->second->OrderStatus,msg.c_str());
        }
    }
    else if(c == string("c")){
        if(array.size() < 2) return;
        for(int i=1;i<array.size();i++){
            if(!isInt(array[i].c_str())) return;

            int no = atoi(array[i].c_str());
            string key = n_2_order[no];
            auto p = order_datas[key];
            TdOp::ReqOrderAction(p);
        }
    }
    else if(c == string("password")){
        if(array.size() < 2) return;
        TdOp::ReqUserPasswordUpdate(array[1].c_str());
    }
    else if(c == string("wxy")){
        if(array.size()<2) return;
        if(array[1] == string("kd")){
            if(array.size()<4) return;
            int i = atoi(array[3].c_str());
            string name = getFullName(array[2]);
            wxy_long_datas[name] = i;
        }
        if(array[1] == string("kk")){
            if(array.size()<4) return;
            int i = atoi(array[3].c_str());
            string name = getFullName(array[2]);
            wxy_short_datas[name] = i;
        }
        if(array[1] == string("cd")){
            if(array.size()<3) return;
            string name = getFullName(array[2]);
            wxy_long_datas.erase(name);
        }
        if(array[1] == string("ck")){
            if(array.size()<3) return;
            string name = getFullName(array[2]);
            wxy_short_datas.erase(name);
        }
        if(array[1]==string("show")){
            printf("========wxy=======\n");
            for(auto it=wxy_long_datas.begin();it!=wxy_long_datas.end();++it){
                printf("long %s %d\n",it->first.c_str(),it->second);
            }
            for(auto it=wxy_short_datas.begin();it!=wxy_short_datas.end();++it){
                printf("short %s %d\n",it->first.c_str(),it->second);
            }
        }
    }
    else if(c == string("sub")){
        if(array.size() < 2) return;
        for(int i=1;i<array.size();i++){
            MdOp::SubscribeMarketData(getFullName(array[i]).c_str());
        }
    }
    else if(c == string("df")){
        if(array.size() < 3) return;
        diff_map[array[1]] = array[2];
    }
    else if(c == string("call")){
        for(auto it=order_datas.begin();it!=order_datas.end();++it)
        {
            if(it->second->OrderStatus != THOST_FTDC_OST_PartTradedQueueing and it->second->OrderStatus != THOST_FTDC_OST_NoTradeQueueing) continue;
            TdOp::ReqOrderAction(it->second);
        }
    }
    add_history(cmd);
}
