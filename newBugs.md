# New bugs reported by CrashMonkey

### Bug 1 : Acknowledged on btrfs ###
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

### Bug 2 : Acknowledged and patched on F2FS ###
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