#include <cstring>
#include <iconv.h>
#include <map>
#include "util.h"
#include "ThostFtdcUserApiDataType.h"
#include "ThostFtdcTraderApi.h"

using namespace std;

extern map<string,CThostFtdcOrderField*> order_datas;

string GbkToUtf8(char *str_str)
{
    char buf[512];
    memset(buf,0,sizeof(buf));
    GbkToUtf8(str_str,strlen(str_str),buf,512);
    return string(buf);
}

int GbkToUtf8(char *str_str, size_t src_len, char *dst_str, size_t dst_len)
{
    iconv_t cd;
    char **pin = &str_str;
    char **pout = &dst_str;

    cd = iconv_open("utf8", "gbk");
    if (cd == 0)
        return -1;
    memset(dst_str, 0, dst_len);
    if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
        return -1;
    iconv_close(cd);
    *pout = 0;

    return 0;
}

std::vector<std::string> splitWithStl(const std::string &str,const std::string &pattern)
{
    std::vector<std::string> resVec;

    if ("" == str)
    {
        return resVec;
    }
    //方便截取最后一段数据
    std::string strs = str + pattern;

    size_t pos = strs.find(pattern);
    size_t size = strs.size();

    while (pos != std::string::npos)
    {
        std::string x = strs.substr(0,pos);
        resVec.push_back(x);
        strs = strs.substr(pos+1,size);
        pos = strs.find(pattern);
    }

    return resVec;
}

string& replace_all(string& str,const string& old_value,const string& new_value)
{
    while(true){
        string::size_type   pos(0);
        if((pos=str.find(old_value))!=string::npos)
            str.replace(pos,old_value.length(),new_value);
        else   break;
    }
    return   str;
}

bool isInt(const char* str)
{
    bool isNum = false;

    int index = 0;
    for (; *str != '\0'; str++, index++)
    {
        switch (*str)
        {
            case '0':case'1':case'2':case'3':case'4':case'5':
            case'6':case'7':case'8':case'9':
                isNum = true;
                break;
            case '-':case '+':
                if (index != 0)
                {
                    return false;
                }
                break;
            default:
                return false;
        }
    }

    if (!isNum)
    {
        return false;
    }

    return true;
}

bool isFloat(const char * str)
{
    bool isE = false,
         isPoint = false,
         numBefore = false,
         numBehind = false;

    int index = 0;
    for (; *str != '\0'; str++, index++)
    {
        switch (*str)
        {
            case '0':case'1':case'2':case'3':case'4':case'5':
            case'6':case'7':case'8':case'9':
                if (isE)
                {
                    numBehind = true;
                }
                else
                {
                    numBefore = true;
                }
                break;
            case '+':case '-':
                if (index != 0)
                {
                    return false;
                }
                break;
            case 'e':case 'E':
                if (isE || !numBefore)
                {
                    return false;
                }
                else
                {
                    isPoint = true;
                    index = -1;
                    isE = true;
                }
                break;
            case '.':
                if (isPoint)
                {
                    return false;
                }
                else
                {
                    isPoint = true;
                }
                break;
            default:
                return false;
        }
    }

    if (!numBefore)
    {
        return false;
    }
    else if (isE && !numBehind)
    {
        return false;
    }

    return true;
}

const char* getDir(char c)
{
    if(c==THOST_FTDC_D_Buy) return "买";
    if(c==THOST_FTDC_D_Sell) return "卖";
}

const char* getPositionDir(char c)
{
    if(c==THOST_FTDC_PD_Long) return "多";
    if(c==THOST_FTDC_PD_Short) return "空";
    if(c==THOST_FTDC_PD_Net) return "净";
    return "unknown";
}

const char* getOffset(char c)
{
    if(c==THOST_FTDC_OF_Open) return "开";
    if(c==THOST_FTDC_OF_Close) return "平";
    return "unknown";
}

string getOrderStatus(char c,char submit_c,char* str)
{
    string status;
    if(submit_c == THOST_FTDC_OSS_InsertRejected){
        status += "报单被拒绝:  " + GbkToUtf8(str) + "!!!";
        return status;
    }

    if(c==THOST_FTDC_OST_AllTraded){
        status = "全部成交";
    }
    else if(c==THOST_FTDC_OST_PartTradedQueueing){
        status == "部分成交";
    }
    else if(c == THOST_FTDC_OST_Canceled){
        status = GbkToUtf8(str);
    }
    else{
        status = GbkToUtf8(str);
    }
    return status;
}

// 平仓锁定的持仓
int get_close_frozen(string InstrumentID,string dir){
    char d = THOST_FTDC_D_Buy;
    int volume = 0;
    if(dir==string("sell")) d = THOST_FTDC_D_Sell;
    for(auto it=order_datas.begin();it!=order_datas.end();++it)
    {
        if(string(it->second->InstrumentID) != InstrumentID) continue;

        if(it->second->OrderStatus != THOST_FTDC_OST_PartTradedQueueing and it->second->OrderStatus != THOST_FTDC_OST_NoTradeQueueing) continue;
        char offset = it->second->CombOffsetFlag[0];
        if(offset == THOST_FTDC_OF_Open) continue;

        char dir = it->second->Direction;
        if(dir == d){
            volume += it->second->VolumeTotal;
        }
    }
    return volume;
}

// 未成交单
int get_valid_order_volume(string InstrumentID,string dir){
    char d = THOST_FTDC_D_Buy;
    int volume = 0;
    if(dir==string("sell")) d = THOST_FTDC_D_Sell;
    for(auto it=order_datas.begin();it!=order_datas.end();++it)
    {
        if(string(it->second->InstrumentID) != InstrumentID) continue;

        if(it->second->OrderStatus != THOST_FTDC_OST_PartTradedQueueing and it->second->OrderStatus != THOST_FTDC_OST_NoTradeQueueing) continue;
        char offset = it->second->CombOffsetFlag[0];
        if(offset != THOST_FTDC_OF_Open) continue;

        char dir = it->second->Direction;
        if(dir == d){
            volume += it->second->VolumeTotal;
        }
    }
    return volume;
}

string getFullName(string name){
    if(name.find("&") != string::npos){
        if(name.find(" ") == string::npos){
            vector<string> array = splitWithStl(name,"&");
            if(array.size()==2){
                name = "SP " + getFullName(array[0]) + "&" + getFullName(array[1]);
            }
        }
        return name;
    }

    if(isInt(name.c_str())){
        if(name.size()==2){
            int month = atoi(name.c_str());
            char year_buf[32] = {0};
            time_t currtime;
            time(&currtime);
            struct tm *today = localtime(&currtime);
            if(month < today->tm_mon+1){
                sprintf(year_buf,"%d", today->tm_year - 100 + 1);
            }else{

                sprintf(year_buf,"%d", today->tm_year - 100);
            }
            name = year_buf + name;
        }

        name = "jd" + name;
    }

    return name;
}
