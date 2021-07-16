/////////////////////////////////////////////////////////////////////////
///@system ��һ������ϵͳ
///@company SunGard China
///@file KSVocApi.h
///@brief �����˿ͻ��˿ͻ����ƽӿ�
/////////////////////////////////////////////////////////////////////////

#ifndef __KSVOCAPI_H_INCLUDED_
#define __KSVOCAPI_H_INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "KSUserApiStructEx.h"
#include "KSVocApiStruct.h"

#if defined(ISLIB) && defined(WIN32) && !defined(KSTRADEAPI_STATIC_LIB)
#ifdef LIB_TRADER_API_EXPORT
#define TRADER_VOCAPI_EXPORT __declspec(dllexport)
#else
#define TRADER_VOCAPI_EXPORT __declspec(dllimport)
#endif
#else
#ifdef WIN32
#define TRADER_VOCAPI_EXPORT 
#else
#define TRADER_VOCAPI_EXPORT __attribute__((visibility("default")))
#endif

#endif

namespace KingstarAPI
{

	class CKSVocSpi
	{
	public:
		///��ѯ����ǰ�ĳֲ���ϸӦ��
		virtual void OnRspQryInvestorOpenPosition(CThostFtdcInvestorPositionDetailField *pInvestorPositionDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��ѯ����ǰ����ϳֲ���ϸӦ��
		virtual void OnRspQryInvestorOpenCombinePosition(CThostFtdcInvestorPositionCombineDetailField *pInvestorPositionCombineDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///������������������Ӧ
		virtual void OnRspBulkCancelOrder(CThostFtdcBulkCancelOrderField *pBulkCancelOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///ƽ�ֲ��Բ�ѯ��Ӧ
		virtual void OnRspQryCloseStrategy(CKSCloseStrategyResultField *pCloseStrategy, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

		///��ϲ��Բ�ѯ��Ӧ
		virtual void OnRspQryCombStrategy(CKSCombStrategyResultField *pCombStrategy, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

		///��Ȩ��ϲ��Բ�ѯ��Ӧ
		virtual void OnRspQryOptionCombStrategy(CKSOptionCombStrategyResultField *pOptionCombStrategy, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

		///�����ѯ�ͻ�ת����Ϣ��Ӧ
		virtual void OnRspQryTransferInfo(CKSTransferInfoResultField *pResultField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯ����֪ͨ��Ӧ
		virtual void OnRspQryKSTradingNotice(CKSTradingNoticeField *pTradingNotice, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�û��˲�Ʒ��Դ��ѯӦ��
		virtual void OnRspQryUserProductUrl(CKSUserProductUrlField *pUserProductUrl, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ϲ�ֵ�����ѯ������Ӧ
		virtual void OnRspQryMaxCombActionVolume(CKSMaxCombActionVolumeField *pMaxCombActionVolume, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///����֪ͨ
		virtual void OnRtnKSTradingNotice(CKSTradingNoticeField *pTradingNoticeInfo) {};

		///�����ѯ��Ȩ��Լ��������Ӧ
		virtual void OnRspQryKSInstrCommRate(CKSInstrCommRateField *pInstrCommRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯETF��Ȩ��Լ��������Ӧ
		virtual void OnRspQryKSETFOptionInstrCommRate(CKSETFOptionInstrCommRateField *pETFOptionInstrCommRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯ��Լ��֤������Ӧ
		virtual void OnRspQryKSInstrumentMarginRate(CKSInstrumentMarginRateField *pInstrumentMarginRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�ͻ�ÿ�ճ�����������Ӧ
		virtual void OnRspWithdrawCreditApply(CKSInputWithdrawCreditApplyField *pWithdrawCreditApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�ͻ�ÿ�ճ����Ȳ�ѯ��Ӧ
		virtual void OnRspQryWithdrawCredit(CKSRspQryWithdrawCreditField *pRspQryWithdrawCredit, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�ͻ�ÿ�ճ����������ѯ��Ӧ
		virtual void OnRspQryWithdrawCreditApply(CKSRspQryWithdrawCreditApplyField *pRspQryWithdrawCreditApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///������ԤԼ��ȡ����������Ӧ
		virtual void OnRspLargeWithdrawApply(CKSLargeWithdrawApplyField *pLargeWithdrawApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///������ԤԼ��ѯ��Ӧ
		virtual void OnRspQryLargeWithdrawApply(CKSRspLargeWithdrawApplyField *pRspLargeWithdrawApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯ���ɵ��ű�֤����Ӧ
		virtual void OnRspQryKSOptionMargin(CKSOptionMarginField *pOptionMargin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯKS���ײ�����Ӧ
		virtual void OnRspQryKSTradingParams(CKSTradingParamsField *pTradingParams, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯKS�ʽ��˻���Ӧ
		virtual void OnRspQryKSTradingAccount(CKSTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯԤԼ�����ʺ���Ӧ
		virtual void OnRspQryWithdrawReservedAccount(CKSWithdrawReservedAccountField *pWithdrawReservedAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///ԤԼ����������Ӧ
		virtual void OnRspWithdrawReservedApply(CKSWithdrawReservedApplyField *pWithdrawReservedApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///ԤԼ������������Ӧ
		virtual void OnRspWithdrawReservedCancel(CKSWithdrawReservedCancelField *pWithdrawReservedCancel, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯԤԼ������Ӧ
		virtual void OnRspQryWithdrawReservedApply(CKSRspWithdrawReservedApplyField *pRspWithdrawReservedApply, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��Ȩ�ԶԳ����Ӧ��
		virtual void OnRspOptionSelfCloseUpdate(CKSInputOptionSelfCloseField *pInputOptionSelfClose, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��Ȩ�ԶԳ���»ر�
		virtual void OnRtnOptionSelfCloseUpdate(CKSOptionSelfCloseField *pOptionSelfClose) {};

		///��Ȩ�ԶԳ����Ӧ��
		virtual void OnRspOptionSelfCloseAction(CKSOptionSelfCloseActionField *pOptionSelfCloseAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��Ȩ�ԶԳ��ѯӦ��
		virtual void OnRspQryOptionSelfClose(CKSOptionSelfCloseField *pOptionSelfClose, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		//�����ѯ��ϱ�֤���Լ������Ӧ
		virtual void OnRspQryCombInstrumentMarginRule(CKSCombInstrumentMarginRuleField *pCombInstrumentMarginRuleField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};
	
		//�����ѯ��Ϻ�Լ������Ӧ
		virtual void OnRspQryCombInstrumentName(CKSCombInstrumentNameField *pCombInstrumentNameField, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///9909����Ӧ��
		virtual void OnRspEncryptionTest(CKSEncryptionTestField *pEncryptionTest, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};
	};

	class TRADER_VOCAPI_EXPORT CKSVocApi
	{
	public:
		///��ѯ����ǰ�ĳֲ���ϸ
		virtual int ReqQueryInvestorOpenPosition(CThostFtdcQryInvestorPositionDetailField *pQryInvestorOpenPosition, int nRequestID) = 0;

		///��ѯ����ǰ����ϳֲ���ϸ
		virtual int ReqQueryInvestorOpenCombinePosition(CThostFtdcQryInvestorPositionCombineDetailField *pQryInvestorOpenCombinePosition, int nRequestID) = 0;

		///��������
		virtual int ReqBulkCancelOrder (CThostFtdcBulkCancelOrderField *pBulkCancelOrder, int nRequestID) = 0;

		///ƽ�ֲ��Բ�ѯ����
		virtual int ReqQryCloseStrategy(CKSCloseStrategy *pCloseStrategy, int nRequestID) = 0;

		///��ϲ��Բ�ѯ����
		virtual int ReqQryCombStrategy(CKSCombStrategy *pCombStrategy, int nRequestID) = 0;

		///��Ȩ��ϲ��Բ�ѯ����
		virtual int ReqQryOptionCombStrategy(CKSOptionCombStrategy *pOptionCombStrategy, int nRequestID) = 0;

		///�����ѯ�ͻ�ת����Ϣ
		virtual int ReqQryTransferInfo(CKSTransferInfo *pTransferInfo, int nRequestID) = 0;

		///�����ѯ����֪ͨ
		virtual int ReqQryKSTradingNotice(CKSQryTradingNoticeField *pQryTradingNotice, int nRequestID) = 0;

		///�û��˲�Ʒ��Դ��ѯ����
		virtual int ReqQryUserProductUrl (CKSQryUserProductUrlField *pQryUserProductUrl, int nRequestID) = 0;

		///�����ϲ�ֵ�����ѯ����
		virtual int ReqQryMaxCombActionVolume(CKSQryMaxCombActionVolumeField *pQryMaxCombActionVolume, int nRequestID) = 0;

		///�����ѯ��Լ������
		virtual int ReqQryKSInstrCommRate(CKSQryInstrCommRateField *pQryInstrCommRate, int nRequestID) = 0;

		///�����ѯETF��Ȩ��Լ������
		virtual int ReqQryKSETFOptionInstrCommRate(CKSQryETFOptionInstrCommRateField *pQryETFOptionInstrCommRate, int nRequestID) = 0;

		///�����ѯ��Լ��֤����
		virtual int ReqQryKSInstrumentMarginRate(CKSQryInstrumentMarginRateField *pQryInstrumentMarginRate, int nRequestID) = 0;

		///�ͻ�ÿ�ճ���������
		virtual int ReqWithdrawCreditApply(CKSInputWithdrawCreditApplyField *pWithdrawCreditApply, int nRequestID) = 0;

		///�ͻ�ÿ�ճ����Ȳ�ѯ
		virtual int ReqQryWithdrawCredit(CKSQryWithdrawCreditField *pQryWithdrawCredit, int nRequestID) = 0;

		///�ͻ�ÿ�ճ����������ѯ
		virtual int ReqQryWithdrawCreditApply(CKSQryWithdrawCreditApplyField *pQryWithdrawCreditApply, int nRequestID) = 0;

		///������ԤԼ��ȡ��������
		virtual int ReqLargeWithdrawApply(CKSLargeWithdrawApplyField *pLargeWithdrawApply, int nRequestID) = 0;

		///������ԤԼ��ѯ
		virtual int ReqQryLargeWithdrawApply(CKSQryLargeWithdrawApplyField *pQryLargeWithdrawApply, int nRequestID) = 0;

		///�߻���֤�����¼����
		virtual int ReqKSEMailLogin(CKSReqEMailLoginField *pReqEMailLoginField, int nRequestID) = 0;

		///�����ѯ���ɵ��ű�֤��
		virtual int ReqQryKSOptionMargin(CKSQryOptionMarginField *pQryOptionMargin, int nRequestID) = 0;

		///�����ѯKS���ײ���
		virtual int ReqQryKSTradingParams(CKSQryTradingParamsField *pQryTradingParams, int nRequestID) = 0;

		///�����ѯKS�ʽ��˻�
		virtual int ReqQryKSTradingAccount(CKSQryTradingAccountField *pQryTradingAccount, int nRequestID) = 0;

		///ԤԼ�����ʺŲ�ѯ
		virtual int ReqQryWithdrawReservedAccount(CKSQryWithdrawReservedAccountField *pQryWithdrawReservedAccount, int nRequestID) = 0;

		///ԤԼ��������
		virtual int ReqWithdrawReservedApply(CKSWithdrawReservedApplyField *pWithdrawReservedApply, int nRequestID) = 0;

		///ԤԼ����������
		virtual int ReqWithdrawReservedCancel(CKSWithdrawReservedCancelField *pWithdrawReservedCancel, int nRequestID) = 0;

		///ԤԼ�����ѯ
		virtual int ReqQryWithdrawReservedApply(CKSQryWithdrawReservedApplyField *pQryWithdrawReservedApply, int nRequestID) = 0;

		///��Ȩ�ԶԳ��������
		virtual int ReqOptionSelfCloseUpdate(CKSInputOptionSelfCloseField *pInputOptionSelfClose, int nRequestID) = 0;

		///��Ȩ�ԶԳ��������
		virtual int ReqOptionSelfCloseAction(CKSOptionSelfCloseActionField *pOptionSelfCloseAction, int nRequestID) = 0;

		///��Ȩ�ԶԳ��ѯ����
		virtual int ReqQryOptionSelfClose(CKSQryOptionSelfCloseField *pQryOptionSelfClose, int nRequestID) = 0;

		///�����ѯ��ϱ�֤���Լ����
		virtual int ReqQryCombInstrumentMarginRule(CKSQryCombInstrumentMarginRuleField *pQryCombInstrumentMarginRule, int nRequestId) = 0;
	
		///�����ѯ��Ϻ�Լ����
		virtual int ReqQryCombInstrumentName(CKSQryCombInstrumentNameField *pQryCombInstrumentName, int nRequestID) = 0;

		///��ѯȷ�Ͻ��㵥ģʽ
		///SettlementInfoΪ����ֶ�
		virtual int ReqQryTraderParameter(CKSSettlementInfoField *SettlementInfo, int nRequestID) = 0;

		///9909���Խӿ�
		virtual int ReqEncryptionTest(CKSEncryptionTestField *pEncryptionTest, int nRequestID) = 0;

	protected:
		~CKSVocApi(){};
	};

}	// end of namespace KingstarAPI
#endif