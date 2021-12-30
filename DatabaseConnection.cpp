//
// Created by Acane on 2021/12/30.
//

#include "DatabaseConnection.h"

int SQLITE_COMMON_CALLBACK_F(void* _, int argc, char** argv, char** azColName) {
    return 0;
}