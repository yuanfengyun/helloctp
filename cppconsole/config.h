#pragma once

#include <string>
#include "ThostFtdcMdApi.h"
#include "ThostFtdcTraderApi.h"

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
