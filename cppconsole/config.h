#pragma once

#include <string>
#include <map>
#include "ThostFtdcMdApi.h"
#include "ThostFtdcTraderApi.h"
#include "position.h"

using namespace std;

extern char MD_FRONT[];
extern char TD_FRONT[];
extern char BrokerID[];
extern char UserID[];
extern char Password[];
extern char AppID[];
extern char AuthCode[];
extern char CLIENT_IP[];

extern CThostFtdcMdApi* mdapi;
extern CThostFtdcTraderApi* tdapi;

extern map<string,CThostFtdcDepthMarketDataField*> market_datas;
extern map<string,CThostFtdcInvestorPositionField*> position_datas;
extern map<string,CThostFtdcTradeField*> trade_datas;
extern map<string,CThostFtdcOrderField*> order_datas;
extern map<string,Position*> positions;

extern map<string,int> wxy_long_datas;
extern map<string,int> wxy_short_datas;


