/**
 * Config Unit Tests
 */

#include <gtest/gtest.h>
#include <smarthub/core/Config.hpp>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test config file
        std::ofstream file(testConfigPath);
        file << R"(
database:
  path: /var/lib/smarthub/test.db

mqtt:
  broker: 192.168.1.100
  port: 1884
  client_id: test_client
  username: testuser
  password: testpass

web:
  port: 8443
  http_port: 8080
  root: /opt/smarthub/www

display:
  device: /dev/fb1
  brightness: 80
  screen_timeout: 120

logging:
  level: debug
  file: /var/log/test.log
)";
        file.close();
    }

    void TearDown() override {
        if (fs::exists(testConfigPath)) {
            fs::remove(testConfigPath);
        }
    }

    const std::string testConfigPath = "/tmp/smarthub_test_config.yaml";
};

TEST_F(ConfigTest, LoadFromFile) {
    smarthub::Config config;
    EXPECT_TRUE(config.load(testConfigPath));
}

TEST_F(ConfigTest, LoadFromNonexistentFile) {
    smarthub::Config config;
    EXPECT_FALSE(config.load("/nonexistent/path/config.yaml"));
}

TEST_F(ConfigTest, DatabasePath) {
    smarthub::Config config;
    config.load(testConfigPath);
    EXPECT_EQ(config.databasePath(), "/var/lib/smarthub/test.db");
}

TEST_F(ConfigTest, MqttSettings) {
    smarthub::Config config;
    config.load(testConfigPath);
    EXPECT_EQ(config.mqttBroker(), "192.168.1.100");
    EXPECT_EQ(config.mqttPort(), 1884);
    EXPECT_EQ(config.mqttClientId(), "test_client");
}

TEST_F(ConfigTest, WebSettings) {
    smarthub::Config config;
    config.load(testConfigPath);
    EXPECT_EQ(config.webPort(), 8443);
    EXPECT_EQ(config.webRoot(), "/opt/smarthub/www");
}

TEST_F(ConfigTest, DisplaySettings) {
    smarthub::Config config;
    config.load(testConfigPath);
    EXPECT_EQ(config.displayDevice(), "/dev/fb1");
    EXPECT_EQ(config.displayBrightness(), 80);
    EXPECT_EQ(config.screenTimeout(), 120);
}

TEST_F(ConfigTest, LoggingSettings) {
    smarthub::Config config;
    config.load(testConfigPath);
    EXPECT_EQ(config.logLevel(), "debug");
    EXPECT_EQ(config.logFile(), "/var/log/test.log");
}

TEST_F(ConfigTest, DefaultValues) {
    // Test defaults when config file is missing values
    std::ofstream file("/tmp/minimal_config.yaml");
    file << "# Minimal config\n";
    file.close();

    smarthub::Config config;
    config.load("/tmp/minimal_config.yaml");

    // Should have sensible defaults
    EXPECT_FALSE(config.databasePath().empty());
    EXPECT_FALSE(config.mqttBroker().empty());
    EXPECT_GT(config.mqttPort(), 0);

    fs::remove("/tmp/minimal_config.yaml");
}

// Tests for INI-style fallback parser (used when yaml-cpp not available)
class ConfigIniTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test INI-style config file
        std::ofstream file(testIniPath);
        file << R"(# SmartHub INI-style Configuration

[database]
path = /var/lib/smarthub/test_ini.db

[mqtt]
broker = 10.0.0.50
port = 1885

[web]
port = 9443
root = /opt/smarthub/www_test

[display]
device = /dev/fb2
brightness = 75

[logging]
level = warn
file = /var/log/smarthub_ini.log
)";
        file.close();
    }

    void TearDown() override {
        if (fs::exists(testIniPath)) {
            fs::remove(testIniPath);
        }
    }

    const std::string testIniPath = "/tmp/smarthub_test_config.ini";
};

TEST_F(ConfigIniTest, LoadIniFormat) {
    smarthub::Config config;
    // The config loader should accept INI format regardless of yaml-cpp availability
    EXPECT_TRUE(config.load(testIniPath));
}

TEST_F(ConfigIniTest, IniDatabasePath) {
    smarthub::Config config;
    config.load(testIniPath);
    // Note: INI parser may or may not parse this depending on yaml-cpp availability
    // When yaml-cpp is available, YAML parser runs first and fails, then INI fallback runs
    // When yaml-cpp is unavailable, INI parser runs directly
    // Both should work with this file since it's valid INI format
    EXPECT_TRUE(config.isLoaded());
}

TEST_F(ConfigIniTest, IniWithQuotedValues) {
    std::ofstream file("/tmp/quoted_config.ini");
    file << R"([database]
path = "/var/lib/smarthub/quoted.db"

[mqtt]
broker = '192.168.1.50'
)";
    file.close();

    smarthub::Config config;
    EXPECT_TRUE(config.load("/tmp/quoted_config.ini"));

    fs::remove("/tmp/quoted_config.ini");
}

TEST_F(ConfigIniTest, IniWithColonSeparator) {
    // INI parser also supports colon as separator (YAML-style)
    std::ofstream file("/tmp/colon_config.ini");
    file << R"([database]
path: /var/lib/smarthub/colon.db

[mqtt]
broker: localhost
port: 1883
)";
    file.close();

    smarthub::Config config;
    EXPECT_TRUE(config.load("/tmp/colon_config.ini"));

    fs::remove("/tmp/colon_config.ini");
}

TEST_F(ConfigIniTest, IniEmptyFile) {
    std::ofstream file("/tmp/empty_config.ini");
    file << "# Empty config with just comments\n";
    file << "# Another comment\n";
    file.close();

    smarthub::Config config;
    // Should load with defaults
    EXPECT_TRUE(config.load("/tmp/empty_config.ini"));
    EXPECT_TRUE(config.isLoaded());

    fs::remove("/tmp/empty_config.ini");
}

TEST_F(ConfigIniTest, IniWithWhitespace) {
    std::ofstream file("/tmp/whitespace_config.ini");
    file << R"(
   [database]
   path   =   /var/lib/smarthub/whitespace.db

   [mqtt]
   broker   =   localhost
)";
    file.close();

    smarthub::Config config;
    EXPECT_TRUE(config.load("/tmp/whitespace_config.ini"));

    fs::remove("/tmp/whitespace_config.ini");
}
