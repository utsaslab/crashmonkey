#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Copyright (c) $CURRENT_YEAR The University of Texas at Austin.  All Rights Reserved.
#
# FS QA Test $TEST_NUMBER
#
# Test case created by CrashMonkey
#
# Test if we create a hard link to a file and persist either of the files, all
# the names persist.
#
seq=`basename $0`
seqres=$RESULT_DIR/$seq
echo "QA output created by $seq"

here=`pwd`
tmp=/tmp/$$
status=1	# failure is the default!
trap "_cleanup; exit \$status" 0 1 2 3 15

_cleanup()
{
	_cleanup_flakey
	cd /
	rm -f $tmp.*
}

# get standard environment, filters and checks
. ./common/rc
. ./common/filter
. ./common/dmflakey

# 256MB in byte
fssize=$((2**20 * 256))

# remove previous $seqres.full before test
rm -f $seqres.full

# real QA test starts here
_supported_fs $FILESYSTEM_TYPE
_supported_os Linux
_require_scratch_nocheck
_require_dm_target flakey

# initialize scratch device
_scratch_mkfs_sized $fssize >> $seqres.full 2>&1
_require_metadata_journaling $SCRATCH_DEV
_init_flakey

stat_opt='-c  %n - blocks: %b size: %s inode: %i links: %h'

# Rename wraps 'mv' except that 'rename A/ B/' 
# would replace B, instead of creating A/B
rename() { 
    [ -d $1 ] && [ -d $2 ] && rm -rf $2
    mv $1 $2
}

# Usage: general_stat [--data-only] file1 [--data-only] [file2] ..
# If the --data-only flag precedes a file, then only that file's
# data will be printed.
general_stat() {
    data_only="false"
    while (( "$#" )); do
        case $1 in
            --data-only)
                data_only="true"
                ;;
            *)
                local file="$1"
                echo "-- $file --"
                if [ ! -f "$file" ] && [ ! -d "$file" ]; then
                    echo "Doesn't exist!"
                elif [ "$data_only" = "true" ] && [ -d "$file" ]; then
                    # Directory with --data-only
                    echo "Directory Data"
                    [[ -z "$(ls -A $file)" ]] || ls -1 "$file" | sort
                elif [ "$data_only" = "true" ]; then
                    # File with --data-only
                    echo "File Data"
                    od "$file"
                elif [ -d "$file" ]; then
                    # Directory with metadata and data
                    echo "Directory Metadata"
                    stat "$stat_opt" "$file"
                    echo "Directory Data"
                    [[ -z "$(ls -A $file)" ]] || ls -1 "$file" | sort
                else
                    # File with metadata and data
                    echo "File Metadata"
                    stat "$stat_opt" "$file"
                    echo "File Data"
                    od "$file"
                fi
                data_only="false"
                ;;
        esac
        shift
	done
}

# Open a file with O_DIRECT and write a byte into a range
_dwrite_byte() {
	local pattern="$1"
	local offset="$2"
	local len="$3"
	local file="$4"
	local xfs_io_args="$5"

	$XFS_IO_PROG $xfs_io_args -f -c "pwrite -S $pattern $offset $len" "$file"
}

_mwrite_byte_and_msync() {
	local pattern="$1"
	local offset="$2"
	local len="$3"
	local mmap_len="$4"
	local file="$5"

	$XFS_IO_PROG -f -c "mmap -rw 0 $mmap_len" -c "mwrite -S $pattern $offset $len" "$file" -c "msync -s"
}
check_consistency()
{
	before=$(general_stat "$@")
	_flakey_drop_and_remount
	after=$(general_stat "$@")

	if [ "$before" != "$after" ]; then
		echo -e "Before:\n$before"
		echo -e "After:\n$after"
	fi
}

# Using _scratch_mkfs instead of cleaning up the  working directory,
# adds about 10 seconds of delay in total for the 37 tests.
clean_dir()
{
	rm -rf $(find $SCRATCH_MNT/* | grep -v "lost+found")
	sync
	_unmount_flakey
}

ensure_file_size_one_block() {
    size=$(stat -c %s $1)
    [[ size -lt 4192 ]] && _pwrite_byte 0x22 0 32768 $1 > /dev/null
}

translate_range() {
    size=$(stat -c %s $1)
    case $2 in
        append)
            offset=$size
            length=$((offset + 32768))
        ;;
        overlap_unaligned_start)
            offset=0
            length=5000
        ;;
        overlap_unaligned_end)
            offset=$((size - 5000))
            length=5000
        ;;
        overlap_start)
            offset=0
            length=8192
        ;;
        overlap_end)
            offset=$((size - 8192))
            length=8192
        ;;
        overlap_extend)
            offset=$((size - 2000))
            length=5000
        ;;
    esac
    echo $offset $length
}

do_falloc() {
    local file="$1"
    local mode="$2"
    local range="$3"

    size=$(stat -c %s $file)
    read offset length <<< $(translate_range $file $range)

    case $mode in
        FALLOC_FL_ZERO_RANGE)
            $XFS_IO_PROG -f -c "fzero $offset $length" $file
        ;;
        FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE)
            $XFS_IO_PROG -f -c "fzero -k $offset $length" $file
        ;;
        FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE)
            $XFS_IO_PROG -f -c "fpunch $offset $length" $file
        ;;
        FALLOC_FL_KEEP_SIZE)
            $XFS_IO_PROG -f -c "falloc -k $offset $length" $file
        ;;
        0)
            $XFS_IO_PROG -f -c "falloc $offset $length" $file
        ;;
    esac
}

# Test cases


# Success, all done
echo "Silence is golden"
status=0
exit
