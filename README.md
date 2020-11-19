# Device Mapper Proxy module for Linux Kernel

This module was tested on Ubuntu 20.04

File descriptions:
* dmp.c - source code
* Makefile - build script for make
* test.sh - script for building and testing module
* clean.sh - script for cleaning up system after testing module

# How to use

* clone this repo
* write the following commands
```bash
cd dmp
chmod +x ./test.sh
sudo ./test.sh
```
* then you can cleanup your system
```bash
chmod +x ./clean.sh
sudo ./clean.sh
```
