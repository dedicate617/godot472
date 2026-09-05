// uaclient.h
#pragma once
#include <OpcUaGenericClient.hh>
#include <thread>
#include <atomic>
#include <chrono>
#include "ICallback.h"

class UAClient : public OPCUA::GenericClient {
private:
    ICallback *pcb;
    std::atomic<bool> running_{false};
    std::thread th_run;

    static void run(UAClient *self);

protected:
    void handleVariableValueUpdate(const std::string &entityName, const OPCUA::Variant &value) override;
    void handleVariableValueUpdates(const std::vector<std::string> &entityNames, const std::vector<OPCUA::Variant> &values) override;
    void handleEvent(const unsigned int &eventId, const std::map<std::string, OPCUA::Variant> &eventData) override;
    void handleStatusCallback(const int status) override;
    void handleVariableReadHisCallback(const std::string &nodePath, const std::vector<UA_DataValue> &datas, const bool moreData) override;
    void handleInvokeMethodCallback(const unsigned int &reqId, const std::vector<OPCUA::Variant> &outputs) override;

public:
    // pollInterval forwards to GenericClient's minSubscriptionInterval, which
    // gates the run-thread cadence (and thus the read/write latency floor).
    // Wrapper default is 100 ms; we default to 10 ms.
    UAClient(std::chrono::steady_clock::duration pollInterval = std::chrono::milliseconds(10));
    ~UAClient();
    void Start();
    void Stop();
    void setVariableValueUpdateCallback(ICallback *cb);
};
