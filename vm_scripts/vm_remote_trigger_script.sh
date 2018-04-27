#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide the filesystem as the parameter;"
    exit 1
fi
fs=$1

# If xfsMonkey is already running, skip running again
num=`ps aux | grep xfsMonkey | grep -v grep | wc -l`

if [ $num -ne 0 ]
then
        echo `date` 'xfsMonkey is already running.. Skipping the triggering of xfsMonkey..  '
	exit
fi

num=`ps aux | grep vm_remote_trigger_script | grep -v grep | wc -l`

if [ $num -gt 2 ]
then
        echo `date` 'trigger script might already be running (check the processes below).. Skipping the triggering of xfsMonkey..  '
	ps aux | grep vm_remote_trigger_script
        exit
fi


# install libattr for xattr headers
echo `date` 'Installing libattr1-dev..'
echo alohomora | sudo -S apt-get install --yes libattr1-dev btrfs-tools f2fs-tools xfsprogs

# update crashmonkey repo
echo `date` 'Updating crashmonkey repo..'
cd projects/crashmonkey

echo alohomora | sudo -S git checkout .
echo alohomora | sudo -S git pull
echo alohomora | sudo -S git checkout testing_seq_1

echo `date`  'Removing log files..'
echo alohomora | sudo -S rm *.log
echo alohomora | sudo -S rm build/*.log
echo alohomora | sudo -S rm diff_results/*

# Do cm cleanup -> rm cow_brd, disk_wrapper, umount etc
echo alohomora | sudo -S bash /home/pandian/cm_cleanup.sh

echo `date`  'Creating /mnt/snapshot..'
echo alohomora | sudo -S mkdir -p /mnt/snapshot

echo alohomora | sudo -S rm code/tests/j-lang*.cpp
echo alohomora | sudo -S rm build/tests/j-lang*.so

echo `date`  'Copying cpp workload files to code/tests/..'
cp ~/seq2/* code/tests/

echo `date`  'Compiling..'
make

echo `date`  'Moving built so files to xfsMonkeyTests/..'

echo alohomora | sudo -S rm -r build/xfsMonkeyTests

echo alohomora | sudo -S mkdir -p build/xfsMonkeyTests
echo alohomora | sudo -S mv build/tests/j-lang*.so build/xfsMonkeyTests/

echo `date` 'Triggering xfsMonkey run..'
nohup echo alohomora | sudo -S python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t $fs -e 102400 > xfsmonkey_seq2_"$fs"_run1.log &
echo `date`  'Triggered.'
