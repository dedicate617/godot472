//============================================================================
// Name        : mqttclient.cpp
// Author      : Bosmutus
// Version     :
// Copyright   : copyright by Bosmutus
// Description : paho mqtt client implement
//============================================================================

#include "mqttclient.h"
#include "core/os/os.h"
#include <signal.h>
#include <iostream>
#if defined(__linux__)
#include "util.h"
#endif
//#include <mutex>


//std::mutex mutex;

MqttClient::MqttClient() :
#ifdef _WIN32
		mqtt::async_client(str_format("mqtt://%1:%2", DEFAULT_HOST, DEFAULT_PORT), ""),
#elif defined(__linux__)
		mqtt::async_client(util::Format("mqtt://{0}:{1}", DEFAULT_HOST, DEFAULT_PORT), ""),
#endif
		m_host(DEFAULT_HOST),
		m_port(DEFAULT_PORT),
		m_clientId("") {
	pcb = nullptr;
}

MqttClient::MqttClient(const mqtt::create_options &opts,
		const std::string &host /* = DEFAULT_HOST*/,
		const uint16_t port /* = DEFAULT_PORT*/,
		const std::string &clientId /* = ""*/,
		mqtt::iclient_persistence *persistence /* = nullptr*/) :
#ifdef _WIN32
		mqtt::async_client(str_format("mqtt://%s:%d", host, port), clientId, opts, persistence),
#elif defined(__linux__)
		mqtt::async_client(util::Format("mqtt://{0}:{1}", host, port), clientId, opts, persistence),
#endif
		m_host(host),
		m_port(port),
		m_clientId(clientId) {
	pcb = nullptr;
}

MqttClient::~MqttClient() {}

bool MqttClient::isConnectedToHost() {
	return is_connected();
}

bool MqttClient::disconnectFromHost() {
	// case 1: wait infinite
	//    disconnect()->wait();
	// case 2: wait for some time
	std::chrono::milliseconds timeout_ = std::chrono::seconds(5);
	if (!disconnect()->wait_for(timeout_))
		return false;
	return true;
}

std::string MqttClient::hostname() const {
	return m_host;
}

uint16_t MqttClient::port() const {
	return m_port;
}

std::string MqttClient::clientId() const {
	return m_clientId;
}

void MqttClient::setCallback(ICallback *cb){
	pcb = cb;
}
