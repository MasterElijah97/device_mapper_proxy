#!/bin/bash
echo "Removing dmp"
dmsetup remove dmp1
echo "Removing test block device"
dmsetup remove zero1
echo "Removing module from kernel"
rmmod dmp.ko
