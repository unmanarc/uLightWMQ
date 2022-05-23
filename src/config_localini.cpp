#include "config_localini.h"

boost::property_tree::ptree Config_LocalIni::pLocalConfig;

Config_LocalIni::Config_LocalIni()
{

}

void Config_LocalIni::setLocalInitConfig(const boost::property_tree::ptree &config)
{
    // Initial config:
    pLocalConfig = config;
}



