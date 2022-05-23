# Manual Build/Install Instructions

This guide will help you in case you want to understand/build/install uLightWMQ from scratch.

***

## STEP 1: Building

### Overall Pre-requisites:

* gcc-c++ or C++11 Compatible Compiler (like GCC >=5)
* SystemD enabled system (ubuntu, centos, fedora, etc)
* libMantids-devel
* libMantids-sqlite
* pthread
* zlib-devel
* openssl-devel
* boost-devel
* sqlite-devel

***

Having these prerequisites (eg. by yum install), you can start the build process (as root) by doing:

```
cd /root
git clone https://github.com/unmanarc/uLightWMQ
cmake -B../builds/uLightWMQ . -DCMAKE_VERBOSE_MAKEFILE=ON
cd ../builds/uLightWMQ
make -j12 install
```

Now, the application is installed in the operating system, you can proceed to the next step

## STEP 2: Installing files and configs

Then:
- copy the **/etc/ulightwmq** directory
- create the **/var/lib/ulightwmq** if does not exist

```
cp -a ~/uLightWMQ/etc/ulightwmq /etc/
chmod 600 /etc/ulightwmq/keys/web_snakeoil.key
mkdir -p /var/lib/ulightwmq
```

Security Alert:

`Remember to change the snakeoil X.509 Certificates with your own ones, if not the communication can be eavesdropped or tampered!!!`

## STEP 3: Service Intialization

We are going to create the services by executing:

```
cat << 'EOF' | install -m 640 /dev/stdin /usr/lib/systemd/system/ulightwmq.service
[Unit]
Description=Unmanarc Lightweight HTTPS Web Message Queue
After=network.target

[Service]
Type=simple
Restart=always
RestartSec=5
WorkingDirectory=/tmp
ExecStart=/usr/bin/uLightWMQ
Environment=

[Install]
WantedBy=multi-user.target
EOF

cat << 'EOF' | install -m 640 /dev/stdin /etc/default/ulightwmq
LD_LIBRARY_PATH=/usr/local/lib:
EOF

systemctl daemon-reload
systemctl enable --now ulightwmq.service
```


Now you are done!

Congrats.