//============================================================================
// Name        : icallback.h
// Author      : Bosmutus
// Version     :
// Copyright   : copyright by Bosmutus
// Description : callback interface for mqttmanager
//============================================================================
#ifndef ICALLBACK_H
#define ICALLBACK_H

#include "core/variant/variant.h"
#include <iostream>
#include <map>
#include <vector>

class ICallback {
public:
	ICallback(){};
	virtual ~ICallback() {};

	virtual void onMqttConnected();
	virtual void onMqttLostconnect();
	virtual void onMqttDisconnected();
	virtual void onMqttMsgReceived(const String &topic, const String &msg, const int qos);

};

#endif // ICALLBACK_H
