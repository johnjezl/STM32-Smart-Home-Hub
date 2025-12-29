/**
 * Database Unit Tests
 */

#include <gtest/gtest.h>
#include <smarthub/database/Database.hpp>
#include <filesystem>

namespace fs = std::filesystem;

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any existing test database
        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
    }

    void TearDown() override {
        // Clean up test database
        if (fs::exists(testDbPath)) {
            fs::remove(testDbPath);
        }
    }

    const std::string testDbPath = "/tmp/smarthub_test.db";
};

TEST_F(DatabaseTest, OpenCreatesNewDatabase) {
    smarthub::Database db(testDbPath);
    EXPECT_TRUE(db.initialize());
    EXPECT_TRUE(db.isOpen());
    EXPECT_TRUE(fs::exists(testDbPath));
}

TEST_F(DatabaseTest, Close) {
    smarthub::Database db(testDbPath);
    db.initialize();
    EXPECT_TRUE(db.isOpen());
    db.close();
    EXPECT_FALSE(db.isOpen());
}

TEST_F(DatabaseTest, ExecuteCreateTable) {
    smarthub::Database db(testDbPath);
    db.initialize();

    bool result = db.execute(
        "CREATE TABLE test_table ("
        "  id INTEGER PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  value REAL"
        ")"
    );
    EXPECT_TRUE(result);
}

TEST_F(DatabaseTest, ExecuteInsert) {
    smarthub::Database db(testDbPath);
    db.initialize();

    db.execute("CREATE TABLE items (id INTEGER PRIMARY KEY, name TEXT)");
    EXPECT_TRUE(db.execute("INSERT INTO items (name) VALUES ('item1')"));
    EXPECT_TRUE(db.execute("INSERT INTO items (name) VALUES ('item2')"));

    // Verify last insert ID
    EXPECT_GT(db.lastInsertId(), 0);
}

TEST_F(DatabaseTest, ExecuteInvalidSQL) {
    smarthub::Database db(testDbPath);
    db.initialize();

    bool result = db.execute("INVALID SQL STATEMENT");
    EXPECT_FALSE(result);
    EXPECT_FALSE(db.lastError().empty());
}

TEST_F(DatabaseTest, PreparedStatement) {
    smarthub::Database db(testDbPath);
    db.initialize();

    db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");

    auto stmt = db.prepare("INSERT INTO users (name, age) VALUES (?, ?)");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->isValid());

    stmt->bind(1, "Alice").bind(2, 30).execute();
    stmt->reset();
    stmt->bind(1, "Bob").bind(2, 25).execute();
}

TEST_F(DatabaseTest, PreparedStatementQuery) {
    smarthub::Database db(testDbPath);
    db.initialize();

    db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");
    db.execute("INSERT INTO users (name, age) VALUES ('Alice', 30)");
    db.execute("INSERT INTO users (name, age) VALUES ('Bob', 25)");

    auto stmt = db.prepare("SELECT name, age FROM users ORDER BY name");
    ASSERT_NE(stmt, nullptr);

    std::vector<std::pair<std::string, int>> results;
    while (stmt->step()) {
        results.emplace_back(stmt->getString(0), stmt->getInt(1));
    }

    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].first, "Alice");
    EXPECT_EQ(results[0].second, 30);
    EXPECT_EQ(results[1].first, "Bob");
    EXPECT_EQ(results[1].second, 25);
}

TEST_F(DatabaseTest, Transaction) {
    smarthub::Database db(testDbPath);
    db.initialize();

    db.execute("CREATE TABLE counter (value INTEGER)");
    db.execute("INSERT INTO counter VALUES (0)");

    EXPECT_TRUE(db.beginTransaction());
    db.execute("UPDATE counter SET value = 1");
    db.execute("UPDATE counter SET value = 2");
    EXPECT_TRUE(db.commit());

    auto stmt = db.prepare("SELECT value FROM counter");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getInt(0), 2);
}

TEST_F(DatabaseTest, TransactionRollback) {
    smarthub::Database db(testDbPath);
    db.initialize();

    db.execute("CREATE TABLE counter (value INTEGER)");
    db.execute("INSERT INTO counter VALUES (0)");

    EXPECT_TRUE(db.beginTransaction());
    db.execute("UPDATE counter SET value = 99");
    EXPECT_TRUE(db.rollback());

    auto stmt = db.prepare("SELECT value FROM counter");
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->step());
    EXPECT_EQ(stmt->getInt(0), 0);  // Should be unchanged
}

TEST_F(DatabaseTest, SchemaCreation) {
    smarthub::Database db(testDbPath);
    EXPECT_TRUE(db.initialize());

    // Check devices table exists
    auto stmt = db.prepare(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='devices'"
    );
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->step());  // Table should exist
}

TEST_F(DatabaseTest, SensorHistoryTable) {
    smarthub::Database db(testDbPath);
    db.initialize();

    auto stmt = db.prepare(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='sensor_history'"
    );
    ASSERT_NE(stmt, nullptr);
    EXPECT_TRUE(stmt->step());  // Table should exist
}

TEST_F(DatabaseTest, PrepareInvalidSQL) {
    smarthub::Database db(testDbPath);
    db.initialize();

    auto stmt = db.prepare("SELECT * FROM nonexistent_table");
    // Either nullptr or invalid statement
    if (stmt != nullptr) {
        EXPECT_FALSE(stmt->step());  // Should fail to execute
    }
}

TEST_F(DatabaseTest, LastError) {
    smarthub::Database db(testDbPath);
    db.initialize();

    db.execute("INVALID SQL");
    std::string error = db.lastError();
    EXPECT_FALSE(error.empty());
}

TEST_F(DatabaseTest, BindNull) {
    smarthub::Database db(testDbPath);
    db.initialize();

    db.execute("CREATE TABLE test (id INTEGER, value TEXT)");

    auto stmt = db.prepare("INSERT INTO test VALUES (?, ?)");
    ASSERT_NE(stmt, nullptr);
    stmt->bind(1, 1).bindNull(2).execute();

    auto query = db.prepare("SELECT value FROM test WHERE id = 1");
    ASSERT_NE(query, nullptr);
    EXPECT_TRUE(query->step());
    EXPECT_TRUE(query->isNull(0));
}
