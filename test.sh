#!/bin/bash
echo "Make dmp module"
make
export size=512
echo "Inserting module to kernel"
insmod dmp.ko
echo "Creating test block device"
dmsetup create zero1 --table "0 $size zero"
echo "Checking if device is created"
ls -al /dev/mapper/*
echo "Creatint dmp"
dmsetup create dmp1 --table "0 $size dmp /dev/mapper/zero1 0"
echo "Checking if dmp is created"
ls -al /dev/mapper/*
echo "Reading"
dd if=/dev/random of=/dev/mapper/dmp1 bs=4k count=1
echo "Writing"
dd of=/dev/null if=/dev/mapper/dmp1 bs=4k count=1
echo "Statistics:"
cat /sys/module/dmp/stat/volumes
