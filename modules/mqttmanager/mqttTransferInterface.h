//============================================================================
// Name        : mqttTransferInterface.h
// Author      : Bosmutus
// Version     :
// Copyright   : copyright by Bosmutus
// Description : 使用mqtt作为传输协议时的调用接口
//============================================================================
#ifndef MQTTTRANSFERINTERFACE_H
#define MQTTTRANSFERINTERFACE_H

#include "core/variant/variant.h"
#include "core/object/class_db.h"
#include <iostream>
#include <map>
#include <vector>

class MqttTransferInterface : public Object {
	GDCLASS(MqttTransferInterface, Object);
public:
	//MqttTransferInterface(){};
	//virtual ~MqttTransferInterface(){};
protected:
	static void _bind_methods() {
		ADD_SIGNAL(MethodInfo("MqttConnected1"));
		ADD_SIGNAL(MethodInfo("MqttLostconnect1"));
		ADD_SIGNAL(MethodInfo("MqttDisconnected1"));
		ADD_SIGNAL(MethodInfo("MqttMsgReceived1", PropertyInfo(Variant::STRING, "topic"), PropertyInfo(Variant::STRING, "msg"), PropertyInfo(Variant::INT, "qos")));
		ADD_SIGNAL(MethodInfo("MqttProcessRequest1", PropertyInfo(Variant::STRING, "req_topic"), PropertyInfo(Variant::STRING, "req_msg"), PropertyInfo(Variant::STRING, "rsp_topic")));
	}

public:
	virtual Error init(const String &p_prjPath = "") = 0;
	virtual Error init_bydict(const Dictionary &p_mqttConfDict) = 0;

	virtual bool connect_start(const String &p_host = "", const int p_port = 1883, const String &p_clientId = "") = 0;
	virtual bool connect_reqrsp_start(const String &p_host = "", const int p_port = 1883, const String &p_clientId = "") = 0;
	virtual Error invokeMethod(const String &p_nodePath, const Array &p_inputArguments, Array p_outputArguments, const String &p_remoteClientId) = 0;
	virtual bool subscribe1(const String &p_topic, const int &p_qos = 0) = 0;
	virtual String req_rsp1(const String &p_reqtopic, const String &p_reqmsg, const String &p_rsptopic, const String &p_remoteClientId, int p_qos = 2, int p_secs_timeout = 5, bool p_auto_mode = true) = 0;
};

#endif // MQTTTRANSFERINTERFACE_H
