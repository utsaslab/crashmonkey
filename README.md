# CrashMonkey #
### What is CrashMonkey? ###

CrashMonkey is a file-system agnostic testing framework for file-system consistency. It is meant to explore many crash states that are possible when a computer crashes in the middle of a write operation. CrashMonkey is made up of 3 main parts:

1. file-system agnostic kernel module for bio logging and disk snapshotting
1. user space test harness which coordinates everything
1. user space, user defined, test cases which specify the workload to be tested and, optionally, data consistency tests to run on each generated crash state.

The HotStorage'17 paper *CrashMonkey: A Framework to Automatically Test File-System Crash Consistency* has a more detailed explanation of the internals of CrashMonkey. [Paper PDF]( http://www.cs.utexas.edu/~vijay/papers/hotstorage17-crashmonkey.pdf), [Slides](http://www.cs.utexas.edu/~vijay/papers/hotstorage17-crashmonkey-slides.pdf), [Bibtex](http://www.cs.utexas.edu/~vijay/bibtex/hotstorage17-crashmonkey.bib)  

CrashMonkey also makes use of common Linux file system checker and repair programs like `fsck`.

### Research that uses CrashMonkey ###
1. *Barrier-Enabled IO Stack for Flash Storage*. Youjip Won, Hanyang University; Jaemin Jung, Texas A&M University; Gyeongyeol Choi, Joontaek Oh, and Seongbae Son, Hanyang University; Jooyoung Hwang and Sangyeun Cho, Samsung Electronics. Proceedings of the 16th USENIX Conference on File and Storage Technologies (FAST 18). [Link](https://www.usenix.org/conference/fast18/presentation/won)

### Getting Setup ###

#### Setting Up a VM ####
The easiest (and recommended) way to start working on (or using) CrashMonkey is to setup a virtual machine and run everything in the VM. This is partly so that any bugs in the kernel module don't bring down your whole system and partly because I just find it easier. In the future I may try to get a Docker running with all the needed packages and files so that things are easy to setup and get running. In the meantime, you should spin up an Ubuntu 14.04 LTS or Ubuntu 16.04.2 LTS VM and work on there. **CrashMonkey is known to work on kernel versions 3.13.0-121-generic, 4.4.0-62-generic, 4.15.0-041500-generic, and 4.16.0-041600rc7-generic.** The Ubuntu VM that you create will also need the following packages to properly build and run CrashMonkey:

* make
* git
* gcc
* g++
* libattr1-dev
* linux kernel headers
     * install with `sudo apt-get install linux-headers-$(uname -r)`
* packages for file systems that aren't included on the VM by default, for example:
     * `btrfs-tools` for btrfs
     * `xfsprogs` for xfs
     * `f2fs-tools` for f2fs

Furthermore, the VM should have enough disk space to build and compile CrashMonkey as well as enough RAM to run any tests you want. I mention RAM because CrashMonkey uses a RAM block device during its tests, so you will need to give it at least as much RAM as the largest test you plan on running. For small tests, a 20 GB hard drive for the Ubuntu install and also all other files (I'm lazy and don't feel like trimming it down more than that) and 2-4 GB of RAM should be more than enough.

To get CrashMonkey source files into your VM, run`git clone --recursive` CrashMonkey repo into a directory of your choosing.

### Compiling CrashMonkey ###
CrashMonkey can be built simply by running `make` in the root directory of the
repository. This will build all the needed kernel modules, tests, and test
harness components needed to run CrashMonkey.

#### Compiling User Defined Tests ####
User defined tests reside in the `code/tests` directory. They can be compiled
into static objects with `make tests`.

#### Compiling Tests for CrashMonkey ####
Some tests for CrashMonkey reside in the `test` directory of the repo. Tests leverage `googletests` and are used to ensure the correctness and functionality of some of the user space portions of CrashMonkey (ex. the descendants of the `Permuter` class). Right now you'll have to examine the outputted binary names to determine what each binary tests. In the future, the build system will be updated to run the tests after compiling them.

### Running CrashMonkey ###
CrashMonkey can be run either as a standalone program or as a background program. When in standalone mode, Crashmonkey will automatically load and run the user defined C++ setup and workload methods. In both modes, CrashMonkey will look for user defined data consistency tests in the `.so` test file provided to CrashMonkey on the command line. When run as a background process, the user is allowed to run setup and workload methods outside of CrashMonkey use a series of simple stub programs to communicate with CrashMonkey. In both modes of operation, command line flags have the same meaning.
CrashMonkey has some strange flag values that need cleaned up in future version.
Until that happens, **don't be worried by the fact that you need the
`-d /dev/cow_ram0` flag even though that device doesn't exist.** It is a device
presented by CrashMonkey's cow_brd kernel module and I never got around to
removing the flag.

#### Running as a Standalone Program ####
Before running any tests with CrashMonkey, you will have to create a directory
at `/mnt/snapshot` for the test harness to mount test devices at. If you would
like to run CrashMonkey by hand, you must run the `c_harness` binary and at
least provide the following:

* `-f` - block device to copy device queue flags from. This controls what flags (FUA, flush, etc) will be allowed to propagate to the device wrapper. Something like `/dev/vda` should work for this
* `-t` - file system type, right now CrashMonkey is only tested on ext4
* `-d` - device to run tests on. Currently the only valid option is `/dev/cow_ram0`. This flag should hopefully go away soon.

To run your own CrashMonkey, use: `./c_harness <flags> <user defined workload>`
from the `build` directory.

**Examples.** 
1. **Rename Workload**. You can run
`./c_harness -f /dev/vda -d /dev/cow_ram0 -t ext2 tests/rename_root_to_sub.so`
to run a test on an ext2 file system that tries to move a file between
directories. Once the test completes, open up the `<date_timestamp>-rename_root_to_sub.log`
file to see a printout of what tests failed and why.

2. **Creates and Deletes Workload**. To run a workload that creates, writes to and deletes files(default set to 10 files), on ext4 filesystem, use the following command:
`./c_harness -f /dev/vda -d /dev/cow_ram0 -t ext4 -e 10240 -l create -v tests/create_delete.so` This sets the size of the filesystem to 10MB (with a block size of 1024), and saves the snapshot to a log file named create. To load this snapshot and rerun the test, simply run:
`./c_harness -f /dev/vda -d /dev/cow_ram0 -t ext4 -e 10240 -r create -v tests/create_delete.so` This is useful in cases where you modify the check_test method in the workload to add additional checks for each crash state (in this example - crashmonkey/code/tests/create_delete.cpp). As long as the bio sequence during profiling does not change, it is safe to rerun the tests by loading the saved profile with -r option.

1. **Fdatasync Incorrect EOF Blocks Workload**. To run a workload that has an
incorrect number of data blocks after a file fallocate and fdatasync followed by
a computer crash (using the ext4 file system), use the following command:
`./c_harness -f /dev/vda -d /dev/cow_ram0 -t ext4 -e 10240 -v -P tests/eof_blocks_loss.so`
The `-P` flag tells CrashMonkey not to run tests which reorder the blocks in the
recorded workload. Therefore, for this test, CrashMonkey will only replay the
logged workload in order, stopping at each checkpoint in the log to run fsck and
any provided user data tests. By default, both tests that reorder the logged
workload and tests that just replay the workload in order are run.

A full listing of flags for CrashMonkey can be found in `code/harness/c_harness.c`

#### Running as a Background Process ####
There are currently no scripts or pre-defined `make` rules for running CrashMonkey as a background process. However, an example of how to run a simple CrashMonkey smoke test in background mode is shown below. **Before running either of these tests, you will have to create a directory at `/mnt/snapshot` for the test harness to mount test devices at.**

1. open 2 shells in you virtual machine and `cd` into the root directory of the repository
1. shell 1: `make`
1. shell 1: `cd build`
1. shell 2: `cd build`
1. shell 1: `sudo ./c_harness -f /dev/sda -t ext2 -d /dev/cow_ram0 -e 10240 -b tests/rename_root_to_sub.so`
    1. `-e` specifies the RAM block device size to use in KB
    1. `-f` specifies another block device to copy IO scheduler flags from
1. shell 2: `sudo mkdir /mnt/snapshot/test_dir`
1. shell 2: `sudo touch /mnt/snapshot/test_file`
1. shell 2: `sudo chmod 0777 /mnt/snapshot/test_file`
1. shell 2: `echo <some string>`
1. shell 2: `sudo user_tools/begin_log`
1. shell 2: `sudo mv /mnt/snapshot/test_file /mnt/snapshot/test_dir/test_file`
1. shell 2: `sudo user_tools/end_log`
1. shell 2: `sudo user_tools/begin_tests`

**Please note that in this test, unless you copy the text data from the `.cpp`
file the `rename_root_to_sub` test is built from, you will get many failures due
to incorrect file data.**
Again, a full list of flags for CrashMonkey can be found in
`code/harness/c_harness.c` and log output for the tests can be found in a file
named `<date_fimestamp-<test name>.log`

### XFSMonkey ###
XFSMonkey is a file-system crash consistency test suite that uses CrashMonkey. XFSMonkey reproduces the crash-consistency tests reported on [xfstests](https://github.com/kdave/xfstests) and linux-kernel mailing lists. It does not require setting up the xfstest suite. 

#### Running XFSMonkey ####
To run XFSMonkey, first get CrashMonkey up and working. XFSMonkey accepts the same parameters as CrashMonkey standalone tests. To get a list of all supported flags and their default setting, run `python xfsMonkey.py -h` in the root directory of CrashMonkey repository. You can kickstart the XFSMonkey test suite on `ext4` filesystem with default configurations (disk size of 200MB and 1000 iterations per test) by running `python xfsMonkey.py -t ext4`. The test summary can be found in a log file `<date_timestamp>-xfsMonkey.log`. In addition, each test case that was run has a detailed log file `<date_timestamp>-test_name.log`, that can be found in the `build` directory.

The complete list of bugs reproducible with XFSMonkey can be found [here](bugs.md)



### Contribution Guidelines ###

* Contributed code should follow [Google's C++ Style Guide](https://google.github.io/styleguide/cppguide.html) (the current code loosely follows that already).

#### Contributing User Defined Tests ####
* Contributed user defined tests can currently use any method to write to the file system under test. This can include using C/C++ to open/write to files or using the `system` call in C++ to call a shell function.
* All user defined tests must adhere to the interface defined in `code/tests/BaseTestCase.h` and must inherit from this class
* All user defined tests must include `test_case_get_instance()` and `test_case_delete_instance()` method implementations (see `code/tests/echo_sub_dir.cpp` for an example
    * In the future this will become a macro that is added at the end of the file
    * This is used by the test harness to create and destroy tests on the fly without recompiling the entire harness

#### Contributing User Defined Permuters ####
* All user defined permuters must adhere to the interface defined in `code/permuters/Permuter.h` and must inherit from this class
* All user defined tests must include `permuter_get_instance()` and `permuter_delete_instance()` method implementations (see `code/permuter/RandomPermuter.cpp` for an example
    * In the future this will become a macro that is added at the end of the file
    * This is used by the test harness to create and destroy permuters on the fly without recompiling the entire harness

### Useful Kernel Debugging Tool ###
If you run into system crashes etc. from a buggy CrashMonkey kernel module you may want to try using `stap` to help place print statements in arbitrary places in the kernel. Alternatively, you could put `printk`s in the kernel module itself.

### Future Improvements ###

* Use `gflags` to parse command line flags
    * I need to test if `gflags` can properly pickup and parse flags from dynamically loaded static objects
* Rework the following portions of the test harness
    * Make a class to manage disks, partitions, and formatting disks
    * Make a class to manage kernel modules
    * Make running test cases multi-threaded
    * Make the `disk_wrapper` work on volumes that span multiple block devices
    * Clean up the interface for generating crash states

### Repo Owner Contact Info ###
I can be reached at [ashmrtn@utexas.edu](mailto:ashmrtn@utexas.edu). Please don't spam this email and please begin your subject line with `CrashMonkey:` because I do filter my messages.
