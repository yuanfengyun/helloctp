#include<string>
#include<cstring>
#include<stdio.h>
#include<map>
#include<vector>
#include<iostream>
#include "ThostFtdcUserApiStruct.h"
#include "console.h"
#include "util.h"
#include "msg.h"
#include "td_op.h"

using namespace std;

map<string,CThostFtdcDepthMarketDataField*> market_datas;
map<string,CThostFtdcInvestorPositionField*> position_datas;
map<string,CThostFtdcTradeField*> trade_datas;
map<string,CThostFtdcOrderField*> order_datas;

int n_2_order_n = 0;
map<int,string> n_2_order;
map<string,int> order_2_n;

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
        auto* p = (CThostFtdcInvestorPositionField*)msg->ptr;
        string ins = string(p->InstrumentID);
        ins += " ";
        ins += getPositionDir(p->PosiDirection);
        auto it = position_datas.find(ins);
        if(it!=position_datas.end()){
            delete it->second;
        }
        position_datas[ins] = p;
        printf("position msg:%s\n",ins.c_str());
    }
    if(msg->id == msg_trade_data){
        auto* p = (CThostFtdcTradeField*)msg->ptr;
        sprintf(key_buff,"%s-%s-%s-%s-%c",p->TradeDate,
                p->TradeTime,p->InstrumentID,p->TradeID,p->Direction);
        string key = string(key_buff);
        auto it = trade_datas.find(key);
        if(it!=trade_datas.end()){
            delete it->second;
        }
        trade_datas[key] = p;
        printf("trade msg:%s\n",key.c_str());
    }
    if(msg->id == msg_order_data){
        auto* p = (CThostFtdcOrderField*)msg->ptr;
        sprintf(key_buff,"%s-%d-%d-%s",p->InsertTime,
                p->FrontID,p->SessionID,p->OrderRef);
        string key = string(key_buff);
        auto it = order_datas.find(key);
        if(it!=order_datas.end()){
            delete it->second;
        }
        order_datas[key] = p;
        n_2_order[++n_2_order_n] = key;
        sprintf(p->reserve1,"%d",n_2_order_n);
        printf("order msg:%s\n",key.c_str());
    }
}

void handle_cmd(char* cmd)
{
    if(strlen(cmd)==0) return;
    string scmd(cmd);
    vector<std::string> array = splitWithStl(scmd," ");
    string c = array[0];

    if(c.find("s")==0){
        printf("====aa====\n");
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
    else if(c.find("kd")==0){
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
    else if(c.find("kk")==0){
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
    else if(c.find("pd")==0){
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
    else if(c.find("pk")==0){
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
    else if(c.find("p")==0){
        printf("====positions====\n");
        for(auto it=position_datas.begin();it!=position_datas.end();++it)
        {
            printf("%s\t%s\t%d\t%.2lf\n",
                    it->second->InstrumentID,
                    getPositionDir(it->second->PosiDirection),
                    it->second->Position,
                    it->second->PositionCost/it->second->Position);
        }
    }
    else if(c.find("qp")==0){
        TdOp::ReqQryInvestorPosition();
    }
    else if(c.find("trade")==0){
        printf("====trades====\n");
        for(auto it=trade_datas.begin();it!=trade_datas.end();++it)
        {
            printf("%s-%s %s\t%s %s\t%d\t%.2lf\n",
                    it->second->TradeDate,
                    it->second->TradeTime,
                    it->second->InstrumentID,
                    getDir(it->second->Direction),
                    getOffset(it->second->OffsetFlag),
                    it->second->Volume,
                    it->second->Price);
        }
    }
    else if(c.find("o")==0){
        printf("====orders====\n");
        for(auto it=order_datas.begin();it!=order_datas.end();++it)
        {
            string msg = GbkToUtf8(it->second->StatusMsg);
            printf("no:%s\t%s %s\t%s %s %.2lf %d/%d\t %s\n",
                    it->second->reserve1,it->second->InsertTime,it->second->InstrumentID,
                    getDir(it->second->Direction),it->second->CombOffsetFlag,
                    it->second->LimitPrice,it->second->VolumeTraded,it->second->VolumeTotalOriginal,msg.c_str());
        }
    }
    else if(c.find("c")==0){
        if(array.size() < 2) return;
        if(!isInt(array[1].c_str())) return;

        int no = atoi(array[1].c_str());
        string key = n_2_order[no];
        auto p = order_datas[key];
        TdOp::ReqOrderAction(p);
    }
}
