#include "globals.h"

Mantids::Application::Logs::AppLog * Globals::applog = nullptr;
Mantids::Application::Logs::RPCLog * Globals::rpclog = nullptr;


Mantids::RPC::Web::WebServer * Globals::webServer = nullptr;

Globals::Globals()
{
}

Mantids::Application::Logs::AppLog *Globals::getAppLog()
{
    return applog;
}

void Globals::setAppLog(Mantids::Application::Logs::AppLog *value)
{
    applog = value;
}

Mantids::Application::Logs::RPCLog *Globals::getRPCLog()
{
    return rpclog;
}

void Globals::setRPCLog(Mantids::Application::Logs::RPCLog *value)
{
    rpclog = value;
}

Mantids::RPC::Web::WebServer *Globals::getWebServer()
{
    return webServer;
}

void Globals::setWebServer(Mantids::RPC::Web::WebServer *value)
{
    webServer = value;
}
