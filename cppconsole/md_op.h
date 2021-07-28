#pragma once

class MdOp {
public:
    static void ReqUserLogin();
    static void SubscribeMarketData();

    static void SubscribeMarketData(const char* name);
};
