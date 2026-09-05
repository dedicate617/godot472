//============================================================================
// Name        : util.h
// Author      : Bosmutus
// Version     :
// Copyright   : copyright by Bosmutus
// Description : Hello World in C++, Ansi-style
//============================================================================

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
