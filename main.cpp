#include <iostream>
#include "sqlite3.h"
#include "DatabaseConnection.h"

typedef struct ACANE_DICTIONARY {
    int id;
    std::string src;
    std::string dst;
    std::string code;
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
    ITable<ac_dictionary_tAdapter> DAO;
    DAO.take_connection(&db);
    dict.id = 12;
    dict.src = "Acane";
    dict.dst = "Test";
    // DAO.insert(dict);
    std::vector<ac_dictionary_t> list;
    DAO.get_by_cond(list, "1 = 1");
    for (auto& dict: list) {
      printf("%d %s %s\n", dict.id, dict.dst.c_str(), dict.src.c_str());
    }
}
