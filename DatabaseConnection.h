//
// Created by Acane on 2021/12/30.
//

#ifndef SQLITE3TRANS_DATABASECONNECTION_H
#define SQLITE3TRANS_DATABASECONNECTION_H

#include "sqlite3.h"
#include "ac_common.h"
#include "Bean.h"
#include <iostream>
#include <filesystem>

class DatabaseConnection {
    sqlite3* db;
public:
    bool is_new_db = false;
    bool connect_success = false;

    DatabaseConnection(const char* db_path) {
        is_new_db = !std::filesystem::exists(db_path);
        if (sqlite3_open(db_path, &db)) {
            LOGD("Cannot open database: %s", db_path);
            const char* message = sqlite3_errmsg(db);
            LOGD("  Reason: %s", message);
            sqlite3_free((void*)message);
            return;
        }
        LOGD("Database connected: %s", db_path);
        connect_success = true;
    }
    [[nodiscard]] sqlite3* get_connection() const { return db; }
};

int SQLITE_COMMON_CALLBACK_F(void* _, int argc, char** argv, char** azColName);

template <class BeanAdapter>
class ITable {
    DatabaseConnection* conn;

    sqlite3* get_connection() {
        if (conn == nullptr) {
            LOGE("error: no connection taken");
            return nullptr;
        }
        return conn->get_connection();
    }

    ac_result_t _execute_sql(const char* sql) {
        LOGD("Executing SQL: %s", sql);
        auto db = get_connection();
        if (!db) {
            return AC_FAIL;
        }
        char* errMsg = nullptr;
        int code = sqlite3_exec(db, sql, SQLITE_COMMON_CALLBACK_F, nullptr, &errMsg);
        if (code != SQLITE_OK) {
            LOGE("SQL error: %s", errMsg);
            sqlite3_free(errMsg);
            return AC_FAIL;
        }
        return AC_OK;
    }

    ac_result_t _execute_sql(const char* prepare_sql,
                             const std::function<void(sqlite3_stmt*)>& fill_placeholder,
                             const std::function<void(sqlite3_stmt*)>& interprete_row) {
        LOGD("Executing SQL: %s", prepare_sql);
        auto db = get_connection();
        if (!db) {
            LOGE("Invalid database conenction");
            return AC_FAIL;
        }
        const char* pzTail = nullptr;
        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare(db, prepare_sql, strlen(prepare_sql), &stmt, &pzTail);
        if (!stmt) {
            LOGE("stmt == nullptr");
            return AC_FAIL;
        }
        fill_placeholder(stmt);
        char* sql = sqlite3_expanded_sql(stmt);
        LOGD(" Actual sql is: %s", sql);
        int ret;
        while ((ret = sqlite3_step(stmt)) != SQLITE_DONE) {
            if (ret == SQLITE_ERROR) {
                const char* message = sqlite3_errmsg(db);
                LOGD("SQL Error: %s", message);
                sqlite3_finalize(stmt);
                return AC_FAIL;
            }
            interprete_row(stmt);
        }
        sqlite3_finalize(stmt);
        return AC_OK;
    }

    std::string construct_sql_select_all_with_cond(const char* cond) {
        std::stringstream ss;
        const char* table_name = BeanAdapter::table_name;
        ss << "select * from " << table_name << " where " << cond;
        return std::move(ss.str());
    }

    std::string construct_sql_delete_with_cond(const char* cond) {
        std::stringstream ss;
        const char* table_name = BeanAdapter::table_name;
        ss << "delete from " << table_name << " where " << cond;
        return std::move(ss.str());
    }

public:
    const char* get_table_name() {
        const char* table_name = BeanAdapter::table_name;
        return table_name;
    }

    ac_result_t take_connection(DatabaseConnection* _conn) {
        if (!_conn->connect_success) {
            LOGE("Cannot a take an unavalibe connection");
            return AC_FAIL;
        }
        this->conn = _conn;
        if (_conn->is_new_db) {
            return execute_create_table();
        }
        return AC_OK;
    }

    template <class... Args>
    ac_result_t execute_sql(const char* sql_fmt, Args... args) {
        return _execute_sql(sql_fmt,
                            [&] (sqlite3_stmt* stmt) {
                                sql_formatter<BeanAdapter>(stmt, nullptr, std::forward<Args>(args)...);
                            },
                            [&] (sqlite3_stmt* stmt) {  });
    }

    template <class... Args>
    ac_result_t query_sql(std::vector<typename BeanAdapter::bean_type>& bean_list, const char* sql_fmt, Args... args) {
        return _execute_sql(sql_fmt,
                            [&] (sqlite3_stmt* stmt) {
                                sql_formatter<BeanAdapter>(stmt, nullptr, std::forward<Args>(args)...);
                            },
                            [&] (sqlite3_stmt* stmt) {
                                typename BeanAdapter::bean_type bean;
                                fill_object<BeanAdapter>(stmt, &bean);
                                bean_list.emplace_back(std::move(bean));
                            });
    }

    ac_result_t execute_create_table() {
        std::string sql = get_sql_create_table<BeanAdapter>();
        return _execute_sql(sql.c_str());
    }

    ac_result_t insert(const typename BeanAdapter::bean_type& bean) {
        std::string sql = get_sql_insert_into<BeanAdapter>();
        return _execute_sql(sql.c_str(),
                            [&] (sqlite3_stmt* stmt) {
                                fill_with_data<BeanAdapter>(stmt, const_cast<typename BeanAdapter::bean_type*>(&bean));
                            },
                            [&] (sqlite3_stmt* stmt) {  });
    }

    ac_result_t get_by_id(int id, typename BeanAdapter::bean_type* bean) {
        std::vector< typename BeanAdapter::bean_type> bean_list;
        std::string sql = construct_sql_select_all_with_cond("id=?");
        ac_result_t ret = query_sql(bean_list, sql.c_str(), id);
        if (ret != AC_OK)
            return ret;
        if (bean_list.size() == 0)
            return AC_FAIL; // todo: change it to AC_NOTFOUND
        *bean = std::move(bean_list[0]);
        return AC_OK;
    }

    template <class ...Args>
    ac_result_t get_by_cond(std::vector<typename BeanAdapter::bean_type>& bean_list, const char* cond_fmt, Args... args) {
        std::string sql = construct_sql_select_all_with_cond(cond_fmt);
        return query_sql(bean_list, sql.c_str(), std::forward<Args>(args)...);
    }

    ac_result_t remove_by_id(int id) {
        std::string sql = construct_sql_delete_with_cond("id = ?");
        return execute_sql(sql.c_str(), id);
    }

    template <class... Args>
    ac_result_t remove_by_cond(const char* cond_sql_fmt, Args... args) {
        std::string sql = construct_sql_delete_with_cond(cond_sql_fmt);
        return execute_sql(sql.c_str(), std::forward<Args>(args)...);
    }

    ac_result_t update(typename BeanAdapter::bean_type& bean) {
        std::string set_clause = get_sql_update_set_clause<BeanAdapter>();
        std::string where_clause = " where id = ?";
        std::string sql = set_clause + where_clause;
        return _execute_sql(sql.c_str(),
                            [&] (sqlite3_stmt* stmt) {
                                fill_with_data<BeanAdapter>(stmt, const_cast<typename BeanAdapter::bean_type*>(&bean));
                                sqlite3_bind_int64(stmt, 1, bean.id);
                                sqlite3_bind_int64(stmt, BeanAdapter::N + 1 ,bean.id);
                            },
                            [&] (sqlite3_stmt* stmt) {  });
    }
};

#endif //SQLITE3TRANS_DATABASECONNECTION_H
