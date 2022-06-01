#include "messagedb.h"

#include <mdz_thr_mutex/lock_shared.h>

#include <mdz_mem_vars/a_string.h>
#include <mdz_mem_vars/a_datetime.h>
#include <mdz_mem_vars/a_bool.h>
#include <mdz_mem_vars/a_int64.h>
#include <mdz_mem_vars/a_var.h>


#include <inttypes.h>

#include "../globals.h"

using namespace Mantids::Memory;
using namespace Mantids::Database;
using namespace Mantids::Application;

bool MessageDB::start(const std::string &rcpt)
{
    this->rcpt = rcpt;
    dbPath = Globals::getLC_Database_FilesPath() + "/msgs_" + rcpt + ".db";
    statusOK = db.connect(dbPath) && initSchema();
    return statusOK;
}

std::pair<bool, int64_t> MessageDB::push(const std::string &msg, const std::string &src, const bool & replyable)
{
    std::string lastSQLError;

    unsigned long long rowid=0;

    {
        std::unique_lock<std::mutex> lock(mt);
        auto qi = db.prepareNewQueryInstance();
        qi.query->setFetchLastInsertRowID(true);
        qi.query->setPreparedSQLQuery(
                    "INSERT INTO queue (`msg`,`src`,`replyable`,`cdate`,`expiration`) VALUES(:msg,:src,:replyable,:cdate,:expiration);",
                    {
                        {":msg",new Abstract::STRING(msg)},
                        {":src",new Abstract::STRING(src)},
                        {":replyable",new Abstract::BOOL(replyable)},
                        {":cdate",new Abstract::INT64(time(nullptr))},
                        {":expiration",new Abstract::INT64( Globals::getLC_Database_DefaultExpirationTime() )}
                    });
        if (!qi.query->exec(EXEC_TYPE_INSERT))
        {
            Globals::getAppLog()->log0(__func__,Logs::LEVEL_ERR, "SQL Error: %s", lastSQLError.c_str() );
            return std::make_pair(false,0);
        }

        rowid = qi.query->getLastInsertRowID();
    }

    // Notify readers...
    cvNotEmptyQueue.notify_all();

    return std::make_pair(true,rowid);
}

MessageReply MessageDB::waitForReply(int64_t id, const std::string & src)
{
    MessageReply msgReply;
    std::unique_lock<std::mutex> lock(mt);
    Abstract::STRING reply;
    Abstract::BOOL answered;

    while (1)
    {
        QueryInstance i = db.query("SELECT `reply`,`answered` FROM queue WHERE `rowid`=:rowid and `src`=:src;",
                                   {
                                       {":rowid",new Abstract::INT64(id)},
                                       {":src",new Abstract::STRING(src)}
                                   },
                                   { &reply, &answered  }
                                   );

        if (i.ok && i.query->step())
        {
            if (answered.getValue())
            {
                msgReply.answered = true;
                msgReply.reply = reply.toString();

                // Remove the message here.

                return msgReply;
            }
            else
            {
                if ( cvNotEmptyReplies.wait_for(lock, std::chrono::seconds( Globals::getLC_Database_AnswerTimeout() )) == std::cv_status::timeout )
                {
                    msgReply.timedout = true;
                    msgReply.answered = false;
                    return msgReply;
                }
            }
        }
        else
        {
            msgReply.timedout = false;
            msgReply.answered = false;
            return msgReply;
        }
    }
}

bool MessageDB::reply(int64_t id, const std::string &msgReply)
{
    {
        std::unique_lock<std::mutex> lock(mt);


        QueryInstance qi = db.qInsert("UPDATE queue SET `reply`=:reply, answered='1' WHERE `rowid`=:rowid AND `answered`='0' AND `replyable`='1';",
                                      { {":reply",new Abstract::STRING(msgReply)}, {":rowid",new Abstract::INT64(id)} },{});

        if (!qi.ok)
        {
            Globals::getAppLog()->log0(__func__,Logs::LEVEL_ERR, "SQL Error: %s", qi.query->getLastSQLError().c_str() );
            return false;
        }
        else if ( !qi.query->getAffectedRows() )
        {
            return false;
        }
    }

    // Notify readers that the message was replied...
    cvNotEmptyReplies.notify_all();

    return true;
}

bool MessageDB::remove(int64_t id)
{
    std::unique_lock<std::mutex> lock(mt);
    std::string lastSQLError;

    QueryInstance qi = db.qInsert("DELETE FROM queue WHERE `rowid` = :rowid;", {{":rowid",new Abstract::INT64(id)}},{});
    if (!qi.ok)
    {
        Globals::getAppLog()->log0(__func__,Logs::LEVEL_ERR, "SQL Error: %s", lastSQLError.c_str() );
        return false;
    }
    else if ( !qi.query->getAffectedRows() )
        return false;

    cvNotEmptyReplies.notify_all();
    return true;
}

bool MessageDB::cleanExpired()
{
    std::unique_lock<std::mutex> lock(mt);
    std::string lastSQLError;

    Globals::getAppLog()->log0(__func__,Logs::LEVEL_DEBUG, "Removing expired elements from '%s'", rcpt.c_str());

    if (!db.query(&lastSQLError,"DELETE FROM queue WHERE `cdate`+`expiration`>=strftime('%s', 'now');", {} ))
    {
        Globals::getAppLog()->log0(__func__,Logs::LEVEL_ERR, "SQL Error: %s", lastSQLError.c_str() );
        return false;
    }

    cvNotEmptyReplies.notify_all();

    return true;
}

MessageReg MessageDB::front(bool waitForMSG, bool onlyreplyable)
{
    MessageReg reg;

    Abstract::BOOL replyable;
    Abstract::INT64 id;
    Abstract::STRING msg,src;
    Abstract::INT64 cdate;

    std::unique_lock<std::mutex> lock(mt);

    do
    {
        QueryInstance i =
                onlyreplyable?
                    db.query("SELECT `rowid`,`msg`,`src`,`cdate`,`replyable` FROM queue WHERE `cdate`+`expiration`<strftime('%s', 'now') AND `replyable`='1' ORDER BY `rowid` ASC LIMIT 1;",{}, { &id, &msg , &src,&cdate, &replyable}):
                    db.query("SELECT `rowid`,`msg`,`src`,`cdate`,`replyable` FROM queue WHERE `cdate`+`expiration`<strftime('%s', 'now') ORDER BY `rowid` ASC LIMIT 1;",{}, { &id, &msg , &src,&cdate, &replyable});

        if (i.ok && i.query->step())
        {
            reg.found = true;
            reg.id = id.getValue();
            reg.cdate = cdate.getValue();
            reg.msg = msg.getValue();
            reg.src = src.getValue();
            reg.replyable = replyable.getValue();

            break;
        }

        if (waitForMSG && !reg.found)
        {
            if ( cvNotEmptyQueue.wait_for(lock, std::chrono::seconds( Globals::getLC_Database_MessageQueueFrontTimeout() )) == std::cv_status::timeout )
            {
                return reg;
            }
        }

    } while ( waitForMSG );

    return reg;
}

MessageReg MessageDB::get(int64_t id)
{
    MessageReg reg;

    Abstract::BOOL replyable;
    Abstract::STRING msg,src;
    Abstract::INT64 cdate;

    std::unique_lock<std::mutex> lock(mt);

    QueryInstance i =
            db.qSelect("SELECT `msg`,`src`,`cdate`,`replyable` FROM queue WHERE `cdate`+`expiration`<strftime('%s', 'now') AND `rowid`=:rowid ORDER BY `rowid` ASC LIMIT 1;",
                         { {":rowid",new Abstract::INT64(id)} }, { &msg , &src,&cdate, &replyable});

    if (i.ok && i.query->step())
    {
        reg.found = true;
        reg.id = id;
        reg.cdate = cdate.getValue();
        reg.msg = msg.getValue();
        reg.src = src.getValue();
        reg.replyable = replyable.getValue();
    }


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
                       "       `src`           VARCHAR(256)    NOT NULL,\n"
                       "       `msg`           TEXT            NOT NULL,\n"
                       "       `reply`        TEXT            DEFAULT NULL,\n"
                       "       `answered`      BOOLEAN         DEFAULT '0',\n"
                       "       `replyable`     BOOLEAN         DEFAULT '0',\n"
                       "       `expiration`    BIGINT          NOT NULL DEFAULT 86400,\n"
                       "       `cdate`         BIGINT          NOT NULL\n"
                       ");\n"))
            r=false;


        db.query("CREATE INDEX IF NOT EXISTS idx_queue_answered ON queue(answered);");
        db.query("CREATE INDEX IF NOT EXISTS idx_queue_replyable ON queue(replyable);");
        db.query("CREATE INDEX IF NOT EXISTS cdate ON queue(cdate);");
    }

    return r;
}
