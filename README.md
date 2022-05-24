# uLightWMQ 

Unmanarc Lightweight HTTPS Web Message Queue
  
Author: Aaron Mizrachi (unmanarc) <aaron@unmanarc.com>   
Main License: LGPLv3



***
## Installing packages (HOWTO)


- [Manual build guide](BUILD.md)
- COPR Packages (Fedora/CentOS/RHEL/etc):  
[![Copr build status](https://copr.fedorainfracloud.org/coprs/amizrachi/unmanarc/package/uLightWMQ/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/amizrachi/unmanarc/package/uLightWMQ/)


### Simple installation guide for Fedora/RHEL:

- Proceed to activate our repo's and download/install uLightWMQ:
```bash
# NOTE: for RHEL7 replace dnf by yum
dnf copr enable amizrachi/unmanarc

dnf -y install uLightWMQ
```

- Once installed, you can continue by activating/enabling the service:
```bash
systemctl enable --now uLightWMQ
```

- Don't forget to open the firewall:

```bash
# Add Website port:
firewall-cmd --zone=public --permanent --add-port 60443/tcp
# Reload Firewall rules
firewall-cmd --reload
```

***
## Usage (HOWTO/Examples)

1. First, replace the ca.crt/web_snakeoil.key/web_snakeoil.crt with your own X.509 certs.
2. (re)Start the program in background 
3. Create certificates for you own endpoints (client X.509 certs)
4. Every endpoint should start executing /pop to create their own database.
5. put the hostname (in this case webserver) to /etc/hosts or your own DNS.

After that, you can deliver messages like this  (eg. from alice to bob):

```bash
# Waiting for bob's answer while asking to close the app:
curl --data-binary "Do you really want to close this app?"  -H "Content-Type: application/octet-stream" --cert issued/alice.crt --key private/alice.key --cacert ca.crt  -v 'https://webserver:60443/push?dst=bob&waitForAnswer=1'
```

And answer messages like this (as bob, the msg receptor):

```bash
# As bob, Load the private/public key that authenticate with the ca.crt:
curl --cert issued/bob.crt --key private/bob.key --cacert ca.crt  -v 'https://webserver:60443/front'
```

And then, using the curl verbosity mode, check at the header the message Id and send your answer.

if you want to wait until online the message comes, then:

```bash
# As bob, Load the private/public key that authenticate with the ca.crt:
curl --cert issued/bob.crt --key private/bob.key --cacert ca.crt  -v 'https://webserver:60443/front?wait=1'
```

And if you want to answer a message (don't forget change the msgId):

```bash
# Answer the message by his Id.
curl --data-binary "NO"  -H "Content-Type: application/octet-stream" --cert issued/bob.crt --key private/bob.key --cacert ca.crt  -v 'https://webserver:60443/answer?msgId=12'
```

after that, the web server will deliver the answer to the listener.


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
* sqlite3
* cmake3
* C++11 Compatible Compiler (like GCC >=5)
