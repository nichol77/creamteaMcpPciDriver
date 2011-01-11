#!/bin/bash

DEVICE=`/sbin/lspci -d 0x10ee: | awk ' {print $1 }'`
echo $DEVICE


