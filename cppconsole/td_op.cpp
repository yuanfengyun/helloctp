#include <stdio.h>
#include <cstring>
#include "ThostFtdcTraderApi.h"
#include "td_op.h"
#include "config.h"

int request_id = 1;

// 客户端认证
void TdOp::ReqAuthenticate() {
	printf("客户端认证中...\n");

	CThostFtdcReqAuthenticateField req;
	strcpy(req.BrokerID,BrokerID.c_str());
	strcpy(req.UserID,UserID.c_str());
	strcpy(req.UserProductInfo,AppID.c_str());
	strcpy(req.AuthCode,AuthCode.c_str());

	int iResult = tdapi->ReqAuthenticate(&req, ++request_id);

	if( iResult != 0) {
		printf("发送客户端认证请求失败！%d\n", iResult);
	}
}

int TdOp::ReqUserLogin()
{
	CThostFtdcReqUserLoginField field;
		memset(&field, 0, sizeof(field));
		strcpy(field.BrokerID,BrokerID.c_str());
		strcpy(field.UserID, UserID.c_str());
		strcpy(field.Password, Password.c_str());
		strcpy(field.UserProductInfo, AppID.c_str());
	strcpy(field.LoginRemark, CLIENT_IP.c_str());
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
		strcpy(field.BrokerID,BrokerID .c_str());
		strcpy(field.InvestorID, UserID.c_str());
		int ret = tdapi->ReqSettlementInfoConfirm(&field, 0);
		if(0 != ret)
		{

		}
}

// 查询持仓
void TdOp::ReqQryInvestorPosition()
{
    CThostFtdcQryInvestorPositionField qrypositionfield = {0};
    tdapi->ReqQryInvestorPosition(&qrypositionfield, 0);
}

void TdOp::ReqOrderInsert(string name,string dir,string offset,float price,int volume){
	CThostFtdcInputOrderField o ={0};
    strcpy(o.BrokerID, BrokerID.c_str());
	strcpy(o.InvestorID, UserID.c_str());
	strcpy(o.ExchangeID, "DCE");
	strcpy(o.InstrumentID, name.c_str());
	o.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
    if(dir == "buy"){
	    o.Direction = THOST_FTDC_D_Buy;
    }else if(dir == "sell"){
        o.Direction = THOST_FTDC_D_Sell;
    }
	o.LimitPrice = price;
	o.VolumeTotalOriginal = volume;    
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
}
