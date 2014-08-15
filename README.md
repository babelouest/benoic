angharad
========

Angharad server with JSON/REST interface to command domotic devices

Installation
============

You must have the following development libraries installed to compile:
- libmicrohttpd 1.0
- libconfig
- libsqlite3

Using angharad server
=====================

run the command 'angharad path/to/config/file' to run the server
access to the server commands via a client through the origin address http[s]://angharad-server:port/*prefix*/
