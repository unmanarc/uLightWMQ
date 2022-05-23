#include "messagedb.h"

#include <mdz_thr_mutex/lock_shared.h>

#include <mdz_mem_vars/a_string.h>
#include <mdz_mem_vars/a_datetime.h>
#include <mdz_mem_vars/a_bool.h>
#include <mdz_mem_vars/a_uint32.h>
#include <mdz_mem_vars/a_int64.h>
#include <mdz_mem_vars/a_var.h>


#include <arpa/inet.h>

#include "../globals.h"

using namespace Mantids::Memory;
using namespace Mantids::Database;
using namespace Mantids::Authentication;
using namespace Mantids::Application;

bool MessageDB::start(const std::string &_dbPath, const std::string &rcpt)
{
    this->rcpt = rcpt;
    dbPath = _dbPath + "/msgs_" + rcpt + ".db";
    statusOK = db.connect(dbPath) && initSchema();
    return statusOK;
}

bool MessageDB::push(const std::string &msg, const std::string &src, const bool & waitForAnswer, MessageAnswer * msgAnswer)
{
    std::string lastSQLError;

    // TODO: fill id with the last created table id
    unsigned long long rowid=0;

    {
        std::unique_lock<std::mutex> lock(mt);

        Globals::getAppLog()->log0(__func__,Logs::LEVEL_DEBUG, "Adding message to rcpt='%s'", rcpt.c_str());

        auto qi = db.prepareNewQueryInstance();
        qi.query->setFetchLastInsertRowID(true);
        qi.query->setPreparedSQLQuery(
                                        "INSERT INTO queue (`msg`,`src`,`waitforanswer`,`cdate`,`expiration`) VALUES(:msg,:src,:waitforanswer,:cdate,:expiration);",
                                        {
                                            {":msg",new Abstract::STRING(msg)},
                                            {":src",new Abstract::STRING(src)},
                                            {":waitforanswer",new Abstract::BOOL(waitForAnswer)},
                                            {":cdate",new Abstract::INT64(time(nullptr))},
                                            {":expiration",new Abstract::INT64( Globals::getLC_Database_DefaultExpirationTime() )}
                                        });
        if (!qi.query->exec(EXEC_TYPE_INSERT))
        {
            Globals::getAppLog()->log0(__func__,Logs::LEVEL_ERR, "Can't add message from '%s' to '%s': %s", src.c_str(),rcpt.c_str(),  lastSQLError.c_str() );
            return false;
        }

        rowid = qi.query->getLastInsertRowID();
    }

    // Notify readers...
    cvNotEmptyQueue.notify_all();


    if (waitForAnswer)
    {
        std::unique_lock<std::mutex> lock(mt);
        Abstract::STRING answer;
        Abstract::BOOL answered;

        while (1)
        {
            QueryInstance i = db.query("SELECT `answer`,`answered` FROM queue WHERE rowid=:rowid;",
                                           {
                                                {":rowid",new Abstract::INT64(rowid)}
                                            },
                                           { &answer, &answered  }
                                       );

            if (i.ok && i.query->step())
            {
                if (answered.getValue())
                {
                    if (msgAnswer)
                    {
                        msgAnswer->answered = true;
                        msgAnswer->answer = answer.toString();
                    }
                    return true;
                }
                else
                {
                    Globals::getAppLog()->log0(__func__,Logs::LEVEL_DEBUG, "Waiting for answer from '%s'...", rcpt.c_str());
                    if ( cvNotEmptyAnswers.wait_for(lock, std::chrono::seconds( Globals::getLC_Database_AnswerTimeout() )) == std::cv_status::timeout )
                    {
                        if (msgAnswer)
                            msgAnswer->answered = false;
                        return true;
                    }
                }
            }
            else
            {
                Globals::getAppLog()->log0(__func__,Logs::LEVEL_DEBUG, "Element from '%s' removed before any answer... aborting", rcpt.c_str());
                return false;
            }
        }
    }

    return true;
}

bool MessageDB::answer(uint32_t id, const std::string &msgAnswer)
{
    std::string lastSQLError;

    {
        std::unique_lock<std::mutex> lock(mt);

        Globals::getAppLog()->log0(__func__,Logs::LEVEL_DEBUG, "Setting answer to rcpt='%s', msgid='%lu'", rcpt.c_str(), id);

        if (!
                db.query(&lastSQLError,"UPDATE queue SET `answer`=:answer, answered='1' WHERE `id`=:id AND `answered`='0' AND `waitforanswer`='1';",
                            { {":answer",new Abstract::STRING(msgAnswer)}, {":id",new Abstract::UINT32(id)} }
                         ))
        {
            Globals::getAppLog()->log0(__func__,Logs::LEVEL_ERR, "Can't set answer to rcpt='%s', msgid='%lu': %s", rcpt.c_str(),id,  lastSQLError.c_str() );
            return false;
        }
    }

    // Notify readers that the message was answered...
    cvNotEmptyAnswers.notify_all();

    return true;
}

bool MessageDB::pop(uint32_t id)
{
    std::unique_lock<std::mutex> lock(mt);
    std::string lastSQLError;

    Globals::getAppLog()->log0(__func__,Logs::LEVEL_DEBUG, "Removing element id='%d' from rcpt='%s'", id, rcpt.c_str());

    if (!db.query(&lastSQLError,"DELETE FROM queue WHERE id = :id;", {{":id",new Abstract::UINT32(id)}} ))
    {
        Globals::getAppLog()->log0(__func__,Logs::LEVEL_ERR, "Can't remove messages from '%s'", rcpt.c_str() );
        return false;
    }

    cvNotEmptyAnswers.notify_all();

    return true;
}

bool MessageDB::cleanExpired()
{
    std::unique_lock<std::mutex> lock(mt);
    std::string lastSQLError;

    Globals::getAppLog()->log0(__func__,Logs::LEVEL_DEBUG, "Removing expired elements from '%s'", rcpt.c_str());

    if (!db.query(&lastSQLError,"DELETE FROM queue WHERE `cdate`+`expiration`>=strftime('%s', 'now');", {} ))
    {
        Globals::getAppLog()->log0(__func__,Logs::LEVEL_ERR, "Can't remove messages from '%s'", rcpt.c_str() );
        return false;
    }

    cvNotEmptyAnswers.notify_all();

    return true;
}

MessageReg MessageDB::front(bool waitForMSG, bool onlyWaitForAnswer)
{
    MessageReg reg;

    Abstract::BOOL waitForAnswer;
    Abstract::UINT32 id;
    Abstract::STRING msg,src;
    Abstract::INT64 cdate;

    std::unique_lock<std::mutex> lock(mt);

    Globals::getAppLog()->log0(__func__,Logs::LEVEL_DEBUG, "Obtaining front element from '%s'", rcpt.c_str());

    do
    {
        QueryInstance i =
                onlyWaitForAnswer?
                    db.query("SELECT `id`,`msg`,`src`,`cdate`,`waitforanswer` FROM queue WHERE `cdate`+`expiration`<strftime('%s', 'now') AND `waitforanswer`='1' ORDER BY id ASC LIMIT 1;",{}, { &id, &msg , &src,&cdate, &waitForAnswer}):
                    db.query("SELECT `id`,`msg`,`src`,`cdate`,`waitforanswer` FROM queue WHERE `cdate`+`expiration`<strftime('%s', 'now') ORDER BY id ASC LIMIT 1;",{}, { &id, &msg , &src,&cdate, &waitForAnswer});

        if (i.ok && i.query->step())
        {
            reg.found = true;
            reg.id = id.getValue();
            reg.cdate = cdate.getValue();
            reg.msg = msg.getValue();
            reg.src = src.getValue();
            reg.waitforanswer = waitForAnswer.getValue();
        }

        if (waitForMSG && !reg.found)
        {
            Globals::getAppLog()->log0(__func__,Logs::LEVEL_DEBUG, "Element from '%s' not found, waiting...", rcpt.c_str());

            if ( cvNotEmptyQueue.wait_for(lock, std::chrono::seconds( Globals::getLC_Database_MessageQueueFrontTimeout() )) == std::cv_status::timeout )
            {
                return reg;
            }
        }

    } while ( waitForMSG );

    return reg;
}

bool MessageDB::getStatusOK()
{
    return statusOK;
}

bool MessageDB::initSchema()
{
    bool r = true;

    if (r && !db.dbTableExist("queue"))
    {
        if (! db.query("CREATE TABLE `queue` (\n"
                       "       `id`            INTEGER         NOT NULL PRIMARY KEY AUTOINCREMENT,\n"
                       "       `src`           VARCHAR(256)    NOT NULL,\n"
                       "       `msg`           VARCHAR(8192)   NOT NULL,\n"
                       "       `answer`        VARCHAR(8192)   DEFAULT NULL,\n"
                       "       `answered`      BOOLEAN         DEFAULT '0',\n"
                       "       `waitforanswer` BOOLEAN         DEFAULT '0',\n"
                       "       `expiration`    BIGINT          NOT NULL DEFAULT 86400,\n"
                       "       `cdate`         BIGINT          NOT NULL\n"
                       ");\n"))
            r=false;


        db.query("CREATE INDEX IF NOT EXISTS idx_queue_id ON queue(id);");
        db.query("CREATE INDEX IF NOT EXISTS idx_queue_answered ON queue(answered);");
        db.query("CREATE INDEX IF NOT EXISTS idx_queue_waitforanswer ON queue(waitforanswer);");
        db.query("CREATE INDEX IF NOT EXISTS cdate ON queue(cdate);");
    }

    return r;
}
