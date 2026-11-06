/**
 * Web authentication, end to end.
 *
 * Starts the real Application from a config file and talks to it over HTTP,
 * so these tests exercise the wiring that ships, not a server set up by hand.
 */

#include <gtest/gtest.h>

#include <smarthub/core/Application.hpp>
#include <smarthub/core/Config.hpp>
#include <smarthub/core/EventBus.hpp>
#include <smarthub/database/Database.hpp>
#include <smarthub/devices/Device.hpp>
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/security/ApiTokenManager.hpp>
#include <smarthub/security/CertManager.hpp>
#include <smarthub/security/SessionManager.hpp>
#include <smarthub/security/UserManager.hpp>
#include <smarthub/web/WebServer.hpp>

#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

using namespace smarthub;
namespace fs = std::filesystem;

namespace {

constexpr const char* kAdminName = "owner";
constexpr const char* kAdminPassword = "Correct-Horse-1";

struct Response {
    int status = 0;
    std::string headers;
    std::string body;

    bool hasHeader(const std::string& text) const {
        return headers.find(text) != std::string::npos;
    }

    nlohmann::json json() const {
        return nlohmann::json::parse(body, nullptr, false);
    }

    // The value of the session cookie, or "" if none was set
    std::string sessionCookie() const {
        const std::string key = "Set-Cookie: session=";
        const size_t pos = headers.find(key);
        if (pos == std::string::npos) {
            return "";
        }
        const size_t start = pos + key.size();
        return headers.substr(start, headers.find(';', start) - start);
    }
};

struct RequestOptions {
    RequestOptions(std::string bodyText = "") : body(std::move(bodyText)) {}

    std::string body;
    std::string cookie;
    std::string bearer;
    std::string host;
    std::string contentType = "application/json";
    bool https = false;
};

std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    return quoted + "'";
}

}  // namespace

std::string shellOutput(const std::string& command) {
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            output += buffer;
        }
        pclose(pipe);
    }
    return output;
}

class WebAuthTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_dir = fs::temp_directory_path() /
                ("smarthub_web_auth_" + std::to_string(getpid()) + "_" +
                 ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::remove_all(m_dir);
        fs::create_directories(m_dir / "www");
        std::ofstream(m_dir / "www" / "index.html") << "<html>dashboard</html>";
        s_port++;
    }

    void TearDown() override {
        stopApp();
        fs::remove_all(m_dir);
    }

    void startApp(const std::string& securitySection = "", const std::string& certPath = "",
                  const std::string& keyPath = "") {
        ASSERT_TRUE(tryStartApp(securitySection, certPath, keyPath.empty() ? certPath : keyPath));
    }

    bool tryStartApp(const std::string& securitySection, const std::string& certPath,
                     const std::string& keyPath) {
        const fs::path configPath = m_dir / "config.yaml";
        std::ofstream config(configPath);
        config << "database:\n  path: " << (m_dir / "hub.db").string() << "\n"
               << "mqtt:\n  broker: 127.0.0.1\n  port: 1\n"
               << "web:\n  port: " << s_port << "\n"
               << "  root: " << (m_dir / "www").string() << "\n"
               << "  tls:\n    cert: \"" << certPath << "\"\n"
               << "    key: \"" << keyPath << "\"\n"
               << "rpmsg:\n  device: " << (m_dir / "no-rpmsg").string() << "\n"
               << "zigbee:\n  enabled: false\n"
               << "logging:\n  level: error\n  file: " << (m_dir / "hub.log").string() << "\n"
               << securitySection;
        config.close();

        m_app = std::make_unique<Application>(configPath.string());
        if (!m_app->initialize()) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return true;
    }

    void stopApp() {
        if (m_app) {
            m_app.reset();
        }
    }

    Response request(const std::string& method, const std::string& path,
                     const RequestOptions& options = {}) const {
        const fs::path headersFile = m_dir / "response-headers";
        std::string command = std::string("curl -s ") + (options.https ? "-k " : "") + "--max-time 10 -o - -D " +
                              shellQuote(headersFile.string()) + " -X " + method;
        // Always send a body, as a browser does: Mongoose answers 411 to a POST
        // with no Content-Length.
        if (method != "GET") {
            command += " --data-binary " + shellQuote(options.body);
        }
        command += " -H " + shellQuote("Content-Type: " + options.contentType);
        if (!options.cookie.empty()) {
            command += " -H " + shellQuote("Cookie: " + options.cookie);
        }
        if (!options.host.empty()) {
            command += " -H " + shellQuote("Host: " + options.host);
        }
        if (!options.bearer.empty()) {
            command += " -H " + shellQuote("Authorization: Bearer " + options.bearer);
        }
        command += " " + shellQuote(std::string(options.https ? "https" : "http") + "://127.0.0.1:" +
                                    std::to_string(s_port) + path) +
                   " 2>/dev/null";

        Response response;
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            return response;
        }
        std::array<char, 4096> buffer;
        size_t count;
        while ((count = fread(buffer.data(), 1, buffer.size(), pipe)) > 0) {
            response.body.append(buffer.data(), count);
        }
        pclose(pipe);

        std::ifstream headers(headersFile);
        response.headers.assign(std::istreambuf_iterator<char>(headers),
                                std::istreambuf_iterator<char>());
        if (response.headers.size() > 12) {
            response.status = std::stoi(response.headers.substr(9, 3));
        }
        return response;
    }

    static std::string credentials(const std::string& username, const std::string& password) {
        return nlohmann::json{{"username", username}, {"password", password}}.dump();
    }

    Response login(const std::string& username, const std::string& password) const {
        return request("POST", "/api/auth/login", {credentials(username, password)});
    }

    // Runs first-run setup and returns the administrator's session cookie
    std::string setUpAdmin() const {
        const Response response =
            request("POST", "/api/setup", {credentials(kAdminName, kAdminPassword)});
        EXPECT_EQ(response.status, 201) << response.body;
        return "session=" + response.sessionCookie();
    }

    // Adds an account through the database the application is using
    void addUser(const std::string& username, const std::string& password,
                 const std::string& role) {
        security::UserManager users(m_app->database());
        ASSERT_TRUE(users.createUser(username, password, role));
    }

    std::string loginCookie(const std::string& username, const std::string& password) const {
        const Response response = login(username, password);
        EXPECT_EQ(response.status, 200) << response.body;
        return "session=" + response.sessionCookie();
    }

    fs::path m_dir;
    std::unique_ptr<Application> m_app;
    static inline int s_port = 18600 + (getpid() % 1000);
};

// ---------- Rejection without credentials

TEST_F(WebAuthTest, ProtectedRoutesRejectAnonymousRequests) {
    startApp();
    setUpAdmin();

    for (const char* path : {"/api/devices", "/api/rooms", "/api/automations", "/api/auth/me",
                             "/api/zigbee/pending-devices"}) {
        EXPECT_EQ(request("GET", path).status, 401) << path;
    }
    EXPECT_EQ(request("PUT", "/api/devices/light1", {R"({"on":true})"}).status, 401);
    EXPECT_EQ(request("POST", "/api/rooms", {R"({"name":"Den"})"}).status, 401);
}

TEST_F(WebAuthTest, PublicRoutesNeedNoCredentials) {
    startApp();
    setUpAdmin();

    // A cross-site form arrives without the cookie, and must not clear it
    const Response logout = request("POST", "/api/auth/logout");
    EXPECT_EQ(logout.status, 200);
    EXPECT_FALSE(logout.hasHeader("Set-Cookie"));


    EXPECT_EQ(request("GET", "/api/system/status").status, 200);
    EXPECT_EQ(request("GET", "/api/setup/status").status, 200);
    const Response page = request("GET", "/index.html");
    EXPECT_EQ(page.status, 200);
    EXPECT_NE(page.body.find("dashboard"), std::string::npos);
}

TEST_F(WebAuthTest, ForgedCredentialsAreRejected) {
    startApp();
    setUpAdmin();

    RequestOptions forgedCookie;
    forgedCookie.cookie = "session=not-a-real-session";
    EXPECT_EQ(request("GET", "/api/devices", forgedCookie).status, 401);

    RequestOptions forgedToken;
    forgedToken.bearer = "not-a-real-token";
    EXPECT_EQ(request("GET", "/api/devices", forgedToken).status, 401);
}

// ---------- First-run setup

TEST_F(WebAuthTest, SetupIsRequiredUntilTheFirstAccountExists) {
    startApp();

    EXPECT_EQ(request("GET", "/api/setup/status").json()["setupRequired"], true);
    setUpAdmin();
    EXPECT_EQ(request("GET", "/api/setup/status").json()["setupRequired"], false);
}

TEST_F(WebAuthTest, NoLoginSucceedsBeforeSetup) {
    startApp();

    EXPECT_EQ(login("admin", "Password1").status, 401);
    EXPECT_EQ(login(kAdminName, kAdminPassword).status, 401);
}

TEST_F(WebAuthTest, SetupCreatesAnAdministratorAndLogsIn) {
    startApp();

    const std::string cookie = setUpAdmin();

    RequestOptions options;
    options.cookie = cookie;
    const auto me = request("GET", "/api/auth/me", options).json();
    EXPECT_EQ(me["username"], kAdminName);
    EXPECT_EQ(me["role"], "admin");

    security::UserManager users(m_app->database());
    ASSERT_TRUE(users.getUser(kAdminName).has_value());
    EXPECT_EQ(users.getUser(kAdminName)->role, "admin");
}

TEST_F(WebAuthTest, SetupCannotRunTwice) {
    startApp();
    setUpAdmin();

    const Response second =
        request("POST", "/api/setup", {credentials("intruder", "Password-123")});
    EXPECT_EQ(second.status, 409);
    EXPECT_TRUE(second.sessionCookie().empty());

    security::UserManager users(m_app->database());
    EXPECT_FALSE(users.getUser("intruder").has_value());
}

TEST_F(WebAuthTest, SetupStaysClosedWhenTheOnlyAdminIsDisabled) {
    startApp();
    setUpAdmin();

    security::UserManager users(m_app->database());
    ASSERT_TRUE(users.setUserEnabled(kAdminName, false));

    EXPECT_EQ(request("GET", "/api/setup/status").json()["setupRequired"], false);
    EXPECT_EQ(request("POST", "/api/setup", {credentials("intruder", "Password-123")}).status,
              409);
}

TEST_F(WebAuthTest, SetupRejectsWeakPasswordsAndBadUsernames) {
    startApp();

    const Response weak = request("POST", "/api/setup", {credentials("owner", "short")});
    EXPECT_EQ(weak.status, 400);
    EXPECT_NE(weak.json()["error"].get<std::string>().find("Password"), std::string::npos);

    const Response badName = request("POST", "/api/setup", {credentials("a b", kAdminPassword)});
    EXPECT_EQ(badName.status, 400);
    EXPECT_NE(badName.json()["error"].get<std::string>().find("Username"), std::string::npos);

    EXPECT_EQ(request("GET", "/api/setup/status").json()["setupRequired"], true);
}

TEST_F(WebAuthTest, SetupRefusesPublicDnsNames) {
    startApp();

    // What a page gets after pointing its own domain at the hub
    for (const char* host : {"evil.example", "evil.example:4321", "192.168.1.5.nip.io",
                             "smarthub.local.evil.example"}) {
        RequestOptions rebound(credentials(kAdminName, kAdminPassword));
        rebound.host = host;
        EXPECT_EQ(request("POST", "/api/setup", rebound).status, 403) << host;
    }
    EXPECT_EQ(request("GET", "/api/setup/status").json()["setupRequired"], true);
}

TEST_F(WebAuthTest, ConfiguredHostsAreAccepted) {
    startApp("security:\n  allowed_hosts: [Hub.Fritz.Box]\n");

    RequestOptions other(credentials(kAdminName, kAdminPassword));
    other.host = "other.fritz.box";
    EXPECT_EQ(request("POST", "/api/setup", other).status, 403);

    RequestOptions configured(credentials(kAdminName, kAdminPassword));
    configured.host = "hub.fritz.box:443";
    EXPECT_EQ(request("POST", "/api/setup", configured).status, 201);
}

TEST_F(WebAuthTest, LoginRefusesPublicDnsNames) {
    startApp();
    setUpAdmin();

    RequestOptions rebound(credentials(kAdminName, kAdminPassword));
    rebound.host = "evil.example";
    const Response response = request("POST", "/api/auth/login", rebound);
    EXPECT_EQ(response.status, 403);
    EXPECT_TRUE(response.sessionCookie().empty());

    rebound.host = "smarthub.local";
    EXPECT_EQ(request("POST", "/api/auth/login", rebound).status, 200);
}

TEST_F(WebAuthTest, SetupAcceptsLocalNames) {
    for (const char* host : {"192.168.1.5", "192.168.1.5:8443", "smarthub.local",
                             "SmartHub.LOCAL:443", "smarthub", "localhost:8080", "[fe80::1]:443",
                             "hub.lan", "hub.home", "hub.internal", "hub.home.arpa", "hub.localdomain",
                             "smarthub.local."}) {
        startApp();
        RequestOptions local(credentials(kAdminName, kAdminPassword));
        local.host = host;
        EXPECT_EQ(request("POST", "/api/setup", local).status, 201) << host;
        stopApp();
        fs::remove(m_dir / "hub.db");
        s_port++;
    }
}

TEST_F(WebAuthTest, SetupAndLoginRefuseRequestsThatAreNotJson) {
    startApp();

    // A page on another site can send text/plain without a CORS preflight
    RequestOptions crossSite;
    crossSite.body = credentials(kAdminName, kAdminPassword);
    crossSite.contentType = "text/plain";
    EXPECT_EQ(request("POST", "/api/setup", crossSite).status, 415);
    EXPECT_EQ(request("GET", "/api/setup/status").json()["setupRequired"], true);

    setUpAdmin();
    EXPECT_EQ(request("POST", "/api/auth/login", crossSite).status, 415);
}

// ---------- Login

TEST_F(WebAuthTest, LoginThenAuthenticatedRequest) {
    startApp();
    setUpAdmin();

    const Response response = login(kAdminName, kAdminPassword);
    ASSERT_EQ(response.status, 200) << response.body;
    EXPECT_EQ(response.json()["username"], kAdminName);
    EXPECT_EQ(response.json()["role"], "admin");
    EXPECT_TRUE(response.hasHeader("HttpOnly"));
    EXPECT_TRUE(response.hasHeader("SameSite=Strict"));

    RequestOptions options;
    options.cookie = "session=" + response.sessionCookie();
    EXPECT_EQ(request("GET", "/api/devices", options).status, 200);
}

TEST_F(WebAuthTest, WrongPasswordAndUnknownUserGetTheSameAnswer) {
    startApp();
    setUpAdmin();

    const Response wrongPassword = login(kAdminName, "Wrong-Password-1");
    const Response unknownUser = login("nobody", kAdminPassword);

    EXPECT_EQ(wrongPassword.status, 401);
    EXPECT_EQ(unknownUser.status, 401);
    EXPECT_EQ(wrongPassword.body, unknownUser.body);
    EXPECT_TRUE(wrongPassword.sessionCookie().empty());
    EXPECT_TRUE(unknownUser.sessionCookie().empty());
}

TEST_F(WebAuthTest, DisabledUserCannotLogIn) {
    startApp();
    setUpAdmin();
    addUser("guest", "Guest-Pass-1", "user");

    security::UserManager users(m_app->database());
    ASSERT_TRUE(users.setUserEnabled("guest", false));

    EXPECT_EQ(login("guest", "Guest-Pass-1").status, 401);
}

TEST_F(WebAuthTest, MalformedLoginBodiesAreRejectedWithoutCrashing) {
    startApp();
    setUpAdmin();

    for (const char* body : {"", "not json", "[]", "{}", R"({"username":"owner"})",
                             R"({"username":1,"password":2})",
                             R"({"username":"","password":""})"}) {
        RequestOptions options;
        options.body = body;
        EXPECT_EQ(request("POST", "/api/auth/login", options).status, 400) << body;
    }

    // Quotes used to confuse the hand-written parser this replaced
    EXPECT_EQ(login("own\"er", kAdminPassword).status, 401);
    EXPECT_EQ(login(kAdminName, std::string(kAdminPassword) + "\"").status, 401);
    EXPECT_EQ(request("GET", "/api/system/status").status, 200);
}

TEST_F(WebAuthTest, PasswordWithSpecialCharactersWorks) {
    startApp();
    setUpAdmin();
    const std::string password = "Quote\"Back\\slash:1";
    addUser("guest", password, "user");

    EXPECT_EQ(login("guest", password).status, 200);
}

TEST_F(WebAuthTest, FirstBootCreatesACertificateAndServesTls) {
    const fs::path cert = m_dir / "new" / "certs" / "server.crt";
    const fs::path key = m_dir / "new" / "certs" / "server.key";
    startApp("", cert.string(), key.string());

    EXPECT_TRUE(fs::exists(cert));
    EXPECT_TRUE(fs::exists(key));
    EXPECT_EQ(fs::status(key).permissions() & (fs::perms::group_all | fs::perms::others_all),
              fs::perms::none);

    RequestOptions secure;
    secure.https = true;
    EXPECT_EQ(request("GET", "/api/setup/status", secure).status, 200);

    // Plain HTTP to the same port gets nowhere
    EXPECT_NE(request("GET", "/api/setup/status").status, 200);

    // A login over TLS gets a cookie a browser will only send back over TLS
    secure.body = credentials(kAdminName, kAdminPassword);
    const Response setup = request("POST", "/api/setup", secure);
    ASSERT_EQ(setup.status, 201) << setup.body;
    EXPECT_TRUE(setup.hasHeader("Secure"));

    // The same certificate is used next time
    stopApp();
    const auto created = fs::last_write_time(cert);
    startApp("", cert.string(), key.string());
    EXPECT_EQ(fs::last_write_time(cert), created);
}

// The hub keeps running without a web server, rather than serving anything insecurely
TEST_F(WebAuthTest, ACertificateThatCantBeUsedLeavesTheWebServerDown) {
    const fs::path cert = m_dir / "server.crt";
    const fs::path key = m_dir / "server.key";
    std::ofstream(cert) << "not a certificate";
    std::ofstream(key) << "not a key";

    EXPECT_TRUE(tryStartApp("", cert.string(), key.string()));
    EXPECT_EQ(request("GET", "/api/setup/status").status, 0);
    EXPECT_NO_THROW(m_app->deviceManager().deviceCount());
    stopApp();

    // Left as found, rather than replaced
    std::ifstream file(cert);
    std::string contents;
    std::getline(file, contents);
    EXPECT_EQ(contents, "not a certificate");
}

TEST_F(WebAuthTest, ACertificateWithoutItsKeyLeavesTheWebServerDown) {
    const fs::path cert = m_dir / "server.crt";
    std::ofstream(cert) << "not a certificate";

    EXPECT_TRUE(tryStartApp("", cert.string(), (m_dir / "server.key").string()));
    EXPECT_EQ(request("GET", "/api/setup/status").status, 0);
}

TEST_F(WebAuthTest, ACertificateWithTheWrongKeyLeavesTheWebServerDown) {
    const fs::path first = m_dir / "first";
    const fs::path second = m_dir / "second";
    ASSERT_TRUE(security::CertManager::createSelfSigned((first / "server.crt").string(),
                                                        (first / "server.key").string(), "a"));
    ASSERT_TRUE(security::CertManager::createSelfSigned((second / "server.crt").string(),
                                                        (second / "server.key").string(), "b"));

    EXPECT_TRUE(tryStartApp("", (first / "server.crt").string(), (second / "server.key").string()));
    EXPECT_EQ(request("GET", "/api/setup/status").status, 0);
}

TEST_F(WebAuthTest, OnlyOneTlsPathLeavesTheWebServerDown) {
    EXPECT_TRUE(tryStartApp("", (m_dir / "server.crt").string(), ""));
    EXPECT_EQ(request("GET", "/api/setup/status").status, 0);
}

TEST_F(WebAuthTest, TheSameTlsPathForBothLeavesTheWebServerDown) {
    const fs::path both = m_dir / "both.pem";
    EXPECT_TRUE(tryStartApp("", both.string(), both.string()));
    EXPECT_EQ(request("GET", "/api/setup/status").status, 0);
    EXPECT_FALSE(fs::exists(both));
}

TEST_F(WebAuthTest, AKeyLeftByAnInterruptedFirstBootIsReplaced) {
    const fs::path cert = m_dir / "server.crt";
    const fs::path key = m_dir / "server.key";
    std::ofstream(key) << "left from a power cut";

    startApp("", cert.string(), key.string());

    RequestOptions secure;
    secure.https = true;
    EXPECT_EQ(request("GET", "/api/setup/status", secure).status, 200);
}

TEST_F(WebAuthTest, ALeftoverTemporaryKeyFileCantExposeTheKey) {
    const fs::path cert = m_dir / "server.crt";
    const fs::path key = m_dir / "server.key";
    std::ofstream(m_dir / "server.key.new") << "old";
    fs::permissions(m_dir / "server.key.new", fs::perms::all);

    ASSERT_TRUE(security::CertManager::createSelfSigned(cert.string(), key.string(), "a"));
    EXPECT_EQ(fs::status(key).permissions() & (fs::perms::group_all | fs::perms::others_all),
              fs::perms::none);
}

TEST_F(WebAuthTest, BlankTlsPathsInTheConfigMeanPlainHttp) {
    const fs::path config = m_dir / "blank.yaml";
    std::ofstream(config) << "web:\n  tls:\n    cert:\n    key:\n";
    Config parsed;
    ASSERT_TRUE(parsed.load(config.string()));
    EXPECT_EQ(parsed.tlsCertPath(), "");
    EXPECT_EQ(parsed.tlsKeyPath(), "");
}

TEST_F(WebAuthTest, EmptyTlsPathsMeanPlainHttp) {
    startApp();
    EXPECT_EQ(request("GET", "/api/setup/status").status, 200);
    EXPECT_FALSE(setUpAdmin().empty());
    EXPECT_FALSE(login(kAdminName, kAdminPassword).hasHeader("Secure"));
}

TEST_F(WebAuthTest, ThereIsNoSecondPlainHttpListener) {
    startApp();
    const std::string reached = shellOutput(
        "curl -s --max-time 2 -o /dev/null -w '%{http_code}' http://127.0.0.1:4321/api/setup/status");
    EXPECT_EQ(reached, "000");
}

TEST_F(WebAuthTest, ResponsesAreValidJsonWhateverDevicesAreNamed) {
    startApp();
    RequestOptions admin;
    admin.cookie = setUpAdmin();

    const std::string name = R"(Quote " back\slash <img src=x onerror=alert(1)>)";
    RequestOptions device = admin;
    device.body = nlohmann::json{{"type", "switch"}, {"protocol", "local"},
                                 {"name", name}, {"id", "quoted"}}.dump();
    ASSERT_EQ(request("POST", "/api/devices", device).status, 201);

    RequestOptions room = admin;
    room.body = nlohmann::json{{"name", name}, {"id", "quotedroom"}}.dump();
    const Response createdRoom = request("POST", "/api/rooms", room);
    ASSERT_EQ(createdRoom.status, 201) << createdRoom.body;
    EXPECT_EQ(createdRoom.json()["name"], name);

    EXPECT_EQ(request("GET", "/api/devices", admin).json()[0]["name"], name);
    EXPECT_EQ(request("GET", "/api/devices/quoted", admin).json()["name"], name);
    const nlohmann::json rooms = request("GET", "/api/rooms", admin).json();
    ASSERT_TRUE(rooms.is_array());
    bool found = false;
    for (const auto& entry : rooms) {
        found = found || entry["name"] == name;
    }
    EXPECT_TRUE(found);
}

TEST_F(WebAuthTest, RoomsSurviveADeviceReportingAnOddState) {
    startApp();
    RequestOptions admin;
    admin.cookie = setUpAdmin();

    RequestOptions device = admin;
    device.body = R"({"type":"switch","protocol":"local","name":"Odd","id":"odd","room":"Kitchen"})";
    ASSERT_EQ(request("POST", "/api/devices", device).status, 201);
    m_app->deviceManager().getDevice("odd")->setState("on", "yes");

    EXPECT_EQ(request("GET", "/api/rooms", admin).status, 200);

    // A device with no state at all
    ASSERT_TRUE(m_app->deviceManager().addDevice(
        std::make_shared<Device>("bare", "Bare", DeviceType::Custom)));
    m_app->deviceManager().getDevice("bare")->setRoom("Kitchen");
    EXPECT_EQ(request("GET", "/api/rooms", admin).status, 200);
    EXPECT_EQ(request("GET", "/api/devices", admin).status, 200);

    // Text that isn't valid UTF-8 doesn't lose the whole list either
    m_app->deviceManager().getDevice("odd")->setState("note", std::string("bad\xFF"));
    EXPECT_EQ(request("GET", "/api/devices", admin).status, 200);
}

TEST_F(WebAuthTest, SecurityHeadersAreOnApiAndStaticResponses) {
    startApp();
    for (const char* path : {"/api/setup/status", "/index.html"}) {
        SCOPED_TRACE(path);
        const Response response = request("GET", path);
        EXPECT_EQ(response.status, 200);
        EXPECT_TRUE(response.hasHeader("Content-Security-Policy: default-src 'none'"));
        EXPECT_TRUE(response.hasHeader("X-Content-Type-Options: nosniff"));
        EXPECT_TRUE(response.hasHeader("X-Frame-Options: DENY"));
    }
}

TEST_F(WebAuthTest, OnlyApiResponsesRefuseCaching) {
    startApp();
    EXPECT_TRUE(request("GET", "/api/setup/status").hasHeader("Cache-Control: no-store"));
    EXPECT_FALSE(request("GET", "/index.html").hasHeader("no-store"));
}

TEST_F(WebAuthTest, OnlyTheExactSessionCookieIsAccepted) {
    startApp();
    const std::string cookie = setUpAdmin();

    RequestOptions lookalike;
    lookalike.cookie = "my" + cookie;
    EXPECT_EQ(request("GET", "/api/devices", lookalike).status, 401);

    RequestOptions amongOthers;
    amongOthers.cookie = "theme=dark; " + cookie + "; lang=en";
    EXPECT_EQ(request("GET", "/api/devices", amongOthers).status, 200);
}

// ---------- Logout and expiry

TEST_F(WebAuthTest, LogoutEndsOnlyThatSession) {
    startApp();
    setUpAdmin();
    RequestOptions phone, laptop;
    phone.cookie = loginCookie(kAdminName, kAdminPassword);
    laptop.cookie = loginCookie(kAdminName, kAdminPassword);

    const Response logout = request("POST", "/api/auth/logout", phone);
    EXPECT_EQ(logout.status, 200);
    EXPECT_TRUE(logout.hasHeader("Max-Age=0"));

    EXPECT_EQ(request("GET", "/api/devices", phone).status, 401);
    EXPECT_EQ(request("GET", "/api/devices", laptop).status, 200);
}

TEST_F(WebAuthTest, DisablingAUserEndsTheirSession) {
    startApp();
    setUpAdmin();
    addUser("guest", "Guest-Pass-1", "user");
    RequestOptions guest;
    guest.cookie = loginCookie("guest", "Guest-Pass-1");
    EXPECT_EQ(request("GET", "/api/devices", guest).status, 200);

    security::UserManager users(m_app->database());
    ASSERT_TRUE(users.setUserEnabled("guest", false));
    EXPECT_EQ(request("GET", "/api/devices", guest).status, 401);

    // It stays ended if the account is enabled again
    ASSERT_TRUE(users.setUserEnabled("guest", true));
    EXPECT_EQ(request("GET", "/api/devices", guest).status, 401);
}

TEST_F(WebAuthTest, LogoutWorksEvenIfTheAccountIsGone) {
    startApp();
    setUpAdmin();
    addUser("guest", "Guest-Pass-1", "user");
    RequestOptions guest;
    guest.cookie = loginCookie("guest", "Guest-Pass-1");

    security::UserManager users(m_app->database());
    ASSERT_TRUE(users.deleteUser("guest"));

    const Response logout = request("POST", "/api/auth/logout", guest);
    EXPECT_EQ(logout.status, 200);
    EXPECT_TRUE(logout.hasHeader("Max-Age=0"));
}

TEST_F(WebAuthTest, RemovingAUserEndsTheirSession) {
    startApp();
    setUpAdmin();
    addUser("guest", "Guest-Pass-1", "user");
    RequestOptions guest;
    guest.cookie = loginCookie("guest", "Guest-Pass-1");

    security::UserManager users(m_app->database());
    ASSERT_TRUE(users.deleteUser("guest"));
    EXPECT_EQ(request("GET", "/api/devices", guest).status, 401);
}

TEST_F(WebAuthTest, ChangingARoleAppliesToOpenSessions) {
    startApp();
    setUpAdmin();
    addUser("guest", "Guest-Pass-1", "admin");
    RequestOptions guest(R"({"name":"Den"})");
    guest.cookie = loginCookie("guest", "Guest-Pass-1");
    EXPECT_EQ(request("POST", "/api/rooms", guest).status, 201);

    security::UserManager users(m_app->database());
    ASSERT_TRUE(users.changeRole("guest", "user"));
    EXPECT_EQ(request("POST", "/api/rooms", guest).status, 403);
    EXPECT_EQ(request("GET", "/api/auth/me", guest).json()["role"], "user");
}

TEST_F(WebAuthTest, InvalidSessionTimeoutDoesNotLockEveryoneOut) {
    startApp("security:\n  session_timeout_minutes: 0\n");
    RequestOptions options;
    options.cookie = setUpAdmin();

    EXPECT_EQ(request("GET", "/api/devices", options).status, 200);
}

TEST_F(WebAuthTest, SessionsDoNotSurviveARestart) {
    startApp();
    RequestOptions options;
    options.cookie = setUpAdmin();

    stopApp();
    s_port++;
    startApp();

    EXPECT_EQ(request("GET", "/api/devices", options).status, 401);
    // The account does survive
    EXPECT_EQ(login(kAdminName, kAdminPassword).status, 200);
}

// ---------- API tokens

TEST_F(WebAuthTest, ApiTokenCarriesItsOwnersIdentityAndRole) {
    startApp();
    setUpAdmin();
    addUser("guest", "Guest-Pass-1", "user");

    security::UserManager users(m_app->database());
    security::ApiTokenManager tokens(m_app->database());
    const std::string guestToken = tokens.createToken(users.getUser("guest")->id, "script");
    const std::string adminToken = tokens.createToken(users.getUser(kAdminName)->id, "script");
    ASSERT_FALSE(guestToken.empty());
    ASSERT_FALSE(adminToken.empty());

    RequestOptions guest;
    guest.bearer = guestToken;
    const auto me = request("GET", "/api/auth/me", guest).json();
    EXPECT_EQ(me["username"], "guest");
    EXPECT_EQ(me["role"], "user");
    EXPECT_EQ(me["isApiToken"], true);
    EXPECT_EQ(request("GET", "/api/devices", guest).status, 200);

    guest.body = R"({"name":"Den"})";
    EXPECT_EQ(request("POST", "/api/rooms", guest).status, 403);

    RequestOptions admin;
    admin.bearer = adminToken;
    admin.body = R"({"name":"Den"})";
    EXPECT_EQ(request("POST", "/api/rooms", admin).status, 201);
}

TEST_F(WebAuthTest, ApiTokenStopsWorkingWhenItsOwnerIsDisabled) {
    startApp();
    setUpAdmin();
    addUser("guest", "Guest-Pass-1", "user");

    security::UserManager users(m_app->database());
    security::ApiTokenManager tokens(m_app->database());
    RequestOptions options;
    options.bearer = tokens.createToken(users.getUser("guest")->id, "script");
    EXPECT_EQ(request("GET", "/api/devices", options).status, 200);

    ASSERT_TRUE(users.setUserEnabled("guest", false));
    EXPECT_EQ(request("GET", "/api/devices", options).status, 401);
}

// ---------- Roles

TEST_F(WebAuthTest, RegularUserCanReadAndControlButNotConfigure) {
    startApp();
    setUpAdmin();
    addUser("guest", "Guest-Pass-1", "user");
    RequestOptions guest;
    guest.cookie = loginCookie("guest", "Guest-Pass-1");

    for (const char* path : {"/api/devices", "/api/rooms", "/api/auth/me"}) {
        EXPECT_EQ(request("GET", path, guest).status, 200) << path;
    }

    // Reaches the handler, which reports the unknown device
    RequestOptions control = guest;
    control.body = R"({"on":true})";
    EXPECT_EQ(request("PUT", "/api/devices/no-such-device", control).status, 404);

    struct Route {
        const char* method;
        const char* path;
    };
    const std::vector<Route> adminRoutes = {
        {"POST", "/api/devices"},
        {"DELETE", "/api/devices/light1"},
        {"PUT", "/api/devices/light1/settings"},
        {"POST", "/api/rooms"},
        {"PUT", "/api/rooms/room1"},
        {"DELETE", "/api/rooms/room1"},
        {"POST", "/api/zigbee/permit-join"},
        {"DELETE", "/api/zigbee/permit-join"},
        {"GET", "/api/zigbee/pending-devices"},
        {"POST", "/api/zigbee/devices"},
        {"POST", "/api/automations"},
        {"PUT", "/api/automations/auto1"},
        {"DELETE", "/api/automations/auto1"},
        {"PUT", "/api/automations/auto1/enable"},
    };
    for (const Route& route : adminRoutes) {
        RequestOptions options = guest;
        options.body = "{}";
        EXPECT_EQ(request(route.method, route.path, options).status, 403)
            << route.method << " " << route.path;
    }
}

TEST_F(WebAuthTest, AutomationCreatedThroughTheApiRunsOnADeviceChange) {
    startApp();
    RequestOptions admin;
    admin.cookie = setUpAdmin();

    const auto addSwitch = [&](const char* name) {
        RequestOptions options = admin;
        options.body = std::string(R"({"type":"switch","protocol":"local","name":")") + name + "\"}";
        const Response created = request("POST", "/api/devices", options);
        EXPECT_EQ(created.status, 201) << created.body;
        return created.json()["id"].get<std::string>();
    };
    const std::string source = addSwitch("Source");
    const std::string target = addSwitch("Target");

    RequestOptions automation = admin;
    automation.body = nlohmann::json{
        {"name", "Follow"},
        {"trigger", {{"type", "device_state"}, {"device_id", source},
                     {"property", "on"}, {"to_value", true}}},
        {"actions", {{{"deviceId", target}, {"property", "on"}, {"value", true}}}},
    }.dump();
    const Response created = request("POST", "/api/automations", automation);
    ASSERT_EQ(created.status, 201) << created.body;
    EXPECT_FALSE(created.json()["id"].get<std::string>().empty());

    const Response listed = request("GET", "/api/automations", admin);
    ASSERT_EQ(listed.status, 200) << listed.body;
    EXPECT_EQ(listed.json().size(), 1u);

    RequestOptions turnOn = admin;
    turnOn.body = R"({"on":true})";
    ASSERT_EQ(request("PUT", "/api/devices/" + source, turnOn).status, 200);

    // The action runs on the automation worker, a moment after this request returns
    Response state;
    for (int attempt = 0; attempt < 50; ++attempt) {
        state = request("GET", "/api/devices/" + target, admin);
        ASSERT_EQ(state.status, 200) << state.body;
        if (state.json()["state"]["on"] == true) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    EXPECT_EQ(state.json()["state"]["on"], true) << state.body;
}

TEST_F(WebAuthTest, AutomationsThatCouldNeverRunAreRejected) {
    startApp();
    RequestOptions admin;
    admin.cookie = setUpAdmin();

    const nlohmann::json action = {{"deviceId", "light"}, {"property", "on"}, {"value", true}};
    const std::vector<nlohmann::json> rejected = {
        {{"name", "No trigger"}, {"actions", {action}}},
        {{"name", "Bad type"}, {"trigger", {{"type", "interval"}}}, {"actions", {action}}},
        {{"name", "Zero interval"},
         {"trigger", {{"type", "time_interval"}, {"interval_minutes", 0}}},
         {"actions", {action}}},
        {{"name", "No device"},
         {"trigger", {{"type", "device_state"}, {"property", "on"}}},
         {"actions", {action}}},
        {{"name", "Bad operator"},
         {"trigger", {{"type", "sensor_threshold"}, {"device_id", "s"}, {"property", "t"},
                      {"compare_op", ">"}, {"threshold", 5}}},
         {"actions", {action}}},
        {{"name", "No time"}, {"trigger", {{"type", "time"}}}, {"actions", {action}}},
        {{"name", "Bad hour"},
         {"trigger", {{"type", "time"}, {"hour", 24}, {"minute", 0}}},
         {"actions", {action}}},
        {{"name", "No action"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"actions", nlohmann::json::array()}},
        {{"name", "Empty action"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"actions", {nlohmann::json::object()}}},
        {{"name", "Bad action type"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"actions", {{{"type", "bogus"}, {"deviceId", "x"}}}}},
        {{"name", "Bad id"}, {"id", "a/b"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"actions", {action}}},
        {{"name", "Old value field"},
         {"trigger", {{"type", "device_state"}, {"device_id", "s"}, {"property", "on"},
                      {"value", true}}},
         {"actions", {action}}},
        {{"name", "Overflowing delays"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"actions", {{{"type", "delay"}, {"delayMs", 1}},
                      {{"type", "delay"}, {"delayMs", 2147483647}}}}},
        {{"name", "Old conditions field"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"conditions", nlohmann::json::array()},
         {"actions", {action}}},
        {{"name", "Empty condition group"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"condition", {{"is_group", true}, {"group_op", "or"},
                        {"children", nlohmann::json::array()}}},
         {"actions", {action}}},
        {{"name", "Wrapping interval"},
         {"trigger", {{"type", "time_interval"}, {"interval_minutes", 4294967297LL}}},
         {"actions", {action}}},
        {{"name", "Fractional hour"},
         {"trigger", {{"type", "time"}, {"hour", 12.9}, {"minute", 0}}},
         {"actions", {action}}},
        {{"name", "Text as a number"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"condition", {{"leaf", {{"device_id", "x"}, {"property", "t"}, {"op", "gt"},
                                  {"value", "20"}}}}},
         {"actions", {action}}},
        {{"name", "Enabled as text"}, {"enabled", "yes"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"actions", {action}}},
        {{"name", "Wrapping hour"},
         {"trigger", {{"type", "time"}, {"hour", 18446744073709551615ULL}, {"minute", 0}}},
         {"actions", {action}}},
        {{"name", "Internal property"},
         {"trigger", {{"type", "device_state"}, {"device_id", "s"}, {"property", "_removed"}}},
         {"actions", {action}}},
        {{"name", "Long delay"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"actions", {{{"type", "delay"}, {"delayMs", 60001}}}}},
        {{"name", "Delays add up"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"actions", {{{"type", "delay"}, {"delayMs", 40000}},
                      {{"type", "delay"}, {"delayMs", 40000}}}}},
        {{"name", "Huge interval"},
         {"trigger", {{"type", "time_interval"}, {"interval_minutes", 525601}}},
         {"actions", {action}}},
        {{"name", "Both trigger forms"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"triggers", nlohmann::json::array()},
         {"actions", {action}}},
        {{"name", "Flat condition"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"condition", {{"device_id", "x"}, {"property", "on"}, {"value", true}}},
         {"actions", {action}}},
        {{"name", "Bad condition op"},
         {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
         {"condition", {{"leaf", {{"device_id", "x"}, {"property", "on"}, {"op", ">"},
                                  {"value", true}}}}},
         {"actions", {action}}},
    };
    for (const auto& body : rejected) {
        RequestOptions options = admin;
        options.body = body.dump();
        EXPECT_EQ(request("POST", "/api/automations", options).status, 400) << body.dump();
    }
    EXPECT_EQ(request("GET", "/api/automations", admin).json().size(), 0u);

    // A value nested deeply enough to overflow the stack if it were copied
    std::string nestedValue(50000, '[');
    nestedValue += std::string(50000, ']');
    RequestOptions deepValue = admin;
    deepValue.body = R"({"name":"Deep value","trigger":{"type":"device_state","device_id":"x",)"
                     R"("property":"on","to_value":)" + nestedValue +
                     R"(},"actions":[{"deviceId":"light","property":"on","value":true}]})";
    EXPECT_EQ(request("POST", "/api/automations", deepValue).status, 400);

    // A field of the wrong type gets a clear message
    RequestOptions wrongType = admin;
    wrongType.body = nlohmann::json{{"name", "Numeric property"},
                                    {"trigger", {{"type", "device_state"}, {"device_id", "x"},
                                                 {"property", 5}}},
                                    {"actions", {action}}}.dump();
    const Response wrongTypeResponse = request("POST", "/api/automations", wrongType);
    EXPECT_EQ(wrongTypeResponse.status, 400);
    EXPECT_EQ(wrongTypeResponse.json()["error"], "a field has the wrong type");

    // Conditions nested too deeply
    nlohmann::json deep = {{"leaf", {{"device_id", "x"}, {"property", "on"}, {"op", "eq"},
                                     {"value", true}}}};
    for (int i = 0; i < 20; ++i) {
        deep = {{"is_group", true}, {"children", {deep}}};
    }
    RequestOptions nested = admin;
    nested.body = nlohmann::json{{"name", "Deep"},
                                 {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
                                 {"condition", deep},
                                 {"actions", {action}}}.dump();
    EXPECT_EQ(request("POST", "/api/automations", nested).status, 400);

    // An action on an existing device has to be something a client could do
    RequestOptions sensor = admin;
    sensor.body = R"({"type":"temperature_sensor","name":"Hall","protocol":"local"})";
    const Response createdSensor = request("POST", "/api/devices", sensor);
    ASSERT_EQ(createdSensor.status, 201) << createdSensor.body;
    RequestOptions fake = admin;
    fake.body = nlohmann::json{
        {"name", "Fake reading"},
        {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
        {"actions", {{{"deviceId", createdSensor.json()["id"]}, {"property", "temperature"},
                      {"value", 500}}}},
    }.dump();
    EXPECT_EQ(request("POST", "/api/automations", fake).status, 400);

    // A problem echoing what the client sent is still valid JSON
    RequestOptions quoted = admin;
    quoted.body = nlohmann::json{{"name", "Quote"},
                                 {"trigger", {{"type", "a\"b"}}},
                                 {"actions", {action}}}.dump();
    const Response response = request("POST", "/api/automations", quoted);
    EXPECT_EQ(response.status, 400);
    EXPECT_NE(response.json()["error"].get<std::string>().find("a\"b"), std::string::npos);

    // A duplicate id is the client's conflict, not a server failure
    RequestOptions withId = admin;
    withId.body = nlohmann::json{{"name", "Named"}, {"id", "night"},
                                 {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
                                 {"actions", {action}}}.dump();
    EXPECT_EQ(request("POST", "/api/automations", withId).status, 201);
    EXPECT_EQ(request("POST", "/api/automations", withId).status, 409);

    // -1 is how a time trigger says any hour
    RequestOptions wildcard = admin;
    wildcard.body = nlohmann::json{{"name", "Hourly"},
                                   {"trigger", {{"type", "time"}, {"hour", -1}, {"minute", 0}}},
                                   {"actions", {action}}}.dump();
    EXPECT_EQ(request("POST", "/api/automations", wildcard).status, 201);
}

TEST_F(WebAuthTest, UpdatingAnAutomationKeepsWhatTheRequestLeavesOut) {
    startApp();
    RequestOptions admin;
    admin.cookie = setUpAdmin();

    const nlohmann::json body = {
        {"name", "Morning"},
        {"description", "Lights"},
        {"enabled", false},
        {"condition", {{"leaf", {{"device_id", "x"}, {"property", "on"}, {"op", "eq"},
                                 {"value", true}}}}},
        {"trigger", {{"type", "time"}, {"hour", 8}, {"minute", 0}}},
        {"actions", {{{"deviceId", "light"}, {"property", "on"}, {"value", true}}}},
    };
    RequestOptions create = admin;
    create.body = body.dump();
    const Response created = request("POST", "/api/automations", create);
    ASSERT_EQ(created.status, 201) << created.body;
    const std::string path = "/api/automations/" + created.json()["id"].get<std::string>();

    nlohmann::json update = body;
    update.erase("enabled");
    update.erase("description");
    update.erase("condition");
    update["trigger"]["hour"] = 9;
    RequestOptions put = admin;
    put.body = update.dump();
    ASSERT_EQ(request("PUT", path, put).status, 200);

    const nlohmann::json updated = request("GET", path, admin).json();
    EXPECT_EQ(updated["enabled"], false);
    EXPECT_EQ(updated["description"], "Lights");
    EXPECT_TRUE(updated.contains("condition"));
    EXPECT_EQ(updated["trigger"]["hour"], 9);

    // Sending back what GET returned works, even if an older id wouldn't be allowed now
    update["id"] = "older.id";
    put.body = update.dump();
    EXPECT_EQ(request("PUT", path, put).status, 200);
    update.erase("id");

    update.erase("name");
    put.body = update.dump();
    EXPECT_EQ(request("PUT", path, put).status, 400);

    // Enabling needs to be asked for
    RequestOptions enable = admin;
    enable.body = "{}";
    EXPECT_EQ(request("PUT", path + "/enable", enable).status, 400);
    EXPECT_EQ(request("GET", path, admin).json()["enabled"], false);
}

TEST_F(WebAuthTest, UnknownDeviceTypesAreRejected) {
    startApp();
    RequestOptions admin;
    admin.cookie = setUpAdmin();
    admin.body = R"({"type":"not_a_type","name":"Typo","protocol":"local"})";
    EXPECT_EQ(request("POST", "/api/devices", admin).status, 400);
}

TEST_F(WebAuthTest, OnlyControlPropertiesCanBeWritten) {
    startApp();
    RequestOptions admin;
    admin.cookie = setUpAdmin();

    admin.body = R"({"type":"temperature_sensor","name":"Hall","protocol":"local"})";
    const Response created = request("POST", "/api/devices", admin);
    ASSERT_EQ(created.status, 201) << created.body;
    const std::string sensor = "/api/devices/" + created.json()["id"].get<std::string>();
    admin.body = "";
    const std::string before = request("GET", sensor, admin).json()["state"].dump();

    // Forged readings and stored markup, from any role
    for (const char* body :
         {R"({"temperature":-40})", R"({"motion":true})", R"({"value":"<img src=x onerror=alert(1)>"})",
          R"({"on":"yes"})", R"({"brightness":"<b>"})", R"({"color_h":"x"})",
          R"({"on":true,"value":"x"})", R"({"on":true})", R"({"brightness":50})"}) {
        RequestOptions options = admin;
        options.body = body;
        EXPECT_EQ(request("PUT", sensor, options).status, 400) << body;
    }

    // A rejected property keeps the rest of its request from being applied too
    EXPECT_EQ(request("GET", sensor, admin).json()["state"].dump(), before);
}

TEST_F(WebAuthTest, ControlValuesMustBeInRangeAndSupported) {
    startApp();
    RequestOptions admin;
    admin.cookie = setUpAdmin();
    admin.body = R"({"type":"dimmer","name":"Lamp","protocol":"local"})";
    const Response created = request("POST", "/api/devices", admin);
    ASSERT_EQ(created.status, 201) << created.body;
    const std::string lamp = "/api/devices/" + created.json()["id"].get<std::string>();

    RequestOptions options = admin;
    for (const char* body : {R"({"brightness":50})", R"({"brightness":0})", R"({"brightness":100})",
                             R"({"on":true})"}) {
        options.body = body;
        EXPECT_EQ(request("PUT", lamp, options).status, 200) << body;
    }
    // Out of range, or a capability a dimmer doesn't have
    for (const char* body : {R"({"brightness":101})", R"({"brightness":-5})",
                             R"({"brightness":1e300})", R"({"colorTemp":2700})",
                             R"({"color_r":10})"}) {
        options.body = body;
        EXPECT_EQ(request("PUT", lamp, options).status, 400) << body;
    }
}

TEST_F(WebAuthTest, DevicesCanStillBeControlledAfterARestart) {
    startApp();
    RequestOptions admin;
    admin.cookie = setUpAdmin();
    admin.body = R"({"type":"dimmer","name":"Lamp","protocol":"local"})";
    const Response created = request("POST", "/api/devices", admin);
    ASSERT_EQ(created.status, 201) << created.body;
    const std::string lamp = "/api/devices/" + created.json()["id"].get<std::string>();

    stopApp();
    s_port++;
    startApp();

    RequestOptions options;
    options.cookie = loginCookie(kAdminName, kAdminPassword);
    options.body = R"({"brightness":50})";
    const Response response = request("PUT", lamp, options);
    EXPECT_EQ(response.status, 200) << response.body;
}

TEST_F(WebAuthTest, AllowedHostsAcceptAScalarAndAreNormalized) {
    startApp("security:\n  allowed_hosts: hub.fritz.box:8443\n"
             "  rate_limit_per_minute: 77\n");

    RequestOptions configured(credentials(kAdminName, kAdminPassword));
    configured.host = "hub.fritz.box.:443";
    EXPECT_EQ(request("POST", "/api/setup", configured).status, 201);
}

TEST_F(WebAuthTest, AdministratorCanConfigure) {
    startApp();
    RequestOptions admin;
    admin.cookie = setUpAdmin();
    admin.body = R"({"name":"Den"})";

    EXPECT_EQ(request("POST", "/api/rooms", admin).status, 201);
}

TEST_F(WebAuthTest, RegularUserCanLogOut) {
    startApp();
    setUpAdmin();
    addUser("guest", "Guest-Pass-1", "user");
    RequestOptions guest;
    guest.cookie = loginCookie("guest", "Guest-Pass-1");

    EXPECT_EQ(request("POST", "/api/auth/logout", guest).status, 200);
}

// ---------- Rate limits

TEST_F(WebAuthTest, ConfiguredRateLimitIsEnforced) {
    startApp("security:\n  rate_limit_per_minute: 5\n");

    int limited = 0;
    for (int i = 0; i < 8; ++i) {
        if (request("GET", "/api/system/status").status == 429) {
            limited++;
        }
    }
    EXPECT_EQ(limited, 3);
}

TEST(WebAuthDefaults, RateLimitIsOnAndLeavesRoomForTheDashboard) {
    Config config;
    // While open, the dashboard refreshes every 5 seconds: the devices, the
    // rooms and the automations. Allow for several tabs behind one address.
    const int tabs = 5;
    EXPECT_GE(config.rateLimitPerMinute(), tabs * 12 * 3);
    EXPECT_GT(config.loginMaxFailures(), 0);
}

TEST_F(WebAuthTest, RepeatedLoginFailuresAreThrottled) {
    startApp("security:\n  login_max_failures: 3\n  login_failure_window_seconds: 600\n");
    setUpAdmin();

    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(login(kAdminName, "Wrong-Password-1").status, 401) << i;
    }

    // Now even the right password is refused, or guesses could be confirmed
    const Response throttled = login(kAdminName, kAdminPassword);
    EXPECT_EQ(throttled.status, 429);
    EXPECT_TRUE(throttled.hasHeader("Retry-After:"));
    EXPECT_TRUE(throttled.sessionCookie().empty());

    // Other requests aren't affected
    EXPECT_EQ(request("GET", "/api/system/status").status, 200);
}

TEST_F(WebAuthTest, LoginThrottleEndsWithItsWindow) {
    startApp("security:\n  login_max_failures: 2\n  login_failure_window_seconds: 1\n");
    setUpAdmin();

    EXPECT_EQ(login(kAdminName, "Wrong-Password-1").status, 401);
    EXPECT_EQ(login(kAdminName, "Wrong-Password-1").status, 401);
    EXPECT_EQ(login(kAdminName, kAdminPassword).status, 429);

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_EQ(login(kAdminName, kAdminPassword).status, 200);
}

TEST_F(WebAuthTest, InvalidLoginWindowStillThrottles) {
    startApp("security:\n  login_max_failures: 2\n  login_failure_window_seconds: 0\n");
    setUpAdmin();

    EXPECT_EQ(login(kAdminName, "Wrong-Password-1").status, 401);
    EXPECT_EQ(login(kAdminName, "Wrong-Password-1").status, 401);
    EXPECT_EQ(login(kAdminName, "Wrong-Password-1").status, 429);
}

TEST_F(WebAuthTest, MalformedRequestsDoNotCountAsLoginFailures) {
    startApp("security:\n  login_max_failures: 2\n  login_failure_window_seconds: 600\n");
    setUpAdmin();

    RequestOptions empty;
    empty.body = "{}";
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(request("POST", "/api/auth/login", empty).status, 400);
    }
    EXPECT_EQ(login(kAdminName, kAdminPassword).status, 200);
}

// ---------- Session lifetime

// Needs a look inside the session manager, so this one wires a server by hand.
TEST(WebAuthSessions, ActivityExtendsTheSession) {
    const fs::path dir = fs::temp_directory_path() /
                         ("smarthub_web_session_" + std::to_string(getpid()));
    fs::create_directories(dir);
    {
        Database db((dir / "hub.db").string());
        ASSERT_TRUE(db.initialize());
        EventBus eventBus;
        DeviceManager deviceManager(eventBus, db);
        security::UserManager users(db);
        ASSERT_TRUE(users.initialize());
        security::SessionManager sessions(60);
        const int port = 17600 + (getpid() % 1000);
        WebServer server(eventBus, deviceManager, port, dir.string());
        server.setSecurityManagers(&sessions, nullptr, &users);
        ASSERT_TRUE(server.start());
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        ASSERT_TRUE(users.createUser("owner", kAdminPassword, "admin"));
        const std::string token =
            sessions.createSession(users.getUser("owner")->id, "owner", "admin");
        const uint64_t expiry = sessions.validateSession(token)->expiresAt;

        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        const std::string command = "curl -s -o /dev/null --max-time 10 -H 'Cookie: session=" +
                                    token + "' http://127.0.0.1:" + std::to_string(port) +
                                    "/api/devices";
        ASSERT_EQ(std::system(command.c_str()), 0);

        EXPECT_GT(sessions.validateSession(token)->expiresAt, expiry);
    }
    fs::remove_all(dir);
}
