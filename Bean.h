//
// Created by Acane on 2021/12/30.
//

#ifndef SQLITE3TRANS_BEAN_H
#define SQLITE3TRANS_BEAN_H

#include "sqlite3.h"
#include "ac_common.h"
#include <iostream>
#include <typeinfo>
#include <tuple>
#include <vector>
#include <sstream>

template <int Index>
struct Accessor {
    static constexpr int index_of = Index;
};

template <class T>
struct function_helper;

template <class T>
typename std::enable_if<std::is_integral_v<T>, void>::type
fill_value(sqlite3_stmt* stmt, int index, const T& val) {
    LOGD("          value: %d", val);
    if (stmt) {
        LOGD("    filled INTEGER");
        sqlite3_bind_int64(stmt, index + 1, (sqlite3_int64)val);
    }
}

template <class T>
typename std::enable_if<std::is_floating_point_v<T>, void>::type
fill_value(sqlite3_stmt* stmt, int index, const T& val) {
    LOGD("          value: %f", val);
    if (stmt) {
        LOGD("    filled REAL");
        sqlite3_bind_double(stmt, index + 1, val);
    }
}

template <class T>
typename std::enable_if<std::is_same_v<T, const char*> or std::is_same_v<T, char*>, void>::type
fill_value(sqlite3_stmt* stmt, int index, const T& val) {
    if (!val) {
        LOGD("          no value present");
        LOGD("    filled NULL");
        sqlite3_bind_null(stmt, index + 1);
        return;
    }
    LOGD("          value: %s", val);
    if (stmt) {
        LOGD("    filled TEXT");
        sqlite3_bind_text(stmt, index + 1, val, -1, nullptr);
    }
}

template <class T>
typename std::enable_if<!std::is_same_v<T, const char*> and !std::is_integral_v<T>
and !std::is_floating_point_v<T>, void>::type
fill_value(sqlite3_stmt* stmt, int index, const T& val) {
    LOGD("fill value with unknown type");
    LOGD("  value:  %s", std::to_string(val));
    std::string str = std::to_string(val);
    if (stmt) {
        char* cstr = (char*)malloc(sizeof(char) * str.size() + 1);
        strcpy(cstr, str.c_str());
        sqlite3_bind_text(stmt, index + 1, cstr, str.size(), free);
    }
}

template <int N, class BeanAdapter, class ArgsTupleTy>
typename std::enable_if<N >= std::tuple_size<ArgsTupleTy>::value, void>::type
sql_formatter(sqlite3_stmt* stmt, typename BeanAdapter::bean_type* bean, const ArgsTupleTy& args) {
    // do nothing
    LOGD("done");
    LOGD("tuple size: %d", std::tuple_size<ArgsTupleTy>::value);
}

template <int N, class BeanAdapter, class ArgsTupleTy>
typename std::enable_if<N < std::tuple_size<ArgsTupleTy>::value, void>::type
sql_formatter(sqlite3_stmt* stmt, typename BeanAdapter::bean_type* bean, const ArgsTupleTy& args) {
    LOGD("arg #%d", N);
    BeanAdapter adapter;
    using accessor_ty = std::remove_reference_t<decltype(std::get<N>(args))>;
    if constexpr (std::is_integral_v<accessor_ty> || std::is_same_v<accessor_ty, const char*>
                  || std::is_floating_point_v<accessor_ty>) {
        LOGD("  immediate value");
        fill_value(stmt, N, std::get<N>(args));
    }
    else {
        constexpr int I = accessor_ty::index_of;
        using return_type_t = decltype(adapter.template get_member<I>(bean));
        const char *name = typeid(return_type_t).name();
        const char *field_name = adapter.template get_field_name<I>();
        LOGD("  class name: %s", name);
        LOGD("  accessor name: %s", typeid(accessor_ty).name());
        LOGD("          value: %d", accessor_ty::index_of);
        LOGD("  field name: %s", field_name);
        fill_value(stmt, N, adapter.template get_member<I>(bean));
    }
    sql_formatter<N + 1, BeanAdapter, ArgsTupleTy>(stmt, bean, args);
}

template <class BeanAdapter, class ...Args>
void sql_formatter(sqlite3_stmt* stmt, typename BeanAdapter::bean_type* bean, Args... args) {
    sql_formatter<0, BeanAdapter>(stmt, bean, std::forward_as_tuple(args...));
}

enum {
    BEAN_TYPE_TEXT = 1,
    BEAN_TYPE_INT = 2,
    BEAN_TYPE_NUMBER = 3,
    BEAN_TYPE_OTHER = 4
};

struct BeanFieldInfo {
    std::string name;
    int type;
};

template <class BeanAdapter, int I, int N>
typename std::enable_if<I >= N, void>::type
get_structure(std::vector<BeanFieldInfo>& fields) {
    // do nothing
    LOGD("Get structute done");
}

#define AC_IS_SAME_TYPE(x, y) !strcmp(typeid(x).name(), typeid(y).name())

template <class BeanAdapter, int I, int N>
typename std::enable_if<I < N, void>::type
get_structure(std::vector<BeanFieldInfo>& fields) {
    BeanFieldInfo field;
    BeanAdapter adapter;
    typename BeanAdapter::bean_type bean;
    using return_type_t = decltype(adapter.template get_member<I>(&bean));
    const char *name = typeid(return_type_t).name();
    const char *field_name = adapter.template get_field_name<I>();
    LOGD("  class name: %s", name);
    LOGD("  field name: %s", field_name);
    int type = 0;
    if (AC_IS_SAME_TYPE(return_type_t, const char*) || AC_IS_SAME_TYPE(return_type_t, char*)) {
        type = BEAN_TYPE_TEXT;
    }
    else if (AC_IS_SAME_TYPE(return_type_t, int8_t) || AC_IS_SAME_TYPE(return_type_t, uint8_t) ||
             AC_IS_SAME_TYPE(return_type_t, int16_t) || AC_IS_SAME_TYPE(return_type_t, uint16_t) ||
             AC_IS_SAME_TYPE(return_type_t, int32_t) || AC_IS_SAME_TYPE(return_type_t, uint32_t) ||
             AC_IS_SAME_TYPE(return_type_t, int64_t) || AC_IS_SAME_TYPE(return_type_t, uint64_t) ) {
        type = BEAN_TYPE_INT;
    }
    else if (AC_IS_SAME_TYPE(return_type_t, float) || AC_IS_SAME_TYPE(return_type_t, double)) {
        type = BEAN_TYPE_NUMBER;
    }
    else {
        type = BEAN_TYPE_OTHER;
    }
    field.type = type;
    field.name = field_name;
    fields.emplace_back(field);
    get_structure<BeanAdapter, I+1, N>(fields);
}


template <class BeanAdapter>
void get_structure(std::vector<BeanFieldInfo>& fields) {
    constexpr int N = BeanAdapter::N;
    get_structure<BeanAdapter, 0, N>(fields);
}

template <class BeanAdapter>
std::string get_sql_create_table() {
    std::vector<BeanFieldInfo> fields;
    get_structure<BeanAdapter>(fields);

    std::stringstream ss;
    const char* table_name = BeanAdapter::table_name;
    static const char* type_names[5] = { "", "TEXT", "INTEGER", "REAL", "BLOB" };
    ss << "create table if not exists " << table_name << " (";
    for (int i=0; i<fields.size(); i++) {
        const auto& f = fields[i];
        LOGD("  Type: %d     Field: %s", f.type, f.name.c_str());
        ss << f.name << "  " << type_names[f.type];
        if (i == 0) ss << " PRIMARY KEY AUTOINCREMENT";
        if (i != fields.size() - 1) ss << ", ";
    }
    ss << ")";
    return std::move(ss.str());
}

template <class BeanAdapter>
std::string get_sql_insert_into() {
    std::vector<BeanFieldInfo> fields;
    get_structure<BeanAdapter>(fields);

    std::stringstream ss;
    const char* table_name = BeanAdapter::table_name;
    ss << "insert into " << table_name << " (";
    for (int i=0; i<fields.size(); i++) {
        const auto& f = fields[i];
        LOGD("  Type: %d     Field: %s", f.type, f.name.c_str());
        ss << f.name;
        if (i != fields.size() - 1) ss << ", ";
    }
    ss << ") values (";
    for (int i=0; i<fields.size(); i++) {
        ss << "?";
        if (i != fields.size() - 1) ss << ", ";
    }
    ss << ")";
    return std::move(ss.str());
}


template <class BeanAdapter>
std::string get_sql_update_set_clause() {
    std::vector<BeanFieldInfo> fields;
    get_structure<BeanAdapter>(fields);

    std::stringstream ss;
    const char* table_name = BeanAdapter::table_name;
    ss << "update " << table_name << " set ";
    for (int i=0; i<fields.size(); i++) {
        const auto& f = fields[i];
        LOGD("  Type: %d     Field: %s", f.type, f.name.c_str());
        ss << f.name << "=?";
        if (i != fields.size() - 1) ss << ", ";
    }
    return std::move(ss.str());
}

///  Fill with data

template <class BeanAdapter, int I, int N>
typename std::enable_if<I >= N, void>::type
fill_with_data(sqlite3_stmt* stmt, typename BeanAdapter::bean_type* bean) {
    // do nothing
    LOGD("Get structute done");
}

template <class BeanAdapter, int I, int N>
typename std::enable_if<I < N, void>::type
fill_with_data(sqlite3_stmt* stmt, typename BeanAdapter::bean_type* bean) {
    BeanFieldInfo field;
    BeanAdapter adapter;
    using return_type_t = decltype(adapter.template get_member<I>(bean));
    const char *name = typeid(return_type_t).name();
    const char *field_name = adapter.template get_field_name<I>();
    LOGD("  class name: %s", name);
    LOGD("  field name: %s", field_name);
    if (!strcmp(field_name, "id"))
        fill_value(stmt, I, (const char*)nullptr);
    else
        fill_value(stmt, I, adapter.template get_member<I>(bean));
    fill_with_data<BeanAdapter, I+1, N>(stmt, bean);
}

template <class BeanAdapter>
void fill_with_data(sqlite3_stmt* stmt, typename BeanAdapter::bean_type* bean) {
    constexpr int N = BeanAdapter::N;
    fill_with_data<BeanAdapter, 0, N>(stmt, bean);
}

/// Fill object
template <class BeanAdapter, int I, int N>
typename std::enable_if<I >= N, void>::type
fill_object(sqlite3_stmt* stmt, typename BeanAdapter::bean_type* bean) {
    // do nothing
    LOGD("Get fill_object done");
}

template <class T>
char* copy_to_new_buffer(const T* str) {
    char* buf = (char*)malloc(strlen(str) + 5);
    if (!buf) {
        LOGE("allocate memory failed");
        return nullptr;
    }
    strcpy(buf, str);
    return buf;
}

template <class BeanAdapter, int I, int N>
typename std::enable_if<I < N, void>::type
fill_object(sqlite3_stmt* stmt, typename BeanAdapter::bean_type* bean) {
    BeanFieldInfo field;
    BeanAdapter adapter;
    using return_type_t = std::remove_reference_t<decltype(adapter.template get_member<I>(bean))>;
    const char *name = typeid(return_type_t).name();
    const char *field_name = adapter.template get_field_name<I>();
    LOGD("  class name: %s", name);
    LOGD("  field name: %s", field_name);
    if constexpr (std::is_same_v<return_type_t, const char*> || std::is_same_v<return_type_t, char*>) {
        const char* result = (const char*)sqlite3_column_text(stmt, I);
        LOGD("Set as TEXT, value=%s", result);
        adapter.template get_member<I>(bean) = copy_to_new_buffer(result);
    }
    else if constexpr (std::is_integral_v<return_type_t>) {
        LOGD("Set as INTEGER");
        sqlite3_int64 result = sqlite3_column_int64(stmt, I);
        adapter.template get_member<I>(bean) = result;
    }
    else if constexpr (std::is_floating_point_v<return_type_t>) {
        LOGD("Set as REAL");
        double result = sqlite3_column_double(stmt, I);
        adapter.template get_member<I>(bean) = result;
    }
    else {
        LOGD("Set as BLOB");
        int length = sqlite3_column_bytes(stmt, I);
        const void* content = sqlite3_column_blob(stmt, I);
        void* dst = (void*)&adapter.template get_member<I>(bean);
        memcpy(dst, content, length);
    }
    fill_object<BeanAdapter, I+1, N>(stmt, bean);
}

template <class BeanAdapter>
bool fill_object(sqlite3_stmt* stmt, typename BeanAdapter::bean_type* bean) {
    constexpr int N = BeanAdapter::N;
    int column_count = sqlite3_column_count(stmt);
    if (column_count != N) {
        LOGE("column count does not match: with [ColumnCount=%d] and [N=%d]", column_count, N);
        //return false;
    }
    fill_object<BeanAdapter, 0, N>(stmt, bean);
    return true;
}

// ==========================================
// Macros to define an adapter
// ==========================================
/*
 * Usage:
 *
 * typedef struct ACANE_DICTIONARY {
 *     int id;
 *     const char* from;
 *     const char* to;
 *     const char* code;
 *     int enabled;
 * } ac_dictionary_t;
 *
 * START_DEFINE_BEAN_ADAPTER(ac_dictionary_t, 5)
 *     DEFINE_TABLE_FIELD(0, id)
 *     DEFINE_TABLE_FIELD(1, from)
 *     DEFINE_TABLE_FIELD(2, to)
 *     DEFINE_TABLE_FIELD(3, code)
 *     DEFINE_TABLE_FIELD(4, enabled)
 * END_DEFINE_BEAN_ADAPTER(ac_dictionary_t)
 *
 * int main() {
 *     LOGD("SQL CREATE TABLE is: %s",
 *         get_sql_create_table<ac_dictionary_t_adapter_t>().c_str());
 * }
 */
#define DEFINE_TABLE_NAME(tbl_name)  static constexpr const char * table_name = #tbl_name ;
#define FOR_BEAM_TYPE(beam_ty) using bean_type = beam_ty;
#define DEFINE_MEMBER_COUNT(count)  const static int N = count;
#define DEFINE_ID() DEFINE_TABLE_FIELD(0, id)
#define DEFINE_TABLE_FIELD(index, name) \
    template <int I>                   \
    auto& get_member(typename std::enable_if<I == (index), bean_type*>::type bean) {\
        return bean->name;\
    }                                  \
    template <int I>                   \
    typename std::enable_if<I == (index), const char*>::type get_field_name() {           \
        return #name ;                 \
    }                                  \
    static_assert(index < N);

#define START_DEFINE_BEAN_ADAPTER(bean_name, member_count) \
struct bean_name##_adapter_t {                             \
    DEFINE_MEMBER_COUNT(member_count)        \
    DEFINE_TABLE_NAME(bean_name)             \
    FOR_BEAM_TYPE(bean_name)

#define END_DEFINE_BEAN_ADAPTER(bean_name) \
    static_assert(std::is_same_v<bean_type, bean_name>); \
    static_assert(N > 0);                  \
};

#endif //SQLITE3TRANS_BEAN_H
