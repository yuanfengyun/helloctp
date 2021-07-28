#pragma once

#include<map>

using namespace std;

class WxySchedule{
    public:
    WxySchedule();
    string m_name;
    string m_dir;
    int m_gride;
    map<int,int> m_price_2_volume;

    void init(char*);
    void run();
    void runBuy();
    void runSell();
};
