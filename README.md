# SQLite Bean Warpper

```c++
#include <iostream>
#include "sqlite3.h"
#include "DatabaseConnection.h"

typedef struct ACANE_DICTIONARY {
    int id;
    const char* src;
    const char* dst;
    const char* code;
    int enabled;
} ac_dictionary_t;

START_DEFINE_BEAN_ADAPTER(ac_dictionary_t, 5)
DEFINE_TABLE_FIELD(0, id)
DEFINE_TABLE_FIELD(1, src)
DEFINE_TABLE_FIELD(2, dst)
DEFINE_TABLE_FIELD(3, code)
DEFINE_TABLE_FIELD(4, enabled)
END_DEFINE_BEAN_ADAPTER(ac_dictionary_t)

int main() {
    DatabaseConnection db("test.sqlite");
    ITable<ac_dictionary_t_adapter_t> DAO;
    DAO.take_connection(&db);

    ac_dictionary_t dict;
    dict.src = "School";
    dict.dst = "Home";
    dict.code = "love";
    dict.enabled = 1;
    // insert an object
    DAO.insert(dict);
    DAO.insert(dict);

    // query an object
    DAO.get_by_id(1, &dict);

    // delete an object
    DAO.remove_by_id(2);

    // update an object
    dict.src = "Acane";
    DAO.update(dict);

    std::vector<ac_dictionary_t> dicts;
    // query by condition
    DAO.get_by_cond(dicts, "enabled = ? and src = ?", 1, "School");
}

```