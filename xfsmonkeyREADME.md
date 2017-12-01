# XFSMONKEY #

Run xfstests through crashmonkey.

_Make sure you do_ `sudo mkdir /mnt/scratch_device_mountpoint` _before executing any of the scripts._

## xfsmonkeya.sh ##
To run xfstests through crashmonkey, one simply needs to open xfsmonkeya.sh, fill out the config variables up top, and then run the script (sudo xfsmonkeya.sh).
This script essentially loops through all the specified xfstests and does the following:
for every xfstest:
	run it on crashmonkey
	get the __snap file, __profile file, stdout and stderr, and place them in the output directory

Internally, xfsmonkeya.sh uses xfsmonkey.py

## xfsmonkey.py ##

The only reason you would want to use xfsmonkey.py instead of xfsmonkeya.sh is if you want to run multiple xfstests inside
the same instance of crashmonkey.

Then, run the following command, substituting {scratchDevice, scratchDeviceMountPointer, testFile, testName}
```sh	
sudo ./xfsmonkey.py --primary-dev TEST -t /dev/scrathDevice /mnt/scratchDeviceMountPoint -e testFile/testName
```

Here are some examples: 
```sh
sudo ./xfsmonkey.py --primary-dev TEST -t /dev/sdb /mnt/sdbmount/ -e generic/011                # this test runs the generic 011 test (scratch device is /dev/sdb)
sudo ./xfsmonkey.py --primary-dev TEST -t /dev/sdb /mnt/sdbmount/ -e ext4/011                   # this test runs the ext4 011 test
sudo ./xfsmonkey.py --primary-dev TEST -t /dev/sdb /mnt/sdbmount/ -e ext4											# this test runs all the ext4 tests
```

Something to note, anything that suceedes the -e flag will be given directly as argument to ./check in xfstests.

By default, the FS is assumed to be of type ext4.

Type "./xfsmonkey --help" to check out the details.




