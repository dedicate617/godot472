//============================================================================
// Name        : util.h
// Author      : Bosmutus
// Version     :
// Copyright   : copyright by Bosmutus
// Description : Hello World in C++, Ansi-style
//============================================================================

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <iconv.h> 
#endif

class UAVarUtil {
private:

	int preNUm(unsigned char byte) {
		unsigned char mask = 0x80;
		int num = 0;
		for (int i = 0; i < 8; i++) {
			if ((byte & mask) == mask) {
				mask = mask >> 1;
				num++;
			}
			else {
				break;
			}
		}
		return num;
	}


	bool isUtf8(unsigned char* data, int len) {
		int num = 0;
		int i = 0;
		while (i < len) {
			if ((data[i] & 0x80) == 0x00) {
				// 0XXX_XXXX
				i++;
				continue;
			}
			else if ((num = preNUm(data[i])) > 2) {
				// 110X_XXXX 10XX_XXXX
				// 1110_XXXX 10XX_XXXX 10XX_XXXX
				// 1111_0XXX 10XX_XXXX 10XX_XXXX 10XX_XXXX
				// 1111_10XX 10XX_XXXX 10XX_XXXX 10XX_XXXX 10XX_XXXX
				// 1111_110X 10XX_XXXX 10XX_XXXX 10XX_XXXX 10XX_XXXX 10XX_XXXX
				// preNUm() 返回首个字节8个bits中首�?0bit前面1bit的个数，该数量也是该字符所使用的字节数        
				i++;
				for (int j = 0; j < num - 1; j++) {
					//判断后面num - 1 个字节是不是都是10开
					if ((data[i] & 0xc0) != 0x80) {
						return false;
					}
					i++;
				}
			}
			else {
				//其他情况说明不是utf-8
				return false;
			}
		}
		return true;
	}

	bool isGBK(unsigned char* data, int len) {
		int i = 0;
		while (i < len) {
			if (data[i] <= 0x7f) {
				//编码小于等于127,只有一个字节的编码，兼容ASCII
				i++;
				continue;
			}
			else {
				//大于127的使用双字节编码
				if (data[i] >= 0x81 &&
					data[i] <= 0xfe &&
					data[i + 1] >= 0x40 &&
					data[i + 1] <= 0xfe &&
					data[i + 1] != 0xf7) {
					i += 2;
					continue;
				}
				else {
					return false;
				}
			}
		}
		return true;
	}
	
public:
	UAVarUtil() {};
	~UAVarUtil() {};

	typedef enum {
		GBK,
		UTF8,
		UNKOWN
	} CODING;

	//需要说明的是，isGBK()是通过双字节是否落在gbk的编码范围内实现的，
	//而utf-8编码格式的每个字节都是落在gbk的编码范围内
	//所以只有先调用isUtf8()先判断不是utf-8编码，再调用isGBK()才有意义
	CODING GetCoding(unsigned char* data, int len) {
		CODING coding;
		if (isUtf8(data, len) == true) {
			coding = UTF8;
		}
		else if (isGBK(data, len) == true) {
			coding = GBK;
		}
		else {
			coding = UNKOWN;
		}
		return coding;
	}

	std::string getEncode(const std::string& strTest)
	{
		unsigned char cha = strTest[0];
		int iCode = cha << 8;
		cha = strTest[1];
		iCode += cha;
		std::string strCode;
		switch (iCode)   //判断文本前两个字节
		{
		case 0xfffe:    //65534
			strCode = "Unicode";
			break;
		case 0xfeff:    //65279
			strCode = "Unicode big endian";
			break;
		case 0xefbb:    //61371
			strCode = "UTF-8";
			break;
		default:
			strCode = "ANSI";
		}
		return strCode;
	}

};

#if defined(__linux__)

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace util {
	class ArgBase {
	public:
		ArgBase() {}
		virtual ~ArgBase() {}
		virtual void Format(std::ostringstream &ss, const std::string &fmt) = 0;
	};

	template <class T>
	class Arg : public ArgBase {
	public:
		Arg(T arg) :
				m_arg(arg) {}
		virtual ~Arg() {}
		virtual void Format(std::ostringstream &ss, const std::string &fmt) {
			ss << m_arg;
		}

	private:
		T m_arg;
	};

	class ArgArray : public std::vector<ArgBase *> {
	public:
		ArgArray() {}
		~ArgArray() {
			std::for_each(begin(), end(), [](ArgBase *p) { delete p; });
		}
	};

	static void FormatItem(std::ostringstream &ss, const std::string &item, const ArgArray &args) {
		int index = 0;
		int alignment = 0;
		std::string fmt;

		char *endptr = nullptr;
		index = strtol(&item[0], &endptr, 10);
		if (index < 0 || index >= args.size()) {
			return;
		}

		if (*endptr == ',') {
			alignment = strtol(endptr + 1, &endptr, 10);
			if (alignment > 0) {
				ss << std::right << std::setw(alignment);
			} else if (alignment < 0) {
				ss << std::left << std::setw(-alignment);
			}
		}

		if (*endptr == ':') {
			fmt = endptr + 1;
		}

		args[index]->Format(ss, fmt);

		return;
	}

	template <class T>
	static void Transfer(ArgArray &argArray, T t) {
		argArray.push_back(new Arg<T>(t));
	}

	template <class T, typename... Args>
	static void Transfer(ArgArray &argArray, T t, Args &&...args) {
		Transfer(argArray, t);
		Transfer(argArray, args...);
	}

	template <typename... Args>
	std::string Format(const std::string &format, Args &&...args) {
		if (sizeof...(args) == 0) {
			return format;
		}

		ArgArray argArray;
		Transfer(argArray, args...);
		size_t start = 0;
		size_t pos = 0;
		std::ostringstream ss;
		while (true) {
			pos = format.find('{', start);
			if (pos == std::string::npos) {
				ss << format.substr(start);
				break;
			}

			ss << format.substr(start, pos - start);
			if (format[pos + 1] == '{') {
				ss << '{';
				start = pos + 2;
				continue;
			}

			start = pos + 1;
			pos = format.find('}', start);
			if (pos == std::string::npos) {
				ss << format.substr(start - 1);
				break;
			}

			FormatItem(ss, format.substr(start, pos - start), argArray);
			start = pos + 1;
		}

		return ss.str();
	}
} //namespace util

#endif
