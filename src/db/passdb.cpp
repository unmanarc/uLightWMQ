#include "passdb.h"


#include <mdz_thr_mutex/lock_shared.h>
#include <mdz_mem_vars/a_string.h>
#include <mdz_hlp_functions/crypto.h>

#include "../globals.h"

using namespace Mantids::Memory;
using namespace Mantids::Database;
using namespace Mantids::Application;

std::atomic<bool> PassDB::statusOK;
std::string PassDB::dbPath;
std::mutex PassDB::mt;
Mantids::Database::SQLConnector_SQLite3 PassDB::db;

bool PassDB::start()
{
    dbPath = Globals::getLC_Database_FilesPath() + "/users.db";
    statusOK = db.connect(dbPath) && initSchema();
    return statusOK;
}

bool PassDB::validateUserPass(const std::string &user, const std::string pass)
{
    std::unique_lock<std::mutex> lock(mt);

    QueryInstance i = db.query("SELECT `user` FROM users WHERE user=:user AND UPPER(hash)=UPPER(:hash);",
                               {
                                   {":user",new Abstract::STRING(user)},
                                   {":hash",new Abstract::STRING( Mantids::Helpers::Crypto::calcSHA256(pass) )}
                               },
                               {   }
                               );

    if (i.ok && i.query->step())
    {
        return true;
    }
    return false;
}

bool PassDB::getStatusOK()
{
    return statusOK;
}

bool PassDB::initSchema()
{
    bool r = true;

    if (r && !db.dbTableExist("users"))
    {
        if (! db.query("CREATE TABLE `users` (\n"
                       "       `user`          VARCHAR(256)    NOT NULL PRIMARY KEY,\n"
                       "       `hash`          VARCHAR(512)            NOT NULL,\n"
                       "       `cdate`         DATETIME          NOT NULL DEFAULT CURRENT_TIMESTAMP\n"
                       ");\n"))
            r=false;

        db.query("CREATE INDEX IF NOT EXISTS idx_users_user ON users(user);");
    }

    return r;
}
