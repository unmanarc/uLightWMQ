# uLightWMQ 

Unmanarc Light HTTPS Web Message Queue
  
Author: Aaron Mizrachi (unmanarc) <aaron@unmanarc.com>   
Main License: LGPLv3

***
## Builds

- COPR (Fedora/CentOS/etc):  
[![Copr build status](https://copr.fedorainfracloud.org/coprs/amizrachi/unmanarc/package/uLightWMQ/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/amizrachi/unmanarc/package/uLightWMQ/)


Install in Fedora/RHEL8/9:
```bash
dnf copr enable amizrachi/unmanarc

dnf -y install uLightWMQ
```

Install in RHEL7:
```bash
dnf copr enable amizrachi/unmanarc

yum -y install uLightWMQ
```

***
## Project Description

Simple HTTPS Server for JSON Message Queue

- TLS/SSL Common Name peer authentication
- 100% HTTPS Based
- Instant delivery or disk saved
- Function to wait in the HTTP client for new messages
- Function to wait for peer answer (with timeout)
- Very Lightweight (can be used in IoT)

***
## Building/Installing uLightWMQ

### Building Instructions:

This guide is optimized for centos7 (you can adapt this for your OS), and the generated binary should be compatible with other neewer distributions...

First, as prerequisite, you must have installed libMantids (as static libs, and better if MinSizeRel)

and then...

```
git clone https://github.com/unmanarc/uLightWMQ
cd uLightWMQ
cmake3 . -DPORTABLE=ON -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX:PATH=/opt/osslibs -DEXTRAPREFIX:PATH=/opt/osslibs -B../uLightWMQ-build
cd ../uLightWMQ-build
make -j4 install
```

And if you want to shrink the binary size:

```
strip -x -X -s /opt/osslibs/bin/uLightWMQ
upx -9 --ultra-brute /opt/osslibs/bin/uLightWMQ
```

***
## Compatibility

This program was tested so far in:

* OpenSUSE TumbleWeed
* Fedora Linux 34
* Ubuntu 20.04 LTS (Server)
* CentOS/RHEL 7/8

### Overall Pre-requisites:

* libMantids
* openssl
* jsoncpp
* sqlite3
* cmake3
* C++11 Compatible Compiler (like GCC >=5)
