angharad
========

Angharad server with JSON/REST interface to command domotic devices

Clients available: [Angharadweb](https://github.com/babelouest/angharadweb )

Installation
============

You must have the following development libraries installed to compile:
- libmicrohttpd 1.0
- libconfig
- libsqlite3
- libmath
- libpthread

Create the database:
```shell
$ mkdir /var/cache/angharad
$ sqlite3 /var/cache/angharad/angharad.db < angharad.sql
$ sqlite3 /var/cache/angharad/archive.db < angharad-archive.sql
```

Run the command 
$ make angharad

Using angharad server
=====================

To compile and run the server, run the command
```shell
$ make angharad
$ ./angharad path/to/config/file
```

To install the compiled server in /usr/local/bin, run the command
```shell
$ make install
```

access to the server commands via a client through the origin address http://angharad-server:port/*prefix*/

To use angharad as a Daemon, install the init file as root:
```shell
# cp angharad-init /etc/init.d/angharad
# update-rc.d angharad defaults
# service angharad start
```
