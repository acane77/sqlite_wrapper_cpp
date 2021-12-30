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
    ac_dictionary_t dict;
    DatabaseConnection db("test.sqlite");
    ITable<ac_dictionary_t_adapter_t> DAO;
    DAO.take_connection(&db);
    DAO.get_by_id(16, &dict);
    dict.src = "Acane";
    DAO.update(dict);
}
