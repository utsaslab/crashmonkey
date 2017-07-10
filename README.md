# README #
### What is CrashMonkey? ###

CrashMonkey is a file-system agnostic testing framework for file-system consistency. It is meant to explore many crash states that are possible when a computer crashes in the middle of a write operation. CrashMonkey is made up of 3 main parts:

1. file-system agnostic kernel module for bio logging and disk snapshotting
1. user space test harness which coordinates everything
1. user space, user defined test cases which specify the workload to be tested and, optionally, data consistency tests to run on each generated crash state.

The HotStorage'17 paper *CrashMonkey: A Framework to Automatically Test File-System Crash Consistency* has a more detailed explanation of the internals of CrashMonkey. [Link]( http://www.cs.utexas.edu/~vijay/papers/hotstorage17-crashmonkey.pdf)

CrashMonkey also makes use of common Linux file system checker and repair programs like `fsck`.

### Getting Setup ###
The easiest (and recommended) way to start working on (or using) CrashMonkey is to setup a virtual machine and run everything in the VM. This is partly so that any bugs in the kernel module don't bring down your whole system and partly because I just find it easier. In the future I may try to get a Docker running with all the needed packages and files so that things are easy to setup and get running. In the meantime, I would recommend using `vmbuilder`, `libvert`, `qemu`, `kvm`, and the `vmbuilder` script in the repo to get everything setup (script generously provided by Ian Neal).

To get everything working:

1. Follow steps 1-3 [on this random website about setting up kvm on Ubuntu 16.04 LTS](https://www.cyberciti.biz/faq/installing-kvm-on-ubuntu-16-04-lts-server/)
1. `git clone` CrashMonkey repo into a directory of your choosing
1. edit `setup/ceate_vm.sh` to point to the directory you want the VM disk in, add any additional packages you may want, change user names
1. `setup/create_vm.sh <VM name> <VM IP>` to create a new VM and register it with `libvirt`
    1. Note that you may have to comment out line 153 in `/usr/lib/python2.7/dist-packages/VMBuilder/plugins/ubuntu/dapper.py` of `vmbuilder` python code in order to get it to run properly. Otherwise, it may have an issue with copying over sudo templates.
    1. Sit back and drink some coffee as this process may take a little while
1. Fire up the newly created VM and `ssh` into it
1. `git clone` CrashMonkey repo into a directory of your choosing
    1. sorry, I'll edit the `vmbuilder` script at some point to just copy these files over

### Compiling CrashMonkey ###
The test harness and test portion of CrashMonkey can be compiled with `cd code; make harness`. This will build the kernel module, test harness, and a single test case in the `code/tests` directory.

In the future the build system will be switched over to either `CMake` or `Bazel` (because everyone has heard of a uses `Bazel` right?).

### Compiling User Defined Tests ###
User defined tests reside in the `code/tests` directory. They can be compiled into static objects with `cd code; make user_tests`.

### Compiling Tests for CrashMonkey ###
Some tests for CrashMonkey reside in the `test` directory of the repo. Tests leverage `googletests` and are used to ensure the correctness and functionality of some of the user space portions of CrashMonkey (ex. the descendants of the `Permuter` class). Right now you'll have to examine the outputted binary names to determine what each binary tests. In the future, the build system will be updated to run the tests after compiling them.

### Running CrashMonkey ###
Pre-specified runs can be performed with `make run_no_log` or `make run_no_log_big NUM_TESTS=<number of crash states to test>`. If you would like to run CrashMonkey by hand, you must run the `c_harness` binary and at least provide the following:

* `-f` - block device to copy device queue flags from. This controls what flags (FUA, flush, etc) will be allowed to propagate to the device wrapper
* `-t` - file system type, right now CrashMonkey is only tested on ext4
* `-d` - device to run tests on. Currently the only valid option is `/dev/cow_ram0`. This flag should hopefully go away soon.

To run your own CrashMonkey, use: `./c_harness <flags> <user defined workload>`

### Contribution guidelines ###

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

* Rework scripts to setup the VM, install packages, etc
* Switch to `CMake` or `Bazel` instead of plain, poorly written `Makefiles`
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
