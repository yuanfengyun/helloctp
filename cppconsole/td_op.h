#pragma once

#include <string>

using namespace std;

class TdOp{
public:
    static void ReqAuthenticate();

    static int ReqUserLogin();

    static void ReqConfirmSettlement();

    static void ReqQryInvestorPositionDetail();
    static void ReqQryInvestorPosition();

    static int ReqOrderInsert(string name,string dir,string offset,string price,string volume);

    static void ReqOrderAction(void* arg);

    static void ReqUserPasswordUpdate(const char* password);

    static void ReqQryTradingAccount();
};
