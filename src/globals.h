#ifndef GLOBALS_H
#define GLOBALS_H

#include "config_localini.h"
#include <boost/property_tree/ini_parser.hpp>
#include <mdz_prg_logs/applog.h>
#include <mdz_prg_logs/rpclog.h>
#include <mdz_xrpc_webserver/webserver.h>

#include <mutex>
#include <list>

class Globals : public Config_LocalIni
{
public:
    Globals();

    static Mantids::Application::Logs::AppLog *getAppLog();
    static void setAppLog(Mantids::Application::Logs::AppLog *value);

    static Mantids::Application::Logs::RPCLog *getRPCLog();
    static void setRPCLog(Mantids::Application::Logs::RPCLog *value);

    // Web Server:
    static Mantids::RPC::Web::WebServer *getWebServer();
    static void setWebServer(Mantids::RPC::Web::WebServer *value);

private:
    static Mantids::Application::Logs::AppLog * applog;
    static Mantids::Application::Logs::RPCLog * rpclog;
    static Mantids::RPC::Web::WebServer * webServer;
};


#endif // GLOBALS_H
