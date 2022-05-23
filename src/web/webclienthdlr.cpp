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
        getResponseDataStreamer()->writeString("401: Unauthorized.");
        return ret;
    }

    std::regex regexp("^[a-zA-z0-9]+");
    if(!regex_match(commonName,regexp))
    {
        Response::StatusCode ret  = Response::StatusCode::S_406_NOT_ACCEPTABLE;
        getResponseDataStreamer()->writeString("406: Client Common Name Not Acceptable, should not contain special characters.");
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
                getResponseDataStreamer()->writeString("0: QUEUE ERROR.");
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
                return ret;
            }
            else
            {
                ret = Response::StatusCode::S_200_OK;
                if (!waitForAnswer)
                {
                    getResponseDataStreamer()->writeString("1: QUEUED.");
                    Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                }
                else
                {
                    response().headers->add("Answered", std::to_string(answer.answered));

                    Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "W/1: %s",getRequestURI().c_str());
                    getResponseDataStreamer()->writeString(answer.answer);
                }
                return ret;
            }
        }
        else
        {
            getResponseDataStreamer()->writeString("0: QUEUE NOT FOUND.");
            Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_404_NOT_FOUND;
            return ret;
        }
    }
    if (getRequestURI() == "/answer")
    {
        uint32_t id = strtoul(reqData.VARS_GET->getStringValue("id").c_str(),0,10);

        auto msgDB = DBCollection::getMessageDB(commonName);
        if (msgDB)
        {
            Status wrStatUpd;
            Mantids::Memory::Containers::B_Chunks dataCpy;
            auto streamIn = request().content->getStreamableOuput();
            streamIn->streamTo(&dataCpy,wrStatUpd);

            // TODO: enforce 8192 bytes,
            if (!msgDB->answer( id, dataCpy.toString() ))
            {
                getResponseDataStreamer()->writeString("0: ANSWER QUEUE ERROR.");
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
                ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
                return ret;
            }
            else
            {
                ret = Response::StatusCode::S_200_OK;
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                getResponseDataStreamer()->writeString("1: ANSWER QUEUED.");
                return ret;
            }
        }
        else
        {
            getResponseDataStreamer()->writeString("0: QUEUE NOT FOUND.");
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
            getResponseDataStreamer()->writeString("0: QUEUE CREATE ERROR.");
            Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
            return ret;
        }
        else
        {
            if (msgDB->pop( strtoul(reqData.VARS_GET->getStringValue("id").c_str(),0,10 )))
            {
                ret = Response::StatusCode::S_200_OK;
                getResponseDataStreamer()->writeString("1: QUEUE POP OK.");
                Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_INFO, remotePairAddress,"","", "", "wmqServer", 65535, "R/200: %s",getRequestURI().c_str());
                return ret;
            }
            else
            {
                getResponseDataStreamer()->writeString("0: QUEUE POP FAILED.");
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
            getResponseDataStreamer()->writeString("0: QUEUE CREATE ERROR.");
            Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/503: %s",getRequestURI().c_str());
            ret = Response::StatusCode::S_503_SERVICE_UNAVAILABLE;
            return ret;
        }
        else
        {
            auto frontMsg = msgDB->front( reqData.VARS_GET->getStringValue("wait") == "1" );

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
                getResponseDataStreamer()->writeString("0: QUEUE FRONT MESSAGE NOT FOUND.");
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
        getResponseDataStreamer()->writeString("404: Not Found.");
        Globals::getRPCLog()->log(Mantids::Application::Logs::LEVEL_WARN, remotePairAddress,"","", "", "wmqServer", 65535, "R/404: %s",getRequestURI().c_str());
    }

    return ret;
}

void WebClientHdlr::setWebClientParameters(const webClientParams &newWebClientParameters)
{
    webClientParameters = newWebClientParameters;
}
