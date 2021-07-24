#include <stdio.h>
#include <cstring>
#include "ThostFtdcTraderApi.h"
#include "td_op.h"
#include "config.h"
#include "util.h"

int request_id = 1;

// 客户端认证
void TdOp::ReqAuthenticate() {
    printf("客户端认证中...\n");

    CThostFtdcReqAuthenticateField req;
    strcpy(req.BrokerID,BrokerID);
    strcpy(req.UserID,UserID);
    strcpy(req.UserProductInfo,AppID);
    strcpy(req.AuthCode,AuthCode);

    int iResult = tdapi->ReqAuthenticate(&req, ++request_id);

    if( iResult != 0) {
        printf("发送客户端认证请求失败！%d\n", iResult);
    }
}

int TdOp::ReqUserLogin()
{
    CThostFtdcReqUserLoginField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID,BrokerID);
        strcpy(field.UserID, UserID);
        strcpy(field.Password, Password);
        strcpy(field.UserProductInfo, AppID);
    strcpy(field.LoginRemark, CLIENT_IP);
        int ret = tdapi->ReqUserLogin(&field, ++request_id);
        if (0 != ret)
        {
            printf("发送客户端登陆请求失败！%d\n", ret);
        }
}

void TdOp::ReqConfirmSettlement()
{
    CThostFtdcSettlementInfoConfirmField field;
        memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID,BrokerID);
        strcpy(field.InvestorID, UserID);
        int ret = tdapi->ReqSettlementInfoConfirm(&field, 0);
        if(0 != ret)
        {
            printf("确认结算结果失败！%d\n", ret);
        }
}

// 查询持仓
void TdOp::ReqQryInvestorPosition()
{
    CThostFtdcQryInvestorPositionField field = {0};
    memset(&field, 0, sizeof(field));
        strcpy(field.BrokerID,BrokerID);
        strcpy(field.InvestorID, UserID);
        tdapi->ReqQryInvestorPosition(&field, 0);
}

void TdOp::ReqQryInvestorPositionDetail()
{
    CThostFtdcQryInvestorPositionDetailField field = {0};
    memset(&field, 0, sizeof(field));
    strcpy(field.BrokerID,BrokerID);
    strcpy(field.InvestorID, UserID);
    tdapi->ReqQryInvestorPositionDetail(&field, 0);
}

int TdOp::ReqOrderInsert(string name,string dir,string offset,string price,string volume){
    printf("ReqOrderInsert 1\n");
    if(price.size()>0 && !isFloat(price.c_str())) return -1;
    if(!isInt(volume.c_str())) return -1;
    int vol = atoi(volume.c_str());
    if(vol < 1) return -1;
    printf("ReqOrderInsert 2\n");
    CThostFtdcInputOrderField o ={0};
    strcpy(o.BrokerID, BrokerID);
    strcpy(o.InvestorID, UserID);
    strcpy(o.ExchangeID, "DCE");
    strcpy(o.InstrumentID, name.c_str());
    if(dir == "buy"){
        o.Direction = THOST_FTDC_D_Buy;
    }else if(dir == "sell"){
        o.Direction = THOST_FTDC_D_Sell;
    }

    if(price.length()==0){
        o.LimitPrice = 0.0;
        o.OrderPriceType = THOST_FTDC_OPT_AnyPrice;
    }else{
        o.LimitPrice = atof(price.c_str());
        o.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
    }
    o.VolumeTotalOriginal = vol;
    o.ContingentCondition = THOST_FTDC_CC_Immediately;
    if(offset == "open")
        o.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
    else
        o.CombOffsetFlag[0] = THOST_FTDC_OF_Close;
    o.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;    
    o.TimeCondition = THOST_FTDC_TC_GFD ;
    o.VolumeCondition = THOST_FTDC_VC_AV;    
    o.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;  
    int ret = tdapi->ReqOrderInsert(&o, 0);
    if(ret != 0){
        printf("insert order error:%d\n",ret);
    } else{
        printf("insert_order success:\n");
    }
}

void TdOp::ReqOrderAction(void* arg)
{
    auto o = (CThostFtdcOrderField*)arg;
    CThostFtdcInputOrderActionField r = {0};
    strcpy(r.BrokerID, BrokerID);
    strcpy(r.InvestorID, UserID);
    r.FrontID = o->FrontID;
    r.SessionID = o->SessionID;
    memcpy(r.OrderRef,o->OrderRef,sizeof(r.OrderRef));
    memcpy(r.ExchangeID,o->ExchangeID,sizeof(r.ExchangeID));
    memcpy(r.OrderSysID,o->OrderSysID,sizeof(r.OrderSysID));
    r.ActionFlag = THOST_FTDC_AF_Delete;
    int ret = tdapi->ReqOrderAction(&r,++request_id);
    if(ret != 0){
        printf("ReqOrderAction error:%d\n",ret);
    }else{
        printf("c order ");
    }
}
