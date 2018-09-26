# CrashMonkey and Ace #

## CrashMonkey ##
CrashMonkey is a file-system agnostic record-replay-and-test framework. Unlike existing tools like dm-log-writes which require a manual checker script, CrashMonkey automatically tests for data and metadata consistency of persisted files. CrashMonkey needs only one input to run - the workload to be tested. We have described the rules for writing a workload for CrashMonkey [here](documentation/workload.md). More details about the internals of CrashMonkey and its setup can be found [here](documentation/CrashMonkey.md).


## Automatic Crash Explorer (Ace) ##
Ace is an automatic workload generator, that exhaustively generates sequences of file-system operations (workloads), given certain bounds. Ace consists of a workload synthesizer that generates workloads in a high-level language which we call J-lang. A CrashMonkey adapter that we built, converts these workloads into C++ test files that CrashMonkey can work with. More details on the current bounds imposed by Ace and guidelines on workload generation can be found [here](documentation/Ace.md).


### Bounded Black-Box Crash Testing with CrashMonkey and Ace ###
CrashMonkey combined with Ace, provides an automated testing framework for file system crash-consistency bugs. We call this, the Bounded Black-Box testing (B<sup>3</sup>). B<sup>3</sup> is a black-box testing approach which requires **no modification** to file-system code. B<sup>3</sup> exhaustively generates and tests workloads in a bounded space. The observations that enable an approach like B<sup>3</sup> is detailed in the OSDI'18 paper, *Finding Crash-Consistency Bugs with Bounded Black-Box Crash Testing* <br>
[[Paper PDF]()] [[Slides]()] [[Bibtex]()]  

CrashMonkey and Ace can be used out of the box on any Linux filesystem that implements POSIX API. Our tools have been tested to work with ext2, ext3, ext4, xfs, F2FS, and btrfs, across Linux kernel versions - 3.12, 3.13, 3.16, 4.1, 4.4, 4.15, and 4.16.


## Setup ##

Here is a checklist of dependencies to get CrashMonkey and Ace up and running on your system.
* You need a Linux machine to get started. We recommend spinning up a Ubuntu 14.04 or Ubuntu 16.04 VM with one of the supported kernel versions mentioned above. 20GB disk space and 2-4GB of RAM is recommended, if you plan on running large tests.
* Install dependencies.

  `apt-get install git make gcc g++ libattr1-dev btrfs-tools f2fs-tools xfsprogs libelf-dev python`

  `pip install progress progressbar`

*  Clone the repository.

    `git clone https://github.com/utsaslab/crashmonkey.git`

* Compile CrashMonkey and the kernel modules. Initial compile will take quite some time (2-5 minutes), as we have to compile a test suite of seq-1 workloads (workloads with 1 file-system operation).

  `cd crashmonkey; make`

* Create a directory for the test harness to mount devices to.

  `mkdir /mnt/snapshot`


## Push-button testing for seq-1 workloads ##

This repository contains a pre-generated suite of 328 seq-1 workloads (workloads with 1 file-system operation) [here](code/tests/seq1/). Once you have [set up](#setup) CrashMonkey on your machine (or VM), you can simply run :

```python
python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t btrfs -e 102400 -u build/tests/seq1/ > outfile
```

Sit back and relax. This is going to take about 35 minutes to complete if run on a single machine. This will run all the 328 tests of seq-1 on `btrfs` filesystem of size `100MB`. The bug reports can be found in the folder `diff_results`. The workloads are named j-lang<1-328>, and if any of these resulted in a bug, you will see a bug report with the same name as that of the workload, describing the difference between the expected and actual state.

## Tutorial ##
This tutorial walks you through the workflow of workload generation to testing, using a small bounded space of seq-1 workloads. Generating and running the tests in this tutorial will take less than 2 minutes.

1. **Select Bounds** :
Let us generate workloads of sequence length 1, and test only two file-system operations, `link` and `fallocate`. Our reduced file set consists of just two files.

    ```python
    FileOptions = ['foo']
    SecondFileOptions = ['A/bar']
    ```

    The link and fallocate system calls pick file arguments from the above list. Additionally fallocate allows several modes including `ZERO_RANGE`, `PUNCH_HOLE` etc. We pick one of the modes to bound the space here.

    ```python
    FallocOptions = ['FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE']
    ```

    Fallocate system call also requires offset and length parameters which are chosen to be one among the following.
    ```python
    WriteOptions = ['append', 'overlap_unaligned_start', 'overlap_extend']
    ```

    All these options are configurable in the [ace script](ace/ace.py).


2. **Generate Workloads** :
To generate workloads confining to the above mentioned bounds, run the following command in the `ace` directory  :
    ```python
    cd ace
    python ace.py -l 1 -n False -d True
    ```

    `-l` flag sets the length of the sequence to 1, `-n` is used to indicate if we want to include a nested directory to the file set and `-d` indicates the demo workload set, which appropriately sets the above mentioned bounds on system calls and their parameters.

    This generates about 9 workloads in about a second. You will find the generated workloads at `code/tests/seq1_demo`. Additionally, you can find the J-lang equivalent of these test files at `code/tests/seq1_demo/j-lang-files`

3. **Compile Workloads** : In order to compile the test workloads into `.so` files to be run by CrashMonkey

    1. Copy the generated test files into `generated_workloads` directory.
    ```
    cd ..
    cp code/tests/seq1_demo/j-lang*.cpp code/tests/generated_workloads/
    ```
    2. Compile the new tests.
    ```
    make gentests
    ```

    This will compile all the new tests and place the `.so` files at `build/tests/generated_workloads`

4. **Run** : Now its time to test all these workloads using CrashMonkey. Run the xfsMonkey script, which simply invokes CrashMonkey in a loop, testing one workload at a time.

    For example, let's run the generated tests on `btrfs` file system, on a `100MB` image.

    ```python
    python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t btrfs -e 102400 -u build/tests/generated_workloads/ > outfile
    ```

5. **Bug Reports** : The generated bug reports can be found at `diff_results`. If the test file "x" triggered a bug, you will find a bug report with the same name in this directory.

    For example, j-lang1.cpp will result in a crash-consistency bug on btrfs, as on kernel 4.16 ([Bug #7](newBugs.md)). The corresponding bug report will be as follows.

```    
automated check_test:
                failed: 1

DIFF: Content Mismatch /A/foo

Actual (/mnt/snapshot/A/foo):
---File Stat Atrributes---
Inode     : 258
TotalSize : 0
BlockSize : 4096
#Blocks   : 0
#HardLinks: 1


Expected (/mnt/cow_ram_snapshot2_0/A/foo):
---File Stat Atrributes---
Inode     : 258
TotalSize : 0
BlockSize : 4096
#Blocks   : 0
#HardLinks: 2
```


  Similarly, j-lang4.cpp results in the incorrect block count bug ([bug #8](newBugs.md)) on btrfs as of kernel 4.16. The corresponding bug report is as shown below.

```
automated check_test:
                failed: 1

DIFF: Content Mismatch /A/foo

Actual (/mnt/snapshot/A/foo):
---File Stat Atrributes---
Inode     : 257
TotalSize : 32768
BlockSize : 4096
#Blocks   : 64
#HardLinks: 1


Expected (/mnt/cow_ram_snapshot2_0/A/foo):
---File Stat Atrributes---
Inode     : 257
TotalSize : 32768
BlockSize : 4096
#Blocks   : 128
#HardLinks: 1
```


## Demo ##
All these steps have been assembled for you in the script [here](demo.sh). The link to the demo video is [here](). Try out the demo by running `./demo.sh btrfs`


## Research that uses our tools ##
1. *Barrier-Enabled IO Stack for Flash Storage*. Youjip Won, Hanyang University; Jaemin Jung, Texas A&M University; Gyeongyeol Choi, Joontaek Oh, and Seongbae Son, Hanyang University; Jooyoung Hwang and Sangyeun Cho, Samsung Electronics. Proceedings of the 16th USENIX Conference on File and Storage Technologies (FAST 18). [Link](https://www.usenix.org/conference/fast18/presentation/won)


## Contact Info ##
Please contact us at vijay@cs.utexas.edu with any questions. Drop us a note if you are using or plan to use CrashMonkey or Ace to test your file system.
