/**
 * Runs the real smarthub program and stops it with signals, as init does on the board.
 * Sanitizer builds report problems through the exit code, which these tests check.
 */

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string shellQuote(const std::string& text) {
    std::string quoted = "'";
    for (const char ch : text) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    return quoted + "'";
}

std::string runCommand(const std::string& command) {
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return output;
    }
    std::array<char, 4096> buffer{};
    size_t count = 0;
    while ((count = fread(buffer.data(), 1, buffer.size(), pipe)) > 0) {
        output.append(buffer.data(), count);
    }
    pclose(pipe);
    return output;
}

class ShutdownTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_dir = fs::temp_directory_path() /
                ("smarthub_shutdown_" + std::to_string(getpid()) + "_" +
                 ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::remove_all(m_dir);
        fs::create_directories(m_dir / "www");
        std::ofstream(m_dir / "www" / "index.html") << "<html>dashboard</html>";

        m_port = 19600 + (getpid() % 300) + s_nextPort++;
        std::ofstream(m_dir / "config.yaml")
            << "database:\n  path: " << (m_dir / "hub.db").string() << "\n"
            << "mqtt:\n  broker: 127.0.0.1\n  port: 1\n"
            << "web:\n  port: " << m_port << "\n"
            << "  root: " << (m_dir / "www").string() << "\n"
            << "  tls:\n    cert: \"\"\n    key: \"\"\n"
            << "rpmsg:\n  device: " << (m_dir / "no-rpmsg").string() << "\n"
            << "zigbee:\n  enabled: false\n"
            << "logging:\n  level: info\n";
    }

    void TearDown() override {
        if (m_pid > 0) {
            kill(m_pid, SIGKILL);
            waitpid(m_pid, nullptr, 0);
        }
        fs::remove_all(m_dir);
    }

    void startProgram() {
        m_pid = fork();
        ASSERT_GE(m_pid, 0);
        if (m_pid == 0) {
            const int output = open((m_dir / "output.log").c_str(),
                                    O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(output, STDOUT_FILENO);
            dup2(output, STDERR_FILENO);
            const std::string config = (m_dir / "config.yaml").string();
            execl(SMARTHUB_BINARY, SMARTHUB_BINARY, "-c", config.c_str(),
                  static_cast<char*>(nullptr));
            _exit(127);
        }

        for (int attempt = 0; attempt < 300; ++attempt) {
            if (!runCommand(curl("GET", "/api/setup/status")).empty()) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        FAIL() << "The program didn't start listening:\n" << output();
    }

    std::string curl(const std::string& method, const std::string& path,
                     const std::string& body = "") const {
        std::string command = "curl -s --max-time 40 -X " + method +
                              " -H 'Content-Type: application/json'";
        if (!m_cookie.empty()) {
            command += " -H " + shellQuote("Cookie: " + m_cookie);
        }
        if (method != "GET") {
            command += " --data-binary " + shellQuote(body);
        }
        return command + " " +
               shellQuote("http://127.0.0.1:" + std::to_string(m_port) + path) + " 2>/dev/null";
    }

    void setUpAdmin() {
        const std::string cookieFile = (m_dir / "cookie").string();
        runCommand("curl -s -c " + shellQuote(cookieFile) +
                   " -X POST -H 'Content-Type: application/json' --data-binary " +
                   shellQuote(R"({"username":"shutdownadmin","password":"Shutdown-Test-1!"})") +
                   " " + shellQuote("http://127.0.0.1:" + std::to_string(m_port) + "/api/setup"));
        std::ifstream cookies(cookieFile);
        std::string line;
        while (std::getline(cookies, line)) {
            const auto pos = line.find("session\t");
            if (pos != std::string::npos) {
                m_cookie = "session=" + line.substr(pos + 8);
            }
        }
        ASSERT_FALSE(m_cookie.empty());
    }

    std::string output() const {
        std::ifstream file(m_dir / "output.log");
        std::stringstream text;
        text << file.rdbuf();
        return text.str();
    }

    // Waits for the program, and returns how many seconds it took, or fails
    void expectCleanExit(int timeoutSeconds, double* seconds = nullptr) {
        const auto start = std::chrono::steady_clock::now();
        int status = 0;
        while (waitpid(m_pid, &status, WNOHANG) == 0) {
            const auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed > std::chrono::seconds(timeoutSeconds)) {
                FAIL() << "Still running after " << timeoutSeconds << " s:\n" << output();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        m_pid = -1;
        if (seconds) {
            *seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        }
        ASSERT_TRUE(WIFEXITED(status)) << "Killed by signal " << WTERMSIG(status) << ":\n" << output();
        EXPECT_EQ(WEXITSTATUS(status), 0) << output();
        EXPECT_NE(output().find("SmartHub shutdown complete"), std::string::npos) << output();
    }

    fs::path m_dir;
    pid_t m_pid = -1;
    int m_port = 0;
    std::string m_cookie;
    static inline int s_nextPort = 0;
};

TEST_F(ShutdownTest, SigtermWhileIdleExitsCleanly) {
    startProgram();
    ASSERT_EQ(kill(m_pid, SIGTERM), 0);
    expectCleanExit(20);
}

TEST_F(ShutdownTest, SigintExitsCleanly) {
    startProgram();
    ASSERT_EQ(kill(m_pid, SIGINT), 0);
    expectCleanExit(20);
}

TEST_F(ShutdownTest, SigtermUnderLoadExitsCleanly) {
    startProgram();
    setUpAdmin();

    runCommand(curl("POST", "/api/devices", R"({"type":"switch","protocol":"local","name":"Lamp"})"));

    // Requests that change device state and publish events, through the whole shutdown
    std::atomic<bool> stop{false};
    std::vector<std::thread> clients;
    for (int client = 0; client < 4; ++client) {
        clients.emplace_back([&, client] {
            bool on = false;
            while (!stop) {
                on = !on;
                if (client % 2 == 0) {
                    runCommand(curl("PUT", "/api/devices/device-lamp",
                                    on ? R"({"on":true})" : R"({"on":false})"));
                } else {
                    runCommand(curl("GET", "/api/devices"));
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_EQ(kill(m_pid, SIGTERM), 0);
    expectCleanExit(30);

    stop = true;
    for (auto& client : clients) {
        client.join();
    }
}

TEST_F(ShutdownTest, SigtermWakesAnAutomationWaitingInADelay) {
    startProgram();
    setUpAdmin();

    runCommand(curl("POST", "/api/devices", R"({"type":"switch","protocol":"local","name":"Source"})"));
    runCommand(curl("POST", "/api/devices", R"({"type":"switch","protocol":"local","name":"Target"})"));

    const nlohmann::json automation = {
        {"name", "Slow"},
        {"trigger", {{"type", "device_state"}, {"device_id", "device-source"}, {"property", "on"}}},
        {"actions", {{{"type", "delay"}, {"delayMs", 30000}},
                     {{"deviceId", "device-target"}, {"property", "on"}, {"value", true}}}},
    };
    ASSERT_NE(runCommand(curl("POST", "/api/automations", automation.dump())).find("success"),
              std::string::npos);

    // This request runs the automation on the web thread, and won't get an answer yet
    std::thread request([&] {
        runCommand(curl("PUT", "/api/devices/device-source", R"({"on":true})"));
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ASSERT_EQ(kill(m_pid, SIGTERM), 0);
    double seconds = 0;
    expectCleanExit(20, &seconds);
    EXPECT_LT(seconds, 15) << "Shutdown waited out the delay";
    EXPECT_NE(output().find("stopped by shutdown"), std::string::npos) << output();

    request.join();
}

}  // namespace
