// varmanager_test.cpp — OPC-UA concurrency integration tests
// Requires an OPC-UA server. Configure via user://varmanager_test.cfg.
// Run with: godot --test --test-suite=varmanager
//
// All TEST_CASEs are guarded by TOOLS_ENABLED (SCsub compile condition).

#ifdef TOOLS_ENABLED

#include "../varmanager.h"
#include "tests/test_macros.h"
#include <chrono>
#include <thread>
#include <future>
#include <atomic>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static VarManager::OpcTestConfig s_cfg;

static bool connect_for_test(VarManager *vm) {
	s_cfg = VarManager::loadTestConfig();
	auto sc = vm->connect_start(s_cfg.server_url, s_cfg.object_path, true);
	return sc == VarManager::UAStatusCode::ALL_OK ||
	       sc == VarManager::UAStatusCode::CONNECTED;
}

// ---------------------------------------------------------------------------
// T1: Concurrent read of the SAME variable — deduplication check
// ---------------------------------------------------------------------------
TEST_CASE("[varmanager] concurrent_read_same_var") {
	VarManager vm;
	if (!connect_for_test(&vm)) {
		MESSAGE("Skipped: OPC-UA server not reachable at ", s_cfg.server_url.utf8().get_data());
		return;
	}

	constexpr int N = 8;
	std::vector<std::future<Variant>> futures;
	futures.reserve(N);

	auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; i++) {
		futures.push_back(std::async(std::launch::async, [&vm]() {
			return vm.readValue(s_cfg.test_var_path);
		}));
	}
	for (auto &f : futures) f.get();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();

	CHECK(elapsed < (long long)(N * s_cfg.timeout_ms / 2));
	vm.connect_close();
}

// ---------------------------------------------------------------------------
// T2: Concurrent reads of DIFFERENT variables — batch dispatch check
// ---------------------------------------------------------------------------
TEST_CASE("[varmanager] concurrent_read_diff_vars") {
	VarManager vm;
	if (!connect_for_test(&vm) || s_cfg.batch_var_paths.is_empty()) {
		MESSAGE("Skipped: need batch_var_paths in test config");
		return;
	}

	std::vector<std::future<Variant>> futures;
	auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < s_cfg.batch_var_paths.size(); i++) {
		futures.push_back(std::async(std::launch::async, [&vm, i]() {
			return vm.readValue(s_cfg.batch_var_paths[i]);
		}));
	}
	for (auto &f : futures) f.get();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();

	CHECK(elapsed < (long long)(s_cfg.timeout_ms * 2));
	vm.connect_close();
}

// ---------------------------------------------------------------------------
// T3: Multi-thread sync read — thread safety
// ---------------------------------------------------------------------------
TEST_CASE("[varmanager] sync_read_thread_safety") {
	VarManager vm;
	if (!connect_for_test(&vm)) { MESSAGE("Skipped"); return; }
	Variant probe = vm.readValue(s_cfg.test_var_path);
	if (probe.get_type() == Variant::NIL) {
		MESSAGE("Skipped: var_path not found: ", s_cfg.test_var_path.utf8().get_data());
		vm.connect_close(); return;
	}

	constexpr int THREADS = 4, ITERS = 25;
	std::vector<std::thread> threads;
	std::atomic<int> errors{0};
	for (int t = 0; t < THREADS; t++) {
		threads.emplace_back([&vm, &errors]() {
			for (int i = 0; i < ITERS; i++) {
				Variant v = vm.readValue(s_cfg.test_var_path);
				if (v.get_type() == Variant::NIL) ++errors;
			}
		});
	}
	for (auto &t : threads) t.join();
	CHECK(errors == 0);
	vm.connect_close();
}

// ---------------------------------------------------------------------------
// T4: Write ordering — consecutive sync writes, verify final value
// ---------------------------------------------------------------------------
TEST_CASE("[varmanager] write_ordering") {
	VarManager vm;
	if (!connect_for_test(&vm)) { MESSAGE("Skipped"); return; }
	Variant probe = vm.readValue(s_cfg.test_var_path);
	if (probe.get_type() == Variant::NIL) {
		MESSAGE("Skipped: var_path not found: ", s_cfg.test_var_path.utf8().get_data());
		vm.connect_close(); return;
	}

	vm.writeValue(s_cfg.test_var_path, Variant(1));
	vm.writeValue(s_cfg.test_var_path, Variant(2));
	Variant v = vm.readValue(s_cfg.test_var_path);
	CHECK(int(v) == 2);
	vm.connect_close();
}

// ---------------------------------------------------------------------------
// T5: Timeout behavior — readValue without connecting
// ---------------------------------------------------------------------------
TEST_CASE("[varmanager] timeout_behavior") {
	VarManager vm;
	vm.setReadTimeoutMs(500);
	auto t0 = std::chrono::steady_clock::now();
	Variant v = vm.readValue(s_cfg.test_var_path);
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();
	CHECK(v.get_type() == Variant::NIL);
	CHECK(elapsed < 600);
}

// ---------------------------------------------------------------------------
// T6: Throughput benchmark — async vs sync
// ---------------------------------------------------------------------------
TEST_CASE("[varmanager] throughput_benchmark") {
	VarManager vm;
	if (!connect_for_test(&vm)) { MESSAGE("Skipped"); return; }
	Variant probe = vm.readValue(s_cfg.test_var_path);
	if (probe.get_type() == Variant::NIL) {
		MESSAGE("Skipped: var_path not found: ", s_cfg.test_var_path.utf8().get_data());
		vm.connect_close(); return;
	}
	constexpr int N = 100;

	auto t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; i++) vm.readValue(s_cfg.test_var_path);
	long long sync_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();

	t0 = std::chrono::steady_clock::now();
	for (int i = 0; i < N; i++) vm.readValueAsync(s_cfg.test_var_path);
	std::this_thread::sleep_for(std::chrono::milliseconds(s_cfg.timeout_ms));
	long long async_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - t0).count();

	MESSAGE("sync_ms=", sync_ms, " async_ms=", async_ms);
	CHECK(async_ms * 3 < sync_ms * 4);
	vm.connect_close();
}

// ---------------------------------------------------------------------------
// T7: invokeMethodAsync — stub (requires OPC-UA server with callable method)
// ---------------------------------------------------------------------------
TEST_CASE("[varmanager] invoke_method_async_concurrent") {
	MESSAGE("Skipped: requires OPC-UA server with callable method — fill in method path");
	// Expand when server is available:
	// vm.invokeMethodAsync("Objects/TestMethod", inputs, Array(), obj, "_on_done");
}

#endif // TOOLS_ENABLED
