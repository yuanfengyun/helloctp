/////////////////////////////////////////////////////////////////////////
///@system ��һ������ϵͳ
///@company SunGard China
///@file KSPrdApi.h
///@brief �����˿ͻ���������ҵ��ӿ�
/////////////////////////////////////////////////////////////////////////

#ifndef __KSPRDAPI_H_INCLUDED__
#define __KSPRDAPI_H_INCLUDED__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "KSUserApiStructEx.h"
#include "KSVocApiStruct.h"

#if defined(ISLIB) && defined(WIN32) && !defined(KSTRADEAPI_STATIC_LIB)

#ifdef LIB_TRADER_API_EXPORT
#define TRADER_PRDAPI_EXPORT __declspec(dllexport)
#else
#define TRADER_PRDAPI_EXPORT __declspec(dllimport)
#endif
#else
#ifdef WIN32
#define TRADER_PRDAPI_EXPORT 
#else
#define TRADER_PRDAPI_EXPORT __attribute__((visibility("default")))
#endif

#endif

namespace KingstarAPI
{
	class CKSPrdSpi
	{
	public:
		///������ҵ����֪ͨ
		virtual void OnRtnForSubscribed(CKSPrimeDataBusinessField *pPrimeDataBusiness) {};
		///�����ݹ鵵��
		virtual void OnRtnArchiving() {};
		///������ϸ�ڹ鵵
		virtual void OnRtnDetailArchived(KS_TABLEID_TYPE nTableID) {};
		///�����ݹ鵵���
		virtual void OnRtnArchived(TThostFtdcDescriptionType availabeDescription,TThostFtdcDateType availabeTradingDay) {};// ���� ���ӿ������� 2016��2��1��
	};

	class CKSPrdApi
	{
	public:
		///��������ҵ��
		virtual int ReqSubPrimeData(CKSSubPrimeDataBusinessField *pSubPrimeDataBusiness) = 0;
		///�������ҵ��
		virtual void ReqFillPrimeData(void) = 0;
		///�������ݱ�
		virtual void DestroyPrimeDataDetail(void) = 0;
		// ResetDBEnviroment
		virtual void ResetDBEnviroment() = 0;

	protected:
		~CKSPrdApi(){};
	};

}	// end of namespace KingstarAPI
#endif