#include <stdio.h>
#include <cstring>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "tdspi.h"
#include "td_op.h"
#include "util.h"
#include "msg.h"

bool check_error(char* body,CThostFtdcRspInfoField *pRspInfo)
{
    if(pRspInfo == NULL) return false;

    if(pRspInfo->ErrorID == 0){
        return true;
   }
 
    char buf[256];
    memset(buf,0,sizeof(256));

    char* msg = pRspInfo->ErrorMsg;
    GbkToUtf8(msg,strlen(msg),buf,256);
    printf("[error] %s %s \n",body,buf);
    return false;
}

///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
void TdSpi::OnFrontConnected()
{
    printf("[info] 交易服务器连接成功\n");

    TdOp::ReqAuthenticate();
}

///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
///@param nReason 错误原因
///        0x1001 网络读失败
///        0x1002 网络写失败
///        0x2001 接收心跳超时
///        0x2002 发送心跳失败
///        0x2003 收到错误报文
void TdSpi::OnFrontDisconnected(int nReason)
{
    printf("[error] 交易服务器连接断开  %x\n",nReason);
}

///心跳超时警告。当长时间未收到报文时，该方法被调用。
///@param nTimeLapse 距离上次接收报文的时间
void TdSpi::OnHeartBeatWarning(int nTimeLapse)
{
}

///客户端认证响应
void TdSpi::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!check_error("客户端认证失败:",pRspInfo)){
        return;
    }
    TdOp::ReqUserLogin();
}


///登录请求响应
void TdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!check_error("登陆失败:",pRspInfo)){
        return;
    }

    printf("[info] 交易服务器登陆成功\n");
    printf("[info] 自动确认结算单\n");
    printf("[info] 查询持仓\n");
    TdOp::ReqConfirmSettlement();
    TdOp::ReqQryInvestorPosition();
    TdOp::ReqQryTradingAccount();
}

///登出请求响应
void TdSpi::OnRspUserLogout(CThostFtdcUserLogoutField *pUserLogout, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///用户口令更新请求响应
void TdSpi::OnRspUserPasswordUpdate(CThostFtdcUserPasswordUpdateField *pUserPasswordUpdate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!check_error("口令更改失败: ", pRspInfo)){
        return;
    }

    printf("[notify] 口令修改成功！\n");
}

///资金账户口令更新请求响应
void TdSpi::OnRspTradingAccountPasswordUpdate(CThostFtdcTradingAccountPasswordUpdateField *pTradingAccountPasswordUpdate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///报单录入请求响应
void TdSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!check_error("报单失败: ",pRspInfo)){
        return;
    }
}

///预埋单录入请求响应
void TdSpi::OnRspParkedOrderInsert(CThostFtdcParkedOrderField *pParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(!check_error("",pRspInfo)){
        return;
    }
}

///预埋撤单录入请求响应
void TdSpi::OnRspParkedOrderAction(CThostFtdcParkedOrderActionField *pParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///报单操作请求响应
void TdSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    check_error("撤单失败: ",pRspInfo);
}

///投资者结算结果确认响应
void TdSpi::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///删除预埋单响应
void TdSpi::OnRspRemoveParkedOrder(CThostFtdcRemoveParkedOrderField *pRemoveParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///删除预埋撤单响应
void TdSpi::OnRspRemoveParkedOrderAction(CThostFtdcRemoveParkedOrderActionField *pRemoveParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///执行宣告录入请求响应
void TdSpi::OnRspExecOrderInsert(CThostFtdcInputExecOrderField *pInputExecOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///执行宣告操作请求响应
void TdSpi::OnRspExecOrderAction(CThostFtdcInputExecOrderActionField *pInputExecOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///询价录入请求响应
void TdSpi::OnRspForQuoteInsert(CThostFtdcInputForQuoteField *pInputForQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///报价录入请求响应
void TdSpi::OnRspQuoteInsert(CThostFtdcInputQuoteField *pInputQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///报价操作请求响应
void TdSpi::OnRspQuoteAction(CThostFtdcInputQuoteActionField *pInputQuoteAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///批量报单操作请求响应
void TdSpi::OnRspBatchOrderAction(CThostFtdcInputBatchOrderActionField *pInputBatchOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///期权自对冲录入请求响应
void TdSpi::OnRspOptionSelfCloseInsert(CThostFtdcInputOptionSelfCloseField *pInputOptionSelfClose, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///期权自对冲操作请求响应
void TdSpi::OnRspOptionSelfCloseAction(CThostFtdcInputOptionSelfCloseActionField *pInputOptionSelfCloseAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///申请组合录入请求响应
void TdSpi::OnRspCombActionInsert(CThostFtdcInputCombActionField *pInputCombAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询报单响应
void TdSpi::OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询成交响应
void TdSpi::OnRspQryTrade(CThostFtdcTradeField *pTrade, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询投资者持仓响应
void TdSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    static bool login_position = false;
    if(pInvestorPosition == NULL) return;

    if(pRspInfo && !check_error("查询持仓失败：",pRspInfo)){
        return;
    }

    if(!login_position && bIsLast){
        login_position = true;
        printf("[info] 持仓查询完毕\n");
    }
    auto p = new CThostFtdcInvestorPositionField;
    memcpy(p,pInvestorPosition,sizeof(CThostFtdcInvestorPositionField));
    Msg msg(msg_position_data,p,bIsLast);
    write(pipe_fd,&msg,sizeof(msg));
}

///请求查询资金账户响应
void TdSpi::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(pTradingAccount != NULL) {
        printf("[notify] account: 保证金: %f 可用:%f 总共:%f", pTradingAccount->CurrMargin,pTradingAccount->Available, pTradingAccount->CurrMargin + pTradingAccount->Available);
    }else{
        check_error("查询资金账户",pRspInfo);
    }
}

///请求查询投资者响应
void TdSpi::OnRspQryInvestor(CThostFtdcInvestorField *pInvestor, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询交易编码响应
void TdSpi::OnRspQryTradingCode(CThostFtdcTradingCodeField *pTradingCode, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询合约保证金率响应
void TdSpi::OnRspQryInstrumentMarginRate(CThostFtdcInstrumentMarginRateField *pInstrumentMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询合约手续费率响应
void TdSpi::OnRspQryInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField *pInstrumentCommissionRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询交易所响应
void TdSpi::OnRspQryExchange(CThostFtdcExchangeField *pExchange, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询产品响应
void TdSpi::OnRspQryProduct(CThostFtdcProductField *pProduct, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询合约响应
void TdSpi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询行情响应
void TdSpi::OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询投资者结算结果响应
void TdSpi::OnRspQrySettlementInfo(CThostFtdcSettlementInfoField *pSettlementInfo, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询转帐银行响应
void TdSpi::OnRspQryTransferBank(CThostFtdcTransferBankField *pTransferBank, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询投资者持仓明细响应
void TdSpi::OnRspQryInvestorPositionDetail(CThostFtdcInvestorPositionDetailField *pInvestorPositionDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if(pInvestorPositionDetail == NULL && pRspInfo == NULL) return;
    if(pInvestorPositionDetail==NULL && !check_error("查询持仓明细失败:", pRspInfo)) return;
    auto p = new CThostFtdcInvestorPositionDetailField;
    memcpy(p,pInvestorPositionDetail,sizeof(*p));
    Msg msg(msg_position_detail_data,p);
    write(pipe_fd,&msg,sizeof(msg));
}

///请求查询客户通知响应
void TdSpi::OnRspQryNotice(CThostFtdcNoticeField *pNotice, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询结算信息确认响应
void TdSpi::OnRspQrySettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询投资者持仓明细响应
void TdSpi::OnRspQryInvestorPositionCombineDetail(CThostFtdcInvestorPositionCombineDetailField *pInvestorPositionCombineDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///查询保证金监管系统经纪公司资金账户密钥响应
void TdSpi::OnRspQryCFMMCTradingAccountKey(CThostFtdcCFMMCTradingAccountKeyField *pCFMMCTradingAccountKey, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询仓单折抵信息响应
void TdSpi::OnRspQryEWarrantOffset(CThostFtdcEWarrantOffsetField *pEWarrantOffset, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询投资者品种/跨品种保证金响应
void TdSpi::OnRspQryInvestorProductGroupMargin(CThostFtdcInvestorProductGroupMarginField *pInvestorProductGroupMargin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询交易所保证金率响应
void TdSpi::OnRspQryExchangeMarginRate(CThostFtdcExchangeMarginRateField *pExchangeMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询交易所调整保证金率响应
void TdSpi::OnRspQryExchangeMarginRateAdjust(CThostFtdcExchangeMarginRateAdjustField *pExchangeMarginRateAdjust, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询汇率响应
void TdSpi::OnRspQryExchangeRate(CThostFtdcExchangeRateField *pExchangeRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询二级代理操作员银期权限响应
void TdSpi::OnRspQrySecAgentACIDMap(CThostFtdcSecAgentACIDMapField *pSecAgentACIDMap, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询产品报价汇率
void TdSpi::OnRspQryProductExchRate(CThostFtdcProductExchRateField *pProductExchRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询产品组
void TdSpi::OnRspQryProductGroup(CThostFtdcProductGroupField *pProductGroup, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询做市商合约手续费率响应
void TdSpi::OnRspQryMMInstrumentCommissionRate(CThostFtdcMMInstrumentCommissionRateField *pMMInstrumentCommissionRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询做市商期权合约手续费响应
void TdSpi::OnRspQryMMOptionInstrCommRate(CThostFtdcMMOptionInstrCommRateField *pMMOptionInstrCommRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询报单手续费响应
void TdSpi::OnRspQryInstrumentOrderCommRate(CThostFtdcInstrumentOrderCommRateField *pInstrumentOrderCommRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询资金账户响应
void TdSpi::OnRspQrySecAgentTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询二级代理商资金校验模式响应
void TdSpi::OnRspQrySecAgentCheckMode(CThostFtdcSecAgentCheckModeField *pSecAgentCheckMode, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询期权交易成本响应
void TdSpi::OnRspQryOptionInstrTradeCost(CThostFtdcOptionInstrTradeCostField *pOptionInstrTradeCost, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询期权合约手续费响应
void TdSpi::OnRspQryOptionInstrCommRate(CThostFtdcOptionInstrCommRateField *pOptionInstrCommRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询执行宣告响应
void TdSpi::OnRspQryExecOrder(CThostFtdcExecOrderField *pExecOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询询价响应
void TdSpi::OnRspQryForQuote(CThostFtdcForQuoteField *pForQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询报价响应
void TdSpi::OnRspQryQuote(CThostFtdcQuoteField *pQuote, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询期权自对冲响应
void TdSpi::OnRspQryOptionSelfClose(CThostFtdcOptionSelfCloseField *pOptionSelfClose, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询投资单元响应
void TdSpi::OnRspQryInvestUnit(CThostFtdcInvestUnitField *pInvestUnit, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询组合合约安全系数响应
void TdSpi::OnRspQryCombInstrumentGuard(CThostFtdcCombInstrumentGuardField *pCombInstrumentGuard, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询申请组合响应
void TdSpi::OnRspQryCombAction(CThostFtdcCombActionField *pCombAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询转帐流水响应
void TdSpi::OnRspQryTransferSerial(CThostFtdcTransferSerialField *pTransferSerial, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询银期签约关系响应
void TdSpi::OnRspQryAccountregister(CThostFtdcAccountregisterField *pAccountregister, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///错误应答
void TdSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    check_error("错误：",pRspInfo);
}

///报单通知
void TdSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
   auto p = new CThostFtdcOrderField;
    memcpy(p,pOrder,sizeof(*p));
    Msg msg(msg_order_data,p);
    write(pipe_fd,&msg,sizeof(msg));
}

///成交通知
void TdSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
    auto p = new CThostFtdcTradeField;
    memcpy(p,pTrade,sizeof(*p));
    Msg msg(msg_trade_data,p);
    write(pipe_fd,&msg,sizeof(msg));
}

///报单录入错误回报
void TdSpi::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
    if(!check_error("报单失败:",pRspInfo)){
        return;
    }
    printf("[error] 报单录入错误回报\n");
}

///报单操作错误回报
void TdSpi::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
    if(!check_error("撤单失败: ",pRspInfo)){
        return;
    }
    printf("[error] 撤单操作错误回报\n"); 
}

///合约交易状态通知
void TdSpi::OnRtnInstrumentStatus(CThostFtdcInstrumentStatusField *pInstrumentStatus)
{
    //printf("合约交易状态通知\n"); 
}

///交易所公告通知
void TdSpi::OnRtnBulletin(CThostFtdcBulletinField *pBulletin)
{
}

///交易通知
void TdSpi::OnRtnTradingNotice(CThostFtdcTradingNoticeInfoField *pTradingNoticeInfo)
{
    printf("[notify] 交易通知: %s\n",GbkToUtf8(pTradingNoticeInfo->FieldContent).c_str());
}

///提示条件单校验错误
void TdSpi::OnRtnErrorConditionalOrder(CThostFtdcErrorConditionalOrderField *pErrorConditionalOrder)
{
}

///执行宣告通知
void TdSpi::OnRtnExecOrder(CThostFtdcExecOrderField *pExecOrder)
{
}

///执行宣告录入错误回报
void TdSpi::OnErrRtnExecOrderInsert(CThostFtdcInputExecOrderField *pInputExecOrder, CThostFtdcRspInfoField *pRspInfo)
{
}

///执行宣告操作错误回报
void TdSpi::OnErrRtnExecOrderAction(CThostFtdcExecOrderActionField *pExecOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
}

///询价录入错误回报
void TdSpi::OnErrRtnForQuoteInsert(CThostFtdcInputForQuoteField *pInputForQuote, CThostFtdcRspInfoField *pRspInfo)
{
}

///报价通知
void TdSpi::OnRtnQuote(CThostFtdcQuoteField *pQuote)
{
}

///报价录入错误回报
void TdSpi::OnErrRtnQuoteInsert(CThostFtdcInputQuoteField *pInputQuote, CThostFtdcRspInfoField *pRspInfo)
{
}

///报价操作错误回报
void TdSpi::OnErrRtnQuoteAction(CThostFtdcQuoteActionField *pQuoteAction, CThostFtdcRspInfoField *pRspInfo)
{
}

///询价通知
void TdSpi::OnRtnForQuoteRsp(CThostFtdcForQuoteRspField *pForQuoteRsp)
{
}

///保证金监控中心用户令牌
void TdSpi::OnRtnCFMMCTradingAccountToken(CThostFtdcCFMMCTradingAccountTokenField *pCFMMCTradingAccountToken)
{
}

///批量报单操作错误回报
void TdSpi::OnErrRtnBatchOrderAction(CThostFtdcBatchOrderActionField *pBatchOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
}

///期权自对冲通知
void TdSpi::OnRtnOptionSelfClose(CThostFtdcOptionSelfCloseField *pOptionSelfClose)
{
}

///期权自对冲录入错误回报
void TdSpi::OnErrRtnOptionSelfCloseInsert(CThostFtdcInputOptionSelfCloseField *pInputOptionSelfClose, CThostFtdcRspInfoField *pRspInfo)
{
}

///期权自对冲操作错误回报
void TdSpi::OnErrRtnOptionSelfCloseAction(CThostFtdcOptionSelfCloseActionField *pOptionSelfCloseAction, CThostFtdcRspInfoField *pRspInfo)
{
}

///申请组合通知
void TdSpi::OnRtnCombAction(CThostFtdcCombActionField *pCombAction)
{
}

///申请组合录入错误回报
void TdSpi::OnErrRtnCombActionInsert(CThostFtdcInputCombActionField *pInputCombAction, CThostFtdcRspInfoField *pRspInfo)
{
}

///请求查询签约银行响应
void TdSpi::OnRspQryContractBank(CThostFtdcContractBankField *pContractBank, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询预埋单响应
void TdSpi::OnRspQryParkedOrder(CThostFtdcParkedOrderField *pParkedOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询预埋撤单响应
void TdSpi::OnRspQryParkedOrderAction(CThostFtdcParkedOrderActionField *pParkedOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询交易通知响应
void TdSpi::OnRspQryTradingNotice(CThostFtdcTradingNoticeField *pTradingNotice, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询经纪公司交易参数响应
void TdSpi::OnRspQryBrokerTradingParams(CThostFtdcBrokerTradingParamsField *pBrokerTradingParams, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询经纪公司交易算法响应
void TdSpi::OnRspQryBrokerTradingAlgos(CThostFtdcBrokerTradingAlgosField *pBrokerTradingAlgos, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///请求查询监控中心用户令牌
void TdSpi::OnRspQueryCFMMCTradingAccountToken(CThostFtdcQueryCFMMCTradingAccountTokenField *pQueryCFMMCTradingAccountToken, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///银行发起银行资金转期货通知
void TdSpi::OnRtnFromBankToFutureByBank(CThostFtdcRspTransferField *pRspTransfer)
{
}

///银行发起期货资金转银行通知
void TdSpi::OnRtnFromFutureToBankByBank(CThostFtdcRspTransferField *pRspTransfer)
{
}

///银行发起冲正银行转期货通知
void TdSpi::OnRtnRepealFromBankToFutureByBank(CThostFtdcRspRepealField *pRspRepeal)
{
}

///银行发起冲正期货转银行通知
void TdSpi::OnRtnRepealFromFutureToBankByBank(CThostFtdcRspRepealField *pRspRepeal)
{
}

///期货发起银行资金转期货通知
void TdSpi::OnRtnFromBankToFutureByFuture(CThostFtdcRspTransferField *pRspTransfer)
{
}

///期货发起期货资金转银行通知
void TdSpi::OnRtnFromFutureToBankByFuture(CThostFtdcRspTransferField *pRspTransfer)
{
}

///系统运行时期货端手工发起冲正银行转期货请求，银行处理完毕后报盘发回的通知
void TdSpi::OnRtnRepealFromBankToFutureByFutureManual(CThostFtdcRspRepealField *pRspRepeal)
{
}

///系统运行时期货端手工发起冲正期货转银行请求，银行处理完毕后报盘发回的通知
void TdSpi::OnRtnRepealFromFutureToBankByFutureManual(CThostFtdcRspRepealField *pRspRepeal)
{
}

///期货发起查询银行余额通知
void TdSpi::OnRtnQueryBankBalanceByFuture(CThostFtdcNotifyQueryAccountField *pNotifyQueryAccount)
{
}

///期货发起银行资金转期货错误回报
void TdSpi::OnErrRtnBankToFutureByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo)
{
}

///期货发起期货资金转银行错误回报
void TdSpi::OnErrRtnFutureToBankByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo)
{
}

///系统运行时期货端手工发起冲正银行转期货错误回报
void TdSpi::OnErrRtnRepealBankToFutureByFutureManual(CThostFtdcReqRepealField *pReqRepeal, CThostFtdcRspInfoField *pRspInfo)
{
}

///系统运行时期货端手工发起冲正期货转银行错误回报
void TdSpi::OnErrRtnRepealFutureToBankByFutureManual(CThostFtdcReqRepealField *pReqRepeal, CThostFtdcRspInfoField *pRspInfo)
{
}

///期货发起查询银行余额错误回报
void TdSpi::OnErrRtnQueryBankBalanceByFuture(CThostFtdcReqQueryAccountField *pReqQueryAccount, CThostFtdcRspInfoField *pRspInfo)
{
}

///期货发起冲正银行转期货请求，银行处理完毕后报盘发回的通知
void TdSpi::OnRtnRepealFromBankToFutureByFuture(CThostFtdcRspRepealField *pRspRepeal)
{
}

///期货发起冲正期货转银行请求，银行处理完毕后报盘发回的通知
void TdSpi::OnRtnRepealFromFutureToBankByFuture(CThostFtdcRspRepealField *pRspRepeal)
{
}

///期货发起银行资金转期货应答
void TdSpi::OnRspFromBankToFutureByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///期货发起期货资金转银行应答
void TdSpi::OnRspFromFutureToBankByFuture(CThostFtdcReqTransferField *pReqTransfer, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///期货发起查询银行余额应答
void TdSpi::OnRspQueryBankAccountMoneyByFuture(CThostFtdcReqQueryAccountField *pReqQueryAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
}

///银行发起银期开户通知
void TdSpi::OnRtnOpenAccountByBank(CThostFtdcOpenAccountField *pOpenAccount)
{
}

///银行发起银期销户通知
void TdSpi::OnRtnCancelAccountByBank(CThostFtdcCancelAccountField *pCancelAccount)
{
}

///银行发起变更银行账号通知
void TdSpi::OnRtnChangeAccountByBank(CThostFtdcChangeAccountField *pChangeAccount)
{
}
