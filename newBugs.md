# New bugs reported by CrashMonkey and Ace

#### Summary:
	Total unique bugs reported = 10
		1. Fails on btrfs = 8
		2. Fails on f2fs  = 2


All the bugs were reported on kernel 4.16.0-041600-generic.

Workloads to trigger the following bugs can be found [here](https://github.com/utsaslab/crashmonkey/tree/master/code/tests).

___

### Bug 1 : File lost during rename ###

|Workload (seq-3)| Type| Fails on| Result
| :---:| :---: | :---: | :---: |
|mkdir A <br/> touch A/bar <br /> fsync A/bar <br />mkdir B <br /> touch B/bar <br /> rename B/bar A/bar <br /> touch A/foo  <br /> fsync A/foo <br/> fsync A| seq-3 | btrfs|Rename(B/bar, A/bar) did not go through, but ended up deleting the destination file A/bar as well as did not persist the source file B/bar. <br /> [[Report](https://www.spinics.net/lists/linux-btrfs/msg77290.html)] [[Developer's response](https://www.spinics.net/lists/linux-btrfs/msg77318.html)]


**Output** :
```
automated check_test:
                failed: 1


DIFF: Content Mismatch /A

/mnt/snapshot/A:
---File Stat Atrributes---
Inode     : 257
TotalSize : 6
BlockSize : 4096
#Blocks   : 0
#HardLinks: 1


/mnt/cow_ram_snapshot4_0/A:
---File Stat Atrributes---
Inode     : 257
TotalSize : 12
BlockSize : 4096
#Blocks   : 0
#HardLinks: 1
```




### Bug 8 : Acknowledged on btrfs ###
Whenever we do a fallocate or zero_range operation that extends the file (with keep_size), an fsync operation would not persist the block allocation. On recovery, we lose all the blocks allocated beyond the eof.

**Result** : Fails on btrfs (kernel 4.16). [Recovered file has incorrect block count.](https://www.spinics.net/lists/linux-btrfs/msg75108.html)

**Output** :
```
Reordering tests ran 1000 tests with
	passed cleanly: 263
	passed fixed: 0
	fsck required: 0
	failed: 737
		old file persisted: 0
		file missing: 0
		file data corrupted: 0
		file metadata corrupted: 0
		incorrect block count: 737
		other: 0

```

### Bug 9 : Acknowledged and patched on F2FS ###
Whenever we do a zero_range operation that extends the file (with keep_size), followed by a fdatasync and then crash, the recovered file size would be incorrect.

**Result** : Fails on f2fs (kernel 4.15). [Recovered file has incorrect size.](https://patchwork.kernel.org/patch/10240977/)

**Output** :
```
Timing tests ran 1 tests with
	passed cleanly: 0
	passed fixed: 0
	fsck required: 0
	failed: 1
		old file persisted: 0
		file missing: 0
		file data corrupted: 0
		file metadata corrupted: 1
		incorrect block count: 0
		other: 0

```
