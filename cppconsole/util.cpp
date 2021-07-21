#include <cstring>
#include <iconv.h>
#include "util.h"

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
