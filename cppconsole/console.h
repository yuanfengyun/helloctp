#pragma once
#include "msg.h"

void* handle_msg(Msg* msg);

void handle_cmd(char* cmd);

void init_wxy_schedule(char* filename);
