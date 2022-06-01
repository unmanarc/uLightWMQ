# uLightWMQ 

Unmanarc Lightweight HTTPS Web Message Queue
  
Author: Aaron Mizrachi (unmanarc) <<aaron@unmanarc.com>>   
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
## Usage/HOWTO:

1. First, replace the ca.crt/web_snakeoil.key/web_snakeoil.crt with your own X.509 certs.
2. (re)Start the program in background 
3. Create certificates for you own endpoints (client X.509 certs)
4. Every endpoint should start executing /pop to create their own database.
5. put the hostname (in this case webserver) to /etc/hosts or your own DNS.


***

### For X.509 Authenticated Web Clients

After that, you can deliver/push messages like this  (eg. from alice to bob):

```bash
# Alice is only telling bob something (reply is not required):
curl --data-binary "Are you sure?"  -H "Content-Type: application/octet-stream" --cert issued/alice.crt --key private/alice.key --cacert ca.crt  -v 'https://webserver:60443/push?dst=bob&reqReply=0'

# Alice is asking bob if he is sure about something...
curl --data-binary "Are you sure?"  -H "Content-Type: application/octet-stream" --cert issued/alice.crt --key private/alice.key --cacert ca.crt  -v 'https://webserver:60443/push?dst=bob&reqReply=1'

# Then, the HTTP response will be the message ID in the bob (dst) queue.
```

Then, someone can use the /front (or /get) command to get the messages from his queue:

```bash
# As bob, Load the private/public key that authenticate with the ca.crt:
curl --cert issued/bob.crt --key private/bob.key --cacert ca.crt  -v 'https://webserver:60443/front?wait=0'

# Front can also receive some other parameters:
# /front?wait=0 > default behaviour: will not wait, if there is no message the connection will return immediatly
# /front?wait=1 > will wait online for some seconds until bob receives a new message in his queue, 

# On /front you can also use removeAfterRead for non-answerable messages like this, the message will be immediatle removed:
curl --cert issued/bob.crt --key private/bob.key --cacert ca.crt  -v 'https://webserver:60443/front?removeAfterRead=1&wait=0'

# Also you can use /get to get messages as a list (not as queue)
curl --cert issued/bob.crt --key private/bob.key --cacert ca.crt  -v 'https://webserver:60443/get?msgId=897'
```

Supposing that the ID is 897 and bob wants to answer the alice "Are you sure?" question, then it can be done with /reply:

```bash
# Reply the message by his Id.
curl --data-binary "YES"  -H "Content-Type: application/octet-stream" --cert issued/bob.crt --key private/bob.key --cacert ca.crt  -v 'https://webserver:60443/reply?msgId=897'
# after that, the web server will deliver the reply to the listener.
```

If you want to wait for any reply on any of your sent messages:

```bash
# This is the way you get message reply, it will wait (or timeout) until bob answer the message
curl --data-binary "YES"  -H "Content-Type: application/octet-stream" --cert issued/bob.crt --key private/bob.key --cacert ca.crt  -v 'https://webserver:60443/waitForReply?msgId=897dst=bob&removeAfterRead=1'
# after that, the message will be displayed and the message registry will be dropped
```

Finally, For removing a message from my own queue, you can use /remove:

```bash
# Removing the message
curl --data-binary "YES"  -H "Content-Type: application/octet-stream" --cert issued/bob.crt --key private/bob.key --cacert ca.crt  -v 'https://webserver:60443/remove?msgId=109'
```




***
### For User/Pass Authenticated Web Clients

First, after you start the server, the user database will be created in `/var/lib/ulightwmq/users.db`

there you can create users with an alternate authentication method (http basic user/pass).

1. first create the SHA256 password:

```bash
# Command:
read -s -p 'Pass: ' PASS ; echo;  (echo -n $PASS | sha256sum | awk '{print $1}'); PASS=

# Expected Answer for testing123:
## pass:
## b822f1cd2dcfc685b47e83e3980289fd5d8e3ff3a82def24d7d1d68bb272eb32
```

2. then you insert into the database using `sqlite3 /var/lib/ulightwmq/users.db`

```sql
insert into users(user,hash) values('testing','b822f1cd2dcfc685b47e83e3980289fd5d8e3ff3a82def24d7d1d68bb272eb32');
```

And now you can use this authentication with every server URL like this (beware that ps may expose your password):

```bash
# Pushing a message for bob:
curl --data-binary "Hello Bob" -H "Content-Type: application/octet-stream" --cacert ca.crt  -v 'https://testing:testing123@webserver:60443/push?dst=bob'
```



***
### Don't forget!!!

* For a queue to exist for any other user, you must execute a front or remove operation with the user certificate or password. If not, nobody will be able to send messages to the queue
* Both user/pass and X.509 methods can coexist in the same server
* Both authentications have the same queue backend, you can push using user/pass and retrieve using x509

***

### Overall Pre-requisites:

* libMantids
* openssl
* sqlite3
* cmake3
* C++11 Compatible Compiler (like GCC >=5)
