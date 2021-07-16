/////////////////////////////////////////////////////////////////////////
///@system 风控前置系统
///@company CFFEX
///@file USTPFtdcTraderApi.h
///@brief 定义了客户端接口
///@history
///20130520	佘鹏飞	创建该文件
/////////////////////////////////////////////////////////////////////////

#pragma once

#include "USTPFtdcUserApiStruct.h"

#if defined(ISLIB) && defined(WIN32)
#ifdef LIB_TRADER_API_EXPORT
#define TRADER_API_EXPORT __declspec(dllexport)
#else
#define TRADER_API_EXPORT __declspec(dllimport)
#endif
#else
#define TRADER_API_EXPORT 
#endif

class CUstpFtdcTraderSpi
{
public:
	///当客户端与交易后台建立起通信连接时（还未登录前），该方法被调用。
	virtual void OnFrontConnected(){};
	virtual void OnQryFrontConnected(){};	
	///当客户端与交易后台通信连接断开时，该方法被调用。当发生这个情况后，API会自动重新连接，客户端可不做处理。
	///@param nReason 错误原因
	///        0x1001 网络读失败
	///        0x1002 网络写失败
	///        0x2001 接收心跳超时
	///        0x2002 发送心跳失败
	///        0x2003 收到错误报文
	virtual void OnFrontDisconnected(int nReason){};
	virtual void OnQryFrontDisconnected(int nReason){};
	
	///心跳超时警告。当长时间未收到报文时，该方法被调用。
	///@param nTimeLapse 距离上次接收报文的时间
	virtual void OnHeartBeatWarning(int nTimeLapse){};
	
	///报文回调开始通知。当API收到一个报文后，首先调用本方法，然后是各数据域的回调，最后是报文回调结束通知。
	///@param nTopicID 主题代码（如私有流、公共流、行情流等）
	///@param nSequenceNo 报文序号
	virtual void OnPackageStart(int nTopicID, int nSequenceNo){};
	
	///报文回调结束通知。当API收到一个报文后，首先调用报文回调开始通知，然后是各数据域的回调，最后调用本方法。
	///@param nTopicID 主题代码（如私有流、公共流、行情流等）
	///@param nSequenceNo 报文序号
	virtual void OnPackageEnd(int nTopicID, int nSequenceNo){};


	///错误应答
	virtual void OnRspError(CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///风控前置系统用户登录应答
	virtual void OnRspUserLogin(CUstpFtdcRspUserLoginField *pRspUserLogin, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///用户退出应答
	virtual void OnRspUserLogout(CUstpFtdcRspUserLogoutField *pRspUserLogout, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///用户密码修改应答
	virtual void OnRspUserPasswordUpdate(CUstpFtdcUserPasswordUpdateField *pUserPasswordUpdate, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///请求对账单确认响应
	virtual void OnRspNotifyConfirm(CUstpFtdcInputNotifyConfirmField *pInputNotifyConfirm, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///请求查询对账单确认响应
	virtual void OnRspQryNotifyConfirm(CUstpFtdcNotifyConfirmField *pNotifyConfirm, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///报单录入应答
	virtual void OnRspOrderInsert(CUstpFtdcInputOrderField *pInputOrder, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///报单操作应答
	virtual void OnRspOrderAction(CUstpFtdcOrderActionField *pOrderAction, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///报价录入应答
	virtual void OnRspQuoteInsert(CUstpFtdcInputQuoteField *pInputQuote, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///报价操作应答
	virtual void OnRspQuoteAction(CUstpFtdcQuoteActionField *pQuoteAction, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///询价请求应答
	virtual void OnRspForQuote(CUstpFtdcReqForQuoteField *pReqForQuote, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///客户申请组合应答
	virtual void OnRspMarginCombAction(CUstpFtdcInputMarginCombActionField *pInputMarginCombAction, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///数据流回退通知
	virtual void OnRtnFlowMessageCancel(CUstpFtdcFlowMessageCancelField *pFlowMessageCancel) {};

	///成交回报
	virtual void OnRtnTrade(CUstpFtdcTradeField *pTrade) {};

	///报单回报
	virtual void OnRtnOrder(CUstpFtdcOrderField *pOrder) {};

	///报单录入错误回报
	virtual void OnErrRtnOrderInsert(CUstpFtdcInputOrderField *pInputOrder, CUstpFtdcRspInfoField *pRspInfo) {};

	///报单操作错误回报
	virtual void OnErrRtnOrderAction(CUstpFtdcOrderActionField *pOrderAction, CUstpFtdcRspInfoField *pRspInfo) {};

	///合约交易状态通知
	virtual void OnRtnInstrumentStatus(CUstpFtdcInstrumentStatusField *pInstrumentStatus) {};

	///报价回报
	virtual void OnRtnQuote(CUstpFtdcRtnQuoteField *pRtnQuote) {};

	///报价录入错误回报
	virtual void OnErrRtnQuoteInsert(CUstpFtdcInputQuoteField *pInputQuote, CUstpFtdcRspInfoField *pRspInfo) {};

	///报价撤单错误回报
	virtual void OnErrRtnQuoteAction(CUstpFtdcOrderActionField *pOrderAction, CUstpFtdcRspInfoField *pRspInfo) {};

	///询价回报
	virtual void OnRtnForQuote(CUstpFtdcReqForQuoteField *pReqForQuote) {};

	///增加组合规则通知
	virtual void OnRtnMarginCombinationLeg(CUstpFtdcMarginCombinationLegField *pMarginCombinationLeg) {};

	///客户申请组合确认
	virtual void OnRtnMarginCombAction(CUstpFtdcInputMarginCombActionField *pInputMarginCombAction) {};

	///查询前置系统用户登录应答
	virtual void OnRspQueryUserLogin(CUstpFtdcRspUserLoginField *pRspUserLogin, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///报单查询应答
	virtual void OnRspQryOrder(CUstpFtdcOrderField *pOrder, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///成交单查询应答
	virtual void OnRspQryTrade(CUstpFtdcTradeField *pTrade, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///可用投资者账户查询应答
	virtual void OnRspQryUserInvestor(CUstpFtdcRspUserInvestorField *pRspUserInvestor, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///交易编码查询应答
	virtual void OnRspQryTradingCode(CUstpFtdcRspTradingCodeField *pRspTradingCode, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///投资者资金账户查询应答
	virtual void OnRspQryInvestorAccount(CUstpFtdcRspInvestorAccountField *pRspInvestorAccount, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///合约查询应答
	virtual void OnRspQryInstrument(CUstpFtdcRspInstrumentField *pRspInstrument, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///交易所查询应答
	virtual void OnRspQryExchange(CUstpFtdcRspExchangeField *pRspExchange, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///投资者持仓查询应答
	virtual void OnRspQryInvestorPosition(CUstpFtdcRspInvestorPositionField *pRspInvestorPosition, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///订阅主题应答
	virtual void OnRspSubscribeTopic(CUstpFtdcDisseminationField *pDissemination, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///合规参数查询应答
	virtual void OnRspQryComplianceParam(CUstpFtdcRspComplianceParamField *pRspComplianceParam, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///主题查询应答
	virtual void OnRspQryTopic(CUstpFtdcDisseminationField *pDissemination, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///投资者手续费率查询应答
	virtual void OnRspQryInvestorFee(CUstpFtdcInvestorFeeField *pInvestorFee, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///投资者保证金率查询应答
	virtual void OnRspQryInvestorMargin(CUstpFtdcInvestorMarginField *pInvestorMargin, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///交易编码组合持仓查询应答
	virtual void OnRspQryInvestorCombPosition(CUstpFtdcRspInvestorCombPositionField *pRspInvestorCombPosition, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///交易编码单腿持仓查询应答
	virtual void OnRspQryInvestorLegPosition(CUstpFtdcRspInvestorLegPositionField *pRspInvestorLegPosition, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///合约组信息查询应答
	virtual void OnRspQryInstrumentGroup(CUstpFtdcRspInstrumentGroupField *pRspInstrumentGroup, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///组合保证金类型查询应答
	virtual void OnRspQryClientMarginCombType(CUstpFtdcRspClientMarginCombTypeField *pRspClientMarginCombType, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///行权录入应答
	virtual void OnRspExecOrderInsert(CUstpFtdcInputExecOrderField *pInputExecOrder, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///行权操作应答
	virtual void OnRspExecOrderAction(CUstpFtdcInputExecOrderActionField *pInputExecOrderAction, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///行权通知
	virtual void OnRtnExecOrder(CUstpFtdcExecOrderField *pExecOrder) {};

	///行权录入错误回报
	virtual void OnErrRtnExecOrderInsert(CUstpFtdcInputExecOrderField *pInputExecOrder, CUstpFtdcRspInfoField *pRspInfo) {};

	///行权操作错误回报
	virtual void OnErrRtnExecOrderAction(CUstpFtdcInputExecOrderActionField *pInputExecOrderAction, CUstpFtdcRspInfoField *pRspInfo) {};

	///系统时间查询应答
	virtual void OnRspQrySystemTime(CUstpFtdcRspQrySystemTimeField *pRspQrySystemTime, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///风险通知
	virtual void OnRtnRiskNotify(CUstpFtdcRiskNotifyField *pRiskNotify) {};

	///请求查询投资者结算结果响应
	virtual void OnRspQuerySettlementInfo(CUstpFtdcSettlementRspField *pSettlementRsp, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///资金账户口令更新请求响应
	virtual void OnRspTradingAccountPasswordUpdate(CUstpFtdcTradingAccountPasswordUpdateRspField *pTradingAccountPasswordUpdateRsp, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///请求查询仓单折抵信息响应
	virtual void OnRspQueryEWarrantOffset(CUstpFtdcEWarrantOffsetFieldRspField *pEWarrantOffsetFieldRsp, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///请求查询转帐流水响应
	virtual void OnRspQueryTransferSeriaOffset(CUstpFtdcTransferSerialFieldRspField *pTransferSerialFieldRsp, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///请求查询银期签约关系响应
	virtual void OnRspQueryAccountregister(CUstpFtdcAccountregisterRspField *pAccountregisterRsp, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///期货发起银行资金转期货应答
	virtual void OnRspFromBankToFutureByFuture(CUstpFtdcTransferFieldReqField *pTransferFieldReq, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///期货发起银行资金转期货通知
	virtual void OnRtnFromBankToFutureByFuture(CUstpFtdcTransferFieldReqField *pTransferFieldReq) {};

	///期货发起期货资金转银行应答
	virtual void OnRspFromFutureToBankByFuture(CUstpFtdcTransferFieldReqField *pTransferFieldReq, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///期货发起期货资金转银行通知
	virtual void OnRtnFromFutureToBankByFuture(CUstpFtdcTransferFieldReqField *pTransferFieldReq) {};

	///银行发起期货资金转银行通知
	virtual void OnRtnFromFutureToBankByBank(CUstpFtdcTransferFieldReqField *pTransferFieldReq) {};

	///银行发起银行资金转期货通知
	virtual void OnRtnFromBankToFutureByBank(CUstpFtdcTransferFieldReqField *pTransferFieldReq) {};

	///期货发起查询银行余额应答
	virtual void OnRspQueryBankAccountMoneyByFuture(CUstpFtdcAccountFieldRspField *pAccountFieldRsp, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///期货发起查询银行余额通知
	virtual void OnRtnQueryBankBalanceByFuture(CUstpFtdcAccountFieldRtnField *pAccountFieldRtn) {};

	///银行发起银期开户通知
	virtual void OnRtnOpenAccountByBank(CUstpFtdcSignUpOrCancleAccountRspFieldField *pSignUpOrCancleAccountRspField) {};

	///银行发起银期销户通知
	virtual void OnRtnCancelAccountByBank(CUstpFtdcSignUpOrCancleAccountRspFieldField *pSignUpOrCancleAccountRspField) {};

	///银行发起银行资金转期货应答
	virtual void OnRspFromBankToFutureByBank(CUstpFtdcTransferFieldReqField *pTransferFieldReq, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///银行发起期货资金转银行应答
	virtual void OnRspFromFutureToBankByBank(CUstpFtdcTransferFieldReqField *pTransferFieldReq, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///金仕达发起银行资金转期货通知
	virtual void OnRtnFromBankToFutureByJSD(CUstpFtdcJSDMoneyField *pJSDMoney) {};

	///金仕达发起期货资金转银行通知
	virtual void OnRtnFromFutureToBankByJSD(CUstpFtdcJSDMoneyField *pJSDMoney) {};

	///银期转账投资者资金账户变动通知
	virtual void OnRtnTransferInvestorAccountChanged(CUstpFtdcTransferFieldReqField *pTransferFieldReq) {};

	///期货发起银行账户签约应答
	virtual void OnRspSignUpAccountByFuture(CUstpFtdcSignUpOrCancleAccountRspFieldField *pSignUpOrCancleAccountRspField, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///期货发起银行账户解约应答
	virtual void OnRspCancleAccountByFuture(CUstpFtdcSignUpOrCancleAccountRspFieldField *pSignUpOrCancleAccountRspField, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///请求查询银行签约状态响应
	virtual void OnRspQuerySignUpOrCancleAccountStatus(CUstpFtdcQuerySignUpOrCancleAccountStatusRspFieldField *pQuerySignUpOrCancleAccountStatusRspField, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///期货发起银行账户变更应答
	virtual void OnRspChangeAccountByFuture(CUstpFtdcChangeAccountRspFieldField *pChangeAccountRspField, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///仿真交易帐户申请响应
	virtual void OnRspOpenSimTradeAccount(CUstpFtdcSimTradeAccountInfoField *pSimTradeAccountInfo, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///仿真交易帐户查询响应
	virtual void OnRspCheckOpenSimTradeAccount(CUstpFtdcSimTradeAccountInfoField *pSimTradeAccountInfo, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///查询保证金监管系统经纪公司资金账户密钥响应
	virtual void OnRspCFMMCTradingAccountKey(CUstpFtdcCFMMCTradingAccountKeyRspField *pCFMMCTradingAccountKeyRsp, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///消息通知
	virtual void OnRtnMsgNotify(CUstpFtdcMsgNotifyField *pMsgNotify) {};

	///持仓变化通知
	virtual void OnRtnClientPositionChange(CUstpFtdcClientPositionChangeField *pClientPositionChange) {};

	///资金变化通知
	virtual void OnRtnInvestorAccountChange(CUstpFtdcInvestorAccountChangeField *pInvestorAccountChange) {};

	///允许持仓变化通知应答
	virtual void OnRspQryEnableRtnMoneyPositoinChange(CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///历史订单查询应答
	virtual void OnRspQueryHisOrder(CUstpFtdcOrderField *pOrder, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

	///历史成交单查询应答
	virtual void OnRspQueryHisTrade(CUstpFtdcTradeField *pTrade, CUstpFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};
};

class TRADER_API_EXPORT CUstpFtdcTraderApi
{
public:
	///创建TraderApi
	///@param pszFlowPath 存贮订阅信息文件的目录，默认为当前目录
	///@return 创建出的UserApi
	static CUstpFtdcTraderApi *CreateFtdcTraderApi(const char *pszFlowPath = "");
	
	///获取系统版本号
	///@param nMajorVersion 主版本号
	///@param nMinorVersion 子版本号
	///@return 系统标识字符串
	static const char *GetVersion(int &nMajorVersion, int &nMinorVersion);
	
	///删除接口对象本身
	///@remark 不再使用本接口对象时,调用该函数删除接口对象
	virtual void Release() = 0;
	
	///初始化
	///@remark 初始化运行环境,只有调用后,接口才开始工作
	virtual void Init() = 0;
	
	///等待接口线程结束运行
	///@return 线程退出代码
	virtual int Join() = 0;
	
	///获取当前交易日
	///@retrun 获取到的交易日
	///@remark 只有登录成功后,才能得到正确的交易日
	virtual const char *GetTradingDay() = 0;
	
	///注册前置机网络地址
	///@param pszFrontAddress：前置机网络地址。
	///@remark 网络地址的格式为：“protocol://ipaddress:port”，如：”tcp://127.0.0.1:17001”。 
	///@remark “tcp”代表传输协议，“127.0.0.1”代表服务器地址。”17001”代表服务器端口号。
	virtual void RegisterFront(char *pszFrontAddress) = 0;
	virtual void RegisterQryFront(char *pszFrontAddress) = 0;
	
	///注册名字服务器网络地址
	///@param pszNsAddress：名字服务器网络地址。
	///@remark 网络地址的格式为：“protocol://ipaddress:port”，如：”tcp://127.0.0.1:12001”。 
	///@remark “tcp”代表传输协议，“127.0.0.1”代表服务器地址。”12001”代表服务器端口号。
	///@remark RegisterFront优先于RegisterNameServer
	virtual void RegisterNameServer(char *pszNsAddress) = 0;
	
	///注册回调接口
	///@param pSpi 派生自回调接口类的实例
	virtual void RegisterSpi(CUstpFtdcTraderSpi *pSpi) = 0;
	
	///加载证书
	///@param pszCertFileName 用户证书文件名
	///@param pszKeyFileName 用户私钥文件名
	///@param pszCaFileName 可信任CA证书文件名
	///@param pszKeyFilePassword 用户私钥文件密码
	///@return 0 操作成功
	///@return -1 可信任CA证书载入失败
	///@return -2 用户证书载入失败
	///@return -3 用户私钥载入失败	
	///@return -4 用户证书校验失败
	virtual int RegisterCertificateFile(const char *pszCertFileName, const char *pszKeyFileName, 
		const char *pszCaFileName, const char *pszKeyFilePassword) = 0;

	///订阅私有流。
	///@param nResumeType 私有流重传方式  
	///        USTP_TERT_RESTART:从本交易日开始重传
	///        USTP_TERT_RESUME:从上次收到的续传
	///        USTP_TERT_QUICK:只传送登录后私有流的内容
	///@remark 该方法要在Init方法前调用。若不调用则不会收到私有流的数据。
	virtual void SubscribePrivateTopic(USTP_TE_RESUME_TYPE nResumeType) = 0;
	
	///订阅公共流。
	///@param nResumeType 公共流重传方式  
	///        USTP_TERT_RESTART:从本交易日开始重传
	///        USTP_TERT_RESUME:从上次收到的续传
	///        USTP_TERT_QUICK:只传送登录后公共流的内容
	///@remark 该方法要在Init方法前调用。若不调用则不会收到公共流的数据。
	virtual void SubscribePublicTopic(USTP_TE_RESUME_TYPE nResumeType) = 0;

	///订阅交易员流。
	///@param nResumeType 交易员流重传方式  
	///        USTP_TERT_RESTART:从本交易日开始重传
	///        USTP_TERT_RESUME:从上次收到的续传
	///        USTP_TERT_QUICK:只传送登录后交易员流的内容
	///@remark 该方法要在Init方法前调用。若不调用则不会收到交易员流的数据。
	virtual void SubscribeUserTopic(USTP_TE_RESUME_TYPE nResumeType) = 0;
	///订阅询价流。
	///@param nResumeType 交易员流重传方式  
	///        USTP_TERT_RESTART:从本交易日开始重传
	///        USTP_TERT_RESUME:从上次收到的续传
	///        USTP_TERT_QUICK:只传送登录后交易员流的内容
	///@remark 该方法要在Init方法前调用。若不调用则不会收到交易员流的数据。
	virtual void SubscribeForQuote(USTP_TE_RESUME_TYPE nResumeType) = 0;
	
	///设置心跳超时时间。
	///@param timeout 心跳超时时间(秒)  
	virtual void SetHeartbeatTimeout(unsigned int timeout) = 0;
	
	///打开请求日志文件
	///@param pszReqLogFileName 请求日志文件名  
	///@return 0 操作成功
	///@return -1 打开日志文件失败
	virtual int OpenRequestLog(const char *pszReqLogFileName) = 0;

	///打开应答日志文件
	///@param pszRspLogFileName 应答日志文件名  
	///@return 0 操作成功
	///@return -1 打开日志文件失败
	virtual int OpenResponseLog(const char *pszRspLogFileName) = 0;


	///风控前置系统用户登录请求
	virtual int ReqUserLogin(CUstpFtdcReqUserLoginField *pReqUserLogin, int nRequestID) = 0;

	///用户退出请求
	virtual int ReqUserLogout(CUstpFtdcReqUserLogoutField *pReqUserLogout, int nRequestID) = 0;

	///用户密码修改请求
	virtual int ReqUserPasswordUpdate(CUstpFtdcUserPasswordUpdateField *pUserPasswordUpdate, int nRequestID) = 0;

	///请求对账单确认
	virtual int ReqNotifyConfirm(CUstpFtdcInputNotifyConfirmField *pInputNotifyConfirm, int nRequestID) = 0;

	///请求查询对账单确认
	virtual int ReqQryNotifyConfirm(CUstpFtdcQryNotifyConfirmField *pQryNotifyConfirm, int nRequestID) = 0;

	///报单录入请求
	virtual int ReqOrderInsert(CUstpFtdcInputOrderField *pInputOrder, int nRequestID) = 0;

	///报单操作请求
	virtual int ReqOrderAction(CUstpFtdcOrderActionField *pOrderAction, int nRequestID) = 0;

	///请求报价录入
	virtual int ReqQuoteInsert(CUstpFtdcInputQuoteField *pInputQuote, int nRequestID) = 0;

	///报价操作请求
	virtual int ReqQuoteAction(CUstpFtdcQuoteActionField *pQuoteAction, int nRequestID) = 0;

	///客户询价请求
	virtual int ReqForQuote(CUstpFtdcReqForQuoteField *pReqForQuote, int nRequestID) = 0;

	///客户申请组合请求
	virtual int ReqMarginCombAction(CUstpFtdcInputMarginCombActionField *pInputMarginCombAction, int nRequestID) = 0;

	///报单查询请求
	virtual int ReqQryOrder(CUstpFtdcQryOrderField *pQryOrder, int nRequestID) = 0;

	///成交单查询请求
	virtual int ReqQryTrade(CUstpFtdcQryTradeField *pQryTrade, int nRequestID) = 0;

	///可用投资者账户查询请求
	virtual int ReqQryUserInvestor(CUstpFtdcQryUserInvestorField *pQryUserInvestor, int nRequestID) = 0;

	///交易编码查询请求
	virtual int ReqQryTradingCode(CUstpFtdcQryTradingCodeField *pQryTradingCode, int nRequestID) = 0;

	///投资者资金账户查询请求
	virtual int ReqQryInvestorAccount(CUstpFtdcQryInvestorAccountField *pQryInvestorAccount, int nRequestID) = 0;

	///合约查询请求
	virtual int ReqQryInstrument(CUstpFtdcQryInstrumentField *pQryInstrument, int nRequestID) = 0;

	///交易所查询请求
	virtual int ReqQryExchange(CUstpFtdcQryExchangeField *pQryExchange, int nRequestID) = 0;

	///投资者持仓查询请求
	virtual int ReqQryInvestorPosition(CUstpFtdcQryInvestorPositionField *pQryInvestorPosition, int nRequestID) = 0;

	///订阅主题请求
	virtual int ReqSubscribeTopic(CUstpFtdcDisseminationField *pDissemination, int nRequestID) = 0;

	///合规参数查询请求
	virtual int ReqQryComplianceParam(CUstpFtdcQryComplianceParamField *pQryComplianceParam, int nRequestID) = 0;

	///主题查询请求
	virtual int ReqQryTopic(CUstpFtdcDisseminationField *pDissemination, int nRequestID) = 0;

	///投资者手续费率查询请求
	virtual int ReqQryInvestorFee(CUstpFtdcQryInvestorFeeField *pQryInvestorFee, int nRequestID) = 0;

	///投资者保证金率查询请求
	virtual int ReqQryInvestorMargin(CUstpFtdcQryInvestorMarginField *pQryInvestorMargin, int nRequestID) = 0;

	///交易编码组合持仓查询请求
	virtual int ReqQryInvestorCombPosition(CUstpFtdcQryInvestorCombPositionField *pQryInvestorCombPosition, int nRequestID) = 0;

	///交易编码单腿持仓查询请求
	virtual int ReqQryInvestorLegPosition(CUstpFtdcQryInvestorLegPositionField *pQryInvestorLegPosition, int nRequestID) = 0;

	///查询合约组信息
	virtual int ReqQryInstrumentGroup(CUstpFtdcQryUstpInstrumentGroupField *pQryUstpInstrumentGroup, int nRequestID) = 0;

	///查询组合保证金类型
	virtual int ReqQryClientMarginCombType(CUstpFtdcQryClientMarginCombTypeField *pQryClientMarginCombType, int nRequestID) = 0;

	///行权录入请求
	virtual int ReqExecOrderInsert(CUstpFtdcInputExecOrderField *pInputExecOrder, int nRequestID) = 0;

	///行权操作请求
	virtual int ReqExecOrderAction(CUstpFtdcInputExecOrderActionField *pInputExecOrderAction, int nRequestID) = 0;

	///查询系统时间
	virtual int ReqQrySystemTime(CUstpFtdcReqQrySystemTimeField *pReqQrySystemTime, int nRequestID) = 0;

	///请求查询投资者结算结果
	virtual int ReqQuerySettlementInfo(CUstpFtdcSettlementQryReqField *pSettlementQryReq, int nRequestID) = 0;

	///资金账户口令更新请求
	virtual int ReqTradingAccountPasswordUpdate(CUstpFtdcTradingAccountPasswordUpdateReqField *pTradingAccountPasswordUpdateReq, int nRequestID) = 0;

	///请求查询仓单折抵信息
	virtual int ReqQueryEWarrantOffset(CUstpFtdcEWarrantOffsetFieldReqField *pEWarrantOffsetFieldReq, int nRequestID) = 0;

	///请求查询转帐流水
	virtual int ReqQueryTransferSeria(CUstpFtdcTransferSerialFieldReqField *pTransferSerialFieldReq, int nRequestID) = 0;

	///请求查询银期签约关系
	virtual int ReqQueryAccountregister(CUstpFtdcAccountregisterReqField *pAccountregisterReq, int nRequestID) = 0;

	///期货发起银行资金转期货请求
	virtual int ReqFromBankToFutureByFuture(CUstpFtdcTransferFieldReqField *pTransferFieldReq, int nRequestID) = 0;

	///期货发起期货资金转银行请求
	virtual int ReqFromFutureToBankByFuture(CUstpFtdcTransferFieldReqField *pTransferFieldReq, int nRequestID) = 0;

	///期货发起查询银行余额请求
	virtual int ReqQueryBankAccountMoneyByFuture(CUstpFtdcAccountFieldReqField *pAccountFieldReq, int nRequestID) = 0;

	///期货发起银行账户签约请求
	virtual int ReqSignUpAccountByFuture(CUstpFtdcSignUpOrCancleAccountReqFieldField *pSignUpOrCancleAccountReqField, int nRequestID) = 0;

	///期货发起银行账户解约请求
	virtual int ReqCancleAccountByFuture(CUstpFtdcSignUpOrCancleAccountReqFieldField *pSignUpOrCancleAccountReqField, int nRequestID) = 0;

	///请求查询银行签约状态
	virtual int ReqQuerySignUpOrCancleAccountStatus(CUstpFtdcQuerySignUpOrCancleAccountStatusReqFieldField *pQuerySignUpOrCancleAccountStatusReqField, int nRequestID) = 0;

	///期货发起银行账户变更请求
	virtual int ReqChangeAccountByFuture(CUstpFtdcChangeAccountReqFieldField *pChangeAccountReqField, int nRequestID) = 0;

	///仿真交易帐户申请
	virtual int ReqOpenSimTradeAccount(CUstpFtdcSimTradeAccountInfoField *pSimTradeAccountInfo, int nRequestID) = 0;

	///仿真交易帐户查询
	virtual int ReqCheckOpenSimTradeAccount(CUstpFtdcSimTradeAccountInfoField *pSimTradeAccountInfo, int nRequestID) = 0;

	///请求查询保证金监管系统经纪公司资金账户密钥
	virtual int ReqCFMMCTradingAccountKey(CUstpFtdcCFMMCTradingAccountKeyReqField *pCFMMCTradingAccountKeyReq, int nRequestID) = 0;

	///允许持仓变化通知请求
	virtual int ReqQryEnableRtnMoneyPositoinChange(CUstpFtdcEnableRtnMoneyPositoinChangeField *pEnableRtnMoneyPositoinChange, int nRequestID) = 0;

	///历史订单查询请求
	virtual int ReqQueryHisOrder(CUstpFtdcQryHisOrderField *pQryHisOrder, int nRequestID) = 0;

	///历史成交单查询请求
	virtual int ReqQueryHisTrade(CUstpFtdcQryHisTradeField *pQryHisTrade, int nRequestID) = 0;
protected:
	~CUstpFtdcTraderApi(){};
};

