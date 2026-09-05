//============================================================================
// Name        : icallback.h
// Author      : Bosmutus
// Version     :
// Copyright   : copyright by Bosmutus
// Description : Hello World in C++, Ansi-style
//============================================================================
#ifndef ICALLBACK_H
#define ICALLBACK_H

#include "core/variant/variant.h"
#include <OpcUaVariant.hh>
#include <iostream>
#include <map>
#include <vector>

class ICallback {
public:
	ICallback(){};
	virtual ~ICallback() {};

	virtual void variableChanged(const std::string &varName, const OPCUA::Variant &value) = 0;
	virtual void variablesChanged(const std::vector<std::string> &varNames, const std::vector<OPCUA::Variant> &values) = 0;

	virtual void handleEvent(const unsigned int &eventId, const std::map<std::string, OPCUA::Variant> &eventData) = 0;

	virtual void handleVariableReadHisCallback(const std::string& nodePath, const std::vector<UA_DataValue>& datas, const bool moreData) = 0;

	virtual void handleStatusCallback(const int status) = 0;

	virtual void handleInvokeMethodCallback(const unsigned int &reqId, const std::vector<OPCUA::Variant> &outputs) = 0;

	virtual void drainCommandQueue() {}
	virtual void expireInflight() {}
};

#endif // ICALLBACK_H
