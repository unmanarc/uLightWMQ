#include "webclienthdlr.h"
#include <mdz_mem_vars/b_mmap.h>
#include <mdz_proto_http/streamencoder_url.h>
#include <mdz_hlp_functions/appexec.h>
#include <mdz_mem_vars/streamableprocess.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include <mdz_net_sockets/socket_tls.h>

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

            auto r = msgDB->push( dataCpy.toString(), commonName, reqReply);

            if (!r.first)
            {
                getResponseDataStreamer()->writeString("-1");
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
                return ret;
            }
            else
            {
                getResponseDataStreamer()->writeString(std::to_string(r.second));
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_200_OK;
                return ret;
            }
        }
        else
        {
            response().headers->add("X-ErrorMsg", "Destination Queue Not Found");
            getResponseDataStreamer()->writeString("0");
            Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_404_NOT_FOUND;
            return ret;
        }
    }
    else if (getRequestURI() == "/waitForReply")
    {
        int64_t id = strtoll(reqData.VARS_GET->getStringValue("id").c_str(),0,10);
        std::string dst = reqData.VARS_GET->getStringValue("dst");

        auto msgDB = DBCollection::getMessageDB(dst);
        if (msgDB)
        {
            Status wrStatUpd;
            Mantids::Memory::Containers::B_Chunks dataCpy;
            auto streamIn = request().content->getStreamableOuput();
            streamIn->streamTo(&dataCpy,wrStatUpd);

            MessageReply reply = msgDB->waitForReply( id, commonName );

            response().headers->add("X-Answered", std::to_string(reply.answered));
            response().headers->add("X-TimedOut", std::to_string(reply.timedout));

            if (!reply.answered)
            {
                getResponseDataStreamer()->writeString("");
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_404_NOT_FOUND;
                return ret;
            }
            else
            {
                getResponseDataStreamer()->writeString(reply.message);
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_200_OK;
                return ret;
            }
        }
        else
        {
            response().headers->add("X-ErrorMsg", "Destination Queue Not Found");
            getResponseDataStreamer()->writeString("0");
            Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_404_NOT_FOUND;
            return ret;
        }

    }
    else if (getRequestURI() == "/reply")
    {
        uint32_t msgId = strtoul(reqData.VARS_GET->getStringValue("msgId").c_str(),0,10);

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
                response().headers->add("X-ErrorMsg", "Unable to reply the message");
                getResponseDataStreamer()->writeString("0");
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
                return ret;
            }
            else
            {
                ret = Response::StatusCode::S_200_OK;
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                getResponseDataStreamer()->writeString("1");
                return ret;
            }
        }
        else
        {
            response().headers->add("X-ErrorMsg", "Destination Queue Not Found");
            getResponseDataStreamer()->writeString("0");
            Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_404_NOT_FOUND;
            return ret;
        }
    }
    else if (getRequestURI() == "/remove")
    {
        auto msgDB = DBCollection::getOrCreateMessageDB(commonName);
        if (!msgDB)
        {
            response().headers->add("X-ErrorMsg", "Queue not found and/or can't be created");
            getResponseDataStreamer()->writeString("0");
            Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
            return ret;
        }
        else
        {
            auto msgId = strtoul(reqData.VARS_GET->getStringValue("msgId").c_str(),0,10 );
            if (msgDB->remove( msgId ))
            {
                ret = Response::StatusCode::S_200_OK;
                getResponseDataStreamer()->writeString("1");
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                return ret;
            }
            else
            {
                response().headers->add("X-ErrorMsg", "Queue Pop Failed");
                getResponseDataStreamer()->writeString("0");

                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_404_NOT_FOUND;
                return ret;
            }
        }
    }
    else if (getRequestURI() == "/get")
    {
        auto msgDB = DBCollection::getOrCreateMessageDB(commonName);
        if (!msgDB)
        {
            response().headers->add("X-ErrorMsg", "Queue not found and/or can't be created");
            getResponseDataStreamer()->writeString("0");

            Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
            return ret;
        }
        else
        {
            int64_t id = strtoll(reqData.VARS_GET->getStringValue("id").c_str(),0,10);
            auto frontMsg = msgDB->get( id );

            if (frontMsg.found)
            {
                ret = Response::StatusCode::S_200_OK;

                response().headers->add("From", frontMsg.src);
                response().headers->add("Id", std::to_string(frontMsg.id));
                response().headers->add("RequireReply", std::to_string(frontMsg.replyable?1:0));

                getResponseDataStreamer()->writeString(frontMsg.msg);

                Mantids::Network::HTTP::Common::Date fileModificationDate;
                fileModificationDate.setRawTime(frontMsg.cdate);
                response().headers->add("Last-Modified", fileModificationDate.toString());

                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                return ret;
            }
            else
            {
                response().headers->add("X-ErrorMsg", "Not Found");

                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_404_NOT_FOUND;
                return ret;
            }
        }
    }
    else if (getRequestURI() == "/front")
    {
        auto msgDB = DBCollection::getOrCreateMessageDB(commonName);
        if (!msgDB)
        {
            response().headers->add("X-ErrorMsg", "Queue not found and/or can't be created");
            getResponseDataStreamer()->writeString("0");

            Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
            return ret;
        }
        else
        {
            bool waitForMsg = reqData.VARS_GET->getStringValue("wait") == "1";
            auto frontMsg = msgDB->front( waitForMsg );

            if (frontMsg.found)
            {
                ret = Response::StatusCode::S_200_OK;

                response().headers->add("From", frontMsg.src);
                response().headers->add("Id", std::to_string(frontMsg.id));
                response().headers->add("RequireReply", std::to_string(frontMsg.replyable?1:0));

                getResponseDataStreamer()->writeString(frontMsg.msg);

                Mantids::Network::HTTP::Common::Date fileModificationDate;
                fileModificationDate.setRawTime(frontMsg.cdate);
                response().headers->add("Last-Modified", fileModificationDate.toString());

                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                return ret;
            }
            else
            {
                if (waitForMsg)
                    response().headers->add("X-ErrorMsg", "Timed out");
                else
                    response().headers->add("X-ErrorMsg", "Empty Queue");

                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
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
        Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
    }

    return ret;
}

void WebClientHdlr::setWebClientParameters(const webClientParams &newWebClientParameters)
{
    webClientParameters = newWebClientParameters;
}
