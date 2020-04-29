# Fuzzer

### Overview
The [ACE](Ace.md) framework also supports a fuzzer that can search the space around given J-lang workloads. Specifically, it takes high-level "J-lang" workloads as inputs and exhaustively searches the space of *single-operation modifications* around that workload. For example, given a workload with a write operation, the fuzzer will generate two additional test cases that swap the write operation and for dwrite and mmapwrite respectively. It will do this swap/generation for each operation in the test case.

This fuzzer is useful for searching around longer, n-length workloads, for which exhaustively searching the space of n-length workloads with ACE is not feasible. When given a longer workload that triggers a bug, or triggered a bug at some point in time, this fuzzer may be able to find similar bugs around it.

### Usage
The fuzzer can be found [here](../ace/fuzzer.py). Running `./fuzzer.py -h` will give detailed usage instrutions. The main arguments are shown below:

  *Flags:*

  * `-t FILE`, `--test_file FILE` - Use the given `FILE` as the starting seed for the fuzzer.
  * `-o DIR`, `--output_dir DIR` - Output files into `DIR`.

For each generated test, the fuzzer will create a high level, J-lang file and a Crashmonkey workload (using the Crashmonkey adapter [here](../code/cmAdapter.py)) in the output directory.
