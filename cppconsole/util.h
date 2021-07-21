#pragma once
#include <vector>
#include <string>

std::vector<std::string> splitWithStl(const std::string &str,const std::string &pattern);

int GbkToUtf8(char *str_str, size_t src_len, char *dst_str, size_t dst_len);

bool isInt(const char* str);

bool isFloat(const char * str);

