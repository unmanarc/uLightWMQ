#include "wmqserverimpl.h"
#include "../globals.h"
#include "../config.h"

#include <mdz_net_sockets/socket_tls.h>


using namespace Mantids::Application;
//using namespace Mantids::Authentication;
using namespace Mantids;


WMQServerImpl::WMQServerImpl()
{
}

void WMQServerImpl::prepare()
{
    Network::TLS::Socket_TLS *socketTLS = new Network::TLS::Socket_TLS;

    if (!socketTLS->setTLSPrivateKeyPath(Globals::getLC_WebServer_TLSKeyFilePath().c_str()))
    {
        Globals::getAppLog()->log0(__func__,Logs::LEVEL_CRITICAL, "X.509 Private Key file not found.");
        exit(-10);
    }
    if (!socketTLS->setTLSPublicKeyPath(Globals::getLC_WebServer_TLSCertFilePath().c_str()))
    {
        Globals::getAppLog()->log0(__func__,Logs::LEVEL_CRITICAL, "X.509 Public Certificate file not found.");
        exit(-11);
    }
    if (!socketTLS->setTLSCertificateAuthorityPath(Globals::getLC_WebServer_TLSCAFilePath().c_str()))
    {
        Globals::getAppLog()->log0(__func__,Logs::LEVEL_CRITICAL, "X.509 Certificate Authority File Not Found.");
        exit(-12);
    }

    socketTLS->setUseIPv6( Globals::getLC_WebServer_UseIPv6() );
    if (!socketTLS->listenOn( Globals::getLC_WebServer_ListenPort(), Globals::getLC_WebServer_ListenAddr().c_str() ))
    {
        Globals::getAppLog()->log0(__func__,Logs::LEVEL_CRITICAL, "Unable to listen at %s:%d",
                                            Globals::getLC_WebServer_ListenAddr().c_str(), Globals::getLC_WebServer_ListenPort());
        exit(-20);
    }

    multiThreadedAcceptor.setAcceptorSocket(socketTLS);
    multiThreadedAcceptor.setCallbackOnConnect(_callbackOnConnect,this);
    multiThreadedAcceptor.setCallbackOnInitFail(_callbackOnInitFailed,this);
    multiThreadedAcceptor.setCallbackOnTimedOut(_callbackOnTimeOut,this);
    multiThreadedAcceptor.setMaxConcurrentClients( Globals::getLC_WebServer_MaxThreads() );

}

void WMQServerImpl::start()
{
    multiThreadedAcceptor.startThreaded();
    Globals::getAppLog()->log0(__func__,Logs::LEVEL_INFO, "HTTPS Server Listening @%s:%d", Globals::getLC_WebServer_ListenAddr().c_str(), Globals::getLC_WebServer_ListenPort());
}

bool WMQServerImpl::_callbackOnConnect(void *obj, Network::Streams::StreamSocket *s, const char *, bool)
{
    std::string tlsCN;
    WMQServerImpl * webServer = (WMQServerImpl *)obj;

    bool isSecure;
    if ((isSecure=s->isSecure()) == true)
    {
#ifdef WITH_SSL_SUPPORT
        Network::TLS::Socket_TLS * tlsSock = (Network::TLS::Socket_TLS *)s;
        tlsCN = tlsSock->getTLSPeerCN();
#endif
    }

    // Prepare the web services handler.
    WebClientHdlr webHandler(nullptr,s);

    char inetAddr[INET6_ADDRSTRLEN];
    s->getRemotePair(inetAddr);

    webHandler.setIsSecure(isSecure);
    webHandler.setRemotePairAddress(inetAddr);
    webHandler.setWebClientParameters(webServer->getWebClientParameters());

    // Handle the Web Client here:
    Memory::Streams::Parsing::ParseErrorMSG err;
    webHandler.parseObject(&err);

    return true;
}

bool WMQServerImpl::_callbackOnInitFailed(void *, Network::Streams::StreamSocket *s, const char *, bool)
{
    return true;
}

void WMQServerImpl::_callbackOnTimeOut(void *, Network::Streams::StreamSocket *s, const char *, bool)
{
    s->writeString("HTTP/1.1 503 Service Temporarily Unavailable\r\n");
    s->writeString("Content-Type: text/html; charset=UTF-8\r\n");
    s->writeString("Connection: close\r\n");
    s->writeString("\r\n");
    s->writeString("<center><h1>503 Service Temporarily Unavailable</h1></center><hr>\r\n");
}

webClientParams &WMQServerImpl::getWebClientParameters()
{
    return webClientParameters;
}
