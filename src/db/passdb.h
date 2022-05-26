#ifndef PASSDB_H
#define PASSDB_H

#include <atomic>
#include <utility>

#include <mdz_db_sqlite3/sqlconnector_sqlite3.h>
#include <mdz_hlp_functions/os.h>
#include <mdz_thr_mutex/mutex_shared.h>

class PassDB
{
public:
    PassDB(){}
    static bool start();
    static bool validateUserPass(const std::string & user, const std::string pass);

    ///////////////////
    static bool getStatusOK();

private:
    static bool initSchema();

    static std::atomic<bool> statusOK;
    static std::string dbPath;
    static std::mutex mt;
    static Mantids::Database::SQLConnector_SQLite3 db;
};


#endif // PASSDB_H
