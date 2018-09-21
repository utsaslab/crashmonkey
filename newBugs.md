# New bugs reported by CrashMonkey and Ace

#### Summary:
	Total unique bugs reported = 10
		1. Fails on btrfs = 8
		2. Fails on f2fs  = 2


All the bugs were reported on kernel 4.16.0-041600-generic.

Workloads to trigger the following bugs can be found [here](https://github.com/utsaslab/crashmonkey/tree/master/code/tests).

___

### Bug 1 : File lost during rename ###

|Workload | Type| Fails on| Result
| :---:| :---: | :---: | :---: |
|mkdir A <br> touch A/bar <br> fsync A/bar <br>mkdir B <br> touch B/bar <br> rename B/bar A/bar <br> touch A/foo  <br> fsync A/foo <br> fsync A <br> ----CRASH----| seq-3 | btrfs|Rename(B/bar, A/bar) did not go through, but ended up deleting <br>the destination file A/bar as well as did not persist the source file B/bar. <br> [[Report](https://www.spinics.net/lists/linux-btrfs/msg77290.html)] [[Developer's response](https://www.spinics.net/lists/linux-btrfs/msg77318.html)]


**Output** :
```
automated check_test:
                failed: 1


DIFF: Content Mismatch /A

Actual (/mnt/snapshot/A):
---File Stat Atrributes---
Inode     : 257
TotalSize : 6
BlockSize : 4096
#Blocks   : 0
#HardLinks: 1


Expected (/mnt/cow_ram_snapshot4_0/A):
---File Stat Atrributes---
Inode     : 257
TotalSize : 12
BlockSize : 4096
#Blocks   : 0
#HardLinks: 1
```

### Bug 2 : File persisted in both directories after rename ###

|Workload | Type| Fails on| Result
| :---:| :---: | :---: | :---: |
|mkdir A <br/> mkdir A/C <br> rename A/C B <br> touch B/bar <br> fsync B/bar <br> rename B/bar A/bar <br> rename A B <br> fsync B/bar <br> ----CRASH----| seq-3 (nested) | btrfs| Rename(B/bar, A/bar) was not atomic and persists the file <br> in both source and destination directories. <br> [[Report](https://www.spinics.net/lists/linux-btrfs/msg77415.html)] [[Developer's response](https://www.spinics.net/lists/linux-btrfs/msg81425.html)]


**Output** :
```
automated check_test:
                failed: 1


DIFF: Content Mismatch /B/bar

Actual (/mnt/snapshot/B/bar):
---File Stat Atrributes---
Inode     : 259
TotalSize : 0
BlockSize : 4096
#Blocks   : 0
#HardLinks: 2


Expected (/mnt/cow_ram_snapshot3_0/B/bar):
---File Stat Atrributes---
Inode     : 259
TotalSize : 0
BlockSize : 4096
#Blocks   : 0
#HardLinks: 1
```

### Bug 3 : Directory entries not persisted on fsync ###

|Workload | Type| Fails on| Result
| :---:| :---: | :---: | :---: |
|mkdir A <br/> mkdir B <br> mkdir A/C <br> touch B/foo <br> fsync B/foo <br> link B/foo  A/C/foo <br> fsync A <br> ----CRASH----| seq-3 (nested) | btrfs| In spite of an explicit fsync on directory A,<br> directory A and all its contents go missing after a crash. <br> [[Report](https://www.spinics.net/lists/linux-btrfs/msg77415.html)] [[Developer's response](https://www.spinics.net/lists/linux-btrfs/msg81425.html)] [[Patch](https://www.mail-archive.com/linux-btrfs@vger.kernel.org/msg77875.html)]


**Output** :
```
automated check_test:
                failed: 1


DIFF: Failed stating the file /mnt/snapshot/A
```



### Bug 4 : Rename not persisted after fsync ###

|Workload | Type| Fails on| Result
| :---:| :---: | :---: | :---: |
|mkdir A <br/> sync <br> rename A B <br> touch B/foo <br> fsync B/foo <br> fsync B <br> ----CRASH----| seq-3 | btrfs| In spite of an explicit fsync on directory B,<br> and the file under it, directory B and its contents go missing after a crash. <br> [[Report](https://www.spinics.net/lists/linux-btrfs/msg77219.html)] [[Developer's response](https://www.spinics.net/lists/linux-btrfs/msg77282.html)]


**Output** :
```
automated check_test:
                failed: 1


DIFF: Failed stating the file /mnt/snapshot/B
```

### Bug 5 : Hard links not persisted after fsync ###

|Workload | Type| Fails on| Result
| :---:| :---: | :---: | :---: |
|mkdir A <br/> mkdir B <br> touch A/foo <br> link A/foo B/foo <br> fsync A/foo <br> fsync B/foo <br> ----CRASH----| seq-2 | btrfs| In spite of an explicit fsync on file B/foo,<br> the file (and the directory B) is not persisted. <br> [[Report](https://www.spinics.net/lists/linux-btrfs/msg77219.html)] [[Developer's response](https://www.spinics.net/lists/linux-btrfs/msg77282.html)]


**Output** :
```
automated check_test:
                failed: 1


DIFF: Failed stating the file /mnt/snapshot/B/foo

```

### Bug 6 : Fsync of directory not persisting its files ###

|Workload | Type| Fails on| Result
| :---:| :---: | :---: | :---: |
|mkdir test<br/> mkdir test/A <br> touch test/foo <br> touch test/A/foo<br> fsync test/A/foo <br> fsync test <br> ----CRASH----| seq-2 | btrfs| In spite of an explicit fsync on test directory,<br> the file foo present inside this directory goes missing. <br> [[Report](https://www.spinics.net/lists/linux-btrfs/msg77102.html)] [[Developer's response](https://www.spinics.net/lists/linux-btrfs/msg77929.html)]


**Output** :
```
automated check_test:
                failed: 1


DIFF: Content Mismatch

Actual (/mnt/snapshot):
---File Stat Atrributes---
Inode     : 256
TotalSize : 2
BlockSize : 4096
#Blocks   : 32
#HardLinks: 1


Expected (/mnt/cow_ram_snapshot3_0):
---File Stat Atrributes---
Inode     : 256
TotalSize : 8
BlockSize : 4096
#Blocks   : 32
#HardLinks: 1
```


### Bug 7 : Fsync of file does not persist all its names ###

|Workload | Type| Fails on| Result
| :---:| :---: | :---: | :---: |
|touch foo<br/> mkdir A <br> link foo A/bar <br> fsync foo <br> ----CRASH----| seq-1 | btrfs| Persisting file foo does not persist its hard<br> link A/bar. <br> [[Report](https://www.spinics.net/lists/fstests/msg09378.html)] [[Developer's response](https://www.spinics.net/lists/fstests/msg09379.html)]


**Output** :
```
automated check_test:
                failed: 1

DIFF: Content Mismatch /foo

Actual (/mnt/snapshot/foo):
---File Stat Atrributes---
Inode     : 258
TotalSize : 0
BlockSize : 4096
#Blocks   : 0
#HardLinks: 1


Expected (/mnt/cow_ram_snapshot2_0/foo):
---File Stat Atrributes---
Inode     : 258
TotalSize : 0
BlockSize : 4096
#Blocks   : 0
#HardLinks: 2
```


### Bug 8 : Fallocate beyond the eof loses allocated blocks even after fsync ###


|Workload | Type| Fails on| Result
| :---:| :---: | :---: | :---: |
|write foo 0-16K<br/> fsync foo <br> falloc -k foo 16-20K <br> fsync foo <br> ----CRASH----| seq-1 | btrfs| Whenever we do a fallocate or zero_range <br> operation that extends the file (with keep_size), <br>an fsync operation would not persist the block allocation. <br>On recovery, we lose all the blocks allocated beyond the eof. <br> [[Report](https://www.spinics.net/lists/linux-btrfs/msg75107.html)] [[Developer's response](https://www.spinics.net/lists/linux-btrfs/msg75108.html)][[Patch](https://www.spinics.net/lists/linux-btrfs/msg75108.html)]


**Output** :
```
automated check_test:
                failed: 1

DIFF: Content Mismatch /foo

Actual (/mnt/snapshot/foo):
---File Stat Atrributes---
Inode     : 257
TotalSize : 4096
BlockSize : 4096
#Blocks   : 8
#HardLinks: 1


Expected (/mnt/cow_ram_snapshot2_0/foo):
---File Stat Atrributes---
Inode     : 257
TotalSize : 4096
BlockSize : 4096
#Blocks   : 16
#HardLinks: 1
```



### Bug 9 : Fallocate beyond the eof recovers to an incorrect file size ###



|Workload | Type| Fails on| Result
| :---:| :---: | :---: | :---: |
|write foo 0-16K<br/> fsync foo <br> fzero -k foo 16-20K <br> fsync foo <br> ----CRASH----| seq-1 | F2FS| Whenever we do a zero_range operation <br> that extends the file (with keep_size), followed <br>by a fdatasync and then crash, the recovered file size would be incorrect. <br> [[Report](https://sourceforge.net/p/linux-f2fs/mailman/message/36236482/)] [[Developer's response](https://lore.kernel.org/patchwork/patch/889955/)][[Patch](https://lore.kernel.org/patchwork/patch/889955/)]


**Output** :
```
automated check_test:
                failed: 1

DIFF: Content Mismatch /foo

Actual (/mnt/snapshot/foo):
---File Stat Atrributes---
Inode     : 4
TotalSize : 8192
BlockSize : 4096
#Blocks   : 16
#HardLinks: 1


Expected (/mnt/cow_ram_snapshot2_0/foo):
---File Stat Atrributes---
Inode     : 4
TotalSize : 4096
BlockSize : 4096
#Blocks   : 16
#HardLinks: 1
```


### Bug 10 : Rename not persisted after fsync of a child file ###



|Workload | Type| Fails on| Result
| :---:| :---: | :---: | :---: |
|mkdir A<br/> sync <br> rename A B <br> touch B/foo <br> fsync B/foo <br> ----CRASH----| seq-1 | F2FS| In spite of an explicit fsync on B/foo, the file is persisted in a different directory. <br> [[Report](https://sourceforge.net/p/linux-f2fs/mailman/message/36302516/)] [[Developer's response](https://sourceforge.net/p/linux-f2fs/mailman/message/36302545/)][[Patch](https://lkml.org/lkml/2018/4/25/674)]


**Output** :
```
automated check_test:
                failed: 1

DIFF: Failed stating the file /mnt/snapshot/B/foo
```
