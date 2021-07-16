/////////////////////////////////////////////////////////////////////////
///@system ��һ������ϵͳ
///@company SunGard China
///@file KSOptionApi.h
///@brief �����˿ͻ�����Ȩ��ӿ�
/////////////////////////////////////////////////////////////////////////

#ifndef __KSOPTIONAPI_H_INCLUDED__
#define __KSOPTIONAPI_H_INCLUDED__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "KSVocApiStruct.h"

#if defined(ISLIB) && defined(WIN32) && !defined(KSTRADEAPI_STATIC_LIB)
#ifdef LIB_TRADER_API_EXPORT
#define TRADER_OPTIONAPI_EXPORT __declspec(dllexport)
#else
#define TRADER_OPTIONAPI_EXPORT __declspec(dllimport)
#endif
#else
#ifdef WIN32
#define TRADER_OPTIONAPI_EXPORT 
#else
#define TRADER_OPTIONAPI_EXPORT __attribute__((visibility("default")))
#endif

#endif

namespace KingstarAPI
{

	class CKSOptionSpi
	{
	public:
		///��ѯ��ָ�ֻ�ָ��
		virtual void OnRspQryIndexPrice(CKSIndexPriceField *pIndexPrice, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

		///��ѯ��ָ�ֻ�ָ��
		virtual void OnRspQryOptionInstrGuard(CKSOptionInstrGuardField *pOptionInstrGuard, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

		///��ѯ��������������Ӧ
		virtual void OnRspQryExecOrderVolume(CKSExecOrderVolumeField *pExecOrderVolume, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

		///��ѯ������Ȩָ����Ϣ������Ӧ
		virtual void OnRspQryStockOptionAssignment(CKSStockOptionAssignmentField *pStockOptionAssignment, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��ѯ�ͻ����׼�����Ӧ
		virtual void OnRspQryInvestorTradeLevel(CKSInvestorTradeLevelField *pInvestorTradeLevel, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��ѯ����ϵͳ�����޹������Ӧ
		virtual void OnRspQryPurchaseLimitAmtT(CKSPurchaseLimitAmtField *pPurchaseLimitAmt, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��ѯ����ϵͳ�����޲ֶ����Ӧ
		virtual void OnRspQryPositionLimitVolT(CKSPositionLimitVolTField *pPositionLimitVol, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��ѯ����ϵͳ�����޹������Ӧ
		virtual void OnRspQryPurchaseLimitAmtS(CKSPurchaseLimitAmtSField *pPurchaseLimitAmt, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��ѯ����ϵͳ�����޲ֶ����Ӧ
		virtual void OnRspQryPositionLimitVolS(CKSPositionLimitVolSField *pPositionLimitVol, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����޹���ȱ����Ӧ
		virtual void OnRspPurchaseLimitAmtUpdate(CKSPurchaseLimitAmtField *pPurchaseLimitAmt, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����޲ֶ�ȱ����Ӧ
		virtual void  OnRspPositionLimitVolUpdate(CKSPositionLimitVolField *pPositionLimitVol, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯ������ʷ������Ӧ
		virtual void OnRspQryHistoryOrder(CKSHistoryOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯ������ʷ�ɽ���Ӧ
		virtual void OnRspQryHistoryTrade(CKSHistoryTradeField *pTrade, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯ������ʷ��Ȩָ����ϸ��Ӧ
		virtual void OnRspQryStockOptionHistoryAssignment(CKSHistoryAssignmentField *pHistoryAssignment, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�����ѯ������Ȩ������ϸ��Ӧ
		virtual void OnRspQryStockOptionDelivDetail(CKSSODelivDetailField *pSODelivDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///�Զ���Ȩִ�в�����Ӧ
		virtual void OnRspAutoExecOrderAction(CKSAutoExecOrderActionField *pAutoExecOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///����������¼��������Ӧ
		virtual void OnRspCombActionInsert(CKSInputCombActionField *pInputCombAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///������ɳֲ�ת����Ӧ
		virtual void OnRspPositionConvert(CKSInputPositionConvertField *pInputPositionConvert,CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast){};

		///��ѯ������ϳֲ���ϸӦ��
		virtual void OnRspQryInvestorCombinePosition(CKSInvestorPositionCombineDetailField *pInvestorPositionCombineDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///���ɿ���Ͽɲ������������Ӧ
		virtual void OnRspQryCombActionVolume(CKSCombActionVolumeField *pCombActionVolume, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��֤�����̱��۵��޸�������Ӧ
		virtual void OnRspQuoteUpdate(CKSInputQuoteUpdateField *pInputQuoteUpdate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///���������Ȩί��������Ӧ
		virtual void OnRspCombExecOrderInsert(CKSInputCombExecOrderField *pInputCombExecOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///���������Ȩ����������Ӧ
		virtual void OnRspCombExecOrderAction(CKSInputCombExecOrderActionField *pInputCombExecOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��ѯ���������Ȩ����������������Ӧ
		virtual void OnRspQryCombExecOrderVolume(CKSCombExecOrderVolumeField *pCombExecOrderVolume, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///��ѯ���ɿͻ�����������Ӧ
		virtual void OnRspQryStockOptionAccount(CKSStockOptionAccountField *pStockOptionAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) {};

		///���ɳֲ�ת������֪ͨ
		virtual void OnRtnPositionConvert(CKSPositionConvertField *pPositionConvert){};

		///������ϲ��ί��֪ͨ
		virtual void OnRtnCombAction(CKSCombActionField *pCombAction) {};

		///���������Ȩί��֪ͨ
		virtual void OnRtnCombExecOrder(CKSCombExecOrderField *pCombExecOrder) {};
	};

	class TRADER_OPTIONAPI_EXPORT CKSOptionApi
	{
	public:
		///��ѯ��ָ�ֻ�ָ��
		virtual int ReqQryIndexPrice(CKSQryIndexPriceField *pIndexPrice, int nRequestID) = 0;

		///��ѯ��Ȩ��Լ����ϵ��
		virtual int ReqQryOptionInstrGuard(CKSQryOptionInstrGuardField *pQryOptionInstrGuard, int nRequestID) = 0;

		///��ѯ������������
		virtual int ReqQryExecOrderVolume(CKSQryExecOrderVolumeField *pQryExecOrderVolume, int nRequestID) = 0;

		///��ѯ������Ȩָ����Ϣ
		virtual int ReqQryStockOptionAssignment(CKSQryStockOptionAssignmentField *pQryStockOptionAssignment, int nRequestID) = 0;

		///��ѯ�ͻ����׼���
		virtual int ReqQryInvestorTradeLevel(CKSQryInvestorTradeLevelField *pQryInvestorTradeLevel, int nRequestID) = 0;

		///��ѯ����ϵͳ�����޹����
		virtual int ReqQryPurchaseLimitAmtT(CKSQryOptLimitField *pQryPurchaseLimitAmt, int nRequestID) = 0;

		///��ѯ����ϵͳ�����޲ֶ��
		virtual int ReqQryPositionLimitVolT(CKSQryOptLimitField *pQryPositionLimitVol, int nRequestID) = 0;

		///��ѯ����ϵͳ�����޹����
		virtual int ReqQryPurchaseLimitAmtS(CKSQryOptLimitField *pQryPurchaseLimitAmt, int nRequestID) = 0;

		///��ѯ����ϵͳ�����޲ֶ��
		virtual int ReqQryPositionLimitVolS(CKSQryOptLimitField *pQryPositionLimitVol, int nRequestID) = 0;

		///�����޹���ȱ��
		virtual int ReqPurchaseLimitAmtUpdate(CKSPurchaseLimitAmtField *pPurchaseLimitAmt, int nRequestID) = 0;

		///�����޲ֶ�ȱ��
		virtual int ReqPositionLimitVolUpdate(CKSPositionLimitVolField *pPositionLimitVol, int nRequestID) = 0;

		///�����ѯ������ʷ����
		virtual int ReqQryHistoryOrder(CKSQryHistoryOrderField *pQryHistoryOrder, int nRequestID) = 0;

		///�����ѯ������ʷ�ɽ�
		virtual int ReqQryHistoryTrade(CKSQryHistoryTradeField *pQryHistoryTrade, int nRequestID) = 0;

		///�����ѯ������ʷ��Ȩָ����ϸ
		virtual int ReqQryStockOptionHistoryAssignment(CKSQryHistoryAssignmentField *pQryHistoryAssignment, int nRequestID) = 0;

		///�����ѯ������Ȩ������ϸ
		virtual int ReqQryStockOptionDelivDetail(CKSQrySODelivDetailField *pQrySODelivDetail, int nRequestID) = 0;

		///�Զ���Ȩִ�в���
		virtual int ReqAutoExecOrderAction(CKSAutoExecOrderActionField *pAutoExecOrderAction, int nRequestID) = 0;

		///������ϲ��ί������
		virtual int ReqCombActionInsert(CKSInputCombActionField *pInputCombAction, int nRequestID) = 0;

		///���ɳֲ�ת������
		virtual int ReqPositionConvert(CKSInputPositionConvertField *pInputPositionConvert, int nRequestID) = 0;

		///��ѯ������ϳֲ���ϸ
		virtual int ReqQryInvestorCombinePosition(CKSQryInvestorPositionCombineDetailField *pQryInvestorCombinePosition, int nRequestID) = 0;

		///��ѯ���ɿ���Ͽɲ����������
		virtual int ReqQryCombActionVolume(CKSQryCombActionVolumeField *pQryCombActionVolume, int nRequestID) = 0;

		///��֤�����̱��۵��޸�����
		virtual int ReqQuoteUpdate(CKSInputQuoteUpdateField *pInputQuoteUpdate, int nRequestID) = 0;

		///���������Ȩί������
		virtual int ReqCombExecOrderInsert(CKSInputCombExecOrderField *pInputCombExecOrder, int nRequestID) = 0;

		///���������Ȩ��������
		virtual int ReqCombExecOrderAction(CKSInputCombExecOrderActionField *pInputCombExecOrderAction, int nRequestID) = 0;

		///��ѯ���������Ȩ��������������
		virtual int ReqQryCombExecOrderVolume(CKSQryCombExecOrderVolumeField *pQryCombExecOrderVolume, int nRequestID) = 0;

		///��ѯ���ɿͻ���������
		virtual int ReqQryStockOptionAccount(CKSQryStockOptionAccountField *pQryStockOptionAccount, int nRequestID) = 0;

	protected:
		~CKSOptionApi(){};
	};

}	// end of namespace KingstarAPI
#endif