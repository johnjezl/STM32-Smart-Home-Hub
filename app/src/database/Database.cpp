/**
 * SmartHub Database Implementation
 */

#include <smarthub/database/Database.hpp>
#include <smarthub/core/Logger.hpp>
#include <sqlite3.h>

namespace smarthub {

Database::Database(const std::string& path)
    : m_path(path)
{
}

Database::~Database() {
    close();
}

bool Database::initialize() {
    int rc = sqlite3_open(m_path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to open database %s: %s", m_path.c_str(), sqlite3_errmsg(m_db));
        return false;
    }

    // Enable foreign keys
    execute("PRAGMA foreign_keys = ON");

    // Create schema
    if (!createSchema()) {
        LOG_ERROR("Failed to create database schema");
        close();
        return false;
    }

    LOG_INFO("Database opened: %s", m_path.c_str());
    return true;
}

void Database::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool Database::execute(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        LOG_ERROR("SQL error: %s", errMsg);
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

std::unique_ptr<Database::Statement> Database::prepare(const std::string& sql) {
    return std::make_unique<Statement>(m_db, sql);
}

bool Database::beginTransaction() {
    return execute("BEGIN TRANSACTION");
}

bool Database::commit() {
    return execute("COMMIT");
}

bool Database::rollback() {
    return execute("ROLLBACK");
}

std::string Database::lastError() const {
    if (m_db) {
        return sqlite3_errmsg(m_db);
    }
    return "Database not open";
}

int64_t Database::lastInsertId() const {
    if (m_db) {
        return sqlite3_last_insert_rowid(m_db);
    }
    return 0;
}

bool Database::createSchema() {
    const char* schema = R"SQL(
        -- Devices table
        CREATE TABLE IF NOT EXISTS devices (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            type TEXT NOT NULL,
            protocol TEXT NOT NULL,
            protocol_address TEXT,
            room TEXT,
            config TEXT,
            created_at INTEGER DEFAULT (strftime('%s', 'now')),
            updated_at INTEGER DEFAULT (strftime('%s', 'now'))
        );

        -- Device state (current values)
        CREATE TABLE IF NOT EXISTS device_state (
            device_id TEXT NOT NULL,
            property TEXT NOT NULL,
            value TEXT,
            updated_at INTEGER DEFAULT (strftime('%s', 'now')),
            PRIMARY KEY (device_id, property),
            FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
        );

        -- Sensor history (time series)
        CREATE TABLE IF NOT EXISTS sensor_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            property TEXT NOT NULL,
            value REAL NOT NULL,
            timestamp INTEGER NOT NULL,
            FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
        );
        CREATE INDEX IF NOT EXISTS idx_sensor_history_device_time
            ON sensor_history(device_id, timestamp);

        -- Rooms
        CREATE TABLE IF NOT EXISTS rooms (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            icon TEXT,
            sort_order INTEGER DEFAULT 0
        );

        -- Settings (key-value store)
        CREATE TABLE IF NOT EXISTS settings (
            key TEXT PRIMARY KEY,
            value TEXT
        );

        -- Insert default settings
        INSERT OR IGNORE INTO settings (key, value) VALUES
            ('system.name', 'SmartHub'),
            ('system.timezone', 'UTC'),
            ('display.theme', 'dark'),
            ('display.brightness', '100');
    )SQL";

    return execute(schema);
}

// Statement implementation
Database::Statement::Statement(sqlite3* db, const std::string& sql) {
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &m_stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare statement: %s", sqlite3_errmsg(db));
        m_stmt = nullptr;
    }
}

Database::Statement::~Statement() {
    if (m_stmt) {
        sqlite3_finalize(m_stmt);
    }
}

Database::Statement& Database::Statement::bind(int index, int value) {
    if (m_stmt) {
        sqlite3_bind_int(m_stmt, index, value);
    }
    return *this;
}

Database::Statement& Database::Statement::bind(int index, int64_t value) {
    if (m_stmt) {
        sqlite3_bind_int64(m_stmt, index, value);
    }
    return *this;
}

Database::Statement& Database::Statement::bind(int index, double value) {
    if (m_stmt) {
        sqlite3_bind_double(m_stmt, index, value);
    }
    return *this;
}

Database::Statement& Database::Statement::bind(int index, const std::string& value) {
    if (m_stmt) {
        sqlite3_bind_text(m_stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
    }
    return *this;
}

Database::Statement& Database::Statement::bindNull(int index) {
    if (m_stmt) {
        sqlite3_bind_null(m_stmt, index);
    }
    return *this;
}

bool Database::Statement::execute() {
    if (!m_stmt) return false;
    int rc = sqlite3_step(m_stmt);
    return rc == SQLITE_DONE || rc == SQLITE_ROW;
}

bool Database::Statement::step() {
    if (!m_stmt) return false;
    return sqlite3_step(m_stmt) == SQLITE_ROW;
}

void Database::Statement::reset() {
    if (m_stmt) {
        sqlite3_reset(m_stmt);
        sqlite3_clear_bindings(m_stmt);
    }
}

int Database::Statement::getInt(int column) {
    return m_stmt ? sqlite3_column_int(m_stmt, column) : 0;
}

int64_t Database::Statement::getInt64(int column) {
    return m_stmt ? sqlite3_column_int64(m_stmt, column) : 0;
}

double Database::Statement::getDouble(int column) {
    return m_stmt ? sqlite3_column_double(m_stmt, column) : 0.0;
}

std::string Database::Statement::getString(int column) {
    if (!m_stmt) return "";
    const char* text = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, column));
    return text ? text : "";
}

bool Database::Statement::isNull(int column) {
    return m_stmt ? sqlite3_column_type(m_stmt, column) == SQLITE_NULL : true;
}

int Database::Statement::columnCount() const {
    return m_stmt ? sqlite3_column_count(m_stmt) : 0;
}

std::string Database::Statement::columnName(int column) const {
    if (!m_stmt) return "";
    const char* name = sqlite3_column_name(m_stmt, column);
    return name ? name : "";
}

} // namespace smarthub
