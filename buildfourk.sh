#!/bin/sh

cd ~/Fourk
mkdir -p build
cd build

export CFLAGS="-g3 -O0 -Wall -Wextra -fdiagnostics-show-option -DPUBLIC_SERVER -DSUPPRESS_REPLAY -DWHEREIS"
nice -n 20 ../aimake --with=tilecompile -i /fourkdir-4.3.0.5 --destdir /opt/nethack/chroot -S sudo 2>&1 | tee ../output-4305-dirtybuild.txt
cd /opt/nethack/chroot/fourkdir-4.3.0.5
sudo chmod 755 nhfourk
sudo chown -R games:games save
sudo /opt/nethack/chroot/usr/local/bin/chrootlibs.sh nhfourk lib/*

sudo mkdir -p /opt/nethack/chroot/dgldir/inprogress-4k4305
sudo chown -R games:games /opt/nethack/chroot/dgldir/inprogress-4k4305
sudo mkdir -p /opt/nethack/chroot/dgldir/extrainfo-4k
sudo chown -R games:games  /opt/nethack/chroot/dgldir/extrainfo-4k
