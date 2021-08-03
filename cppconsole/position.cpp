#include "position.h"
#include <cstring>

Position::Position(CThostFtdcInvestorPositionField* p)
{
    memset(this,0,sizeof(Position));
    strcpy(ExchangeID,p->ExchangeID);
    strcpy(InstrumentID,p->InstrumentID);
    // 昨日持仓
    if(p->PosiDirection == THOST_FTDC_PD_Long){
        LastLong = p->YdPosition;
        Long = p->Position;
    }
    else if(p->PosiDirection == THOST_FTDC_PD_Short)
    {
        LastShort = p->YdPosition;
        Short = p->Position;
    }
    // 当前持仓

    // 锁定的持仓
    LongFrozen = p->LongFrozen;
    ShortFrozen = p->ShortFrozen;
}

Position::Position(char* exchangeID,char* instrumentID){
    memset(this,0,sizeof(Position));
    strcpy(ExchangeID,exchangeID); 
    strcpy(InstrumentID,instrumentID);
}
