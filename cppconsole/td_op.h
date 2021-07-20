#pragma once

#include <string>

using namespace std;

class TdOp{
public:
    static void ReqAuthenticate();

    static int ReqUserLogin();

    static void ReqConfirmSettlement();

    static void ReqQryInvestorPosition();

    static void ReqOrderInsert(string name,string dir,string offset,float price,int volume);
};
