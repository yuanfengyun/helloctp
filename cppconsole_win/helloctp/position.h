#pragma once

#include <string>
#include "ThostFtdcUserApiStruct.h"


using namespace std;

class Position{
    public:
        TThostFtdcExchangeIDType ExchangeID; 
        TThostFtdcInstrumentIDType InstrumentID;
        // 昨日持仓
        long LastLong;
        long LastShort;
        // 当前持仓
        long Long;
        long Short;

        // 锁定的持仓
        long LongFrozen;
        long ShortFrozen;

    Position(CThostFtdcInvestorPositionField* p);
    Position(char* exchangeID,char* instrumentID);
};
