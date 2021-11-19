#include <cstring>
#include "md_op.h"
#include "config.h"

void MdOp::ReqUserLogin()
{
    CThostFtdcReqUserLoginField req;
    mdapi->ReqUserLogin(&req, 1);
}

void MdOp::SubscribeMarketData()
{
    char* instruments[] = {
        "jd2205","jd2208","jd2209",
        "lh2201","lh2203","lh2205"
    };
    mdapi->SubscribeMarketData(instruments,sizeof(instruments)/sizeof(char*));
}

void MdOp::SubscribeMarketData(const char* name)
{
    char data[32] = {0};
    strcpy(data,name);
    char* instruments[1] = {data};
    mdapi->SubscribeMarketData(instruments,1);
}
