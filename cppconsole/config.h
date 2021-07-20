#pragma once

#include <string>
#include "ThostFtdcMdApi.h"
#include "ThostFtdcTraderApi.h"

using namespace std;

extern char MD_FRONT[];
extern char TD_FRONT[];
extern string BrokerID;
extern string UserID;
extern string Password;
extern string AppID;
extern string AuthCode;
extern string CLIENT_IP;

extern CThostFtdcMdApi* mdapi;
extern CThostFtdcTraderApi* tdapi;
