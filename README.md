# CrashMonkey and Ace #


### CrashMonkey ###
CrashMonkey is a file-system agnostic record-replay-and-test framework. Unlike existing tools like dm-log-writes which require a manual checker script, CrashMonkey automatically tests for data and metadata consistency of persisted files. CrashMonkey needs only one input to run - the workload to be tested. We have described the rules for writing a workload for CrashMonkey [here](documentation/workload.md). More details about the internals of CrashMonkey and its setup can be found [here](documentation/CrashMonkey.md).

### Automatic Crash Explorer (Ace) ###
Ace is an automatic workload generator, that exhaustively generates sequences of file-system operations (workloads), given certain bounds. Ace consists of a workload synthesizer that generates workloads in a high-level language which we call J-lang. A CrashMonkey adapter that we built, converts these workloads into C++ test files that CrashMonkey can work with. More details on the current bounds imposed by Ace and guidelines on workload generation can be found [here](documentation/Ace.md).

CrashMonkey combined with Ace, provides an automated testing framework for file system crash-consistency bugs. We call this, the Bounded Black-Box testing (B<sup>3</sup>). B<sup>3</sup> is a black-box testing approach, meaning no file-system code is modified. B<sup>3</sup> exhaustively generates and tests workloads in a bounded space. The observations that enable an approach like B<sup>3</sup> is discussed in detail in the OSDI'18 paper *Finding Crash-Consistency Bugs with Bounded Black-Box Crash Testing* <br>
[[Paper PDF]()] [[Slides]()] [[Bibtex]()]  

CrashMonkey and Ace can be used out of the box on any Linux filesystem that implements POSIX API. Our tools have been tested to work with ext2, ext3, ext4, xfs, F2FS, and btrfs, across Linux kernel versions - 3.12, 3.13, 3.16, 4.1, 4.4, 4.15, and 4.16.


### Push-button testing for seq-1 workloads ###

This repository contains a pre-generated suite of seq-1 workloads (workloads with 1 file-system operation) [here](code/tests/seq1/). Once you have set up CrashMonkey on your machine (or VM), You can simply run :

```python
python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t btrfs -e 102400 -u build/tests/seq1/ > outfile
```

This will run all the 328 tests of seq-1 on `btrfs` filesystem of size `100MB`. The bug reports can be found in the folder `diff_results`. The workloads are named j-lang<1-328>, and if any of these resulted in a bug, you will see a bug report with the same name as that of the workload, describing the difference between the expected and actual state.

### Tutorial ###
This tutorial walks you through the workflow of workload generation to testing, using a small bounded space of seq-1 workloads.

1. **Select Bounds** :
Let us generate workloads of sequence length 1, and test only two file-system operations, `link` and `fallocate`. Our file set consists of 2 directories, with 2 files each.

```python
FileOptions = ['B/foo', 'A/foo']
SecondFileOptions = ['B/bar', 'A/bar']

DirOptions = ['A']
TestDirOptions = ['test']
SecondDirOptions = ['B']
```

The link and fallocate system calls pick file arguments from the above list. Additionally fallocate allows several modes including `ZERO_RANGE`, `PUNCH_HOLE` etc. We pick two modes to bound the space here.

```python
FallocOptions = ['FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE','FALLOC_FL_KEEP_SIZE']
```

Fallocate system call also requires offset and length parameters which are chosen to be one among the following.
```python
WriteOptions = ['append', 'overlap_unaligned_start', 'overlap_extend']
```

All these options are configurable in the [ace script](ace/ace.py).


2. **Generate Workloads** :
To generate workloads confining to the above mentioned bounds, run the following command in the `ace` directory  :
```python
python ace.py -l 1 -n False -d True
```

`-l` flag sets the length of the sequence to 1, `-n` is used to indicate if we want to include a nested directory to the file set and `-d` indicates the demo workload set, which appropriately sets the above mentioned bounds on system calls and their parameters.

You fill find the generated workloads at `code/tests/seq1_demo`. Additionally, you can find the J-lang equivalent of these test files at `code/tests/seq1_demo/j-lang-files`

3. **Compile Workloads** : In order to compile the test workloads into `.so` files to be run by CrashMonkey,
  1. Copy the generated test files into `test` directory.
  ```
  cp code/tests/seq1_demo/j-lang*.cpp code/tests/
  ```
  2. Go to the root directory and type
  ```
  make tests
  ```

  This will compile all the new tests and place the `.so` files at `build/tests`

5. **Run** : Now to test all these workloads using CrashMonkey, run the xfsMonkey script, which simply runs CrashMonkey in a loop testing one workload at a time.

For example, let's run the generated tests on `btrfs` file system, on a `100MB` image.

```python
python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t btrfs -e 102400 -u build/tests/ > outfile
```

6. **Bug Reports** : The generated bug reports can be found at `diff_results`. If the test file j-lang<n>.cpp triggered a bug, you will find a bug report with the same name in this directory.

For example, j-lang--.cpp will result in a crash-consistency bug on btrfs, on kernel 4.16. The corresponding bug report will be as follows.

```

```

### Research that uses our tools ###
1. *Barrier-Enabled IO Stack for Flash Storage*. Youjip Won, Hanyang University; Jaemin Jung, Texas A&M University; Gyeongyeol Choi, Joontaek Oh, and Seongbae Son, Hanyang University; Jooyoung Hwang and Sangyeun Cho, Samsung Electronics. Proceedings of the 16th USENIX Conference on File and Storage Technologies (FAST 18). [Link](https://www.usenix.org/conference/fast18/presentation/won)

### Contact Info ###
Please contact us at vijay@cs.utexas.edu with any questions. Drop us a note if you are using or plan to use CrashMonkey or Ace to test your file system.
