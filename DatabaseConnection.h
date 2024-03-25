//
// Created by Acane on 2021/12/30.
//

#ifndef SQLITE3TRANS_DATABASECONNECTION_H
#define SQLITE3TRANS_DATABASECONNECTION_H

#include "sqlite3.h"
#include "Bean.h"
#include <iostream>
#include <functional>
#include <cstring>
#include <unistd.h>

enum SqliteResult {
  kSqliteSuccess = 0,
  kSqliteFailed,
  kSqliteNoData,
};

class DatabaseConnection {
  sqlite3 *db;
 public:
  bool is_new_db = false;
  bool connect_success = false;

  DatabaseConnection(const char *db_path) {
    is_new_db = access(db_path, F_OK) == 0;
    if (sqlite3_open(db_path, &db)) {
      SMART_LOGD("Cannot open database: %s", db_path);
      const char *message = sqlite3_errmsg(db);
      SMART_LOGD("  Reason: %s", message);
      sqlite3_free((void *) message);
      return;
    }
    SMART_LOGD("Database connected: %s", db_path);
    connect_success = true;
  }

  ~DatabaseConnection() { sqlite3_close(db); }

  [[nodiscard]] sqlite3 *get_connection() const { return db; }
};

int SQLITE_COMMON_CALLBACK_F(void *_, int argc, char **argv, char **azColName);

template<class BeanAdapter>
class ITable {
  DatabaseConnection *conn;

  sqlite3 *get_connection() {
    if (conn == nullptr) {
      SMART_LOGE("error: no connection taken");
      return nullptr;
    }
    return conn->get_connection();
  }

  SqliteResult _execute_sql(const char *sql) {
    SMART_LOGD("Executing SQL: %s", sql);
    auto db = get_connection();
    if (!db) {
      return kSqliteFailed;
    }
    char *errMsg = nullptr;
    int code = sqlite3_exec(db, sql, SQLITE_COMMON_CALLBACK_F, nullptr, &errMsg);
    if (code != SQLITE_OK) {
      SMART_LOGE("SQL error: %s", errMsg);
      sqlite3_free(errMsg);
      return kSqliteFailed;
    }
    return kSqliteSuccess;
  }

  SqliteResult _execute_sql(const char *prepare_sql,
                            const std::function<void(sqlite3_stmt *)> &fill_placeholder,
                            const std::function<void(sqlite3_stmt *)> &interprete_row) {
    SMART_LOGD("Executing SQL: %s", prepare_sql);
    auto db = get_connection();
    if (!db) {
      SMART_LOGE("Invalid database conenction");
      return kSqliteFailed;
    }
    const char *pzTail = nullptr;
    sqlite3_stmt *stmt = nullptr;
    sqlite3_prepare(db, prepare_sql, strlen(prepare_sql), &stmt, &pzTail);
    if (!stmt) {
      SMART_LOGE("stmt == nullptr");
      return kSqliteFailed;
    }
    fill_placeholder(stmt);
    char *sql = sqlite3_expanded_sql(stmt);
    SMART_LOGD(" Actual sql is: %s", sql);
    int ret;
    while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
      if (ret == SQLITE_ERROR) {
        const char *message = sqlite3_errmsg(db);
        SMART_LOGD("SQL Error: %s", message);
        sqlite3_finalize(stmt);
        return kSqliteFailed;
      }
      interprete_row(stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_free(sql);
    return kSqliteSuccess;
  }

  std::string construct_sql_select_all_with_cond(const char *cond) {
    std::stringstream ss;
    const char *table_name = BeanAdapter::table_name;
    ss << "select * from " << table_name << " where " << cond;
    return std::move(ss.str());
  }

  std::string construct_sql_delete_with_cond(const char *cond) {
    std::stringstream ss;
    const char *table_name = BeanAdapter::table_name;
    ss << "delete from " << table_name << " where " << cond;
    return std::move(ss.str());
  }

 public:
  const char *get_table_name() {
    const char *table_name = BeanAdapter::table_name;
    return table_name;
  }

  SqliteResult take_connection(DatabaseConnection *_conn) {
    if (!_conn->connect_success) {
      SMART_LOGE("Cannot a take this connection");
      return kSqliteFailed;
    }
    this->conn = _conn;
    SMART_LOGD("Is new DB: false");
    return execute_create_table();
  }

  template<class... Args>
  SqliteResult execute_sql(const char *sql_fmt, Args... args) {
    return _execute_sql(sql_fmt,
                        [&](sqlite3_stmt *stmt) {
                          sql_formatter<BeanAdapter>(stmt, nullptr, std::forward<Args>(args)...);
                        },
                        [&](sqlite3_stmt *stmt) {});
  }

  template<class... Args>
  SqliteResult query_sql(std::vector<typename BeanAdapter::bean_type> &bean_list, const char *sql_fmt, Args... args) {
    return _execute_sql(sql_fmt,
                        [&](sqlite3_stmt *stmt) {
                          sql_formatter<BeanAdapter>(stmt, nullptr, std::forward<Args>(args)...);
                        },
                        [&](sqlite3_stmt *stmt) {
                          typename BeanAdapter::bean_type bean;
                          fill_object<BeanAdapter>(stmt, &bean);
                          bean_list.emplace_back(std::move(bean));
                        });
  }

  SqliteResult execute_create_table() {
    std::string sql = get_sql_create_table<BeanAdapter>();
    return _execute_sql(sql.c_str());
  }

  SqliteResult insert(const typename BeanAdapter::bean_type &bean) {
    std::string sql = get_sql_insert_into<BeanAdapter>();
    return _execute_sql(sql.c_str(),
                        [&](sqlite3_stmt *stmt) {
                          fill_with_data<BeanAdapter>(stmt, const_cast<typename BeanAdapter::bean_type *>(&bean));
                        },
                        [&](sqlite3_stmt *stmt) {});
  }

  SqliteResult get_by_id(int id, typename BeanAdapter::bean_type *bean) {
    std::vector<typename BeanAdapter::bean_type> bean_list;
    std::string sql = construct_sql_select_all_with_cond("id=?");
    SqliteResult ret = query_sql(bean_list, sql.c_str(), id);
    if (ret != kSqliteSuccess)
      return ret;
    if (bean_list.size() == 0)
      return kSqliteNoData;
    *bean = std::move(bean_list[0]);
    return kSqliteSuccess;
  }

  template<class ...Args>
  SqliteResult
  get_by_cond(std::vector<typename BeanAdapter::bean_type> &bean_list, const char *cond_fmt, Args... args) {
    std::string sql = construct_sql_select_all_with_cond(cond_fmt);
    return query_sql(bean_list, sql.c_str(), std::forward<Args>(args)...);
  }

  SqliteResult remove_by_id(int id) {
    std::string sql = construct_sql_delete_with_cond("id = ?");
    return execute_sql(sql.c_str(), id);
  }

  template<class... Args>
  SqliteResult remove_by_cond(const char *cond_sql_fmt, Args... args) {
    std::string sql = construct_sql_delete_with_cond(cond_sql_fmt);
    return execute_sql(sql.c_str(), std::forward<Args>(args)...);
  }

  SqliteResult update(typename BeanAdapter::bean_type &bean) {
    std::string set_clause = get_sql_update_set_clause<BeanAdapter>();
    std::string where_clause = " where id = ?";
    std::string sql = set_clause + where_clause;
    return _execute_sql(sql.c_str(),
                        [&](sqlite3_stmt *stmt) {
                          fill_with_data<BeanAdapter>(stmt, const_cast<typename BeanAdapter::bean_type *>(&bean));
                          sqlite3_bind_int64(stmt, 1, bean.id);
                          sqlite3_bind_int64(stmt, BeanAdapter::N + 1, bean.id);
                        },
                        [&](sqlite3_stmt *stmt) {});
  }
};

#endif //SQLITE3TRANS_DATABASECONNECTION_H
