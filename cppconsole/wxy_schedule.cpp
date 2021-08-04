#include "wxy_schedule.h"
#include <map>
#include <string>
#include "config.h"
#include "util.h"
#include "td_op.h"

using namespace std;

WxySchedule::WxySchedule(){

}

void WxySchedule::init(char* filename)
{
    char buf[256]={0};
    FILE * f = fopen(filename, "r");
    fscanf(f,"%s\n",buf);
    m_name = buf;

    fscanf(f,"%s\n",buf);
    m_dir = buf;

    fscanf(f,"%d\n",&m_gride);

    while(1){
        int price;
        int volume;
        int ret = fscanf(f, "%d %d\n",&price,&volume);
        if(ret==EOF) break;
        m_price_2_volume[price] = volume;
    }
    fclose(f);
}

void WxySchedule::run()
{
    if(m_dir == string("buy")) runBuy();
    else runSell();
}

void WxySchedule::runBuy(){
    wxy_long_datas[m_name] = m_gride;
    int long_position = 0;
    if(positions.find(m_name) != positions.end()){
        long_position = positions[m_name]->Long;
    }
    int long_frozen = get_close_frozen(m_name,"sell");
    int open_order = get_valid_order_volume(m_name,"buy");

    int long_total = 0;
    int opened = open_order;

    auto market = market_datas[m_name];

    // 开
    for(auto it =m_price_2_volume.rbegin();it!=m_price_2_volume.rend();++it){
        long_total += it->second;
        if(long_position + opened < long_total){
            int need_open = long_total - (long_position + opened);
            if(need_open>0){
                float price = it->first;
                if(price > market->UpperLimitPrice) price = market->UpperLimitPrice;
                if(price > market->LowerLimitPrice){
                    TdOp::ReqOrderInsert(m_name,"buy","open",price,need_open);
                }
                opened += need_open;
            }
        }
    }

    // 平
    int valid_long = long_position - long_frozen;
    for(auto it =m_price_2_volume.begin();it!=m_price_2_volume.end();++it){
        long_total -= it->second;
        if(long_total < valid_long){
            int need_close = valid_long - long_total;
            float price = it->first + m_gride;
            if(price < market->LowerLimitPrice) price = market->LowerLimitPrice;
            if(price < market->UpperLimitPrice) TdOp::ReqOrderInsert(m_name,"sell","close",price,need_close);
            valid_long -= need_close;
        }
    }
}

void WxySchedule::runSell(){
    wxy_short_datas[m_name] = m_gride;
    int short_position = 0;
    if(positions.find(m_name) != positions.end()){
        short_position = positions[m_name]->Short;
    }
    int short_frozen = get_close_frozen(m_name,"buy");
    int open_order = get_valid_order_volume(m_name,"sell");

    int short_total = 0;
    int opened = open_order;

    auto market = market_datas[m_name];

    // 开
    for(auto it =m_price_2_volume.begin();it!=m_price_2_volume.end();++it){
        short_total += it->second;
        if(short_position + opened < short_total){
            int need_open = short_total - (short_position + opened);
            if(need_open>0){
                float price = it->first;
                if(price < market->LowerLimitPrice) price = market->LowerLimitPrice;
                if(price < market->UpperLimitPrice){
                    TdOp::ReqOrderInsert(m_name,"sell","open",price,need_open);
                }
                opened += need_open;
            }
        }
    }

    // 平
    int valid_short = short_position - short_frozen;
    for(auto it =m_price_2_volume.rbegin();it!=m_price_2_volume.rend();++it){
        short_total -= it->second;
        if(short_total < valid_short){
            int need_close = valid_short - short_total;
            float price = it->first-m_gride;
            if(price > market->UpperLimitPrice) price = market->UpperLimitPrice;
            if(price > market->LowerLimitPrice) TdOp::ReqOrderInsert(m_name,"buy","close",price,need_close);
            valid_short -= need_close;
        }
    }
}
