#!/bin/bash
echo "Packing the ramdisk files in: $PWD"
cd $1 && find . | cpio -o -H newc | gzip > /tmp/ramdisk.cpio.gz
