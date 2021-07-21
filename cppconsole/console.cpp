#include<string>
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

void* handle_msg(Msg* msg)
{
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
        string ins = string(p->InstrumentID) + p->PosiDirection;
        auto it = position_datas.find(ins);
        if(it!=position_datas.end()){
            delete it->second;
        }
        position_datas[ins] = p;
        //printf("data haha\n");
    }
}

void handle_cmd(char* cmd)
{

    TdOp::ReqQryInvestorPosition();

    string scmd(cmd);
    vector<std::string> array = splitWithStl(scmd," ");
    string c = array[0];
    cout<<"scmd: "<<c<<endl;
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
        if(array.size()>=3)
            string price = array[3];
        TdOp::ReqOrderInsert(ins,"buy","open",price,volume);
    }
    else if(c.find("kk")==0){
        if(array.size() < 3){
            return;
        }
        string ins = array[1];
        string volume = array[2];
        string price = "";
        if(array.size()>=3)
            string price = array[3];
        TdOp::ReqOrderInsert(ins,"sell","open",price,volume);
    }
    else if(c.find("pd")==0){
        if(array.size() < 3){
            return;
        }
        string ins = array[1];
        string volume = array[2];
        string price = "";
        if(array.size()>=3)
            string price = array[3];
        TdOp::ReqOrderInsert(ins,"sell","close",price,volume);
    }
    else if(c.find("pk")==0){
        if(array.size() < 3){
            return;
        }
        string ins = array[1];
        string volume = array[2];
        string price = "";
        if(array.size()>=3)
            string price = array[3];
        TdOp::ReqOrderInsert(ins,"buy","close",price,volume);
    }
    else if(c.find("p")==0){
        printf("====positions====\n");
        for(auto it=position_datas.begin();it!=position_datas.end();++it)
        {
            printf("%s\t%d %d\t%d\n",
                 it->second->InstrumentID,
                 int(it->second->PosiDirection),
                 int(it->second->Position),
                 int(it->second->PositionCost));
        }
    }
 
}
