#include "webclienthdlr.h"
#include <mdz_mem_vars/b_mmap.h>
#include <mdz_proto_http/streamencoder_url.h>
#include <mdz_hlp_functions/appexec.h>
#include <mdz_mem_vars/streamableprocess.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include <mdz_net_sockets/socket_tls.h>

#include <inttypes.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <regex>

#include "../db/dbcollection.h"
#include "../db/passdb.h"
#include "../globals.h"

using namespace Mantids::Network::HTTP;
using namespace Mantids::Memory::Streams;
using namespace Mantids::Application;

WebClientHdlr::WebClientHdlr(void *parent, Mantids::Memory::Streams::Streamable *sock) : HTTPv1_Server(sock)
{
    Mantids::Network::TLS::Socket_TLS * tls = (Mantids::Network::TLS::Socket_TLS *)sock;
    commonName = boost::algorithm::to_lower_copy(tls->getTLSPeerCN());
    ((Mantids::Memory::Containers::B_Chunks *)request().content->getStreamableOuput())->setMaxSize( Globals::getLC_Database_MaxMessageSize() );
}

WebClientHdlr::~WebClientHdlr()
{
}

Response::StatusCode WebClientHdlr::processClientRequest()
{
    sLocalRequestedFileInfo fileInfo;
    Response::StatusCode ret  = Response::StatusCode::S_404_NOT_FOUND;
    auto reqData = getRequestActiveObjects();

    if (commonName.empty())
    {
        if (  PassDB::validateUserPass( boost::algorithm::to_lower_copy(reqData.AUTH_USER), reqData.AUTH_PASS )  )
        {
            commonName = boost::algorithm::to_lower_copy(reqData.AUTH_USER);
        }
        else
        {
            Response::StatusCode ret  = Response::StatusCode::S_401_UNAUTHORIZED;
            getResponseDataStreamer()->writeString("0");
            return ret;
        }
    }

    std::regex regexp("^[a-z0-9]+");
    if(!regex_match(commonName,regexp))
    {
        Response::StatusCode ret  = Response::StatusCode::S_406_NOT_ACCEPTABLE;
        getResponseDataStreamer()->writeString("0");
        return ret;
    }

    // CHECK FUNCTION ->  push, pop, front

    if (getRequestURI() == "/push")
    {
        std::string dst = reqData.VARS_GET->getStringValue("dst");
        bool reqReply = reqData.VARS_GET->getStringValue("reqReply") == "1" ? 1:0;

        auto msgDB = DBCollection::getMessageDB(dst);
        if (msgDB)
        {
            Status wrStatUpd;
            Mantids::Memory::Containers::B_Chunks dataCpy;
            auto streamIn = request().content->getStreamableOuput();
            streamIn->streamTo(&dataCpy,wrStatUpd);

            MessageReply reply;

            Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_DEBUG, "Pushing message to rcpt='%s'", dst.c_str());
            auto r = msgDB->push( dataCpy.toString(), commonName, reqReply);

            if (!r.first)
            {
                getResponseDataStreamer()->writeString("-1");
                Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_ERR, "Failed to push message to rcpt='%s'", dst.c_str());
                Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
                return ret;
            }
            else
            {
                getResponseDataStreamer()->writeString(std::to_string(r.second));
                Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_INFO, "Message pushed to rcpt='%s' with msgId='%" PRId64 "'", dst.c_str(), r.second);
                Globals::getRPCLog()->log(Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_200_OK;
                return ret;
            }
        }
        else
        {
            response().headers->add("X-ErrorMsg", "Destination Queue Not Found");
            getResponseDataStreamer()->writeString("0");
            Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_404_NOT_FOUND;
            return ret;
        }
    }
    else if (getRequestURI() == "/waitForReply")
    {
        int64_t msgId = strtoll(reqData.VARS_GET->getStringValue("msgId").c_str(),0,10);
        std::string dst = reqData.VARS_GET->getStringValue("dst");
        bool reqRemoveAfterRead = reqData.VARS_GET->getStringValue("removeAfterRead") == "0" ? 0:1;

        Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_DEBUG, "Waiting for reply from dst='%s' with msgId='%" PRId64 "'", dst.c_str(), msgId);

        auto msgDB = DBCollection::getMessageDB(dst);
        if (msgDB)
        {
            Status wrStatUpd;
            Mantids::Memory::Containers::B_Chunks dataCpy;
            auto streamIn = request().content->getStreamableOuput();
            streamIn->streamTo(&dataCpy,wrStatUpd);

            MessageReply reply = msgDB->waitForReply( msgId, commonName,reqRemoveAfterRead );

            response().headers->add("X-Answered", std::to_string(reply.answered));
            response().headers->add("X-TimedOut", std::to_string(reply.timedout));
            response().headers->add("X-Removed", std::to_string(reply.removed?1:0));

            if (!reply.answered)
            {
                if (reply.timedout)
                    Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Reply timed out on dst='%s' with msgId='%" PRId64 "'", dst.c_str(),msgId);
                else
                    Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Reply not received from dst='%s' with msgId='%" PRId64 "'", dst.c_str(),msgId);

                getResponseDataStreamer()->writeString("");

                Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_404_NOT_FOUND;
                return ret;
            }
            else
            {
                Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_INFO, "Reply received from dst='%s' with msgId='%" PRId64 "', msgSize='%" PRId64 "'", dst.c_str(),msgId, reply.reply.size());

                getResponseDataStreamer()->writeString(reply.reply);
                Globals::getRPCLog()->log(Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_200_OK;
                return ret;
            }
        }
        else
        {
            Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Reply not received from dst='%s' with msgId='%" PRId64 "', queue not found", dst.c_str(),msgId);
            response().headers->add("X-ErrorMsg", "Destination Queue Not Found");
            getResponseDataStreamer()->writeString("0");
            Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_404_NOT_FOUND;
            return ret;
        }

    }
    else if (getRequestURI() == "/reply")
    {
        uint32_t msgId = strtoul(reqData.VARS_GET->getStringValue("msgId").c_str(),0,10);

        Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_DEBUG, "Replying message with msgId='%" PRId64 "'", msgId);

        auto msgDB = DBCollection::getMessageDB(commonName);
        if (msgDB)
        {
            Status wrStatUpd;
            Mantids::Memory::Containers::B_Chunks dataCpy;
            auto streamIn = request().content->getStreamableOuput();
            streamIn->streamTo(&dataCpy,wrStatUpd);

            // TODO: enforce message size limit in bytes while copying,
            if (!msgDB->reply( msgId, dataCpy.toString() ))
            {
                Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Unable to reply message msgId='%" PRId64 "'", msgId);

                response().headers->add("X-ErrorMsg", "Unable to reply the message");
                getResponseDataStreamer()->writeString("0");
                Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
                return ret;
            }
            else
            {
                ret = Response::StatusCode::S_200_OK;
                Globals::getRPCLog()->log(Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                getResponseDataStreamer()->writeString("1");
                return ret;
            }
        }
        else
        {
            Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Unable to reply message msgId='%" PRId64 "', queue not found", msgId);

            response().headers->add("X-ErrorMsg", "Destination Queue Not Found");
            getResponseDataStreamer()->writeString("0");
            Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_404_NOT_FOUND;
            return ret;
        }
    }
    else if (getRequestURI() == "/remove")
    {
        int64_t msgId = strtoll(reqData.VARS_GET->getStringValue("msgId").c_str(),0,10 );
        auto msgDB = DBCollection::getOrCreateMessageDB(commonName);

        if (!msgDB)
        {
            Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Unable to remove message msgId='%" PRId64 "', queue not found", msgId);

            response().headers->add("X-ErrorMsg", "Queue not found and/or can't be created");
            getResponseDataStreamer()->writeString("0");
            Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
            return ret;
        }
        else
        {
            if (msgDB->remove( msgId ))
            {
                ret = Response::StatusCode::S_200_OK;
                getResponseDataStreamer()->writeString("1");
                Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Message msgId='%" PRId64 "' removed.", msgId);
                Globals::getRPCLog()->log(Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                return ret;
            }
            else
            {
                Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Unable to remove message msgId='%" PRId64 "', message not found", msgId);

                response().headers->add("X-ErrorMsg", "Message Remove Failed");
                getResponseDataStreamer()->writeString("0");

                Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_404_NOT_FOUND;
                return ret;
            }
        }
    }
    else if (getRequestURI() == "/get")
    {
        auto msgDB = DBCollection::getOrCreateMessageDB(commonName);
        int64_t msgId = strtoll(reqData.VARS_GET->getStringValue("msgId").c_str(),0,10);

        if (!msgDB)
        {
            Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Unable to get message msgId='%" PRId64 "', queue not found and/or can't be created", msgId);

            response().headers->add("X-ErrorMsg", "Queue not found and/or can't be created");
            getResponseDataStreamer()->writeString("0");

            Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
            return ret;
        }
        else
        {
            Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_DEBUG, "Getting message msgId='%" PRId64 "'", msgId);

            auto frontMsg = msgDB->get( msgId );

            if (frontMsg.found)
            {
                Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_INFO, "Message msgId='%" PRId64 "' found and fetched, msgSize='%" PRId64 "'", msgId, frontMsg.msg.size());
                ret = Response::StatusCode::S_200_OK;

                response().headers->add("X-From", frontMsg.src);
                response().headers->add("X-MsgId", std::to_string(frontMsg.id));
                response().headers->add("X-RequireReply", std::to_string(frontMsg.replyable?1:0));

                getResponseDataStreamer()->writeString(frontMsg.msg);

                Mantids::Network::HTTP::Common::Date fileModificationDate;
                fileModificationDate.setRawTime(frontMsg.cdate);
                response().headers->add("X-Last-Modified", fileModificationDate.toString());

                Globals::getRPCLog()->log(Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                return ret;
            }
            else
            {
                Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Unable to get message msgId='%" PRId64 "' - NOT FOUND.", msgId);
                response().headers->add("X-ErrorMsg", "Not Found");

                Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_404_NOT_FOUND;
                return ret;
            }
        }
    }
    else if (getRequestURI() == "/front")
    {
        auto msgDB = DBCollection::getOrCreateMessageDB(commonName);
        bool waitForMsg = reqData.VARS_GET->getStringValue("wait") == "1";
        bool reqRemoveAfterRead = reqData.VARS_GET->getStringValue("removeAfterRead") == "1" ? 1:0;

        if (!msgDB)
        {
            Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Unable to get front message, queue not found and/or can't be created");

            response().headers->add("X-ErrorMsg", "Queue not found and/or can't be created");
            getResponseDataStreamer()->writeString("0");

            Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
            return ret;
        }
        else
        {
            Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_DEBUG, "Getting front message with waitMode='%d'", waitForMsg?1:0);


            auto frontMsg = msgDB->front( waitForMsg, reqRemoveAfterRead );

            if (frontMsg.found)
            {
                ret = Response::StatusCode::S_200_OK;

                response().headers->add("X-From", frontMsg.src);
                response().headers->add("X-MsgId", std::to_string(frontMsg.id));
                response().headers->add("X-RequireReply", std::to_string(frontMsg.replyable?1:0));
                response().headers->add("X-Removed", std::to_string(frontMsg.removed?1:0));

                getResponseDataStreamer()->writeString(frontMsg.msg);

                Mantids::Network::HTTP::Common::Date fileModificationDate;
                fileModificationDate.setRawTime(frontMsg.cdate);
                response().headers->add("X-Last-Modified", fileModificationDate.toString());

                Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_INFO, "Front message msgId='%" PRId64 "' found and fetched, msgSize='%" PRId64 "'", frontMsg.id, frontMsg.msg.size());


                Globals::getRPCLog()->log(Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                return ret;
            }
            else
            {
                if (waitForMsg)
                {
                    Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Unable to get front message: Empty Queue + Timed Out");
                    response().headers->add("X-ErrorMsg", "Timed out");
                }
                else
                {
                    Globals::getAppLog()->log2(__func__,  commonName, remotePairAddress, Logs::LEVEL_WARN, "Unable to get front message: Empty queue");
                    response().headers->add("X-ErrorMsg", "Empty Queue");
                }

                Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_404_NOT_FOUND;
                return ret;
            }
        }
    }
    else
    {
        // Unknown command...
        ret = Response::StatusCode::S_404_NOT_FOUND;
        response().headers->add("X-ErrorMsg", "Invalid URL");
        getResponseDataStreamer()->writeString("0");
        Globals::getRPCLog()->log(Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
    }

    return ret;
}

void WebClientHdlr::setWebClientParameters(const webClientParams &newWebClientParameters)
{
    webClientParameters = newWebClientParameters;
}
