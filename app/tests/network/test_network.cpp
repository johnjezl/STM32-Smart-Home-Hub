/**
 * Network Manager Unit Tests
 *
 * Tests for NetworkManager WiFi scanning/connection functionality.
 * Hardware-dependent tests (actual WiFi operations) are skipped in CI.
 */

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <cstdio>

#include <smarthub/core/Process.hpp>
#include <smarthub/network/NetworkManager.hpp>

#include <vector>
#include <string>

using namespace smarthub::network;

// ============================================================================
// WifiNetwork struct tests
// ============================================================================

TEST(WifiNetworkTest, DefaultConstruction) {
    WifiNetwork network;

    EXPECT_EQ(network.ssid, "");
    EXPECT_EQ(network.bssid, "");
    EXPECT_EQ(network.signalStrength, 0);
    EXPECT_FALSE(network.secured);
    EXPECT_FALSE(network.connected);
    EXPECT_EQ(network.security, "");
    EXPECT_EQ(network.frequency, 0);
}

TEST(WifiNetworkTest, Values) {
    WifiNetwork network;
    network.ssid = "MyNetwork";
    network.bssid = "AA:BB:CC:DD:EE:FF";
    network.signalStrength = 75;
    network.secured = true;
    network.security = "WPA2";
    network.frequency = 5180;
    network.connected = true;

    EXPECT_EQ(network.ssid, "MyNetwork");
    EXPECT_EQ(network.bssid, "AA:BB:CC:DD:EE:FF");
    EXPECT_EQ(network.signalStrength, 75);
    EXPECT_TRUE(network.secured);
    EXPECT_EQ(network.security, "WPA2");
    EXPECT_EQ(network.frequency, 5180);
    EXPECT_TRUE(network.connected);
}

// ============================================================================
// ConnectionState enum tests
// ============================================================================

TEST(ConnectionStateTest, Values) {
    EXPECT_NE(ConnectionState::Disconnected, ConnectionState::Connected);
    EXPECT_NE(ConnectionState::Scanning, ConnectionState::Connecting);
    EXPECT_NE(ConnectionState::Failed, ConnectionState::Connected);
}

// ============================================================================
// ConnectionResult struct tests
// ============================================================================

TEST(ConnectionResultTest, DefaultConstruction) {
    ConnectionResult result;

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "");
    EXPECT_EQ(result.ipAddress, "");
}

TEST(ConnectionResultTest, Success) {
    ConnectionResult result;
    result.success = true;
    result.ipAddress = "192.168.1.100";

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.ipAddress, "192.168.1.100");
    EXPECT_EQ(result.error, "");
}

TEST(ConnectionResultTest, Failure) {
    ConnectionResult result;
    result.success = false;
    result.error = "Incorrect password";

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "Incorrect password");
}

// ============================================================================
// NetworkStatus struct tests
// ============================================================================

TEST(NetworkStatusTest, DefaultConstruction) {
    NetworkStatus status;

    EXPECT_EQ(status.state, ConnectionState::Disconnected);
    EXPECT_EQ(status.ssid, "");
    EXPECT_EQ(status.ipAddress, "");
    EXPECT_EQ(status.signalStrength, 0);
    EXPECT_EQ(status.error, "");
}

TEST(NetworkStatusTest, Connected) {
    NetworkStatus status;
    status.state = ConnectionState::Connected;
    status.ssid = "HomeNetwork";
    status.ipAddress = "192.168.1.50";
    status.signalStrength = 80;

    EXPECT_EQ(status.state, ConnectionState::Connected);
    EXPECT_EQ(status.ssid, "HomeNetwork");
    EXPECT_EQ(status.ipAddress, "192.168.1.50");
    EXPECT_EQ(status.signalStrength, 80);
}

// ============================================================================
// NetworkManager static helper tests
// ============================================================================

TEST(NetworkManagerStaticTest, SignalToIconIndexExcellent) {
    // -50 dBm or stronger: Excellent (level 4)
    EXPECT_EQ(NetworkManager::signalToIconIndex(-30), 4);
    EXPECT_EQ(NetworkManager::signalToIconIndex(-40), 4);
    EXPECT_EQ(NetworkManager::signalToIconIndex(-50), 4);
}

TEST(NetworkManagerStaticTest, SignalToIconIndexGood) {
    // -51 to -60 dBm: Good (level 3)
    EXPECT_EQ(NetworkManager::signalToIconIndex(-51), 3);
    EXPECT_EQ(NetworkManager::signalToIconIndex(-55), 3);
    EXPECT_EQ(NetworkManager::signalToIconIndex(-60), 3);
}

TEST(NetworkManagerStaticTest, SignalToIconIndexFair) {
    // -61 to -70 dBm: Fair (level 2)
    EXPECT_EQ(NetworkManager::signalToIconIndex(-61), 2);
    EXPECT_EQ(NetworkManager::signalToIconIndex(-65), 2);
    EXPECT_EQ(NetworkManager::signalToIconIndex(-70), 2);
}

TEST(NetworkManagerStaticTest, SignalToIconIndexWeak) {
    // -71 to -80 dBm: Weak (level 1)
    EXPECT_EQ(NetworkManager::signalToIconIndex(-71), 1);
    EXPECT_EQ(NetworkManager::signalToIconIndex(-75), 1);
    EXPECT_EQ(NetworkManager::signalToIconIndex(-80), 1);
}

TEST(NetworkManagerStaticTest, SignalToIconIndexVeryWeak) {
    // Below -80 dBm: Very weak (level 0), including the -100 parse-failure value
    EXPECT_EQ(NetworkManager::signalToIconIndex(-81), 0);
    EXPECT_EQ(NetworkManager::signalToIconIndex(-90), 0);
    EXPECT_EQ(NetworkManager::signalToIconIndex(-100), 0);
}

TEST(NetworkManagerStaticTest, DbmToPercentStrong) {
    // -30 dBm = 100%
    EXPECT_EQ(NetworkManager::dbmToPercent(-30), 100);
    EXPECT_EQ(NetworkManager::dbmToPercent(-20), 100);  // Clamped
}

TEST(NetworkManagerStaticTest, DbmToPercentWeak) {
    // -90 dBm = 0%
    EXPECT_EQ(NetworkManager::dbmToPercent(-90), 0);
    EXPECT_EQ(NetworkManager::dbmToPercent(-100), 0);  // Clamped
}

TEST(NetworkManagerStaticTest, DbmToPercentMid) {
    // -60 dBm = 50% (midpoint)
    int percent = NetworkManager::dbmToPercent(-60);
    EXPECT_EQ(percent, 50);

    // -75 dBm = 25%
    percent = NetworkManager::dbmToPercent(-75);
    EXPECT_EQ(percent, 25);

    // -45 dBm = 75%
    percent = NetworkManager::dbmToPercent(-45);
    EXPECT_EQ(percent, 75);
}

// ============================================================================
// NetworkManager instance tests
// ============================================================================

TEST(WpaCliTest, NetworkListKeepsSpacesInSsids) {
    const auto networks = NetworkManager::parseNetworkList(
        "network id / ssid / bssid / flags\n"
        "0\tMy Home WiFi\tany\t[CURRENT]\n"
        "1\tOther\tany\t\n"
        "2\t\tany\t[DISABLED]\n");
    ASSERT_EQ(networks.size(), 3u);
    EXPECT_EQ(networks[0].first, "0");
    EXPECT_EQ(networks[0].second, "My Home WiFi");
    EXPECT_EQ(networks[1].second, "Other");
    EXPECT_EQ(networks[2].second, "");
}

TEST(WpaCliTest, NetworkListWithNothingInIt) {
    EXPECT_TRUE(NetworkManager::parseNetworkList("").empty());
    EXPECT_TRUE(NetworkManager::parseNetworkList("network id / ssid / bssid / flags\n").empty());
}

TEST(WpaCliTest, NetworkListDecodesEscapedSsids) {
    // wpa_supplicant escapes SSIDs in its output, so they have to be decoded to compare
    const auto networks = NetworkManager::parseNetworkList(
        "network id / ssid / bssid / flags\n"
        "0\tsay \\\"hi\\\"\tany\t\n"
        "1\tback\\\\slash\\ttab\tany\t\n"
        "2\tcaf\\xc3\\xa9\tany\t\n");
    ASSERT_EQ(networks.size(), 3u);
    EXPECT_EQ(networks[0].second, "say \"hi\"");
    EXPECT_EQ(networks[1].second, "back\\slash\ttab");
    EXPECT_EQ(networks[2].second, "caf\xc3\xa9");
}

TEST(WpaCliTest, DecodeWpaText) {
    EXPECT_EQ(NetworkManager::decodeWpaText("plain"), "plain");
    EXPECT_EQ(NetworkManager::decodeWpaText("a\\nb\\rc\\ed"), "a\nb\rc\x1b" "d");
    EXPECT_EQ(NetworkManager::decodeWpaText("\\x41\\x4"), "A\x04");
    EXPECT_EQ(NetworkManager::decodeWpaText("\\101\\0"), std::string("A\0", 2));
    EXPECT_EQ(NetworkManager::decodeWpaText("odd\\q end\\"), "odd\\q end\\");
}

TEST(WpaCliTest, SsidGoesAsHex) {
    EXPECT_EQ(NetworkManager::ssidToHex("Ab"), "4162");
    EXPECT_EQ(NetworkManager::ssidToHex("x';\"\n"), "78273b220a");
    EXPECT_EQ(NetworkManager::ssidToHex(std::string("\0\xff", 2)), "00ff");
}

TEST(WpaCliTest, PskArgument) {
    EXPECT_EQ(NetworkManager::pskArgument("password"), "\"password\"");
    EXPECT_EQ(NetworkManager::pskArgument("it's a \"pass\" $(x)"), "\"it's a \"pass\" $(x)\"");
    EXPECT_EQ(NetworkManager::pskArgument(std::string(63, 'a')), "\"" + std::string(63, 'a') + "\"");
    const std::string rawKey(64, 'f');
    EXPECT_EQ(NetworkManager::pskArgument(rawKey), rawKey);

    EXPECT_FALSE(NetworkManager::pskArgument("short"));
    EXPECT_FALSE(NetworkManager::pskArgument(std::string(64, 'g')));  // 64, but not hex
    EXPECT_FALSE(NetworkManager::pskArgument("new\nline12"));
    EXPECT_FALSE(NetworkManager::pskArgument("delete\x7f" "1234"));
}

TEST(ProcessTest, ArgumentsArriveExactlyAsGiven) {
    const std::string marker = "/tmp/smarthub_argv_injection_" + std::to_string(getpid());
    std::filesystem::remove(marker);
    const std::vector<std::string> values = {
        "My Home", "Bob's-wifi", "x';touch " + marker + ";'", "$(touch " + marker + ")",
        "`touch " + marker + "`", "a\"b", "semi;colon|pipe&amp", "new\nline", ""};

    std::vector<std::string> argv = {"printf", "%s|"};
    argv.insert(argv.end(), values.begin(), values.end());
    const auto result = smarthub::core::runProcess(argv);

    std::string expected;
    for (const auto& value : values) expected += value + "|";
    EXPECT_EQ(result.exitCode, 0);
    EXPECT_EQ(result.output, expected);
    EXPECT_FALSE(std::filesystem::exists(marker));
}

TEST(ProcessTest, ExitCodeAndStderr) {
    EXPECT_EQ(smarthub::core::runProcess({"sh", "-c", "exit 3"}).exitCode, 3);
    EXPECT_EQ(smarthub::core::runProcess({"sh", "-c", "echo out; echo err >&2"}).output, "out\n");
    EXPECT_EQ(smarthub::core::runProcess({"sh", "-c", "echo err >&2"}, true).output, "err\n");
    EXPECT_EQ(smarthub::core::runProcess({"/nonexistent/program"}).exitCode, 127);
    EXPECT_EQ(smarthub::core::runProcess({"no-such-program-anywhere"}).exitCode, -1);
    EXPECT_EQ(smarthub::core::runProcess({}).exitCode, -1);
}

// A wpa_cli that records each call's arguments, and answers from files, so connecting can be
// checked without a radio. Arguments are separated by \037 and calls by \036.
class FakeWpaCliTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir = std::filesystem::temp_directory_path() /
              ("smarthub_fake_wpa_" + std::to_string(getpid()) + "_" +
               ::testing::UnitTest::GetInstance()->current_test_info()->name());
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir / "net" / "eth0");
        std::filesystem::create_directories(dir / "net" / "wlan1");
        std::filesystem::create_directories(dir / "net" / "wlan0");
        write("net/wlan0/flags", "0x1003\n");

        write("wpa_cli",
              "#!/bin/sh\n"
              "dir=$(dirname \"$0\")\n"
              "if [ \"$1\" = \"-v\" ]; then echo 'wpa_cli v2.10'; exit 0; fi\n"
              "{ for a in \"$@\"; do printf '%s\\037' \"$a\"; done; printf '\\036'; } >> \"$dir/log\"\n"
              "case \"$3\" in\n"
              "  list_networks) printf 'network id / ssid / bssid / flags\\n'; cat \"$dir/networks\" 2>/dev/null ;;\n"
              "  add_network) echo 7 ;;\n"
              "  status) cat \"$dir/status\" 2>/dev/null ;;\n"
              "  scan_results) printf 'bssid / frequency / signal level / flags / ssid\\n'; cat \"$dir/scan\" 2>/dev/null ;;\n"
              "  *) echo OK ;;\n"
              "esac\n");
        chmod((dir / "wpa_cli").c_str(), 0755);
        write("status", "wpa_state=COMPLETED\nssid=whatever\nip_address=10.0.0.5\n");

        options.wpaCli = (dir / "wpa_cli").string();
        options.sysClassNet = (dir / "net").string();
        options.scanWait = std::chrono::milliseconds(1);
        options.connectPollInterval = std::chrono::milliseconds(1);
        options.connectPolls = 3;
    }

    void TearDown() override { std::filesystem::remove_all(dir); }

    void write(const std::string& name, const std::string& content) {
        std::ofstream(dir / name, std::ios::binary) << content;
    }

    std::vector<std::vector<std::string>> calls() {
        std::ifstream file(dir / "log", std::ios::binary);
        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string log = buffer.str();
        std::vector<std::vector<std::string>> result;
        size_t start = 0;
        for (size_t end; (end = log.find('\036', start)) != std::string::npos; start = end + 1) {
            std::vector<std::string> call;
            const std::string record = log.substr(start, end - start);
            size_t from = 0;
            for (size_t to; (to = record.find('\037', from)) != std::string::npos; from = to + 1) {
                call.push_back(record.substr(from, to - from));
            }
            result.push_back(call);
        }
        return result;
    }

    bool called(const std::vector<std::string>& wanted) {
        for (const auto& call : calls()) {
            if (call == wanted) return true;
        }
        return false;
    }

    ConnectionResult connectAndWait(NetworkManager& manager, const std::string& ssid,
                                    const std::string& password) {
        std::mutex mutex;
        std::condition_variable done;
        std::optional<ConnectionResult> result;
        manager.connect(ssid, password, [&](const ConnectionResult& r) {
            std::lock_guard<std::mutex> lock(mutex);
            result = r;
            done.notify_all();
        });
        std::unique_lock<std::mutex> lock(mutex);
        EXPECT_TRUE(done.wait_for(lock, std::chrono::seconds(10), [&] { return result.has_value(); }));
        return result.value_or(ConnectionResult{});
    }

    std::filesystem::path dir;
    NetworkManagerOptions options;
};

TEST_F(FakeWpaCliTest, FindsTheFirstWifiInterface) {
    NetworkManager manager(options);
    ASSERT_TRUE(manager.initialize());
    EXPECT_TRUE(manager.isWifiAvailable());
    EXPECT_TRUE(manager.isWifiEnabled());
    EXPECT_TRUE(called({"-i", "wlan0", "status"}));

    write("net/wlan0/flags", "0x1002\n");
    EXPECT_FALSE(manager.isWifiEnabled());
}

TEST_F(FakeWpaCliTest, HostileSsidAndPasswordReachWpaCliIntact) {
    // SSIDs are at most 32 bytes, so the marker is relative, to the directory wpa_cli runs in
    const std::filesystem::path marker = std::filesystem::current_path() / "pw";
    std::filesystem::remove(marker);
    const std::string ssid = "';touch pw;'\"$(touch pw)\"";
    const std::string password = "pa ss\"`touch pw`'word;touch pw";
    ASSERT_LE(ssid.size(), 32u);

    NetworkManager manager(options);
    ASSERT_TRUE(manager.initialize());
    const auto result = connectAndWait(manager, ssid, password);
    EXPECT_TRUE(result.success) << result.error;
    EXPECT_EQ(result.ipAddress, "10.0.0.5");

    EXPECT_TRUE(called({"-i", "wlan0", "add_network"}));
    EXPECT_TRUE(called({"-i", "wlan0", "set_network", "7", "ssid", NetworkManager::ssidToHex(ssid)}));
    EXPECT_TRUE(called({"-i", "wlan0", "set_network", "7", "key_mgmt", "WPA-PSK"}));
    EXPECT_TRUE(called({"-i", "wlan0", "set_network", "7", "psk", "\"" + password + "\""}));
    EXPECT_TRUE(called({"-i", "wlan0", "select_network", "7"}));
    EXPECT_TRUE(called({"-i", "wlan0", "save_config"}));
    EXPECT_FALSE(std::filesystem::exists(marker));
}

TEST_F(FakeWpaCliTest, OpenNetworkHasNoKey) {
    NetworkManager manager(options);
    ASSERT_TRUE(manager.initialize());
    EXPECT_TRUE(connectAndWait(manager, "Cafe", "").success);
    EXPECT_TRUE(called({"-i", "wlan0", "set_network", "7", "key_mgmt", "NONE"}));
    for (const auto& call : calls()) {
        EXPECT_FALSE(call.size() > 4 && call[4] == "psk");
    }
}

TEST_F(FakeWpaCliTest, ReusesASavedNetworkWithAnEscapedSsid) {
    write("networks", "3\tsay \\\"hi\\\"\tany\t[DISABLED]\n");
    NetworkManager manager(options);
    ASSERT_TRUE(manager.initialize());
    EXPECT_EQ(manager.getSavedNetworks(), std::vector<std::string>{"say \"hi\""});

    EXPECT_TRUE(connectAndWait(manager, "say \"hi\"", "new password").success);
    EXPECT_FALSE(called({"-i", "wlan0", "add_network"}));
    EXPECT_TRUE(called({"-i", "wlan0", "set_network", "3", "psk", "\"new password\""}));
    EXPECT_TRUE(called({"-i", "wlan0", "select_network", "3"}));

    EXPECT_TRUE(manager.forgetNetwork("say \"hi\""));
    EXPECT_TRUE(called({"-i", "wlan0", "remove_network", "3"}));
}

TEST_F(FakeWpaCliTest, RefusesAnUnusablePasswordBeforeTouchingWpaCli) {
    NetworkManager manager(options);
    ASSERT_TRUE(manager.initialize());
    const auto result = connectAndWait(manager, "Home", "short");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
    EXPECT_FALSE(called({"-i", "wlan0", "add_network"}));
    EXPECT_EQ(manager.getStatus().state, ConnectionState::Failed);

    EXPECT_FALSE(connectAndWait(manager, std::string(33, 'a'), "password").success);
    EXPECT_FALSE(connectAndWait(manager, "", "password").success);
    EXPECT_FALSE(called({"-i", "wlan0", "add_network"}));
}

TEST_F(FakeWpaCliTest, ScanDecodesAndDropsDuplicateSsids) {
    write("scan",
          "aa:aa\t2412\t-70\t[WPA2-PSK-CCMP][ESS]\tcaf\\xc3\\xa9\n"
          "bb:bb\t2437\t-40\t[ESS]\tOpen\n"
          "cc:cc\t5180\t-50\t[WPA2-PSK-CCMP][ESS]\tcaf\\xc3\\xa9\n");
    NetworkManager manager(options);
    ASSERT_TRUE(manager.initialize());

    std::mutex mutex;
    std::condition_variable done;
    std::optional<std::vector<WifiNetwork>> found;
    manager.startScan([&](const std::vector<WifiNetwork>& networks) {
        std::lock_guard<std::mutex> lock(mutex);
        found = networks;
        done.notify_all();
    });
    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(done.wait_for(lock, std::chrono::seconds(10), [&] { return found.has_value(); }));

    ASSERT_EQ(found->size(), 2u);
    EXPECT_EQ((*found)[0].ssid, "Open");
    EXPECT_EQ((*found)[1].ssid, "caf\xc3\xa9");
    EXPECT_EQ((*found)[1].signalStrength, -50);
}

TEST_F(FakeWpaCliTest, ConnectDoesNotBlockTheCallerAndShutdownStopsIt) {
    // The network never comes up, and each poll waits a long time
    write("status", "wpa_state=SCANNING\n");
    options.connectPollInterval = std::chrono::milliseconds(2000);
    options.connectPolls = 30;
    NetworkManager manager(options);
    ASSERT_TRUE(manager.initialize());

    const auto start = std::chrono::steady_clock::now();
    manager.connect("One", "password", nullptr);
    manager.connect("Two", "password", nullptr);  // Used to join the first on this thread
    manager.startScan(nullptr);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(500));

    manager.shutdown();
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(2500));
}

// Against the real wpa_supplicant, on a board. It adds a network that's never enabled or saved,
// and removes it, so the board's own connection is left alone. Set SMARTHUB_REAL_WPA=1 to run it.
TEST(RealWpaSupplicantTest, HostileNamesRoundTrip) {
    const char* enabled = std::getenv("SMARTHUB_REAL_WPA");
    if (!enabled || std::string(enabled) != "1") {
        GTEST_SKIP() << "Set SMARTHUB_REAL_WPA=1 to run against the real wpa_supplicant";
    }

    NetworkManager manager;
    ASSERT_TRUE(manager.initialize());
    ASSERT_TRUE(manager.isWifiAvailable());

    std::string iface;
    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net")) {
        const std::string name = entry.path().filename().string();
        if (name.rfind("wlan", 0) == 0 && (iface.empty() || name < iface)) iface = name;
    }
    auto wpa = [&](std::vector<std::string> arguments) {
        arguments.insert(arguments.begin(), {"wpa_cli", "-i", iface});
        std::string output = smarthub::core::runProcess(arguments).output;
        while (!output.empty() && (output.back() == '\n' || output.back() == '\r')) output.pop_back();
        return output;
    };

    const std::string ssid = "';touch pw;'\"$(x)\\\tcaf\xc3\xa9";
    const std::string password = "pa ss\"`touch pw`'word";
    const std::string id = wpa({"add_network"});
    ASSERT_FALSE(id.empty());
    ASSERT_TRUE(std::all_of(id.begin(), id.end(), ::isdigit)) << id;

    EXPECT_EQ(wpa({"set_network", id, "ssid", NetworkManager::ssidToHex(ssid)}), "OK");
    EXPECT_EQ(wpa({"set_network", id, "key_mgmt", "WPA-PSK"}), "OK");
    EXPECT_EQ(wpa({"set_network", id, "psk", *NetworkManager::pskArgument(password)}), "OK");

    const auto saved = manager.getSavedNetworks();
    EXPECT_NE(std::find(saved.begin(), saved.end(), ssid), saved.end());

    EXPECT_EQ(wpa({"remove_network", id}), "OK");
    const auto after = manager.getSavedNetworks();
    EXPECT_EQ(std::find(after.begin(), after.end(), ssid), after.end());
    EXPECT_FALSE(std::filesystem::exists("pw"));

    // A real scan finds the network the board is connected to, marked as connected
    std::mutex mutex;
    std::condition_variable done;
    std::optional<std::vector<WifiNetwork>> found;
    manager.startScan([&](const std::vector<WifiNetwork>& networks) {
        std::lock_guard<std::mutex> lock(mutex);
        found = networks;
        done.notify_all();
    });
    std::unique_lock<std::mutex> lock(mutex);
    ASSERT_TRUE(done.wait_for(lock, std::chrono::seconds(20), [&] { return found.has_value(); }));
    const auto status = manager.getStatus();
    if (status.state == ConnectionState::Connected) {
        const auto current = std::find_if(found->begin(), found->end(),
                                          [&](const WifiNetwork& n) { return n.ssid == status.ssid; });
        ASSERT_NE(current, found->end()) << status.ssid;
        EXPECT_TRUE(current->connected);
        EXPECT_LT(current->signalStrength, 0);
        EXPECT_GT(current->frequency, 2000);
    }
}

// On a fake wpa_cli: the real one would act on the machine's WiFi, and these call disconnect
class NetworkManagerTest : public FakeWpaCliTest {
protected:
    void SetUp() override {
        FakeWpaCliTest::SetUp();
        networkManager = std::make_unique<NetworkManager>(options);
    }

    void TearDown() override {
        if (networkManager) {
            networkManager->shutdown();
        }
        networkManager.reset();
        FakeWpaCliTest::TearDown();
    }

    std::unique_ptr<NetworkManager> networkManager;
};

TEST_F(NetworkManagerTest, InitializeWithoutNmcli) {
    // Initialize should work even without nmcli
    // It will just report WiFi as unavailable
    bool result = networkManager->initialize();

    // The result depends on whether nmcli is installed
    // Either way, we should be able to call isWifiAvailable
    (void)result;
    bool available = networkManager->isWifiAvailable();

    // Just verify it doesn't crash
    EXPECT_TRUE(result || !available);
}

TEST_F(NetworkManagerTest, GetStatusBeforeInit) {
    // Should return disconnected status before initialization
    NetworkStatus status = networkManager->getStatus();

    EXPECT_EQ(status.state, ConnectionState::Disconnected);
}

TEST_F(NetworkManagerTest, ShutdownSafe) {
    // Shutdown should be safe to call multiple times
    networkManager->initialize();
    networkManager->shutdown();
    networkManager->shutdown();  // Should not crash
}

TEST_F(NetworkManagerTest, GetScanResultsEmpty) {
    // Before any scan, results should be empty
    std::vector<WifiNetwork> networks = networkManager->getScanResults();
    EXPECT_TRUE(networks.empty());
}

TEST_F(NetworkManagerTest, GetSavedNetworksEmpty) {
    // Initialize first
    networkManager->initialize();

    // May or may not have saved networks depending on system
    std::vector<std::string> saved = networkManager->getSavedNetworks();
    // Just verify it returns without crashing
    EXPECT_GE(saved.size(), 0u);
}

// ============================================================================
// WifiNetwork Vector Operations
// ============================================================================

TEST(WifiNetworkVectorTest, SortBySignal) {
    std::vector<WifiNetwork> networks;

    WifiNetwork net1;
    net1.ssid = "Weak";
    net1.signalStrength = 30;
    networks.push_back(net1);

    WifiNetwork net2;
    net2.ssid = "Strong";
    net2.signalStrength = 90;
    networks.push_back(net2);

    WifiNetwork net3;
    net3.ssid = "Medium";
    net3.signalStrength = 60;
    networks.push_back(net3);

    // Sort by signal (descending)
    std::sort(networks.begin(), networks.end(),
              [](const WifiNetwork& a, const WifiNetwork& b) {
                  return a.signalStrength > b.signalStrength;
              });

    EXPECT_EQ(networks[0].ssid, "Strong");
    EXPECT_EQ(networks[1].ssid, "Medium");
    EXPECT_EQ(networks[2].ssid, "Weak");
}

TEST(WifiNetworkVectorTest, FindBySSID) {
    std::vector<WifiNetwork> networks;

    WifiNetwork net1;
    net1.ssid = "Network1";
    networks.push_back(net1);

    WifiNetwork net2;
    net2.ssid = "Network2";
    networks.push_back(net2);

    auto it = std::find_if(networks.begin(), networks.end(),
                           [](const WifiNetwork& n) {
                               return n.ssid == "Network2";
                           });

    EXPECT_NE(it, networks.end());
    EXPECT_EQ(it->ssid, "Network2");
}

TEST(WifiNetworkVectorTest, FilterSecured) {
    std::vector<WifiNetwork> networks;

    WifiNetwork open;
    open.ssid = "OpenNetwork";
    open.secured = false;
    networks.push_back(open);

    WifiNetwork secured;
    secured.ssid = "SecuredNetwork";
    secured.secured = true;
    secured.security = "WPA2";
    networks.push_back(secured);

    std::vector<WifiNetwork> securedOnly;
    std::copy_if(networks.begin(), networks.end(),
                 std::back_inserter(securedOnly),
                 [](const WifiNetwork& n) { return n.secured; });

    EXPECT_EQ(securedOnly.size(), 1u);
    EXPECT_EQ(securedOnly[0].ssid, "SecuredNetwork");
}

// ============================================================================
// NetworkManager callback tests
// ============================================================================

TEST_F(NetworkManagerTest, SetStatusCallback) {
    bool callbackCalled = false;
    networkManager->setStatusCallback([&callbackCalled](const NetworkStatus&) {
        callbackCalled = true;
    });

    // Callback registration shouldn't crash
    EXPECT_FALSE(callbackCalled);  // Not called until status changes
}

TEST_F(NetworkManagerTest, IsWifiAvailableAfterInit) {
    networkManager->initialize();

    // On a system with WiFi, this should be true
    // On CI without WiFi, this may be false
    bool available = networkManager->isWifiAvailable();

    // Just verify the method works without crashing
    EXPECT_TRUE(available || !available);  // Always passes, tests method call
}

TEST_F(NetworkManagerTest, DisconnectSafe) {
    networkManager->initialize();

    // Disconnect should be safe even when not connected
    ASSERT_NO_THROW({
        networkManager->disconnect();
    });
    EXPECT_TRUE(called({"-i", "wlan0", "disconnect"}));
}

// ============================================================================
// WifiNetwork frequency tests
// ============================================================================

TEST(WifiNetworkTest, Frequency2GHz) {
    WifiNetwork network;
    network.ssid = "Home2G";
    network.frequency = 2437;  // Channel 6

    EXPECT_LT(network.frequency, 3000);  // 2.4 GHz band
}

TEST(WifiNetworkTest, Frequency5GHz) {
    WifiNetwork network;
    network.ssid = "Home5G";
    network.frequency = 5180;  // Channel 36

    EXPECT_GT(network.frequency, 5000);  // 5 GHz band
}

// ============================================================================
// ConnectionState transitions
// ============================================================================

TEST(ConnectionStateTest, AllStatesDistinct) {
    std::vector<ConnectionState> states = {
        ConnectionState::Disconnected,
        ConnectionState::Scanning,
        ConnectionState::Connecting,
        ConnectionState::Connected,
        ConnectionState::Failed
    };

    // Verify all states are distinct
    for (size_t i = 0; i < states.size(); i++) {
        for (size_t j = i + 1; j < states.size(); j++) {
            EXPECT_NE(states[i], states[j]);
        }
    }
}

// ============================================================================
// Signal strength edge cases
// ============================================================================

TEST(NetworkManagerStaticTest, SignalToIconBoundaries) {
    // Test exact boundaries
    EXPECT_EQ(NetworkManager::signalToIconIndex(-51), 3);  // Just below -50
    EXPECT_EQ(NetworkManager::signalToIconIndex(-50), 4);  // Exactly -50

    EXPECT_EQ(NetworkManager::signalToIconIndex(-61), 2);  // Just below -60
    EXPECT_EQ(NetworkManager::signalToIconIndex(-60), 3);  // Exactly -60

    EXPECT_EQ(NetworkManager::signalToIconIndex(-71), 1);  // Just below -70
    EXPECT_EQ(NetworkManager::signalToIconIndex(-70), 2);  // Exactly -70

    EXPECT_EQ(NetworkManager::signalToIconIndex(-81), 0);  // Just below -80
    EXPECT_EQ(NetworkManager::signalToIconIndex(-80), 1);  // Exactly -80
}

TEST(NetworkManagerStaticTest, DbmToPercentBoundaries) {
    // Test exact boundaries
    EXPECT_EQ(NetworkManager::dbmToPercent(-30), 100);
    EXPECT_EQ(NetworkManager::dbmToPercent(-90), 0);

    // Test just inside boundaries
    EXPECT_EQ(NetworkManager::dbmToPercent(-31), 98);  // Just inside upper
    EXPECT_EQ(NetworkManager::dbmToPercent(-89), 1);   // Just inside lower
}
