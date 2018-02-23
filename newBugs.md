# New bugs reported by CrashMonkey

### Bug 1 ###
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
