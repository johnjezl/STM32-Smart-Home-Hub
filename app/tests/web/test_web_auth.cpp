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
#include <smarthub/devices/DeviceManager.hpp>
#include <smarthub/security/ApiTokenManager.hpp>
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

    void startApp(const std::string& securitySection = "", const std::string& certPath = "") {
        const fs::path configPath = m_dir / "config.yaml";
        std::ofstream config(configPath);
        config << "database:\n  path: " << (m_dir / "hub.db").string() << "\n"
               << "mqtt:\n  broker: 127.0.0.1\n  port: 1\n"
               << "web:\n  port: " << s_port << "\n"
               << "  root: " << (m_dir / "www").string() << "\n"
               << "  tls:\n    cert: \"" << certPath << "\"\n"
               << "    key: \"" << certPath << "\"\n"
               << "rpmsg:\n  device: " << (m_dir / "no-rpmsg").string() << "\n"
               << "zigbee:\n  enabled: false\n"
               << "logging:\n  level: error\n  file: " << (m_dir / "hub.log").string() << "\n"
               << securitySection;
        config.close();

        m_app = std::make_unique<Application>(configPath.string());
        ASSERT_TRUE(m_app->initialize());
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    void stopApp() {
        if (m_app) {
            m_app.reset();
        }
    }

    Response request(const std::string& method, const std::string& path,
                     const RequestOptions& options = {}) const {
        const fs::path headersFile = m_dir / "response-headers";
        std::string command = "curl -s --max-time 10 -o - -D " +
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
        command += " " + shellQuote("http://127.0.0.1:" + std::to_string(s_port) + path) +
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
    EXPECT_EQ(request("POST", "/api/auth/logout").status, 401);
}

TEST_F(WebAuthTest, PublicRoutesNeedNoCredentials) {
    startApp();
    setUpAdmin();

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

TEST_F(WebAuthTest, SetupAcceptsLocalNames) {
    for (const char* host : {"192.168.1.5", "192.168.1.5:8443", "smarthub.local",
                             "SmartHub.LOCAL:443", "smarthub", "localhost:8080", "[fe80::1]:443"}) {
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

TEST_F(WebAuthTest, CookieIsNotSecureWhenTlsFellBackToHttp) {
    // A configured certificate that can't be read leaves the server on plain
    // HTTP, where a browser would throw away a Secure cookie.
    startApp("", (m_dir / "missing.pem").string());
    setUpAdmin();

    const Response response = login(kAdminName, kAdminPassword);
    ASSERT_EQ(response.status, 200);
    EXPECT_FALSE(response.hasHeader("Secure"));
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
