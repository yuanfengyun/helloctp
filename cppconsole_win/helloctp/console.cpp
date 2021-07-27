#include<string>
#include<cstring>
#include<stdio.h>
#include<map>
#include<vector>
#include<iostream>
//#include <readline/history.h>
#include "ThostFtdcUserApiStruct.h"
#include "console.h"
#include "util.h"
#include "msg.h"
#include "td_op.h"
#include "position.h"

using namespace std;

bool position_inited = false;
map<string,CThostFtdcDepthMarketDataField*> market_datas;
map<string,CThostFtdcInvestorPositionField*> position_datas;
map<string,CThostFtdcTradeField*> trade_datas;
map<string,CThostFtdcOrderField*> order_datas;
map<string,Position*> positions;

int n_2_order_n = 0;
map<int,string> n_2_order;
map<string,int> order_2_n;
map<string,int> ordersysid_2_key;

int get_order_id(CThostFtdcOrderField* p){
    char key_buff[256]={0};
    sprintf(key_buff,"%s-%d-%d-%s",p->InsertTime,
            p->FrontID,p->SessionID,p->OrderRef);
    string key = string(key_buff);
    return order_2_n[key];
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

// 平仓锁定的持仓
int get_close_frozen(string InstrumentID,string dir){
    char d = THOST_FTDC_D_Buy;
    int volume = 0;
    if(dir==string("sell")) d = THOST_FTDC_D_Sell;
    for(auto it=order_datas.begin();it!=order_datas.end();++it)
    {
        if(string(it->second->InstrumentID) != InstrumentID) continue;

        if(it->second->OrderStatus != THOST_FTDC_OST_PartTradedQueueing && it->second->OrderStatus != THOST_FTDC_OST_NoTradeQueueing) continue;
        char offset = it->second->CombOffsetFlag[0];
        if(offset == THOST_FTDC_OF_Open) continue;

        char dir = it->second->Direction;
        if(dir == d){
            volume += it->second->VolumeTotal;
        }
    }
    return volume;
}

// 开仓锁定（money)
int get_open_frozen(){
	return 0;
}

void update_position_with_order(){
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
            if(p->PosiDirection == THOST_FTDC_PD_Long)
                itp->second->LastLong = p->YdPosition;
            else if(p->PosiDirection == THOST_FTDC_PD_Short)
                itp->second->LastShort = p->YdPosition;
        }

        //  根据成交记录整理出当前持仓
        if(msg->isLast){
            for(auto it = trade_datas.begin();it!=trade_datas.end();it++){
                update_position_with_trade(it->second);
            }
            position_inited = true;
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
        }
    }
    if(msg->id == msg_order_data){
        auto* p = (CThostFtdcOrderField*)msg->ptr;
        sprintf(key_buff,"%s-%d-%d-%s",p->InsertTime,
                p->FrontID,p->SessionID,p->OrderRef);
        string key = string(key_buff);
        auto it = order_datas.find(key);
        int id = 0;
        if(it!=order_datas.end()){
            id = order_2_n[key];
            delete it->second;
        }else{
            id = ++n_2_order_n;
            order_2_n[key] = id;
        }
        order_datas[key] = p;
        n_2_order[id] = key;

        if(p->OrderSysID[0] > 0){
            ordersysid_2_key[string(p->OrderSysID)] = id;
        }
        if(position_inited)
            printf("[notify] %s 订单: %4d  %s\t%s %s %6.0lf %3d/%d\t %s\n",
                    p->InsertTime, id, p->InstrumentID,
                    getDir(p->Direction),getOffset(p->CombOffsetFlag[0]),
                    p->LimitPrice,p->VolumeTraded,p->VolumeTotalOriginal,
                    getOrderStatus(p->OrderStatus,p->OrderSubmitStatus,p->StatusMsg).c_str());
    }
}

void handle_cmd(char* cmd)
{
    if(strlen(cmd)==0) return;
    string scmd(cmd);
    vector<std::string> array = splitWithStl(scmd," ");
    string c = array[0];

    if(c == string("s") || c == string("show")){
        printf("========\n");
        for(auto it=market_datas.begin();it!=market_datas.end();++it)
        {
            printf("%s\t%d %d\t%d %d\t%d\n",
                    it->second->InstrumentID,
                    int(it->second->BidPrice1),
                    int(it->second->AskPrice1),
                    int(it->second->HighestPrice),
                    int(it->second->LowestPrice),
                    int(it->second->OpenInterest - it->second->PreOpenInterest));
        }
    }
    else if(c == string("kd")){
        if(array.size() < 3){
            return;
        }
        string ins = array[1];
        string volume = array[2];
        string price = "";
        if(array.size()>3)
            price = array[3];
        TdOp::ReqOrderInsert(ins,"buy","open",price,volume);
    }
    else if(c == string("kk")){
        if(array.size() < 3){
            return;
        }
        string ins = array[1];
        string volume = array[2];
        string price = "";
        if(array.size()>3)
            price = array[3];
        TdOp::ReqOrderInsert(ins,"sell","open",price,volume);
    }
    else if(c == string("pd")){
        if(array.size() < 3){
            return;
        }
        string ins = array[1];
        string volume = array[2];
        string price = "";
        if(array.size()>3)
            price = array[3];
        TdOp::ReqOrderInsert(ins,"sell","close",price,volume);
    }
    else if(c == string("pk")){
        if(array.size() < 3){
            return;
        }
        string ins = array[1];
        string volume = array[2];
        string price = "";
        if(array.size()>3)
            price = array[3];
        TdOp::ReqOrderInsert(ins,"buy","close",price,volume);
    }
    else if(c == string("p")){
        printf("====position====\n");
        for(auto it=positions.begin();it!=positions.end();++it)
        {
            printf("%s\t long:%3d  frozen:%3d\tshort:%3d frozen:%3d\n",
                    it->second->InstrumentID,
                    it->second->Long,
                    get_close_frozen(it->second->InstrumentID,"sell"),
                    it->second->Short,
                    get_close_frozen(it->second->InstrumentID,"buy")
                  );
        }
    }
    else if(c == string("qp")){
        TdOp::ReqQryInvestorPosition();
    }
    else if(c == string("trade")){
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
            if(it->second->OrderStatus != THOST_FTDC_OST_PartTradedQueueing && it->second->OrderStatus != THOST_FTDC_OST_NoTradeQueueing) continue;
            string msg = GbkToUtf8(it->second->StatusMsg);
            printf("%s %s\t%s\t%s  %s %6.0lf  %2d/%d\t %s\n",
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
        if(!isInt(array[1].c_str())) return;

        int no = atoi(array[1].c_str());
        string key = n_2_order[no];
        auto p = order_datas[key];
        TdOp::ReqOrderAction(p);
    }
    else if(c == string("password")){
        if(array.size() < 2) return;
        TdOp::ReqUserPasswordUpdate(array[1].c_str());
    }
  //  add_history(cmd);
}
