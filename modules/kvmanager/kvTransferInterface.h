//============================================================================
// Name        : kvTransferInterface.h
// Author      : Bosmutus
// Version     :
// Copyright   : copyright by Bosmutus
// Description : 使用mqtt作为传输协议时通知KV模块回调接口
//============================================================================
#ifndef KVTRANSFERINTERFACE_H
#define KVTRANSFERINTERFACE_H

#include "core/variant/variant.h"
#include <iostream>
#include <map>
#include <vector>

class KVTransferInterface : public Object {
public:
	//KVTransferInterface(){};
	//virtual ~KVTransferInterface(){};
protected:

public:
	virtual void init(const String &p_rootDir  = "") = 0;
	virtual void kvWithID1(int p_mode = 1, const String &p_id = "DEFAULT", const String &p_cryptKey = "", const String &p_rootPath = "") = 0;
	virtual void onVarRefreshed(const String &topic, const String &msg, const int qos) = 0;
};

#endif // KVTRANSFERINTERFACE_H
