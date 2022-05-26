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

    static std::string getLC_WebServerX509AUTH_TLSCAFilePath()
    {
        return pLocalConfig.get<std::string>("WebServerX509AUTH.CAFile", ".." + dirSlash + "keys" + dirSlash + "ca.crt");
    }

    static std::string getLC_WebServerX509AUTH_TLSCertFilePath()
    {
        return pLocalConfig.get<std::string>("WebServerX509AUTH.CertFile", ".." + dirSlash + "keys" + dirSlash + "web_snakeoil.crt");
    }

    static std::string getLC_WebServerX509AUTH_TLSKeyFilePath()
    {
        return pLocalConfig.get<std::string>("WebServerX509AUTH.KeyFile",  ".." + dirSlash + "keys" + dirSlash + "web_snakeoil.key");
    }

    static uint32_t getLC_WebServerX509AUTH_MaxThreads()
    {
        return pLocalConfig.get<uint32_t>("WebServerX509AUTH.MaxThreads",128);
    }

    static uint16_t getLC_WebServerX509AUTH_ListenPort()
    {
        return pLocalConfig.get<uint16_t>("WebServerX509AUTH.ListenPort",60443);
    }

    static uint16_t getLC_WebServerX509AUTH_Enabled()
    {
        return pLocalConfig.get<bool>("WebServerX509AUTH.Enabled",true);
    }

    static std::string getLC_WebServerX509AUTH_ListenAddr()
    {
        return pLocalConfig.get<std::string>("WebServerX509AUTH.ListenAddr","0.0.0.0");
    }

    static bool getLC_WebServerX509AUTH_UseIPv6()
    {
        return pLocalConfig.get<bool>("WebServerX509AUTH.ipv6",false);
    }

    static std::string getLC_WebServerUSERPASS_TLSCertFilePath()
    {
        return pLocalConfig.get<std::string>("WebServerUSERPASS.CertFile", ".." + dirSlash + "keys" + dirSlash + "web_snakeoil.crt");
    }

    static std::string getLC_WebServerUSERPASS_TLSKeyFilePath()
    {
        return pLocalConfig.get<std::string>("WebServerUSERPASS.KeyFile",  ".." + dirSlash + "keys" + dirSlash + "web_snakeoil.key");
    }

    static uint32_t getLC_WebServerUSERPASS_MaxThreads()
    {
        return pLocalConfig.get<uint32_t>("WebServerUSERPASS.MaxThreads",128);
    }

    static uint16_t getLC_WebServerUSERPASS_ListenPort()
    {
        return pLocalConfig.get<uint16_t>("WebServerUSERPASS.ListenPort",60443);
    }

    static uint16_t getLC_WebServerUSERPASS_Enabled()
    {
        return pLocalConfig.get<bool>("WebServerUSERPASS.Enabled",true);
    }

    static std::string getLC_WebServerUSERPASS_ListenAddr()
    {
        return pLocalConfig.get<std::string>("WebServerUSERPASS.ListenAddr","0.0.0.0");
    }

    static bool getLC_WebServerUSERPASS_UseIPv6()
    {
        return pLocalConfig.get<bool>("WebServerUSERPASS.ipv6",false);
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
