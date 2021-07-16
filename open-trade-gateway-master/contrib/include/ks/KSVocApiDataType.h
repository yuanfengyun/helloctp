/////////////////////////////////////////////////////////////////////////
///@system ��һ������ϵͳ
///@company SunGard China
///@file KSVocApiDataType.h
///@brief �����˿ͻ��˽ӿ�ʹ�õ�ҵ����������
/////////////////////////////////////////////////////////////////////////

#ifndef __KSVOCAPIDATATYPE_H_INCLUDED_
#define __KSVOCAPIDATATYPE_H_INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

namespace KingstarAPI
{
	//���ݱ��ʶ����
	enum KS_TABLEID_TYPE
	{
		KS_DTI_Product = 0,
		KS_DTI_Instrument,
		KS_DTI_DepthMarketData,
		KS_DTI_InstrumentRate,
		KS_DTI_TradingAccount,
		KS_DTI_InvestorPosition
	};

	//��չAPI����
	enum KS_EXTAPI_TYPE
	{
		KS_COS_API = 0,
		KS_OPT_API,
		KS_VOC_API,
		KS_VOC_MDAPI,
		KS_PRD_API
	};

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalTypeType��һ����������
	/////////////////////////////////////////////////////////////////////////
	///���ڵ���������
#define KSCOS_GreaterEqualTermPrice '0'
	///С�ڵ���������
#define KSCOS_LesserThanTermPrice '1'

	typedef char TKSConditionalTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderType��һ������������
	/////////////////////////////////////////////////////////////////////////
	///���鴥��
#define KSCOS_TRIGGERTYPE_QUOTATION '0'
	///���̴���
#define KSCOS_TRIGGERTYPE_OPEN '1'
	///ʱ�䴥��
#define KSCOS_TRIGGERTYPE_TIME '2'
	///�����ʱ�䴥��
#define KSCOS_TRIGGERTYPE_QUOTAIONANDTIME '5' 

	typedef char TKSConditionalOrderType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderStateAlterType��һ����ͣ�򼤻������־����
	/////////////////////////////////////////////////////////////////////////
	///��ͣ
#define KSCOS_State_PAUSE '0'
	///����
#define KSCOS_State_ACTIVE '1'

	typedef char TKSConditionalOrderStateAlterType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderSelectResultType��һ��ѡ��������
	/////////////////////////////////////////////////////////////////////////
	// ����
#define KSCOS_Select_AGAIN '0'
	// ����
#define KSCOS_Select_SKIP '1'
	// ��ֹ
#define KSCOS_Select_ABORT '2'

	typedef char TKSConditionalOrderSelectResultType;

	/////////////////////////////////////////////////////////////////////////
	///TKSOrderPriceTypeType��һ�������۸���������
	/////////////////////////////////////////////////////////////////////////
	// ���¼�
#define KSCOS_OrderPrice_LastPrice '0'
	// ���
#define KSCOS_OrderPrice_BidPrice '1'
	//����
#define KSCOS_OrderPrice_AskPrice '2'

	typedef char TKSOrderPriceTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSScurrencyTypeType��һ��ί�м۸���������
	/////////////////////////////////////////////////////////////////////////
	//���¼�
#define KSCOS_Scurrency_LastPrice '1'
	//������
#define KSCOS_Scurrency_SalePrice '2'
	//ָ����
#define KSCOS_Scurrency_AnyPrice '3'
	//�м�
#define KSCOS_Scurrency_MarketPrice '4'
	//�ǵ�ͣ��
#define KSCOS_Scurrency_CHGPrice '5'

	typedef char TKSScurrencyTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalSourceType��һ����������Դ����
	/////////////////////////////////////////////////////////////////////////
	//��ͨ
#define KSCOS_ConditionalSource_Ordinary '0'
	//ӯ������
#define KSCOS_ConditionalSource_ProfitAndLoss '1'

	typedef char TKSConditionalSourceType;

	/////////////////////////////////////////////////////////////////////////
	///TKSCloseModeType��һ��ƽ�ּ۸�����
	/////////////////////////////////////////////////////////////////////////
	// �м�
#define KSPL_Close_MarketPrice '0'
	// �Ӽ�������
#define KSPL_Close_SalePrice '1'
	//�Ӽ����¼�
#define KSPL_Close_LastPrice '2'

	typedef char TKSCloseModeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSOffsetValueType��һ������ֹ��ֹӯ�۵ķ�ʽ����
	/////////////////////////////////////////////////////////////////////////
	// ָ��ֵ
#define KSPL_OffsetValue_TermPrice '0'
	// ���ֳɽ��۵����ƫ��ֵ
#define KSPL_OffsetValue_TradePrice '1'

	typedef char TKSOffsetValueType;

	/////////////////////////////////////////////////////////////////////////
	///TKSSpringTypeType��һ�������۸���������
	/////////////////////////////////////////////////////////////////////////
	///���¼�
#define KSPL_SPRING_LastPrice '0'
	///������
#define KSPL_SPRING_SalePrice '1'

	typedef char TKSSpringTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderStatusType��һ��������״̬����
	/////////////////////////////////////////////////////////////////////////
	// ��ͣ
#define KSCOS_OrderStatus_PAUSENOTOUCHED      '0'
	// δ����
#define KSCOS_OrderStatus_ACTIVENOTOUCHED    '1'
	//ɾ��
#define KSCOS_OrderStatus_Deleted                     '2'
	//�Ѵ���δ����
#define KSCOS_OrderStatus_TOUCHEDNOSEND   '3'
	// ���ͳ�ʱ
#define KSCOS_OrderStatus_SENDTIMEOUT   '4'
	//���ͳɹ�
#define KSCOS_OrderStatus_SENDSUCCESS   '5'
	// �ȴ�ѡ��
#define KSCOS_OrderStatus_WAITSELECT   '6'
	// ѡ������
#define KSCOS_OrderStatus_SELECTSKIP   '7'
	// ѡ����ֹ
#define KSCOS_OrderStatus_SELECTABORT   '8'

	typedef char TKSConditionalOrderStatusType;

	/////////////////////////////////////////////////////////////////////////
	///TKSProfitAndLossOrderStatusType��һ��ֹ��ֹӯ��״̬����
	/////////////////////////////////////////////////////////////////////////
	// δ����������
#define KSZSZY_OrderStatus_NOTGENERATED	'0'
	// ����ֹ��ί�е�
#define KSZSZY_OrderStatus_GENERATEORDERZS '1'
	// ����������
#define KSZSZY_OrderStatus_ALREADYGENERATED '2'
	// ����ί��
#define KSZSZY_OrderStatus_ERRORDER	'3'
	// ����ֹӯί�е�
#define KSZSZY_OrderStatus_GENERATEORDERZY	'4'
	// ɾ��
#define KSZSZY_OrderStatus_DELETED	'd'

	typedef char TKSProfitAndLossOrderStatusTyp;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderIDType��һ���������������
	/////////////////////////////////////////////////////////////////////////
	typedef int TKSConditionalOrderIDType;

	/////////////////////////////////////////////////////////////////////////
	///TKSProfitAndLossOrderIDType��һ��ֹ��ֹӯ���������
	/////////////////////////////////////////////////////////////////////////
	typedef int TKSProfitAndLossOrderIDType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConditionalOrderSelectTypeType��һ��������ѡ��ʽ����
	/////////////////////////////////////////////////////////////////////////
	// ȷ�ϡ�ȡ�� 
#define KSCOS_Select_ConfirmORCancel '1'
	// ���ԡ���������ֹ 
#define KS_Select_AgainOrSkipOrAbort '2'

	typedef char TKSConditionalOrderSelectTypeType;


	/////////////////////////////////////////////////////////////////////////
	const int  MAX_ORDER_COUNT = 20;

	/////////////////////////////////////////////////////////////////////////
	///TKSCloseStrategyType��һ��ƽ�ֲ�������
	/////////////////////////////////////////////////////////////////////////
	///�ȿ���ƽ������ͨ�����
#define KSVOC_OpenFCloseF_OrdiComb '1'
	///����ͨ����ϣ��ȿ���ƽ
#define KSVOC_OrdiComb_OpenFCloseF '2'
	///�������ڵ���(�����ڵĳֲ���ƽ������ƽ���)
#define KSVOC_TodayF_TIME '3'

	typedef char TKSCloseStrategyType;

	/////////////////////////////////////////////////////////////////////////
	///TKSStrategyIDType��һ�����Դ�������
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSStrategyIDType[12];

	/////////////////////////////////////////////////////////////////////////
	///TKSCombTypeType��һ�������������
	/////////////////////////////////////////////////////////////////////////
	///����
#define KSVOC_CombType_Arbitrage '0'
	///����
#define KSVOC_CombType_Swap '1'

	typedef char TKSCombTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSStrikePriceType��һ��ִ�м�����
	/////////////////////////////////////////////////////////////////////////
	// ִ�м۹����ȵͶ��ȸ�
#define KSVOC_StrikePrice_Low 'L'
	// ִ�м۹����ȸ߶��ȵ�
#define KSVOC_StrikePrice_High 'H'
	// ִ�м۹���������ͬ
#define KSVOC_StrikePrice_Minus 'E'
	// ��У��۸��ϵ
#define KSVOC_StrikePrice_Plus 'N'

	typedef char TKSStrikePriceType;

	/////////////////////////////////////////////////////////////////////////
	///TKSCalcFlagType��һ�������������
	/////////////////////////////////////////////////////////////////////////
	// ��
#define KSVOC_CalcFlag_Plus '1'
	// ��
#define KSVOC_CalcFlag_Minus '2'

	typedef char TKSCalcFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TThostFtdcVolRatioType��һ����������
	/////////////////////////////////////////////////////////////////////////
	typedef double TThostFtdcVolRatioType;

	/////////////////////////////////////////////////////////////////////////
	///TThostFtdcMoneyRatioType��һ���ʽ�����
	/////////////////////////////////////////////////////////////////////////
	typedef double TThostFtdcMoneyRatioType;

	/////////////////////////////////////////////////////////////////////////
	///TKSInfoTypeType��һ����Ϣ��������
	/////////////////////////////////////////////////////////////////////////
	// ��ͨ
#define KSVOC_InfoType_Common '1'
	// ����
#define KSVOC_InfoType_Warn '2'
	// Σ��
#define KSVOC_InfoType_Risk '3'
	// ����
#define KSVOC_InfoType_Roll '4'
	// ǿ��ȷ����
#define KSVOC_InfoType_Force '5'

	typedef char TKSInfoTypeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSConfirmFlagType��һ��ȷ�ϱ�־����
	/////////////////////////////////////////////////////////////////////////
	// δȷ��
#define KSVOC_ConfirmFlag_UnConfirm '0'
	// ��ȷ��
#define KSVOC_ConfirmFlag_Confirmed '1'

	typedef char TKSConfirmFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TKSProductVersionType��һ���汾������
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSProductVersionType[21];

	/////////////////////////////////////////////////////////////////////////
	///TKSLimitFlagType��һ���޲���������
	/////////////////////////////////////////////////////////////////////////
	// Ȩ����
#define KSVOC_PF_F  'F'
	// �ֲܳ�
#define KSVOC_PF_T  'T'
	// ����������
#define KSVOC_PF_D 'D'
	// ���տ����޶�
#define KSVOC_PF_O 'O'

	typedef char TKSLimitFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TKSControlRangeType��һ�����Ʒ�Χ����
	/////////////////////////////////////////////////////////////////////////
	// Ʒ��
#define KSVOC_CR_Product '0'
	// ����
#define KSVOC_CR_ALL  '1'

	typedef char TKSControlRangeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSTradeLevelType��һ�����׼�������
	/////////////////////////////////////////////////////////////////////////
	// һ��
#define KSVOC_TL_Level1 '1'
	// ����
#define KSVOC_TL_Level2  '2'
	// ����
#define KSVOC_TL_Level3  '3'

	typedef char TKSTradeLevelType;

	/////////////////////////////////////////////////////////////////////////
	///TKSSOPosiDirectionType��һ�����ɳֲַ�������
	/////////////////////////////////////////////////////////////////////////
	// Ȩ����
#define KSVOC_SOPD_Buy '1'
	// �����
#define KSVOC_SOPD_Sell  '2'

	typedef char TKSSOPosiDirectionType;

	/////////////////////////////////////////////////////////////////////////
	///TKSSODelivModeType��һ�����ɽ��ղ�ѯ����
	/////////////////////////////////////////////////////////////////////////
	// ��Ȩ��ľ����
#define KSVOC_SODM_Product '1'
	// ��Ȩ�ֽ���㽻����ϸ
#define KSVOC_SODM_Cash  '2'
	// ��ȨΥԼ���ÿ�ȯ������
#define KSVOC_SODM_Dispos  '3'

	typedef char TKSSODelivModeType;

	/////////////////////////////////////////////////////////////////////////
	///TKSCombActionType��һ��ǿ��������
	/////////////////////////////////////////////////////////////////////////
	///��ǿ��
#define KSVOC_CAT_False '0'
	///ǿ��
#define KSVOC_CAT_True '1'

	typedef char TKSCombActionType;

	/////////////////////////////////////////////////////////////////////////
	///TKSCombStrategyIDType��һ��������ϲ��Դ�������
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSCombStrategyIDType[12];

	/////////////////////////////////////////////////////////////////////////
	///TKSProfitAndLossFlagType��һ��ֹ��ֹӯ��־����
	/////////////////////////////////////////////////////////////////////////
	// ��ֹ���ֹӯ
#define KSCOS_PLF_NotProfitNotLoss '0'
	// ֹ�� 
#define KSCOS_PLF_Loss '1'
	// ֹӯ 
#define KSCOS_PLF_Profit '2'
	// ֹ��ֹӯ
#define KSCOS_PLF_ProfitAndLoss '3'

	typedef char TKSProfitAndLossFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TKSFOCreditApplyType��һ�����ճ����Ȳ�������
	/////////////////////////////////////////////////////////////////////////
	///����
#define KSVOC_FOCAT_Confirm '0'
	///ȡ������
#define KSVOC_FOCAT_NoConfirm '1'

	typedef char TKSFOCreditApplyType;

	/////////////////////////////////////////////////////////////////////////
	///TKSFOCreditStatusType��һ�����ճ����ȴ���״̬����
	/////////////////////////////////////////////////////////////////////////
	///�����
#define KSVOC_FOCST_Sending '0'
	///��ͨ��
#define KSVOC_FOCST_Accepted '1'
	///�ѷ��
#define KSVOC_FOCST_Reject '2'

	typedef char TKSFOCreditStatusType;


	/////////////////////////////////////////////////////////////////////////
	///TKSSOBusinessFlagType��һ��ҵ���־����
	/////////////////////////////////////////////////////////////////////////
	///���뿪��
#define KSVOC_SOOF_BuyOpen "0d"
	///����ƽ��
#define KSVOC_SOOF_SellClose "0e"
	///��������
#define KSVOC_SOOF_SellOpen "0f"
	///����ƽ��
#define KSVOC_SOOF_BuyClose "0g"
	///���ҿ���
#define KSVOC_SOOF_CoveredOpen "0h"
	///����ƽ��
#define KSVOC_SOOF_CoveredClose "0i"
	///��Ȩ��Ȩ
#define KSVOC_SOOF_ExecOrder "0j"
	///�Ϲ���Ȩ��Ȩָ��Ȩ����
#define KSVOC_SOOF_CallAssignedRight "2a"
	///�Ϲ���Ȩ��Ȩָ��Ȩ����
#define KSVOC_SOOF_PutAssignedRight "2b"
	///�Ϲ���Ȩ��Ȩָ�����񷽣��Ǳ��ң�
#define KSVOC_SOOF_CallAssignedDuty "2c"
	///�Ϲ���Ȩ��Ȩָ������
#define KSVOC_SOOF_PutAssignedDuty "2d"
	///�Ϲ���Ȩ��Ȩָ�����񷽣����ң�
#define KSVOC_SOOF_CallAssignedDuty_Covered "2e"
	///�Ϲ���Ȩ��Ȩ����Ȩ����
#define KSVOC_SOOF_CallSettledRight "2f"
	///�Ϲ���Ȩ��Ȩ����Ȩ����
#define KSVOC_SOOF_PutSettledRight "2g"
	///�Ϲ���Ȩ�н������� (�Ǳ���)
#define KSVOC_SOOF_CallSettledDuty "2h"
	///�Ϲ���Ȩ�н�������
#define KSVOC_SOOF_PutSettledDuty "2i"
	///�Ϲ���Ȩ�н������� (����)
#define KSVOC_SOOF_CallSettledDuty_Covered "2j"
	///��Ȩ���ս�ȯΥԼ(���ΥԼһ��)
#define KSVOC_SOOF_ExecSettledDefault "2k"
	///��Ȩ���ս�ȯΥԼ(��Ա�ΥԼһ��)
#define KSVOC_SOOF_ExecSettledDefaulted "2l"
	///Ȩ������Լ�ֱֲ䶯����
#define KSVOC_SOOF_RightPosition "2m"
	///���񷽺�Լ�ֱֲ䶯����(�Ǳ���)
#define KSVOC_SOOF_DutyPosition "2n"
	///���񷽺�Լ�ֱֲ䶯����(����)
#define KSVOC_SOOF_DutyPosition_Covered "2o"
	///ת���û���
#define KSVOC_SOOF_DisposalOut "2p"
	///ת���û���
#define KSVOC_SOOF_DisposalBack "2q"
	///��Ȩ����Ӧ��֤ȯ
#define KSVOC_SOOF_ExecSettledRecv "2r"
	///��Ȩ����Ӧ��֤ȯ
#define KSVOC_SOOF_ExecSettledDeal "2s"
	///��Ȩ��ȨǷ���ȯ���գ����ȯ�̴��˻���
#define KSVOC_SOOF_BrokerAccountDeal "2t"
	///������ȯҵ�����
#define KSVOC_SOOF_CoveredLock "2u"

	typedef char TKSSOBusinessFlagType[3];

	/////////////////////////////////////////////////////////////////////////
	///TKSExecFrozenMarginParamsType��һ��ʵֵ���Ƿ����ִ�ж��ᱣ֤���������
	/////////////////////////////////////////////////////////////////////////
	///ʵֵ�����ִ�ж��ᱣ֤��
#define KSVOC_EFMP_ITM_NotInclude '0'
	///ʵֵ�����ִ�ж��ᱣ֤��
#define KSVOC_EFMP_ITM_Include '1'

	typedef char TKSExecFrozenMarginParamsType;

	/////////////////////////////////////////////////////////////////////////
	///TKSCancelAutoExecParamsType��һ��ȡ���������Զ���Ȩ����ʽ��������
	/////////////////////////////////////////////////////////////////////////
	///����Ϊ0����Ȩָ��
#define KSVOC_CAEP_0 '0'
	///ʵֵ�����ִ�ж��ᱣ֤��
#define KSVOC_CAEP_1 '1'

	typedef char TKSCancelAutoExecParamsType;

	/////////////////////////////////////////////////////////////////////////
	///TKSRiskLevelType��һ�����ռ�������
	/////////////////////////////////////////////////////////////////////////
	// ����
#define KSVOC_RL_Level0 '0'
	// ׷��
#define KSVOC_RL_Level1  '1'
	// ǿƽ
#define KSVOC_RL_Level2  '2'
	// ����
#define KSVOC_RL_Level3  '3'
	// ��Ȩ
#define KSVOC_RL_Level4  '4'

	typedef char TKSRiskLevelType;

	/////////////////////////////////////////////////////////////////////////
	///TKSStatusMsgType��һ������״̬��Ϣ����
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSStatusMsgType[256];

	/////////////////////////////////////////////////////////////////////////
	///TKSResultMsgType��һ����������Ϣ����
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSResultMsgType[256];

	/////////////////////////////////////////////////////////////////////////
	///TKSOptSelfCloseFlagType��һ����Ȩ��Ȩ�Ƿ��ԶԳ�����
	/////////////////////////////////////////////////////////////////////////
	///�ԶԳ���Ȩ��λ
#define KSVOC_OSCF_CloseSelfOptionPosition '1'
	///������Ȩ��λ
#define KSVOC_OSCF_ReserveOptionPosition '2'
	///�ԶԳ�������Լ����ڻ���λ
#define KSVOC_OSCF_SellCloseSelfFuturePosition '3'
	///ȡ���������Զ���Ȩ
#define KSVOC_OSCF_RemoveAutoRightPosition '4'

	typedef char TKSOptSelfCloseFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TThostFtdcHedgingFlagType��һ���Գ��־����
	/////////////////////////////////////////////////////////////////////////
	///���Գ�
#define KSVOC_HF_UnHedging '0'
	///�Գ�
#define KSVOC_HF_Hedging '1'

	typedef char TThostFtdcHedgingFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TKSTurnResOrTurnCashFlagType��һ��ת����ת�ֽ��־����
	/////////////////////////////////////////////////////////////////////////
	///ת����
#define KSVOC_ROC_TurnRes 'h'
	///ת�ֽ�
#define KSVOC_ROC_TurnCash 'i'

	typedef char TKSTurnResOrTurnCashFlagType;

	/////////////////////////////////////////////////////////////////////////
	///TKSBusinessLocalIDType��һ������ҵ���ʶ����
	/////////////////////////////////////////////////////////////////////////
	//typedef int TKSBusinessLocalIDType;

	/////////////////////////////////////////////////////////////////////////
	///TKSOptionSelfCloseSysIDType��һ����Ȩ�ԶԳ�ϵͳ�������
	/////////////////////////////////////////////////////////////////////////
	typedef char TKSOptionSelfCloseSysIDType[13];

	/////////////////////////////////////////////////////////////////////////
	///TKSTradingParamsTypeType��һ�����ײ����������
	/////////////////////////////////////////////////////////////////////////
	///ʵֵ���Ƿ����ִ�ж��ᱣ֤�����
#define KSVOC_TPT_ExecFrozenMargin '1'
	///�Գ�
#define KSVOC_TPT_CancelAutoExec '2'

	typedef char TKSTradingParamsTypeType;

	////////////////////////////////////////////////////////////////////////
	///TKSSettlementTypeType��һ����½ʱ���̨���ײ�������
	////////////////////////////////////////////////////////////////////////
	//��ȡ���˵�
#define KSVOC_ST_NOAchieveAccount '0'
	//ȡ���˵�������ȷ��
#define KSVOC_ST_AchieveAndNoConfirm '1'
	//ȡ���˵�������ȷ��
#define KSVOC_ST_AchieveAndConfirm '2'

	typedef char TKSSettlementTypeType;

	////////////////////////////////////////////////////////////////////////
	///TKSSourceType��һ����Դ����
	////////////////////////////////////////////////////////////////////////
	//��̨����
#define KSVOC_SOURCE_CounterCommon '0'
	//������
#define KSVOC_SOURCE_Exchange '1'
	//��̨֪ͨ
#define KSVOC_SOURCE_CounterRTN '2'
	//����֪ͨ
#define KSVOC_SOURCE_SPXGateway '3'

	typedef char TKSSourceType;
}	// end of namespace KingstarAPI
#endif