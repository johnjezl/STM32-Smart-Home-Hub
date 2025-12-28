/**
 * SmartHub Database
 *
 * SQLite database wrapper for persistent storage.
 */
#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <optional>

// Forward declaration
struct sqlite3;
struct sqlite3_stmt;

namespace smarthub {

/**
 * SQLite database wrapper
 */
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    // Non-copyable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /**
     * Initialize database and create schema
     */
    bool initialize();

    /**
     * Close database connection
     */
    void close();

    /**
     * Check if database is open
     */
    bool isOpen() const { return m_db != nullptr; }

    /**
     * Execute SQL statement without results
     */
    bool execute(const std::string& sql);

    /**
     * Prepared statement wrapper
     */
    class Statement {
    public:
        Statement(sqlite3* db, const std::string& sql);
        ~Statement();

        bool isValid() const { return m_stmt != nullptr; }

        Statement& bind(int index, int value);
        Statement& bind(int index, int64_t value);
        Statement& bind(int index, double value);
        Statement& bind(int index, const std::string& value);
        Statement& bindNull(int index);

        bool execute();
        bool step();
        void reset();

        int getInt(int column);
        int64_t getInt64(int column);
        double getDouble(int column);
        std::string getString(int column);
        bool isNull(int column);

        int columnCount() const;
        std::string columnName(int column) const;

    private:
        sqlite3_stmt* m_stmt = nullptr;
    };

    /**
     * Create prepared statement
     */
    std::unique_ptr<Statement> prepare(const std::string& sql);

    /**
     * Transaction support
     */
    bool beginTransaction();
    bool commit();
    bool rollback();

    /**
     * Get last error message
     */
    std::string lastError() const;

    /**
     * Get last insert rowid
     */
    int64_t lastInsertId() const;

private:
    bool createSchema();

    std::string m_path;
    sqlite3* m_db = nullptr;
};

} // namespace smarthub
