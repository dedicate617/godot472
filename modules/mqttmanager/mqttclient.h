//============================================================================
// Name        : mqttclient.cpp
// Author      : Bosmutus
// Version     :
// Copyright   : copyright by Bosmutus
// Description : paho mqtt client implement
//============================================================================

#include <iostream>
#include <cstdlib>
#include <string>
#include <thread>	// For sleep
#include <atomic>
#include <chrono>
#include <cstring>
#include "mqtt/async_client.h"
#include "mqtt/client.h"
#include "mqtt/create_options.h"
#include "mqtt/iclient_persistence.h"
#include "ICallback.h"

using namespace std;
using namespace std::chrono;
using namespace mqtt;

const uint16_t CONNECTION_TM = 1100;
const uint16_t CMD_EXE_TM = 1100;
const int MAX_BUFFERED_MESSAGES = 120;
const uint16_t N_RETRY_ATTEMPTS = 3;

const std::string GW2SERVER_HEARTBEAT_TOPIC = "Status/%s"; // 心跳包
const auto TOPICS = mqtt::string_collection::create({ "req/#", "cmd" });
const vector<int> VEC_QOS{ 1, 1 };
constexpr auto RESPONSE_TOPIC = mqtt::property::RESPONSE_TOPIC;
constexpr auto CORRELATION_DATA = mqtt::property::CORRELATION_DATA;
const std::string KEY_STORE{ "mqtt_client.pem" };
const std::string TRUST_STORE{ "mqtt_root_ca.crt" };

const std::string DEFAULT_HOST{ "127.0.0.1" };
const uint16_t DEFAULT_PORT{ 1883 };

// std::string的字符串格式化函数
template <typename... Args>
static std::string str_format(const std::string &format, Args... args) {
	auto size_buf = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
	std::unique_ptr<char[]> buf(new (std::nothrow) char[size_buf]);

	if (!buf)
		return std::string("");

	std::snprintf(buf.get(), size_buf, format.c_str(), args...);
	return std::string(buf.get(), buf.get() + size_buf - 1);
}

// std::wstring的字符串格式化函数
template <typename... Args>
static std::wstring wstr_format(const std::wstring &format, Args... args) {
	auto size_buf = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
	std::unique_ptr<char[]> buf(new (std::nothrow) char[size_buf]);

	if (!buf)
		return std::wstring("");

	std::snprintf(buf.get(), size_buf, format.c_str(), args...);
	return std::wstring(buf.get(), buf.get() + size_buf - 1);
}

static uint64_t timestamp() {
	auto now = system_clock::now();
	auto tse = now.time_since_epoch();
	auto msTm = duration_cast<milliseconds>(tse);
	return uint64_t(msTm.count());
}
// At some point, when the library gets updated to C++17, we can use
// std::filesystem to make a portable version of this.

#if !defined(_WIN32)

	#include <sys/stat.h>
	#include <sys/types.h>
	#include <dirent.h>
	#include <unistd.h>
	#include <fstream>

// Example of user-based file persistence with a simple XOR encoding scheme.
//
// Similar to the built-in file persistence, this just creates a
// subdirectory for the persistence data, then places each key into a
// separate file using the key as the file name.
//
// With user-defined persistence, you can transform the data in any way you
// like, such as with encryption/decryption, and you can store the data any
// place you want, such as here with disk files, or use a local DB like
// SQLite or a local key/value store like Redis.

// todo: use mmkv to store the persistence msg
class encoded_file_persistence : virtual public mqtt::iclient_persistence {
	// The name of the store
	// Used as the directory name
	string name_;

	// A key for encoding the data
	string encodeKey_;

	// Simple, in-place XOR encoding and decoding
	void encode(string &s) const {
		size_t n = encodeKey_.size();
		if (n == 0 || s.empty())
			return;

		for (size_t i = 0; i < s.size(); ++i)
			s[i] ^= encodeKey_[i % n];
	}

	// Gets the persistence file name for the supplied key.
	string path_name(const string &key) const { return name_ + "/" + key; }

public:
	// Create the persistence object with the specified encoding key
	encoded_file_persistence(const string &encodeKey) :
			encodeKey_(encodeKey) {}

	// "Open" the persistence store.
	// Create a directory for persistence files, using the client ID and
	// serverURI to make a unique directory name. Note that neither can be
	// empty. In particular, the app can't use an empty `clientID` if it
	// wants to use persistence. (This isn't an absolute rule for your own
	// persistence, but you do need a way to keep data from different apps
	// separate).
	void open(const string &clientId, const string &serverURI) override {
		if (clientId.empty() || serverURI.empty())
			throw mqtt::persistence_exception();

		name_ = serverURI + "-" + clientId;
		std::replace(name_.begin(), name_.end(), ':', '-');

		mkdir(name_.c_str(), S_IRWXU | S_IRWXG);
	}

	// Close the persistent store that was previously opened.
	// Remove the persistence directory, if it's empty.
	void close() override {
		rmdir(name_.c_str());
	}

	// Clears persistence, so that it no longer contains any persisted data.
	// Just remove all the files from the persistence directory.
	void clear() override {
		DIR *dir = opendir(name_.c_str());
		if (!dir)
			return;

		dirent *next;
		while ((next = readdir(dir)) != nullptr) {
			auto fname = string(next->d_name);
			if (fname == "." || fname == "..")
				continue;
			string path = name_ + "/" + fname;
			remove(path.c_str());
		}
		closedir(dir);
	}

	// Returns whether or not data is persisted using the specified key.
	// We just look for a file in the store directory with the same name as
	// the key.
	bool contains_key(const string &key) override {
		DIR *dir = opendir(name_.c_str());
		if (!dir)
			return false;

		dirent *next;
		while ((next = readdir(dir)) != nullptr) {
			if (string(next->d_name) == key) {
				closedir(dir);
				return true;
			}
		}
		closedir(dir);
		return false;
	}

	// Returns the keys in this persistent data store.
	// We just make a collection of the file names in the store directory.
	mqtt::string_collection keys() const override {
		mqtt::string_collection ks;
		DIR *dir = opendir(name_.c_str());
		if (!dir)
			return ks;

		dirent *next;
		while ((next = readdir(dir)) != nullptr) {
			auto fname = string(next->d_name);
			if (fname == "." || fname == "..")
				continue;
			ks.push_back(fname);
		}

		closedir(dir);
		return ks;
	}

	// Puts the specified data into the persistent store.
	// We just encode the data and write it to a file using the key as the
	// name of the file. The multiple buffers given here need to be written
	// in order - and a scatter/gather like writev() would be fine. But...
	// the data will be read back as a single buffer, so here we first
	// concat a string so that the encoding key lines up with the data the
	// same way it will on the read-back.
	void put(const string &key, const std::vector<mqtt::string_view> &bufs) override {
		auto path = path_name(key);

		ofstream os(path, ios_base::binary);
		if (!os)
			throw mqtt::persistence_exception();

		string s;
		for (const auto &b : bufs)
			s.append(b.data(), b.size());

		encode(s);
		os.write(s.data(), s.size());
	}

	// Gets the specified data out of the persistent store.
	// We look for a file with the name of the key, read the contents,
	// decode, and return it.
	string get(const string &key) const override {
		auto path = path_name(key);

		ifstream is(path, ios_base::ate | ios_base::binary);
		if (!is)
			throw mqtt::persistence_exception();

		// Read the whole file into a string
		streamsize sz = is.tellg();
		if (sz == 0)
			return string();

		is.seekg(0);
		string s(sz, '\0');
		is.read(&s[0], sz);
		if (is.gcount() < sz)
			s.resize(is.gcount());

		encode(s);
		return s;
	}

	// Remove the data for the specified key.
	// Just remove the file with the same name as the key, if found.
	void remove(const string &key) override {
		auto path = path_name(key);
		::remove(path.c_str());
	}
};
#endif

// Callbacks for the success or failures of requested actions.
// This could be used to initiate further action, but here we just log the
// results to the console.

class mqtt_action_listener : public virtual mqtt::iaction_listener {
	std::string name_;

	void on_failure(const mqtt::token &tok) override {
		std::cout << name_ << " failure";
		if (tok.get_message_id() != 0)
			std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
		std::cout << std::endl;
	}

	void on_success(const mqtt::token &tok) override {
		std::cout << name_ << " success";
		if (tok.get_message_id() != 0)
			std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
		auto top = tok.get_topics();
		if (top && !top->empty())
			std::cout << "\ttoken topic: '" << (*top)[0] << "', ..." << std::endl;
		std::cout << std::endl;
	}

public:
	mqtt_action_listener(const std::string &name) :
			name_(name) {}
};

/////////////////////////////////////////////////////////////////////////////
/**
 * Local callback & listener class for use with the client connection.
 * This is primarily intended to receive messages, but it will also monitor
 * the connection to the broker. If the connection is lost, it will attempt
 * to restore the connection and re-subscribe to the topic.
 */
class mqttConnectCallback : public virtual mqtt::callback,
	public virtual mqtt::iaction_listener

{
	// Counter for the number of connection retries
	int nretry_;
	// The MQTT client
	mqtt::async_client &cli_;
	// Options to use if we need to reconnect
	mqtt::connect_options &connOpts_;
	// An action listener to display the result of actions.
	mqtt_action_listener subListener_;

	// This deomonstrates manually reconnecting to the broker by calling
	// connect() again. This is a possibility for an application that keeps
	// a copy of it's original connect_options, or if the app wants to
	// reconnect with different options.
	// Another way this can be done manually, if using the same options, is
	// to just call the async_client::reconnect() method.
	void reconnect() {
		std::this_thread::sleep_for(std::chrono::milliseconds(2500));
		try {
			cli_.connect(connOpts_, nullptr, *this);
		} catch (const mqtt::exception &exc) {
			std::cerr << "Error: " << exc.what() << std::endl;
			exit(1);
		}
	}

	// Re-connection failure
	void on_failure(const mqtt::token &tok) override {
		std::cout << "Connection attempt failed" << std::endl;
		if (++nretry_ > N_RETRY_ATTEMPTS)
			exit(1);
		reconnect();
	}

	// (Re)connection success
	// Either this or connected() can be used for callbacks.
	void on_success(const mqtt::token &tok) override {}

	// (Re)connection success
	void connected(const std::string &cause) override {
		std::cout << "\nConnection success" << std::endl;
		// TODO: resubscribe to mqtt broker when reconnected
		//		std::cout << "\nSubscribing to topic '" << TOPIC << "'\n"
		//			<< "\tfor client " << CLIENT_ID
		//			<< " using QoS" << QOS << "\n"
		//			<< "\nPress Q<Enter> to quit\n" << std::endl;

		//		cli_.subscribe(TOPIC, QOS, nullptr, subListener_);
	}

	// Callback for when the connection is lost.
	// This will initiate the attempt to manually reconnect.
	void connection_lost(const std::string &cause) override {
		std::cout << "\nConnection lost" << std::endl;
		if (!cause.empty())
			std::cout << "\tcause: " << cause << std::endl;

		std::cout << "Reconnecting..." << std::endl;
		nretry_ = 0;
		reconnect();
	}

	// Callback for when a message arrives.
	void message_arrived(mqtt::const_message_ptr msg) override {
		std::cout << "Message arrived" << std::endl;
		std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
		std::cout << "\tpayload: '" << msg->to_string() << "'\n"
				  << std::endl;
	}

	void delivery_complete(mqtt::delivery_token_ptr token) override {}

public:
	mqttConnectCallback(mqtt::async_client &cli, mqtt::connect_options &connOpts) :
			nretry_(0), cli_(cli), connOpts_(connOpts), subListener_("Subscription") {}
};

class MqttClient : public mqtt::async_client {
private:
	ICallback *pcb;

protected:
	/*void handleVariableValueUpdate(const std::string &entityName, const OPCUA::Variant &value) override;

	void handleVariableValueUpdates(const std::vector<std::string> &entityNames, const std::vector<OPCUA::Variant> &values) override;

	void handleEvent(const unsigned int &eventId, const std::map<std::string, OPCUA::Variant> &eventData) override;

	void handleStatusCallback(const int status) override;

	void handleVariableReadHisCallback(const std::string &nodePath, const std::vector<UA_DataValue> &datas, const bool moreData) override;

	void handleInvokeMethodCallback(const unsigned int &reqId, const std::vector<OPCUA::Variant> &outputs) override;*/

public:
	MqttClient();
	MqttClient(const mqtt::create_options& opts,
                 const std::string& host = DEFAULT_HOST,
                 const uint16_t port = DEFAULT_PORT,
                 const std::string& clientId = "",
				 mqtt::iclient_persistence *persistence = nullptr
	);
	~MqttClient();

	void setCallback(ICallback* cb);

	bool isConnectedToHost();
	bool disconnectFromHost();

	std::string hostname() const;
	uint16_t port() const;
	std::string clientId() const;

private:
	std::string m_host;
	uint16_t m_port;
	std::string m_clientId;

};
