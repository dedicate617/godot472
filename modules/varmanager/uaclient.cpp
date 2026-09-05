// uaclient.cpp
#include "uaclient.h"
#include "core/os/os.h"

UAClient::UAClient(std::chrono::steady_clock::duration pollInterval)
    : OPCUA::GenericClient(pollInterval), pcb(nullptr) {}
// Safety net: guarantee the run thread is joined before the std::thread member
// (and the underlying UA_Client) is destroyed. Without this, destroying the
// object while the run thread is still iterating crashes at process teardown.
UAClient::~UAClient() { Stop(); }

void UAClient::run(UAClient *self) {
    while (self->running_.load(std::memory_order_acquire)) {
        if (self->pcb) self->pcb->drainCommandQueue();
        self->run_once();
        if (self->pcb) self->pcb->expireInflight();
    }
}

void UAClient::handleVariableValueUpdate(const std::string &entityName, const OPCUA::Variant &value) {
    if (pcb) pcb->variableChanged(entityName, value);
}

void UAClient::handleVariableValueUpdates(const std::vector<std::string> &entityNames, const std::vector<OPCUA::Variant> &values) {
    if (pcb) pcb->variablesChanged(entityNames, values);
}

void UAClient::handleEvent(const unsigned int &eventId, const std::map<std::string, OPCUA::Variant> &eventData) {
    if (pcb) pcb->handleEvent(eventId, eventData);
}

void UAClient::handleStatusCallback(const int status) {
    if (pcb) pcb->handleStatusCallback(status);
}

void UAClient::handleVariableReadHisCallback(const std::string &nodePath, const std::vector<UA_DataValue> &datas, const bool moreData) {
    if (pcb) pcb->handleVariableReadHisCallback(nodePath, datas, moreData);
}

void UAClient::handleInvokeMethodCallback(const unsigned int &reqId, const std::vector<OPCUA::Variant> &outputs) {
    if (pcb) pcb->handleInvokeMethodCallback(reqId, outputs);
}

void UAClient::Start() {
    if (!running_.exchange(true)) {
        th_run = std::thread(run, this);
        OS::get_singleton()->print("UAClient thread id=(%llu)\n", (unsigned long long)std::hash<std::thread::id>{}(th_run.get_id()));
    }
}

void UAClient::Stop() {
    running_.store(false, std::memory_order_release);
    if (th_run.joinable()) th_run.join();
}

void UAClient::setVariableValueUpdateCallback(ICallback *cb) {
    // Must be called before Start(); no synchronization against the run loop.
    pcb = cb;
}
