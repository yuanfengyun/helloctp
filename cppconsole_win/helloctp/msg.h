#pragma once

struct Msg{
    int id;
    void* ptr;
    bool isLast;
public:

    Msg(){}
    Msg(int id,void* ptr,bool isLast=false){
        this->id = id;
        this->ptr = ptr;
        this->isLast = isLast;
    }
};

enum MsgType{
    msg_login = 1,
    msg_market_data = 2,
    msg_position_data = 3,
    msg_position_detail_data = 4,
    msg_trade_data = 5,
    msg_order_data = 6
};
