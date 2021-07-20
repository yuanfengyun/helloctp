#include "md_op.h"
#include "config.h"

void MdOp::ReqUserLogin()
{
    CThostFtdcReqUserLoginField req;
    mdapi->ReqUserLogin(&req, 1);
}

void MdOp::SubscribeMarketData()
{
    char* instruments[] = {"jd2108","jd2109","jd2110","jd2111","jd2112","jd2201"};
    mdapi->SubscribeMarketData(instruments,6);
}
