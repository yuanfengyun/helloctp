#include "condition_order_manager.h"
#include "condition_order_serializer.h"
#include "config.h"
#include "utility.h"
#include "ins_list.h"
#include "numset.h"
#include "datetime.h"

#include <chrono>
#include <cmath>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>

using namespace std;

ConditionOrderManager::ConditionOrderManager(const std::string& userKey
        , ConditionOrderData& condition_order_data
        , ConditionOrderHisData& condition_order_his_data
        , IConditionOrderCallBack& callBack)
        :m_userKey(userKey)
        ,m_user_file_path("")
        ,m_condition_order_data(condition_order_data)
        ,m_current_day_condition_order_count(0)
        ,m_current_valid_condition_order_count(0)
        ,m_condition_order_his_data(condition_order_his_data)
        ,m_callBack(callBack)
        ,m_run_server(true)
        ,m_openmarket_condition_order_map()
        , m_time_condition_order_set()
        ,m_price_condition_order_map()
        ,m_localTime(0)
        ,m_SHFETime(0)
        ,m_DCETime(0)
        ,m_INETime(0)
        ,m_FFEXTime(0)
        ,m_CZCETime(0)
        ,_instrumentTradeStatusInfoMap()
{        
        m_run_server = g_condition_order_config.run_server;
}

ConditionOrderManager::~ConditionOrderManager()
{
}

void ConditionOrderManager::NotifyPasswordUpdate(const std::string& strOldPassword,
        const std::string& strNewPassword)
{
        if ((strOldPassword == m_condition_order_data.user_password) 
                && (strNewPassword!= m_condition_order_data.user_password))
        {
                m_condition_order_data.user_password = strNewPassword;
                SaveCurrent();
        }

        if ((strOldPassword == m_condition_order_his_data.user_password)
                && (strNewPassword != m_condition_order_his_data.user_password))
        {
                m_condition_order_his_data.user_password = strNewPassword;
                SaveHistory();
        }
}

void ConditionOrderManager::Load(const std::string& bid
        , const std::string& user_id
        , const std::string& user_password
        , const std::string& trading_day)
{
		m_condition_order_data.condition_orders.clear();
		m_condition_order_his_data.his_condition_orders.clear();

        if (!g_config.user_file_path.empty())
        {
             m_user_file_path = g_config.user_file_path + "/" + bid;
        }
        else
        {
             m_user_file_path = "/var/local/lib/open-trade-gateway/" + bid;
        }
                
        //?????????????????????
        std::vector<ConditionOrder> tmp_his_condition_orders;
        std::string fn = m_user_file_path + "/" + m_userKey +".co";
		//??????????????????????????????
		if (!boost::filesystem::exists(fn))
		{
			//????????????????????????
			m_condition_order_data.broker_id = bid;
			m_condition_order_data.user_id = user_id;
			m_condition_order_data.user_password = user_password;
			m_condition_order_data.trading_day = trading_day;
			m_condition_order_data.condition_orders.clear();
			m_current_valid_condition_order_count = 0;
			m_current_day_condition_order_count = 0;

			//??????????????????????????????
			m_condition_order_his_data.broker_id = bid;
			m_condition_order_his_data.user_id = user_id;
			m_condition_order_his_data.user_password = user_password;
			m_condition_order_his_data.trading_day = trading_day;
			m_condition_order_his_data.his_condition_orders.clear();

			return;
		}

        SerializerConditionOrderData nss;
        bool loadfile = nss.FromFile(fn.c_str());
		//???????????????????????????
        if (!loadfile)
        {
                Log().WithField("fun","Load")
                        .WithField("key",m_userKey)
                        .WithField("bid",bid)
                        .WithField("user_name",user_id)
                        .WithField("fileName",fn)
                        .Log(LOG_INFO, "ConditionOrderManager load condition order data file failed!");                        
                
                m_condition_order_data.broker_id = bid;
                m_condition_order_data.user_id = user_id;
                m_condition_order_data.user_password = user_password;
                m_condition_order_data.trading_day = trading_day;
				m_condition_order_data.condition_orders.clear();
                m_current_valid_condition_order_count = 0;
                m_current_day_condition_order_count = 0;
        }
        else
        {
                Log().WithField("fun","Load")
                        .WithField("key",m_userKey)
                        .WithField("bid",bid)
                        .WithField("user_name",user_id)
                        .WithField("fileName",fn)
                        .Log(LOG_INFO, "ConditionOrderManager load condition order data file success!");

				m_condition_order_data.condition_orders.clear();				
                nss.ToVar(m_condition_order_data);
				
                m_condition_order_data.broker_id = bid;
                m_condition_order_data.user_id = user_id;                
                m_condition_order_data.user_password = user_password;
                
                //??????????????????????????????,????????????????????????????????????
                if (m_condition_order_data.trading_day != trading_day)
                {
                        for (auto it = m_condition_order_data.condition_orders.begin();
                                it != m_condition_order_data.condition_orders.end();)
                        {
                                ConditionOrder& order = it->second;

                                //?????????????????????????????????
                                for (auto& cond : order.condition_list)
                                {
                                        std::string strSymbol = cond.exchange_id + "." + cond.instrument_id;
                                        Instrument* inst=GetInstrument(strSymbol);
                                        if ((nullptr == inst) || (inst->expired))
                                        {
                                                order.status = EConditionOrderStatus::discard;
                                                break;
                                        }
                                }

                                //???????????????????????????
                                for (auto& o : order.order_list)
                                {
                                        std::string strSymbol = o.exchange_id + "." + o.instrument_id;
                                        Instrument* inst = GetInstrument(strSymbol);
                                        if ((nullptr == inst) || (inst->expired))
                                        {
                                                order.status = EConditionOrderStatus::discard;
                                                break;
                                        }
                                }
                                                                
                                //??????????????????????????????????????????
                                if ((order.status == EConditionOrderStatus::cancel)
                                        || (order.status == EConditionOrderStatus::touched)
                                        || (order.status == EConditionOrderStatus::discard)
                                        )
                                {
                                        //????????????????????????????????????
                                        if (order.trading_day != atoi(trading_day.c_str()))
                                        {                                                
                                                //???????????????????????????
                                                tmp_his_condition_orders.push_back(order);
                                                //?????????????????????????????????
                                                m_condition_order_data.condition_orders.erase(it++);
                                                continue;
                                        }                                        
                                }

                                //?????????????????????????????????
                                if (order.time_condition_type == ETimeConditionType::GFD)
                                {
                                        //????????????????????????????????????
                                        if (order.trading_day != atoi(trading_day.c_str()))
                                        {
                                                order.status = EConditionOrderStatus::discard;
                                                //???????????????????????????
                                                tmp_his_condition_orders.push_back(order);
                                                //?????????????????????????????????
                                                m_condition_order_data.condition_orders.erase(it++);
                                                continue;
                                        }                                        
                                }

                                //?????????????????????????????????
                                if (order.time_condition_type == ETimeConditionType::GTD)
                                {
                                        //?????????????????????????????????
                                        if (order.GTD_date < atoi(trading_day.c_str()))
                                        {
                                                order.status = EConditionOrderStatus::discard;
                                                //???????????????????????????
                                                tmp_his_condition_orders.push_back(order);
                                                //?????????????????????????????????
                                                m_condition_order_data.condition_orders.erase(it++);
                                                continue;
                                        }
                                }        

                                it++;
                        }

                        m_condition_order_data.trading_day = trading_day;
                }
                
                //????????????????????????????????????????????????????????????
                m_current_valid_condition_order_count = 0;
                m_current_day_condition_order_count = 0;
                for (auto it = m_condition_order_data.condition_orders.begin();
                        it != m_condition_order_data.condition_orders.end();)
                {
                        ConditionOrder& order = it->second;

                        order.changed = true;

                        if (order.trading_day == atoi(trading_day.c_str()))
                        {
                                m_current_day_condition_order_count++;
                        }

                        m_current_valid_condition_order_count++;

                        it++;
                }

        }
        
        //???????????????????????????
        fn = m_user_file_path + "/" + m_userKey + ".coh";
		
        SerializerConditionOrderData nss_his;
        loadfile = nss_his.FromFile(fn.c_str());
        if (!loadfile)
        {
                Log().WithField("fun","Load")
                        .WithField("key",m_userKey)
                        .WithField("bid",bid)
                        .WithField("user_name",user_id)
                        .WithField("fileName",fn)
                        .Log(LOG_INFO, "ConditionOrderManager load history condition order file failed!");
                
                m_condition_order_his_data.broker_id = bid;
                m_condition_order_his_data.user_id = user_id;
                m_condition_order_his_data.user_password = user_password;
                m_condition_order_his_data.trading_day = trading_day;
                //?????????????????????
                if (!tmp_his_condition_orders.empty())
                {
                        m_condition_order_his_data.his_condition_orders.assign(
                                tmp_his_condition_orders.begin()
                                , tmp_his_condition_orders.end());
                } 				
        }
        else
        {
                Log().WithField("fun","Load")
                        .WithField("key",m_userKey)
                        .WithField("bid",bid)
                        .WithField("user_name",user_id)
                        .WithField("fileName",fn)
                        .Log(LOG_INFO, "ConditionOrderManager load history condition order file success!");
                
                nss_his.ToVar(m_condition_order_his_data);
                m_condition_order_his_data.broker_id = bid;
                m_condition_order_his_data.user_id = user_id;
                m_condition_order_his_data.user_password = user_password;
                if (m_condition_order_his_data.trading_day != trading_day)
                {
                        for (auto it = m_condition_order_his_data.his_condition_orders.begin();
                                it != m_condition_order_his_data.his_condition_orders.end();)
                        {
                                ConditionOrder& order = *it;
                                int currentTime = GetTouchedTime(order);
                                if (currentTime > order.insert_date_time)
                                {
                                        //?????????,??????:??? 
                                        int timeSpan = currentTime - order.insert_date_time;
                                        //???????????????????????????30???(?????????)
                                        int thirty_days = 30 * 24 * 60 * 60;
                                        if (timeSpan > thirty_days)
                                        {
                                                it=m_condition_order_his_data.his_condition_orders.erase(it);
                                                continue;
                                        }
                                }                                
                                it++;                                
                        }

                        //?????????????????????
                        if (!tmp_his_condition_orders.empty())
                        {
                                m_condition_order_his_data.his_condition_orders.assign(
                                        tmp_his_condition_orders.begin()
                                        , tmp_his_condition_orders.end());
                        }

                        m_condition_order_his_data.trading_day = trading_day;
                }
        }

        //?????????????????????????????????,??????????????????????????????????????????
        if (!m_run_server)
        {
                for (auto it = m_condition_order_data.condition_orders.begin();
                        it != m_condition_order_data.condition_orders.end(); it++)
                {
                        ConditionOrder& order = it->second;

                        if (order.status == EConditionOrderStatus::live)
                        {
                                order.status = EConditionOrderStatus::suspend;
                                order.touched_time = GetTouchedTime(order);
                                order.changed = true;
                        }
                }
        }

		SaveCurrent();

        SaveHistory();
        
        BuildConditionOrderIndex();

        m_callBack.OnUserDataChange();
}

void ConditionOrderManager::BuildConditionOrderIndex()
{
        m_openmarket_condition_order_map.clear();
        m_time_condition_order_set.clear();
        m_price_condition_order_map.clear();

        for (auto it : m_condition_order_data.condition_orders)
        {
                const std::string& order_id = it.first;
                ConditionOrder& co = it.second;
                if (co.status != EConditionOrderStatus::live)
                {
                        continue;
                }

                for (auto& c : co.condition_list)
                {
                        if (c.is_touched)
                        {
                                continue;
                        }

                        if (c.contingent_type == EContingentType::market_open)
                        {
                                std::string strInstId = c.instrument_id;
                                CutDigital_Ex(strInstId);
                                std::string strSymbol = c.exchange_id + "." + strInstId;
                                std::vector<std::string>& orderIdList = m_openmarket_condition_order_map[strSymbol];
                                orderIdList.push_back(order_id);
                        }
                        else if (c.contingent_type == EContingentType::time)
                        {
                                if (m_time_condition_order_set.find(order_id) == m_time_condition_order_set.end())
                                {
                                        m_time_condition_order_set.insert(order_id);
                                }                                
                        }
                        else
                        {
                                std::string strSymbol = c.exchange_id + "." + c.instrument_id;
                                std::vector<std::string>& orderIdList = m_price_condition_order_map[strSymbol];
                                orderIdList.push_back(order_id);
                        }
                }

        }        
}

void ConditionOrderManager::SaveCurrent()
{
        //?????????????????????
        std::string fn = m_user_file_path + "/" + m_userKey + ".co";

        Log().WithField("fun","SaveCurrent")
                .WithField("key",m_userKey)
                .WithField("bid",m_condition_order_data.broker_id)
                .WithField("user_name", m_condition_order_data.user_id)
                .WithField("fileName", fn)
                .Log(LOG_INFO, "try to save current condition order file!");
        
        SerializerConditionOrderData nss;
        nss.dump_all = true;
        nss.FromVar(m_condition_order_data);
        bool saveFile = nss.ToFile(fn.c_str());
        if (!saveFile)
        {
                Log().WithField("fun","SaveCurrent")
                        .WithField("key",m_userKey)
                        .WithField("bid",m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithField("fileName",fn)
                        .Log(LOG_INFO, "save condition order data file failed!");        
        }
}

void ConditionOrderManager::SaveHistory()
{
        //???????????????????????????
        std::string fn = m_user_file_path + "/" + m_userKey + ".coh";
        SerializerConditionOrderData nss_his;
        nss_his.dump_all = true;
        nss_his.FromVar(m_condition_order_his_data);
        bool saveFile = nss_his.ToFile(fn.c_str());
        if (!saveFile)
        {
                Log().WithField("fun","SaveHistory")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithField("fileName", fn)
                        .Log(LOG_INFO, "save history condition order data file failed!");                
        }
}

bool ConditionOrderManager::ValidConditionOrder(ConditionOrder& order)
{
        //????????????        
        bool logic_is_or = (1== order.condition_list.size())
                || (ELogicOperator::logic_or ==order.conditions_logic_oper );
        
        for (auto& cond : order.condition_list)
        {
                std::string symbol = cond.exchange_id + "." + cond.instrument_id;
                Instrument* ins = GetInstrument(symbol);
                if (nullptr == ins)
                {
                        Log().WithField("fun", "ValidConditionOrder")
                                .WithField("key", m_userKey)
                                .WithField("bid", m_condition_order_data.broker_id)
                                .WithField("user_name", m_condition_order_data.user_id)        
                                .WithField("order_id", order.order_id)
                                .Log(LOG_INFO, u8"??????????????????????????????,?????????????????????????????????ID?????????:" + symbol);

                        m_callBack.OutputNotifyAll(501
                                , u8"??????????????????????????????,?????????????????????????????????ID?????????:"+ symbol
                                , "WARNING","MESSAGE");
                        return false;
                }

                if (cond.contingent_type == EContingentType::time)
                {                        
                        if (cond.contingent_time <= GetExchangeTime(cond.exchange_id))
                        {
                                if (logic_is_or)
                                {
                                        Log().WithField("fun", "ValidConditionOrder")
                                                .WithField("key", m_userKey)
                                                .WithField("bid", m_condition_order_data.broker_id)
                                                .WithField("user_name", m_condition_order_data.user_id)
                                                .WithField("order_id", order.order_id)
                                                .Log(LOG_INFO, u8"??????????????????????????????,?????????????????????????????????????????????????????????");

                                        m_callBack.OutputNotifyAll(502
                                                , u8"??????????????????????????????,?????????????????????????????????????????????????????????"
                                                , "WARNING", "MESSAGE");
                                        return false;
                                }                        
                                else
                                {
                                        cond.is_touched = true;
                                }
                        }
                }
                else if (cond.contingent_type == EContingentType::price)
                {
                        if (!IsValid(cond.contingent_price))
                        {
                                Log().WithField("fun", "ValidConditionOrder")
                                        .WithField("key", m_userKey)
                                        .WithField("bid", m_condition_order_data.broker_id)
                                        .WithField("user_name", m_condition_order_data.user_id)
                                        .WithField("order_id", order.order_id)
                                        .Log(LOG_INFO, u8"??????????????????????????????,????????????????????????????????????????????????");

                                m_callBack.OutputNotifyAll(503
                                        , u8"??????????????????????????????,????????????????????????????????????????????????"
                                        , "WARNING", "MESSAGE");
                                return false;
                        }

                        bool flag = false;
                        double last_price = ins->last_price;
                        if ((kProductClassCombination == ins->product_class)
                                && (order.order_list.size() > 0))
                        {
                                if (EOrderDirection::buy == order.order_list[0].direction)
                                {
                                        last_price = ins->ask_price1;
                                }
                                else
                                {
                                        last_price = ins->bid_price1;
                                }
                        }

                        switch (cond.price_relation)
                        {
                        case EPriceRelationType::G:
                                if (isgreater(last_price, cond.contingent_price))
                                {
                                        if (InstrumentLastTradeStatusIsContinousTrading(cond.instrument_id))
                                        {
                                                flag = true;
                                        }                                        
                                }
                                break;
                        case EPriceRelationType::GE:
                                if (isgreaterequal(last_price,cond.contingent_price))
                                {
                                        if (InstrumentLastTradeStatusIsContinousTrading(cond.instrument_id))
                                        {
                                                flag = true;
                                        }                                        
                                }
                                break;
                        case EPriceRelationType::L:
                                if (isless(last_price,cond.contingent_price))
                                {
                                        if (InstrumentLastTradeStatusIsContinousTrading(cond.instrument_id))
                                        {
                                                flag = true;
                                        }
                                }
                                break;
                        case EPriceRelationType::LE:
                                if (islessequal(last_price,cond.contingent_price))
                                {
                                        if (InstrumentLastTradeStatusIsContinousTrading(cond.instrument_id))
                                        {
                                                flag = true;
                                        }
                                }
                                break;
                        default:
                                break;
                        }
                        
                        if (flag)
                        {
                                if (logic_is_or)
                                {
                                        Log().WithField("fun", "ValidConditionOrder")
                                                .WithField("key", m_userKey)
                                                .WithField("bid", m_condition_order_data.broker_id)
                                                .WithField("user_name", m_condition_order_data.user_id)
                                                .WithField("order_id", order.order_id)
                                                .Log(LOG_INFO, u8"??????????????????????????????,?????????????????????????????????,???????????????");

                                        m_callBack.OutputNotifyAll(504
                                                , u8"??????????????????????????????,?????????????????????????????????,???????????????"
                                                , "WARNING", "MESSAGE");
                                        return false;
                                }
                                else
                                {
                                        cond.is_touched = true;
                                }
                        }

                }
                else if (cond.contingent_type == EContingentType::price_range)
                {
                        if (!IsValid(cond.contingent_price_left)                                
                                ||!IsValid(cond.contingent_price_right)                                
                                || (cond.contingent_price_left> cond.contingent_price_right)
                                )
                        {
                                Log().WithField("fun", "ValidConditionOrder")
                                        .WithField("key", m_userKey)
                                        .WithField("bid", m_condition_order_data.broker_id)
                                        .WithField("user_name", m_condition_order_data.user_id)
                                        .WithField("order_id", order.order_id)
                                        .Log(LOG_INFO, u8"??????????????????????????????,??????????????????????????????????????????????????????");

                                m_callBack.OutputNotifyAll(505
                                        , u8"??????????????????????????????,??????????????????????????????????????????????????????"
                                        , "WARNING", "MESSAGE");
                                return false;
                        }

                        bool flag = false;
                        double last_price = ins->last_price;
                        if ((kProductClassCombination == ins->product_class)
                                && (order.order_list.size() > 0))
                        {
                                if (EOrderDirection::buy == order.order_list[0].direction)
                                {
                                        last_price = ins->ask_price1;
                                }
                                else
                                {
                                        last_price = ins->bid_price1;
                                }
                        }
                        if (islessequal(last_price,cond.contingent_price_right)
                                && isgreaterequal(last_price,cond.contingent_price_left))
                        {
                                if (InstrumentLastTradeStatusIsContinousTrading(cond.instrument_id))
                                {
                                        flag = true;
                                }                                
                        }

                        if (flag)
                        {
                                if (logic_is_or)
                                {
                                        Log().WithField("fun", "ValidConditionOrder")
                                                .WithField("key", m_userKey)
                                                .WithField("bid", m_condition_order_data.broker_id)
                                                .WithField("user_name", m_condition_order_data.user_id)
                                                .WithField("order_id", order.order_id)
                                                .Log(LOG_INFO, u8"??????????????????????????????,?????????????????????????????????,???????????????");

                                        m_callBack.OutputNotifyAll(504
                                                , u8"??????????????????????????????,?????????????????????????????????,???????????????"
                                                , "WARNING", "MESSAGE");
                                        return false;
                                }
                                else
                                {
                                        cond.is_touched = true;
                                }                                
                        }

                }
                else if (cond.contingent_type == EContingentType::break_even)
                {
                        if (!IsValid(cond.break_even_price))
                        {
                                Log().WithField("fun", "ValidConditionOrder")
                                        .WithField("key", m_userKey)
                                        .WithField("bid", m_condition_order_data.broker_id)
                                        .WithField("user_name", m_condition_order_data.user_id)
                                        .WithField("order_id", order.order_id)
                                        .Log(LOG_INFO, u8"??????????????????????????????,????????????????????????????????????????????????????????????");

                                m_callBack.OutputNotifyAll(506
                                        , u8"??????????????????????????????,????????????????????????????????????????????????????????????"
                                        , "WARNING", "MESSAGE");
                                return false;
                        }
                        
                        bool flag = false;
                        double last_price = ins->last_price;
                        if ((kProductClassCombination == ins->product_class)
                                && (order.order_list.size() > 0))
                        {
                                if (EOrderDirection::buy == order.order_list[0].direction)
                                {
                                        last_price = ins->ask_price1;
                                }
                                else
                                {
                                        last_price = ins->bid_price1;
                                }
                        }
                        //??????
                        if (cond.break_even_direction == EOrderDirection::buy)
                        {
                                //?????????????????????
                                if (isgreater(last_price,cond.break_even_price))
                                {                                
                                        if (InstrumentLastTradeStatusIsContinousTrading(cond.instrument_id))
                                        {
                                                flag = true;
                                        }                                        
                                }
                        }
                        //??????
                        else if (cond.break_even_direction == EOrderDirection::sell)
                        {
                                //????????????????????????
                                if (isless(last_price, cond.break_even_price))
                                {                                        
                                        if (InstrumentLastTradeStatusIsContinousTrading(cond.instrument_id))
                                        {
                                                flag = true;
                                        }
                                }
                        }
                        
                        if (flag)
                        {
                                if (logic_is_or)
                                {
                                        Log().WithField("fun", "ValidConditionOrder")
                                                .WithField("key", m_userKey)
                                                .WithField("bid", m_condition_order_data.broker_id)
                                                .WithField("user_name", m_condition_order_data.user_id)
                                                .WithField("order_id", order.order_id)
                                                .Log(LOG_INFO, u8"??????????????????????????????,?????????????????????????????????,???????????????");

                                        m_callBack.OutputNotifyAll(504
                                                , u8"??????????????????????????????,?????????????????????????????????,???????????????"
                                                , "WARNING", "MESSAGE");
                                        return false;
                                }
                                else
                                {
                                        cond.is_touched = true;
                                }
                        }
                }
        }

        if (!logic_is_or)
        {
                bool flag = true;
                for (auto& cond : order.condition_list)
                {
                        flag = flag && cond.is_touched;
                }

                if (flag)
                {
                        Log().WithField("fun", "ValidConditionOrder")
                                .WithField("key", m_userKey)
                                .WithField("bid", m_condition_order_data.broker_id)
                                .WithField("user_name", m_condition_order_data.user_id)
                                .WithField("order_id", order.order_id)
                                .Log(LOG_INFO, u8"??????????????????????????????,??????????????????????????????,???????????????");

                        m_callBack.OutputNotifyAll(540
                                , u8"??????????????????????????????,??????????????????????????????,???????????????"
                                , "WARNING", "MESSAGE");
                        return false;
                }
        }

        //????????????
        for (auto& co : order.order_list)
        {
                std::string symbol = co.exchange_id + "." + co.instrument_id;
                Instrument* ins = GetInstrument(symbol);
                if (nullptr == ins)
                {
                        Log().WithField("fun", "ValidConditionOrder")
                                .WithField("key", m_userKey)
                                .WithField("bid", m_condition_order_data.broker_id)
                                .WithField("user_name", m_condition_order_data.user_id)
                                .WithField("order_id", order.order_id)
                                .Log(LOG_INFO, u8"??????????????????????????????,??????????????????????????????????????????ID?????????:" + symbol);

                        m_callBack.OutputNotifyAll(507
                                , u8"??????????????????????????????,??????????????????????????????????????????ID?????????:" + symbol
                                , "WARNING", "MESSAGE");
                        return false;
                }

                if (co.volume_type == EVolumeType::num)
                {
                        if (co.volume <= 0)
                        {
                                Log().WithField("fun", "ValidConditionOrder")
                                        .WithField("key", m_userKey)
                                        .WithField("bid", m_condition_order_data.broker_id)
                                        .WithField("user_name", m_condition_order_data.user_id)
                                        .WithField("order_id", order.order_id)
                                        .Log(LOG_INFO, u8"??????????????????????????????,?????????????????????????????????????????????");

                                m_callBack.OutputNotifyAll(508
                                        , u8"??????????????????????????????,?????????????????????????????????????????????"
                                        , "WARNING", "MESSAGE");
                                return false;
                        }
                }

                if (co.price_type == EPriceType::limit)
                {
                        if (!IsValid(co.limit_price))
                        {
                                Log().WithField("fun", "ValidConditionOrder")
                                        .WithField("key", m_userKey)
                                        .WithField("bid", m_condition_order_data.broker_id)
                                        .WithField("user_name", m_condition_order_data.user_id)
                                        .WithField("order_id", order.order_id)
                                        .Log(LOG_INFO, u8"??????????????????????????????,?????????????????????????????????????????????");

                                m_callBack.OutputNotifyAll(509
                                        , u8"??????????????????????????????,?????????????????????????????????????????????"
                                        , "WARNING", "MESSAGE");
                                return false;
                        }
                }
        }

        if (order.time_condition_type == ETimeConditionType::GTD)
        {
                if (order.GTD_date < order.trading_day)
                {
                        Log().WithField("fun", "ValidConditionOrder")
                                .WithField("key", m_userKey)
                                .WithField("bid", m_condition_order_data.broker_id)
                                .WithField("user_name", m_condition_order_data.user_id)
                                .WithField("order_id", order.order_id)
                                .Log(LOG_INFO, u8"??????????????????????????????,????????????????????????????????????");

                        m_callBack.OutputNotifyAll(510
                                , u8"??????????????????????????????,????????????????????????????????????"
                                , "WARNING", "MESSAGE");
                        return false;
                }
        }

        if (m_current_day_condition_order_count + 1 > g_condition_order_config.max_new_cos_per_day)
        {
                Log().WithField("fun", "ValidConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithField("order_id", order.order_id)
                        .Log(LOG_INFO, u8"??????????????????????????????,????????????????????????????????????????????????????????????");

                m_callBack.OutputNotifyAll(511
                        , u8"??????????????????????????????,????????????????????????????????????????????????????????????:"+std::to_string(g_condition_order_config.max_new_cos_per_day)
                        , "WARNING", "MESSAGE");
                return false;
        }


        if (m_current_valid_condition_order_count + 1 > g_condition_order_config.max_valid_cos_all)
        {
                Log().WithField("fun", "ValidConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithField("order_id", order.order_id)
                        .Log(LOG_INFO, u8"??????????????????????????????,???????????????????????????????????????????????????");

                m_callBack.OutputNotifyAll(512
                        , u8"??????????????????????????????,???????????????????????????????????????????????????:" + std::to_string(g_condition_order_config.max_valid_cos_all)
                        , "WARNING", "MESSAGE");
                return false;
        }
        
        return true;        
}

void ConditionOrderManager::InsertConditionOrder(const std::string& msg)
{
        SerializerConditionOrderData nss;
        if (!nss.FromString(msg.c_str()))
        {
                Log().WithField("fun","InsertConditionOrder")
                        .WithField("key",m_userKey)
                        .WithField("bid",m_condition_order_data.broker_id)
                        .WithField("user_name",m_condition_order_data.user_id)
                        .Log(LOG_INFO,"not invalid InsertConditionOrder msg!");                
                return;
        }
        
        if (!m_run_server)
        {
                Log().WithField("fun", "InsertConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack",msg)
                        .Log(LOG_INFO,u8"??????????????????????????????,??????:??????????????????????????????????????????");
                m_callBack.OutputNotifyAll(513
                        ,u8"??????????????????????????????,??????:??????????????????????????????????????????"
                        ,"WARNING","MESSAGE");
                return;
        }

        req_insert_condition_order insert_co;
        nss.ToVar(insert_co);

        if (insert_co.order_id.empty())
        {
                insert_co.order_id = std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>
                        (std::chrono::steady_clock::now().time_since_epoch()).count());        
        }

        std::string order_key = insert_co.order_id;
        auto it = m_condition_order_data.condition_orders.find(order_key);
        if (it != m_condition_order_data.condition_orders.end())
        {
                Log().WithField("fun", "InsertConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithField("order_id", insert_co.order_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????,??????:????????????");

                m_callBack.OutputNotifyAll(514
                        , u8"??????????????????????????????,??????:????????????"
                        , "WARNING","MESSAGE");
                return;
        }

        if (insert_co.user_id.substr(0,m_condition_order_data.user_id.size())
                != m_condition_order_data.user_id)
        {
                Log().WithField("fun", "InsertConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithField("order_id",insert_co.order_id)
                        .WithPack("co_req_pack",msg)
                        .Log(LOG_INFO, u8"??????????????????????????????,??????:?????????????????????????????????");

                m_callBack.OutputNotifyAll(515
                        , u8"??????????????????????????????,??????:?????????????????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }
                
        ConditionOrder order;
        order.order_id = insert_co.order_id;
        order.trading_day = atoi(m_condition_order_data.trading_day.c_str());        
        order.condition_list.assign(insert_co.condition_list.begin(),
                insert_co.condition_list.end());
        order.conditions_logic_oper = insert_co.conditions_logic_oper;
        order.order_list.assign(insert_co.order_list.begin(),
                insert_co.order_list.end());
        order.time_condition_type = insert_co.time_condition_type;
        order.GTD_date = insert_co.GTD_date;
        order.is_cancel_ori_close_order = insert_co.is_cancel_ori_close_order;
        order.insert_date_time = GetTouchedTime(order);
        
        if (ValidConditionOrder(order))
        {
                order.status = EConditionOrderStatus::live;
                order.touched_time = GetTouchedTime(order);
                order.changed = true;
                m_condition_order_data.condition_orders.insert(
                        std::map<std::string, ConditionOrder>::value_type(order.order_id
                                ,order));
                m_current_day_condition_order_count++;
                m_current_valid_condition_order_count++;
                m_callBack.OutputNotifyAll(516, u8"?????????????????????","INFO","MESSAGE");
                
                SerializerConditionOrderData nss;
                nss.FromVar(order);
                std::string strMsg = "";
                nss.ToString(&strMsg);

                Log().WithField("fun","InsertConditionOrder")
                        .WithField("key",m_userKey)
                        .WithField("bid",m_condition_order_data.broker_id)
                        .WithField("user_name",m_condition_order_data.user_id)
                        .WithPack("co_req_pack",msg)
                        .WithPack("co_pack",strMsg)
                        .Log(LOG_INFO,u8"?????????????????????");

                SaveCurrent();

                BuildConditionOrderIndex();

                m_callBack.OnUserDataChange();
        }
        else
        {
                order.status = EConditionOrderStatus::discard;
                order.touched_time = GetTouchedTime(order);
                order.changed = true;

                SerializerConditionOrderData nss2;
                nss2.FromVar(order);
                std::string strMsg = "";
                nss2.ToString(&strMsg);

                Log().WithField("fun", "InsertConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .WithPack("co_pack", strMsg)
                        .Log(LOG_INFO, u8"?????????????????????");
        }
}

void ConditionOrderManager::CancelConditionOrder(const std::string& msg)
{
        SerializerConditionOrderData nss;
        if (!nss.FromString(msg.c_str()))
        {
                Log().WithField("fun","CancelConditionOrder")
                        .WithField("key",m_userKey)
                        .WithField("bid",m_condition_order_data.broker_id)
                        .WithField("user_name",m_condition_order_data.user_id)
                        .Log(LOG_INFO,"not invalid CancelConditionOrder msg!");                
                return;
        }

        if (!m_run_server)
        {
                Log().WithField("fun", "CancelConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack",msg)
                        .Log(LOG_INFO,u8"??????????????????????????????????????????,??????:??????????????????????????????????????????");

                m_callBack.OutputNotifyAll(517
                        , u8"??????????????????????????????????????????,??????:??????????????????????????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        req_cancel_condition_order cancel_co;
        nss.ToVar(cancel_co);

        if (cancel_co.user_id.substr(0, m_condition_order_data.user_id.size())
                != m_condition_order_data.user_id)
        {
                Log().WithField("fun", "CancelConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:?????????????????????????????????");

                m_callBack.OutputNotifyAll(518
                        , u8"??????????????????????????????????????????,??????:?????????????????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        std::string order_key = cancel_co.order_id;
        auto it = m_condition_order_data.condition_orders.find(order_key);
        if (it == m_condition_order_data.condition_orders.end())
        {
                Log().WithField("fun", "CancelConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:???????????????");

                m_callBack.OutputNotifyAll(519
                        , u8"??????????????????????????????????????????,??????:???????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        ConditionOrder& order = it->second;
        if (order.status == EConditionOrderStatus::touched)
        {
                Log().WithField("fun","CancelConditionOrder")
                        .WithField("key",m_userKey)
                        .WithField("bid",m_condition_order_data.broker_id)
                        .WithField("user_name",m_condition_order_data.user_id)
                        .WithPack("co_req_pack",msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:??????????????????");

                m_callBack.OutputNotifyAll(520
                        , u8"??????????????????????????????????????????,??????:??????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        if (order.status == EConditionOrderStatus::cancel)
        {
                Log().WithField("fun","CancelConditionOrder")
                        .WithField("key",m_userKey)
                        .WithField("bid",m_condition_order_data.broker_id)
                        .WithField("user_name",m_condition_order_data.user_id)
                        .WithPack("co_req_pack",msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:???????????????");

                m_callBack.OutputNotifyAll(521
                        , u8"??????????????????????????????????????????,??????:???????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        if (order.status == EConditionOrderStatus::discard)
        {
                Log().WithField("fun", "CancelConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:??????????????????");

                m_callBack.OutputNotifyAll(522
                        , u8"??????????????????????????????????????????,??????:??????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        it->second.status = EConditionOrderStatus::cancel;
        it->second.touched_time = GetTouchedTime(it->second);
        it->second.changed = true;

        SerializerConditionOrderData nss2;
        nss2.FromVar(it->second);
        std::string strMsg = "";
        nss2.ToString(&strMsg);

        Log().WithField("fun", "CancelConditionOrder")
                .WithField("key", m_userKey)
                .WithField("bid", m_condition_order_data.broker_id)
                .WithField("user_name", m_condition_order_data.user_id)
                .WithPack("co_req_pack",msg)
                .WithPack("co_pack",strMsg)
                .Log(LOG_INFO, u8"?????????????????????");

        m_callBack.OutputNotifyAll(523,u8"?????????????????????", "INFO", "MESSAGE");
        m_current_day_condition_order_count--;
        m_current_valid_condition_order_count--;
        SaveCurrent();
        BuildConditionOrderIndex();
        m_callBack.OnUserDataChange();
}

void ConditionOrderManager::PauseConditionOrder(const std::string& msg)
{
        SerializerConditionOrderData nss;
        if (!nss.FromString(msg.c_str()))
        {
                Log().WithField("fun", "PauseConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .Log(LOG_INFO, "not invalid PauseConditionOrder msg!");                
                return;
        }

        if (!m_run_server)
        {
                Log().WithField("fun","PauseConditionOrder")
                        .WithField("key",m_userKey)
                        .WithField("bid",m_condition_order_data.broker_id)
                        .WithField("user_name",m_condition_order_data.user_id)
                        .WithPack("co_req_pack",msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:??????????????????????????????????????????");

                m_callBack.OutputNotifyAll(524
                        , u8"??????????????????????????????????????????,??????:??????????????????????????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        req_pause_condition_order pause_co;
        nss.ToVar(pause_co);

        if (pause_co.user_id.substr(0, m_condition_order_data.user_id.size())
                != m_condition_order_data.user_id)
        {
                Log().WithField("fun", "PauseConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:?????????????????????????????????");

                m_callBack.OutputNotifyAll(525
                        , u8"??????????????????????????????????????????,??????:?????????????????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        std::string order_key = pause_co.order_id;
        auto it = m_condition_order_data.condition_orders.find(order_key);
        if (it == m_condition_order_data.condition_orders.end())
        {
                Log().WithField("fun", "PauseConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:???????????????");

                m_callBack.OutputNotifyAll(526
                        , u8"??????????????????????????????????????????,??????:???????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        ConditionOrder& order = it->second;
        if (order.status == EConditionOrderStatus::touched)
        {
                Log().WithField("fun", "PauseConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:??????????????????");

                m_callBack.OutputNotifyAll(527
                        , u8"??????????????????????????????????????????,??????:??????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        if (order.status == EConditionOrderStatus::cancel)
        {
                Log().WithField("fun", "PauseConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:???????????????");

                m_callBack.OutputNotifyAll(528
                        , u8"??????????????????????????????????????????,??????:???????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        if (order.status == EConditionOrderStatus::discard)
        {
                Log().WithField("fun", "PauseConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:??????????????????");

                m_callBack.OutputNotifyAll(529
                        , u8"??????????????????????????????????????????,??????:??????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        if (order.status == EConditionOrderStatus::suspend)
        {
                Log().WithField("fun", "PauseConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:?????????????????????");

                m_callBack.OutputNotifyAll(530
                        , u8"??????????????????????????????????????????,??????:?????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        it->second.status = EConditionOrderStatus::suspend;
        it->second.touched_time = GetTouchedTime(it->second);
        it->second.changed = true;

        SerializerConditionOrderData nss2;
        nss2.FromVar(it->second);
        std::string strMsg = "";
        nss2.ToString(&strMsg);

        Log().WithField("fun", "PauseConditionOrder")
                .WithField("key", m_userKey)
                .WithField("bid", m_condition_order_data.broker_id)
                .WithField("user_name", m_condition_order_data.user_id)
                .WithPack("co_req_pack", msg)
                .WithPack("co_pack",strMsg)
                .Log(LOG_INFO, u8"?????????????????????");
        
        m_callBack.OutputNotifyAll(531,u8"?????????????????????", "INFO", "MESSAGE");
        SaveCurrent();        
        BuildConditionOrderIndex();
        m_callBack.OnUserDataChange();
}

void ConditionOrderManager::ResumeConditionOrder(const std::string& msg)
{
        SerializerConditionOrderData nss;
        if (!nss.FromString(msg.c_str()))
        {
                Log().WithField("fun", "ResumeConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .Log(LOG_INFO, "not invalid ResumeConditionOrder msg!");        
                return;
        }

        if (!m_run_server)
        {
                Log().WithField("fun","ResumeConditionOrder")
                        .WithField("key",m_userKey)
                        .WithField("bid",m_condition_order_data.broker_id)
                        .WithField("user_name",m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:??????????????????????????????????????????");

                m_callBack.OutputNotifyAll(532
                        , u8"??????????????????????????????????????????,??????:??????????????????????????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        req_resume_condition_order resume_co;
        nss.ToVar(resume_co);

        if (resume_co.user_id.substr(0, m_condition_order_data.user_id.size())
                != m_condition_order_data.user_id)
        {
                Log().WithField("fun", "ResumeConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:?????????????????????????????????");

                m_callBack.OutputNotifyAll(533
                        , u8"??????????????????????????????????????????,??????:?????????????????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        std::string order_key = resume_co.order_id;
        auto it = m_condition_order_data.condition_orders.find(order_key);
        if (it == m_condition_order_data.condition_orders.end())
        {
                Log().WithField("fun", "ResumeConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:???????????????");

                m_callBack.OutputNotifyAll(534
                        , u8"??????????????????????????????????????????,??????:???????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        ConditionOrder& order = it->second;
        if (order.status != EConditionOrderStatus::suspend)
        {
                Log().WithField("fun", "ResumeConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .WithPack("co_req_pack", msg)
                        .Log(LOG_INFO, u8"??????????????????????????????????????????,??????:?????????????????????????????????");

                m_callBack.OutputNotifyAll(535
                        , u8"??????????????????????????????????????????,??????:?????????????????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }
                
        it->second.status = EConditionOrderStatus::live;
        it->second.touched_time = GetTouchedTime(it->second);
        it->second.changed = true;

        SerializerConditionOrderData nss2;
        nss2.FromVar(it->second);
        std::string strMsg = "";
        nss2.ToString(&strMsg);

        Log().WithField("fun", "ResumeConditionOrder")
                .WithField("key", m_userKey)
                .WithField("bid", m_condition_order_data.broker_id)
                .WithField("user_name", m_condition_order_data.user_id)
                .WithPack("co_req_pack",msg)
                .WithPack("co_pack",strMsg)
                .Log(LOG_INFO, u8"?????????????????????");

        m_callBack.OutputNotifyAll(536, u8"?????????????????????", "INFO", "MESSAGE");
        SaveCurrent();        
        BuildConditionOrderIndex();
        m_callBack.OnUserDataChange();
}

void ConditionOrderManager::ChangeCOSStatus(const std::string& msg)
{
        SerializerConditionOrderData nss;
        if (!nss.FromString(msg.c_str()))
        {
                Log().WithField("fun", "ChangeCOSStatus")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .Log(LOG_INFO, "not invalid ChangeCOSStatus msg!");                
                return;
        }

        req_ccos_status req;
        nss.ToVar(req);
        m_run_server = req.run_server;
        if (!m_run_server)
        {
                Log().WithField("fun", "ChangeCOSStatus")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .Log(LOG_INFO, "cos is stoped!");                
        }
        else
        {
                Log().WithField("fun", "ChangeCOSStatus")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_data.broker_id)
                        .WithField("user_name", m_condition_order_data.user_id)
                        .Log(LOG_INFO,"cos is started!");
        }
}

TInstOrderIdListMap& ConditionOrderManager::GetOpenmarketCoMap()
{
        return m_openmarket_condition_order_map;
}

std::set<std::string>& ConditionOrderManager::GetTimeCoSet()
{
        return m_time_condition_order_set;
}

TInstOrderIdListMap& ConditionOrderManager::GetPriceCoMap()
{
        return m_price_condition_order_map;
}

void ConditionOrderManager::OnUpdateInstrumentTradeStatus(const InstrumentTradeStatusInfo
        & instTradeStatusInfo)
{        
        TInstrumentTradeStatusInfoMap::iterator it = _instrumentTradeStatusInfoMap.find(instTradeStatusInfo.InstrumentId);
        if (it == _instrumentTradeStatusInfoMap.end())
        {
                _instrumentTradeStatusInfoMap.insert(TInstrumentTradeStatusInfoMap::value_type(instTradeStatusInfo.InstrumentId,
                        instTradeStatusInfo));
        }
        else
        {
                it->second = instTradeStatusInfo;
        }

        //????????????
        if ((instTradeStatusInfo.instumentStatus != EInstrumentStatus::auctionOrdering)
                && (instTradeStatusInfo.instumentStatus != EInstrumentStatus::continousTrading))
        {
                return;
        }

        //?????????????????????,???????????????????????????
        if (!instTradeStatusInfo.IsDataReady)
        {
                return;
        }

        //???????????????????????????????????????60?????????,???????????????????????????
        //if (std::abs(instTradeStatusInfo.serverEnterTime - instTradeStatusInfo.localEnterTime) > 60)
        //{
        //        return;
        //}

        //????????????
        std::string strExchangeId = instTradeStatusInfo.ExchangeId;
        std::string strInstId = instTradeStatusInfo.InstrumentId;
        std::string strSymbolId = strExchangeId + "." + strInstId;
        TInstOrderIdListMap& om = GetOpenmarketCoMap();
        TInstOrderIdListMap::iterator it2 = om.find(strSymbolId);
        if (it2 == om.end())
        {
                return;
        }

        OnMarketOpen(strSymbolId,instTradeStatusInfo.instumentStatus);
}

bool ConditionOrderManager::InstrumentLastTradeStatusIsContinousTrading(const std::string& instId)
{
        std::string strInstId = instId;
        CutDigital_Ex(strInstId);
        TInstrumentTradeStatusInfoMap::iterator it = _instrumentTradeStatusInfoMap.find(strInstId);
        if (it == _instrumentTradeStatusInfoMap.end())
        {
                return true;
        }
        else
        {
                return (EInstrumentStatus::continousTrading==it->second.instumentStatus);
        }
}

void ConditionOrderManager::OnMarketOpen(const std::string& strSymbol,EInstrumentStatus instStatus)
{
        if (!m_run_server)
        {
                return;
        }

        TInstOrderIdListMap::iterator it = m_openmarket_condition_order_map.find(strSymbol);
        if (it == m_openmarket_condition_order_map.end())
        {
                return;
        }

        bool flag = false;
        std::vector<std::string>& orderIdList = it->second;
        for (auto orderId : orderIdList)
        {
                std::map<std::string, ConditionOrder>::iterator it 
                        = m_condition_order_data.condition_orders.find(orderId);
                if (it == m_condition_order_data.condition_orders.end())
                {
                        continue;
                }

                ConditionOrder& conditionOrder = it->second;
                if (conditionOrder.status != EConditionOrderStatus::live)
                {
                        continue;
                }

                std::vector<ContingentCondition>& condition_list = conditionOrder.condition_list;
                for (ContingentCondition& c : condition_list)
                {
                        if (c.contingent_type != EContingentType::market_open)
                        {
                            continue;
                        }

                        if (c.is_touched)
                        {
                            continue;
                        }

						//???????????????????????????????????????????????????
						if (IsCombinationInst(c.instrument_id) 
							&& (instStatus== EInstrumentStatus::auctionOrdering))
						{
							continue;
						}

                        std::string strInstId= c.instrument_id;
                        CutDigital_Ex(strInstId);
                        std::string symbol = c.exchange_id + "." + strInstId;
                        if (symbol != strSymbol)
                        {
                            continue;
                        }

                        c.is_touched = true;
                        flag = true;
                }

                if (IsContingentConditionTouched(condition_list, conditionOrder.conditions_logic_oper))
                {
                        conditionOrder.status = EConditionOrderStatus::touched;
                        conditionOrder.touched_time = GetTouchedTime(conditionOrder);
                        conditionOrder.changed = true;
                        //??????
                        m_callBack.OnTouchConditionOrder(conditionOrder);
                        flag = true;
                }                
        }

        if (flag)
        {
                SaveCurrent();                
                BuildConditionOrderIndex();
                m_callBack.OnUserDataChange();
        }
}

void ConditionOrderManager::OnCheckTime()
{
        if (!m_run_server)
        {
                return;
        }

        bool flag = false;
        for (auto orderId : m_time_condition_order_set)
        {
                std::map<std::string, ConditionOrder>::iterator it
                        = m_condition_order_data.condition_orders.find(orderId);
                if (it == m_condition_order_data.condition_orders.end())
                {
                        continue;
                }

                ConditionOrder& conditionOrder = it->second;
                if (conditionOrder.status != EConditionOrderStatus::live)
                {
                        continue;
                }

                std::vector<ContingentCondition>& condition_list = conditionOrder.condition_list;

                for (ContingentCondition& c : condition_list)
                {
                        if (c.contingent_type != EContingentType::time)
                        {
                                continue;
                        }

                        if (c.is_touched)
                        {
                                continue;
                        }

                        int exchangeTime = GetExchangeTime(c.exchange_id);

                        if ((exchangeTime>=c.contingent_time)
                                &&(exchangeTime< c.contingent_time+100))//????????????????????????????????????,????????????????????????
                        {
                                c.is_touched = true;
                                flag = true;
                        }
                }

                if (IsContingentConditionTouched(condition_list, conditionOrder.conditions_logic_oper))
                {
                        conditionOrder.status = EConditionOrderStatus::touched;
                        conditionOrder.touched_time = GetTouchedTime(conditionOrder);
                        conditionOrder.changed = true;
                        //??????
                        m_callBack.OnTouchConditionOrder(conditionOrder);
                        flag = true;
                }
        }

        if (flag)
        {
                SaveCurrent();                
                BuildConditionOrderIndex();
                m_callBack.OnUserDataChange();
        }
}

int ConditionOrderManager::GetTouchedTime(ConditionOrder& conditionOrder)
{
        int touched_time = 0;
        for (auto& c : conditionOrder.condition_list)
        {
                int exchangeTime = GetExchangeTime(c.exchange_id);
                if (exchangeTime > touched_time)
                {
                        touched_time = exchangeTime;
                }
        }
        for (auto& o : conditionOrder.order_list)
        {
                int exchangeTime = GetExchangeTime(o.exchange_id);
                if (exchangeTime > touched_time)
                {
                        touched_time = exchangeTime;
                }
        }
        return touched_time;
}

void ConditionOrderManager::OnCheckPrice()
{
        if (!m_run_server)
        {
                return;
        }

        bool flag = false;
        for (auto it : m_price_condition_order_map)
        {
                std::string strSymbol = it.first;
                std::vector<std::string>& orderIdList = it.second;
                Instrument* ins = GetInstrument(strSymbol);
                if (nullptr==ins)
                {
                        continue;
                }
                double last_price = ins->last_price;                        
                for (auto orderId : orderIdList)
                {
                        std::map<std::string, ConditionOrder>::iterator it
                                = m_condition_order_data.condition_orders.find(orderId);
                        if (it == m_condition_order_data.condition_orders.end())
                        {
                                continue;
                        }

                        ConditionOrder& conditionOrder = it->second;
                        if (conditionOrder.status != EConditionOrderStatus::live)
                        {
                                continue;
                        }

                        if ((kProductClassCombination ==ins->product_class )
                                &&(conditionOrder.order_list.size()>0))
                        {
                                if (EOrderDirection::buy ==conditionOrder.order_list[0].direction )
                                {
                                        last_price = ins->ask_price1;
                                }
                                else
                                {
                                        last_price = ins->bid_price1;
                                }
                        }                        

                        std::vector<ContingentCondition>& condition_list = conditionOrder.condition_list;

                        for (ContingentCondition& c : condition_list)
                        {
                                if (c.is_touched)
                                {
                                        continue;
                                }

                                std::string strInstId = c.instrument_id;
                                CutDigital_Ex(strInstId);
                                TInstrumentTradeStatusInfoMap::iterator it = _instrumentTradeStatusInfoMap.find(strInstId);
                                if (it != _instrumentTradeStatusInfoMap.end())
                                {
                                        InstrumentTradeStatusInfo& instTradeStatusInfo = it->second;
                                        if (instTradeStatusInfo.instumentStatus != EInstrumentStatus::continousTrading)
                                        {
                                                continue;
                                        }
                                }
                                
                                if (c.contingent_type == EContingentType::price)
                                {
                                        switch (c.price_relation)
                                        {
                                                case EPriceRelationType::G:
                                                        if (isgreater(last_price, c.contingent_price))
                                                        {
                                                                c.is_touched = true;
                                                                flag = true;
                                                        }
                                                        break;
                                                case EPriceRelationType::GE:
                                                        if (isgreaterequal(last_price, c.contingent_price))
                                                        {
                                                                c.is_touched = true;
                                                                flag = true;
                                                        }
                                                        break;
                                                case EPriceRelationType::L:
                                                        if (isless(last_price, c.contingent_price))
                                                        {
                                                                c.is_touched = true;
                                                                flag = true;
                                                        }
                                                        break;
                                                case EPriceRelationType::LE:
                                                        if (islessequal(last_price, c.contingent_price))
                                                        {
                                                                c.is_touched = true;
                                                                flag = true;
                                                        }
                                                        break;
                                        default:
                                                        break;
                                        }
                                }
                                else if (c.contingent_type == EContingentType::price_range)
                                {
                                        if (
                                                islessequal(last_price, c.contingent_price_right)
                                                && isgreaterequal(last_price, c.contingent_price_left)
                                                )
                                        {
                                                c.is_touched = true;
                                                flag = true;
                                        }
                                }
                                else if (c.contingent_type == EContingentType::break_even)
                                {
                                        //??????
                                        if (c.break_even_direction == EOrderDirection::buy)
                                        {
                                                if (c.m_has_break_event)
                                                {
                                                        //????????????????????????
                                                        if (islessequal(last_price, c.break_even_price))
                                                        {
                                                                c.is_touched = true;
                                                                flag = true;
                                                        }
                                                }
                                                //????????????????????????
                                                else
                                                {
                                                        if (isgreater(last_price, c.break_even_price))
                                                        {
                                                                c.m_has_break_event = true;
                                                                flag = true;
                                                        }
                                                }
                                        }
                                        //??????
                                        else if (c.break_even_direction == EOrderDirection::sell)
                                        {
                                                if (c.m_has_break_event)
                                                {
                                                        //????????????????????????
                                                        if (isgreaterequal(last_price, c.break_even_price))
                                                        {
                                                                c.is_touched = true;
                                                                flag = true;
                                                        }
                                                }
                                                else
                                                {
                                                        //????????????????????????
                                                        if (isless(last_price, c.break_even_price))
                                                        {
                                                                c.m_has_break_event = true;
                                                                flag = true;
                                                        }
                                                }
                                        }                                        
                                }
                                else
                                {
                                        continue;
                                }
                                
                        }

                        if (IsContingentConditionTouched(condition_list, conditionOrder.conditions_logic_oper))
                        {
                                conditionOrder.status = EConditionOrderStatus::touched;
                                conditionOrder.touched_time = GetTouchedTime(conditionOrder);
                                conditionOrder.changed = true;
                                //??????
                                m_callBack.OnTouchConditionOrder(conditionOrder);
                                flag = true;
                        }
                }
        }

        if (flag)
        {
                SaveCurrent();                
                BuildConditionOrderIndex();
                m_callBack.OnUserDataChange();
        }
}


bool ConditionOrderManager::IsContingentConditionTouched(std::vector<ContingentCondition>& condition_list
        ,ELogicOperator logicOperator)
{
        if (condition_list.size() == 0)
        {
                return false;
        }
        else if (condition_list.size() == 1)
        {
                return condition_list[0].is_touched;
        }

        if (logicOperator == ELogicOperator::logic_and)
        {
                bool flag = true;
                for (const ContingentCondition& c : condition_list)
                {
                        flag = flag && c.is_touched;
                }
                return flag;
        }
        else if (logicOperator == ELogicOperator::logic_or)
        {
                bool flag = false;
                for (const ContingentCondition& c : condition_list)
                {
                        flag = flag || c.is_touched;
                        if (flag)
                        {
                                break;
                        }
                }
                return flag;
        }
        else
        {
                return false;
        }        
}

void  ConditionOrderManager::SetExchangeTime(int localTime, int SHFETime, int DCETime
        , int INETime, int FFEXTime, int CZCETime)
{
        m_localTime = localTime;
        m_SHFETime = SHFETime;
        m_DCETime = DCETime;
        m_INETime = INETime;
        m_FFEXTime = FFEXTime;
        m_CZCETime = CZCETime;        
}

int ConditionOrderManager::GetExchangeTime(const std::string& exchange_id)
{
        boost::posix_time::ptime tm = boost::posix_time::second_clock::local_time();
        DateTime dtLocalTime;
        dtLocalTime.date.year = tm.date().year();
        dtLocalTime.date.month = tm.date().month();
        dtLocalTime.date.day = tm.date().day();
        dtLocalTime.time.hour = tm.time_of_day().hours();
        dtLocalTime.time.minute = tm.time_of_day().minutes();
        dtLocalTime.time.second = tm.time_of_day().seconds();
        dtLocalTime.time.microsecond = 0;

        int nLocalTime = DateTimeToEpochSeconds(dtLocalTime);
        int nTimeDelta = nLocalTime - m_localTime;
        if (exchange_id == "SHFE")
        {
                return m_SHFETime + nTimeDelta;
        }
        else if (exchange_id == "INE")
        {
                return m_INETime + nTimeDelta;
        }
        else if (exchange_id == "CZCE")
        {
                return m_CZCETime + nTimeDelta;
        }
        else if (exchange_id == "DCE")
        {
                return m_DCETime + nTimeDelta;
        }
        else
        {
                return m_FFEXTime+ nTimeDelta;
        }
}

void ConditionOrderManager::QryHisConditionOrder(int connId,const std::string& msg)
{
        SerializerConditionOrderData nss;
        if (!nss.FromString(msg.c_str()))
        {
                Log().WithField("fun", "QryHisConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_his_data.broker_id)
                        .WithField("user_name", m_condition_order_his_data.user_id)
                        .Log(LOG_INFO, "not invalid QryHisConditionOrder msg!");                
                return;
        }

        if (!m_run_server)
        {
                Log().WithField("fun", "QryHisConditionOrder")
                        .WithField("key", m_userKey)
                        .WithField("bid", m_condition_order_his_data.broker_id)
                        .WithField("user_name", m_condition_order_his_data.user_id)
                        .Log(LOG_INFO, u8"????????????????????????????????????????????????,??????:??????????????????????????????????????????");

                m_callBack.OutputNotifyAll(537
                        , u8"????????????????????????????????????????????????,??????:??????????????????????????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        qry_histroy_condition_order qry_his_co;
        nss.ToVar(qry_his_co);

        if (qry_his_co.user_id.substr(0, m_condition_order_his_data.user_id.size())
                != m_condition_order_his_data.user_id)
        {
                m_callBack.OutputNotifyAll(538
                        , u8"????????????????????????????????????????????????,??????:?????????????????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        if (qry_his_co.action_day <= 0)
        {
                m_callBack.OutputNotifyAll(539
                        , u8"????????????????????????????????????????????????,??????:????????????????????????????????????"
                        , "WARNING", "MESSAGE");
                return;
        }

        std::vector<ConditionOrder> condition_orders;
        for (auto& order : m_condition_order_his_data.his_condition_orders)
        {
                DateTime dt;
                SetDateTimeFromEpochSeconds(&dt, order.insert_date_time);
                int insert_day = dt.date.year * 10000 + dt.date.month * 100 + dt.date.day;
                if (insert_day == qry_his_co.action_day)
                {
                        condition_orders.push_back(order);
                }
        }

        Log().WithField("fun","QryHisConditionOrder")
                .WithField("key",m_userKey)
                .WithField("bid",m_condition_order_his_data.broker_id)
                .WithField("user_name",m_condition_order_his_data.user_id)
                .WithField("user_id",m_condition_order_his_data.user_id)                
                .WithField("qry_day",qry_his_co.action_day)
                .WithField("his_co_size",(int)condition_orders.size())
                .Log(LOG_INFO,u8"???????????????????????????");
        
        SerializerConditionOrderData nss_his;

        rapidjson::Pointer("/aid").Set(*nss_his.m_doc, "rtn_his_condition_orders");
        rapidjson::Pointer("/user_id").Set(*nss_his.m_doc, m_condition_order_data.user_id);
        rapidjson::Pointer("/action_day").Set(*nss_his.m_doc, qry_his_co.action_day);

        rapidjson::Value node_data;
        nss_his.FromVar(condition_orders, &node_data);
        rapidjson::Pointer("/his_condition_orders").Set(*nss_his.m_doc, node_data);

        std::string json_str;
        nss_his.ToString(&json_str);

        m_callBack.SendDataDirect(connId,json_str);
}