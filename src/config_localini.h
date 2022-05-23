#ifndef CONFIG_LOCALINI_H
#define CONFIG_LOCALINI_H

#include <boost/property_tree/ini_parser.hpp>

#ifdef _WIN32
static std::string dirSlash =  "\\";
#else
static std::string dirSlash =  "/";
#endif

class Config_LocalIni
{
public:
    Config_LocalIni();

    static void setLocalInitConfig(const boost::property_tree::ptree &config);

    static bool getLC_LogsUsingSyslog()
    {
        return pLocalConfig.get<bool>("Logs.Syslog",true);
    }

    static bool getLC_LogsShowColors()
    {
         return pLocalConfig.get<bool>("Logs.ShowColors",true);
    }

    static bool getLC_LogsShowDate()
    {
        return pLocalConfig.get<bool>("Logs.ShowDate",true);
    }

    static bool getLC_LogsDebug()
    {
        return pLocalConfig.get<bool>("Logs.Debug",false);
    }

    static std::string getLC_WebServer_TLSCAFilePath()
    {
        return pLocalConfig.get<std::string>("WebServer.CAFile", ".." + dirSlash + "keys" + dirSlash + "ca.crt");
    }

    static std::string getLC_WebServer_TLSCertFilePath()
    {
        return pLocalConfig.get<std::string>("WebServer.CertFile", ".." + dirSlash + "keys" + dirSlash + "web_snakeoil.crt");
    }

    static std::string getLC_WebServer_TLSKeyFilePath()
    {
        return pLocalConfig.get<std::string>("WebServer.KeyFile",  ".." + dirSlash + "keys" + dirSlash + "web_snakeoil.key");
    }

    static uint32_t getLC_WebServer_MaxThreads()
    {
        return pLocalConfig.get<uint32_t>("WebServer.MaxThreads",128);
    }

    static uint16_t getLC_WebServer_ListenPort()
    {
        return pLocalConfig.get<uint16_t>("WebServer.ListenPort",60443);
    }

    static std::string getLC_WebServer_ListenAddr()
    {
        return pLocalConfig.get<std::string>("WebServer.ListenAddr","0.0.0.0");
    }

    static bool getLC_WebServer_UseIPv6()
    {
        return pLocalConfig.get<bool>("WebServer.ipv6",false);
    }

    static std::string getLC_Database_FilesPath()
    {
        return pLocalConfig.get<std::string>("DB.FilesPath","/var/lib/ulightwmq");
    }

    static uint64_t getLC_Database_DefaultExpirationTime()
    {
        // default: 1 week.
        return pLocalConfig.get<uint64_t>("Message.DefaultExpirationTime",86400*7);
    }

    static uint32_t getLC_Database_MaxMessageSize()
    {
        return pLocalConfig.get<uint32_t>("Message.MaxSize",4096);
    }

    static uint32_t getLC_Database_MessageQueueFrontTimeout()
    {
        return pLocalConfig.get<uint32_t>("Message.QueueFrontTimeout",30);
    }
    static uint32_t getLC_Database_AnswerTimeout()
    {
        return pLocalConfig.get<uint32_t>("Message.AnswerTimeout",30);
    }

    static uint32_t getLC_Database_ExpiredGCCleanUpEvery()
    {
        return pLocalConfig.get<uint32_t>("Message.ExpiredGCCleanUpEvery",30);
    }

private:
    static boost::property_tree::ptree pLocalConfig;

};

#endif // CONFIG_LOCALINI_H
