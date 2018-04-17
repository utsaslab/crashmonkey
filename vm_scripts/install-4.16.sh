#!/bin/bash

cd /tmp

wget http://kernel.ubuntu.com/~kernel-ppa/mainline/v4.16/linux-headers-4.16.0-041600_4.16.0-041600.201804012230_all.deb
wget http://kernel.ubuntu.com/~kernel-ppa/mainline/v4.16/linux-headers-4.16.0-041600-generic_4.16.0-041600.201804012230_amd64.deb
wget http://kernel.ubuntu.com/~kernel-ppa/mainline/v4.16/linux-image-4.16.0-041600-generic_4.16.0-041600.201804012230_amd64.deb

sudo dpkg -i *.deb

