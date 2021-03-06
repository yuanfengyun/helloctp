#pragma once
#include <vector>
#include <string>

using namespace std;

vector<std::string> splitWithStl(const std::string &str,const std::string &pattern);

string& replace_all(string& str,const string& old_value,const string& new_value);

string GbkToUtf8(char *str_str);

int GbkToUtf8(char *str_str, size_t src_len, char *dst_str, size_t dst_len);

bool isInt(const char* str);

bool isFloat(const char * str);

const char* getDir(char c);

const char* getPositionDir(char c);

const char* getOffset(char c);

string getOrderStatus(char c,char submit_c,char* str);

int get_close_frozen(string InstrumentID,string dir);

int get_valid_order_volume(string InstrumentID,string dir);

string getFullName(string name);
