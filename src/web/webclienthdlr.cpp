#include "webclienthdlr.h"
#include <mdz_mem_vars/b_mmap.h>
#include <mdz_proto_http/streamencoder_url.h>
#include <mdz_hlp_functions/appexec.h>
#include <mdz_mem_vars/streamableprocess.h>
#include <boost/algorithm/string/predicate.hpp>

#include <mdz_net_sockets/socket_tls.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <regex>

#include "../db/dbcollection.h"
#include "../globals.h"

using namespace Mantids::Network::HTTP;
using namespace Mantids::Memory::Streams;

WebClientHdlr::WebClientHdlr(void *parent, Mantids::Memory::Streams::Streamable *sock) : HTTPv1_Server(sock)
{
    Mantids::Network::TLS::Socket_TLS * tls = (Mantids::Network::TLS::Socket_TLS *)sock;
    commonName = tls->getTLSPeerCN();
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
        Response::StatusCode ret  = Response::StatusCode::S_401_UNAUTHORIZED;
        getResponseDataStreamer()->writeString("0");
        return ret;
    }

    std::regex regexp("^[a-zA-z0-9]+");
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
        bool waitForAnswer = reqData.VARS_GET->getStringValue("waitForAnswer") == "1" ? 1:0;

        auto msgDB = DBCollection::getMessageDB(dst);
        if (msgDB)
        {
            Status wrStatUpd;
            Mantids::Memory::Containers::B_Chunks dataCpy;
            auto streamIn = request().content->getStreamableOuput();
            streamIn->streamTo(&dataCpy,wrStatUpd);

            MessageAnswer answer;

            if (waitForAnswer)
            {
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "W/0: %s",getRequestURI().c_str());
            }

            if (!msgDB->push( dataCpy.toString(), commonName, waitForAnswer, &answer ))
            {
                getResponseDataStreamer()->writeString("0");
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
                return ret;
            }
            else
            {
                ret = Response::StatusCode::S_200_OK;
                response().headers->add("X-Answered", std::to_string(answer.answered));
                if (!waitForAnswer)
                {
                    getResponseDataStreamer()->writeString("1");
                    Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                }
                else
                {
                    Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "W/1: %s",getRequestURI().c_str());
                    getResponseDataStreamer()->writeString(answer.answer);
                }
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
    if (getRequestURI() == "/answer")
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
            if (!msgDB->answer( msgId, dataCpy.toString() ))
            {
                response().headers->add("X-ErrorMsg", "Unable to answer the message");
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
    else if (getRequestURI() == "/pop")
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
            if (msgDB->pop( msgId ))
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
                response().headers->add("RequireAnswer", std::to_string(frontMsg.waitforanswer?1:0));

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
