#ifndef WMQUSERPASSSERVERIMPL_H
#define WMQUSERPASSSERVERIMPL_H

#include <mdz_net_sockets/socket_tls.h>
#include <mdz_net_sockets/socket_acceptor_multithreaded.h>

#include "webclienthdlr.h"

class WMQUserPassServerImpl
{
public:
    WMQUserPassServerImpl();
    void prepare();
    void start();

    webClientParams &getWebClientParameters();

private:
    /**
     * callback when connection is fully established (if the callback returns false, connection socket won't be automatically closed/deleted)
     */
    static bool _callbackOnConnect(void *, Mantids::Network::Streams::StreamSocket *, const char *, bool);
    /**
     * callback when protocol initialization failed (like bad X.509 on TLS) (if the callback returns false, connection socket won't be automatically closed/deleted)
     */
    static bool _callbackOnInitFailed(void *, Mantids::Network::Streams::StreamSocket *, const char *, bool);
    /**
     * callback when timed out (all the thread queues are saturated) (this callback is called from acceptor thread, you should use it very quick)
     */
    static void _callbackOnTimeOut(void *, Mantids::Network::Streams::StreamSocket *, const char *, bool);

    webClientParams webClientParameters;
    Mantids::Network::Sockets::Acceptors::Socket_Acceptor_MultiThreaded multiThreadedAcceptor;

};

#endif // WMQUSERPASSSERVERIMPL_H
