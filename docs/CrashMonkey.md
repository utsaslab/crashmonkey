# CrashMonkey #
### Overview ###

CrashMonkey is a file-system agnostic testing framework for file-system crash consistency. It is meant to explore many crash states that are possible when a computer crashes in the middle of a file-system operation. CrashMonkey is made up of 3 main parts:

1. File-system agnostic kernel modules for bio logging and disk snapshotting
3. An auto-checker that tests if the recovered file-system state is consistent
4. A user space test harness which coordinates everything


The HotStorage'17 paper *CrashMonkey: A Framework to Automatically Test File-System Crash Consistency* has a more detailed explanation of the internals of CrashMonkey. <br>
[[Paper PDF]( http://www.cs.utexas.edu/~vijay/papers/hotstorage17-crashmonkey.pdf)] [[Slides](http://www.cs.utexas.edu/~vijay/papers/hotstorage17-crashmonkey-slides.pdf)] [[Bibtex](http://www.cs.utexas.edu/~vijay/bibtex/hotstorage17-crashmonkey.bib)]

CrashMonkey also makes use of common Linux file-system checker and repair programs like `fsck` in cases where the recovered file-system image is unmountable.

___
### Getting Setup ###

#### Setting Up a VM ####
The easiest (and recommended) way to start working on (or using) CrashMonkey is to setup a virtual machine and run everything in the VM. This is partly so that any bugs in the kernel module don't bring down your whole system and partly because it is easier. Take a look at our [VM setup guide](vmsetup.md) to get started.

 **CrashMonkey is known to work on kernel versions 3.13.0-121-generic, 4.4.0-62-generic, 4.9.0-040900rc8-generic, 4.15.0-041500-generic, and 4.16.0-041600rc7-generic.**

#### Dependencies and Build ####
Before you begin compilation, ensure you have satisfied all dependencies mentioned in the [setup guide](../README.md/#setup)

#### Compiling CrashMonkey ####
CrashMonkey can be built simply by running `make` in the root directory of the repository. This will build all the needed kernel modules, tests, and test harness components needed to run CrashMonkey. CrashMonkey is compiled to have a writeback delay of 2s by default. CrashMonkey waits for `writeback delay` amount of time, before unmounting the file system, to ensure pending writes (if any) are propagated to disk. This 2s delay works fine if you simulate crashes after persistence operations such as fsync(). However, if you want to test a workload by crashing at places other than persistence operations, then build using `CM=1 make`. This ensures that an appropriate [writeback delay](https://github.com/utsaslab/crashmonkey/blob/master/code/harness/FsSpecific.h) is set for each file system.

#### Compiling User Defined Tests ####
You can add user-defined tests in `code/tests` directory, conforming to the rules [here](workload.md). They can be compiled
into static objects with `make tests`.

#### Compiling Tests for CrashMonkey ####
Some tests for CrashMonkey reside in the `test` directory of the repo. Tests leverage `googletests` and are used to ensure the correctness and functionality of some of the user space portions of CrashMonkey (ex. the descendants of the `Permuter` class). Right now you'll have to examine the outputted binary names to determine what each binary tests. In the future, the build system will be updated to run the tests after compiling them.

___
### Running CrashMonkey ###

CrashMonkey can be run either as a standalone program or as a background program. When in standalone mode, CrashMonkey will automatically load and run the user defined C++ setup and workload methods. You can optionally provide user-defined consistency checks in this test file, or allow the auto-checker to validate persisted files in the workload. When run as a background process, the user is allowed to run setup and workload methods outside of CrashMonkey using a series of simple stub programs to communicate with CrashMonkey. In both modes of operation, command line flags have the same meaning.


#### Running as a Standalone Program ####
If you would
like to run CrashMonkey by hand, you must run the `c_harness` binary and at
least provide the following:

* `-f` - block device to copy device queue flags from. This controls what flags (FUA, flush, etc) will be allowed to propagate to the device wrapper. Something like `/dev/vda` or `/dev/sda` should work for this

* `-t` - file system type, right now CrashMonkey is tested on ext2, ext3, ext4, xfs, btrfs and F2FS.

* `-d` - device to run tests on. Currently the only valid option is `/dev/cow_ram0`. **don't be worried by the fact that you need the
`-d /dev/cow_ram0` flag even though that device doesn't exist.** It is a device presented by CrashMonkey's cow_brd kernel module.

Other useful flags that CrashMonkey supports are:

* `-e` - the size of file system image in KB. Some filesystem like btrfs and F2FS require a minimum of 100MB partition. So `102400` is a safe option that works across all tested file systems. Default is 10MB.

* `-P` - this flag ensures that the recorded block IOs are replayed in order. Skipping this flag allows CrashMonkey to permute block IOs within barrier operations. Optionally, if you skip the -P flag, you might want to include the `-s` flag to indicate how many permuted crash states you want to test. The default is to test 10K states.

* `-c` - This flag is required to enable automatic crash-consistency checking. If you don't pass this flag, then CrashMonkey relies on user-defined consistency checks in the test file.

A full listing of flags for CrashMonkey can be found in `code/harness/c_harness.c`
To run your own CrashMonkey, use the following commands:
```
cd build
./c_harness <flags> <user defined workload>
```

**Examples.**
1. **Rename Workload**. You can run
`./c_harness -f /dev/vda -d /dev/cow_ram0 -t ext2 tests/rename_root_to_sub.so`
to run a test on an ext2 file system that tries to move a file between
directories. Once the test completes, open up the `<date_timestamp>-rename_root_to_sub.log`
file to see a printout of what tests failed and why.

2. **Creates and Deletes Workload**. To run a workload that creates, writes to and deletes files(default set to 10 files), on the ext4 file system, use the following command:
`./c_harness -f /dev/vda -d /dev/cow_ram0 -t ext4 -e 10240 -l create -v tests/create_delete.so` This sets the size of the file system to 10MB (with a block size of 1024), and saves the snapshot to a log file named create. To load this snapshot and rerun the test, simply run:
`./c_harness -f /dev/vda -d /dev/cow_ram0 -t ext4 -e 10240 -r create -v tests/create_delete.so` This is useful in cases where you modify the check_test method in the workload to add additional checks for each crash state (in this example - crashmonkey/code/tests/create_delete.cpp). As long as the bio sequence during profiling does not change, it is safe to rerun the tests by loading the saved profile with -r option.

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

Log output for the tests can be found in a file
named `<date_fimestamp>-<test name>.log`



___
### Contribution Guidelines ###

* Contributed code should follow [Google's C++ Style Guide](https://google.github.io/styleguide/cppguide.html) (the current code loosely follows that already).

#### Contributing User Defined Tests ####
* Contributed user defined tests can currently use any method to write to the file system under test. This can include using C/C++ to open/write to files or using the `system` call in C++ to call a shell function.
* All user defined tests must adhere to the interface defined in `code/tests/BaseTestCase.h` and must inherit from this class
* All user defined tests must include `test_case_get_instance()` and `test_case_delete_instance()` method implementations (see `code/tests/echo_sub_dir.cpp` for an example
    * In the future this will become a macro that is added at the end of the file
    * This is used by the test harness to create and destroy tests on the fly without recompiling the entire harness

See a detailed example of CrashMonkey test case anatomy [here](workload.md).

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

### Contact Info ###
You can direct CrashMonkey related queries to  [ashmrtn@utexas.edu](mailto:ashmrtn@utexas.edu). Please don't spam this email and please begin your subject line with `CrashMonkey:` because I do filter my messages.
