#!/bin/bash

DEVICE="04:0C"
ADDRESS=0xC08000
LENGTH=128

echo Prepping device at PCI $DEVICE...
setpci -v -s $DEVICE 04.w=147
./test w 0x18 $ADDRESS
./test w 0x19 $LENGTH
