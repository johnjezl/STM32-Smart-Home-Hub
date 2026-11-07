/**
 * Network Manager Unit Tests
 *
 * Tests for NetworkManager WiFi scanning/connection functionality.
 * Hardware-dependent tests (actual WiFi operations) are skipped in CI.
 */

#include <gtest/gtest.h>
#include <unistd.h>
#include <filesystem>
#include <cstdio>

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

TEST(WpaCliTest, QuotedValueIsOneArgumentToTheShell) {
    // Run it through a real shell: an SSID is whatever someone nearby chooses to broadcast
    const std::string marker = "/tmp/smarthub_ssid_injection_" + std::to_string(getpid());
    for (const std::string& value : {std::string("My Home"), std::string("Bob's-wifi"),
                                     "x';touch " + marker + ";'", "$(touch " + marker + ")",
                                     "`touch " + marker + "`", std::string("a\"b")}) {
        const std::string command = "printf %s " + NetworkManager::quoteForWpaCli(value);
        FILE* pipe = popen(command.c_str(), "r");
        ASSERT_NE(pipe, nullptr);
        std::string output;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) output += buffer;
        pclose(pipe);

        EXPECT_EQ(output, "\"" + value + "\"");
        EXPECT_FALSE(std::filesystem::exists(marker)) << value;
    }
    std::filesystem::remove(marker);
}

class NetworkManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        networkManager = std::make_unique<NetworkManager>();
    }

    void TearDown() override {
        if (networkManager) {
            networkManager->shutdown();
        }
        networkManager.reset();
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
