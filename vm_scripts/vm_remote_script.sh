#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters; Please provide the filesystem as the parameter;"
    exit 1
fi
fs=$1

# install libattr for xattr headers
echo Installing libattr1-dev..
echo alohomora | sudo -S apt-get install --yes libattr1-dev btrfs-tools f2fs-tools xfsprogs

# update crashmonkey repo
echo Updating crashmonkey repo..
cd projects/crashmonkey

echo alohomora | sudo -S git checkout .
echo alohomora | sudo -S git pull
echo alohomora | sudo -S git checkout testing_seq_1

echo Removing log files..
echo alohomora | sudo -S rm *.log
echo alohomora | sudo -S rm build/*.log
echo alohomora | sudo -S rm diff_results/*

echo Creating /mnt/snapshot..
echo alohomora | sudo -S mkdir -p /mnt/snapshot

echo Copying cpp workload files to code/tests/..
cp ~/seq2/* code/tests/

echo Compiling..
make

echo Moving built so files to xfsMonkeyTests/..
echo alohomora | sudo -S mkdir -p build/xfsMonkeyTests
echo alohomora | sudo -S mv build/tests/j-lang*.so build/xfsMonkeyTests/

echo 'Triggering xfsMonkey run..'
nohup echo alohomora | sudo -S python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t $fs -e 102400 > xfsmonkey_seq2_"$fs"_run1.log &
echo 'Triggered.'
