#pragma once

struct Msg{
    int id;
    void* ptr;
public:

    Msg(){}
    Msg(int id,void* ptr){
        this->id = id;
        this->ptr = ptr;
    }
};

enum MsgType{
    msg_market_data = 1
};
