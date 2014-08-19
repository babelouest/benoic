angharad
========

Angharad server with JSON/REST interface to command domotic devices

Installation
============

You must have the following development libraries installed to compile:
- libmicrohttpd 1.0
- libconfig
- libsqlite3

Create the database:
```shell
$ mkdir /var/cache/angharad
$ sqlite /var/cache/angharad/angharad.db < angharad.sql
```

Run the command 
$ make angharad

Using angharad server
=====================

run the command `$ angharad path/to/config/file` to run the server
access to the server commands via a client through the origin address http[s]://angharad-server:port/*prefix*/

To use angharad as a Daemon, install the init file as root:
```shell
# cp angharad-init /etc/init.d/angharad
# update-rc.d angharad defaults
# service angharad start
```
