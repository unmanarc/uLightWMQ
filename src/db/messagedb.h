#ifndef DB_H
#define DB_H

#include <atomic>
#include <utility>

#include <mdz_db_sqlite3/sqlconnector_sqlite3.h>
#include <mdz_hlp_functions/os.h>
#include <mdz_thr_mutex/mutex_shared.h>

struct MessageReg
{
    MessageReg()
    {
        found = false;
        id = 0;
        cdate = 0;
    }
    bool found, replyable;
    int64_t id;
    std::string msg, src;
    time_t cdate;
};

struct MessageReply
{
    MessageReply()
    {
        answered=false;
        timedout=false;
    }

    std::string reply;
    bool answered, timedout;
};

class MessageDB
{
public:
    MessageDB(){}
    bool start(const std::string & rcpt);

    /**
     * @brief push Push Message from source
     * @param msg Message
     * @param src Source
     * @return true if inserted into the database, false otherwise.
     */
    std::pair<bool,int64_t> push(const std::string &msg, const std::string &src, const bool & waitForAnswer = false);

    /**
     * @brief waitForReply Wait until the message is answered
     * @param id message id
     * @return message
     */
    MessageReply waitForReply(int64_t id, const std::string &src);

    /**
     * @brief answer Answer some request by id.
     * @param id Id of the request
     * @param msgAnswer Answer
     * @return true if answered correctly
     */
    bool reply(int64_t id, const std::string & msgAnswer);

    /**
     * @brief pop Remove Message identified by ID
     * @param id Message ID
     * @return true if removed.
     */
    bool remove(int64_t id);

    /**
     * @brief cleanExpired Remove expired messages.
     * @return true if removed.
     */
    bool cleanExpired();

    /**
     * @brief front
     * @param waitForMSG
     * @return
     */
    MessageReg front(bool waitForMSG, bool onlyWaitForAnswer = false);


    /**
     * @brief get  Get Message
     * @param id Message ID
     * @return Message
     */
    MessageReg get(int64_t id);



    ///////////////////
    bool getStatusOK();

private:
    bool initSchema();

    std::atomic<bool> statusOK;
    std::string dbPath, rcpt;
    //Mantids::Threads::Sync::Mutex_Shared mutex;
    std::mutex mt;
    std::condition_variable cvNotEmptyQueue, cvNotEmptyReplies;

    Mantids::Database::SQLConnector_SQLite3 db;
};


#endif // DB_H
