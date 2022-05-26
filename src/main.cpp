#include <mdz_prg_service/application.h>
#include <mdz_prg_logs/applog.h>
#include <mdz_net_sockets/socket_tls.h>

#include "config.h"
#include "globals.h"

#include "web/wmq509authserverimpl.h"
#include "web/wmquserpassserverimpl.h"
#include "db/passdb.h"
#include "db/dbcollection.h"

using namespace Mantids;
using namespace Mantids::Application;

class uLightWMQ : public Mantids::Application::Application
{
public:
    uLightWMQ() {
    }

    void _shutdown()
    {
        Globals::getAppLog()->log0(__func__,Logs::LEVEL_INFO, "SIGTERM received.");
    }

    void _initvars(int argc, char *argv[], Arguments::GlobalArguments * globalArguments)
    {
        globalArguments->setInifiniteWaitAtEnd(true);

        globalArguments->setVersion( atoi(PROJECT_VER_MAJOR), atoi(PROJECT_VER_MINOR), atoi(PROJECT_VER_PATCH), "a" );
        globalArguments->setLicense("LGPLv3");
        globalArguments->setAuthor("Aarón Mizrachi");
        globalArguments->setEmail("aaron@unmanarc.com");
        globalArguments->setDescription(PROJECT_DESCRIPTION);

        webserverX509Auth.getWebClientParameters().softwareVersion = globalArguments->getVersion();
        webserverUserPass.getWebClientParameters().softwareVersion = globalArguments->getVersion();

        globalArguments->addCommandLineOption("Service Options", 'c', "config-dir" , "Configuration directory"  , "/etc/ulightwmq", Mantids::Memory::Abstract::TYPE_STRING );
    }

    bool _config(int argc, char *argv[], Arguments::GlobalArguments * globalArguments)
    {
        unsigned int logMode = Logs::MODE_STANDARD;

        Mantids::Network::TLS::Socket_TLS::prepareTLS();

        Logs::AppLog initLog(Logs::MODE_STANDARD);
        initLog.setPrintEmptyFields(true);
        initLog.setUsingAttributeName(false);
        initLog.setUserAlignSize(1);

        std::string configDir = globalArguments->getCommandLineOptionValue("config-dir")->toString();

        initLog.log0(__func__,Logs::LEVEL_INFO, "Loading configuration: %s", (configDir + "/config.ini").c_str());

        boost::property_tree::ptree pRunningConfig;

        if (access(configDir.c_str(),R_OK))
        {
            initLog.log0(__func__,Logs::LEVEL_CRITICAL, "Missing configuration dir: %s", configDir.c_str());
            return false;
        }

        chdir(configDir.c_str());

        if (!access((configDir + "/config.ini").c_str(),R_OK))
            boost::property_tree::ini_parser::read_ini((configDir + "/config.ini").c_str(),pRunningConfig);
        else
        {
            initLog.log0(__func__,Logs::LEVEL_CRITICAL, "Missing configuration: %s", (configDir + "/config.ini").c_str());
            return false;
        }

        Globals::setLocalInitConfig(pRunningConfig);

        if ( Globals::getLC_LogsUsingSyslog() ) logMode|=Logs::MODE_SYSLOG;

        Globals::setAppLog(new Logs::AppLog(logMode));
        Globals::getAppLog()->setPrintEmptyFields(true);
        Globals::getAppLog()->setUsingColors(Globals::getLC_LogsShowColors());
        Globals::getAppLog()->setUsingPrintDate(Globals::getLC_LogsShowDate());
        Globals::getAppLog()->setModuleAlignSize(26);
        Globals::getAppLog()->setUsingAttributeName(false);
        Globals::getAppLog()->setDebug(Globals::getLC_LogsDebug());

        Globals::setRPCLog(new Logs::RPCLog(logMode));
        Globals::getRPCLog()->setPrintEmptyFields(true);
        Globals::getRPCLog()->setUsingColors(Globals::getLC_LogsShowColors());
        Globals::getRPCLog()->setUsingPrintDate(Globals::getLC_LogsShowDate());
        Globals::getRPCLog()->setDisableDomain(true);
        Globals::getRPCLog()->setDisableModule(true);
        Globals::getRPCLog()->setModuleAlignSize(26);
        Globals::getRPCLog()->setUsingAttributeName(false);
        Globals::getRPCLog()->setStandardLogSeparator(",");
        Globals::getRPCLog()->setDebug(Globals::getLC_LogsDebug());

        webserverUserPass.prepare();
        webserverX509Auth.prepare();

        Globals::getAppLog()->log0(__func__,Logs::LEVEL_INFO, "Configuration file loaded OK.");

        return true;
    }

    int _start(int argc, char *argv[], Arguments::GlobalArguments * globalArguments)
    {
        PassDB::start();
        DBCollection::start();
        webserverX509Auth.start();
        webserverUserPass.start();
        return 0;
    }

private:
    WMQX509AUTHServerImpl webserverX509Auth;
    WMQUserPassServerImpl webserverUserPass;

};

int main(int argc, char *argv[])
{
    uLightWMQ * main = new uLightWMQ;
    return StartApplication(argc,argv,main);
}
