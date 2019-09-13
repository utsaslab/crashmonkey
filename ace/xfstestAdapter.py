#!/usr/bin/env python

# To run : python xfstestAdapter.py -b base_xfstest.sh -t <jlang-file> \
#               -p <output_directory> -n <test_number>
#
# For example, with output_directory = "output" and test_number = "001",
# the converted test files will be located at:
#   - output/001
#   - output/001.out

import os
import re
import sys
import stat
import subprocess
import argparse
import time
import datetime
import itertools
from shutil import copyfile
from string import maketrans


# All functions that has options go here
FallocOptions = {
    '0'                                       : '$XFS_IO_PROG -f -c \"falloc {offset} {length}\" {filename}',
    'FALLOC_FL_KEEP_SIZE'                     : '$XFS_IO_PROG -f -c \"falloc -k {offset} {length}\" {filename}',
    'FALLOC_FL_ZERO_RANGE'                    : '$XFS_IO_PROG -f -c \"fzero {offset} {length}\" {filename}',
    'FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE': '$XFS_IO_PROG -f -c \"fzero -k {offset} {length}\" {filename}',
    'FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE': '$XFS_IO_PROG -f -c \"fpunch {offset} {length}\" {filename}'
}

FsyncOptions = ['fsync','fdatasync']

RemoveOptions = ['remove','unlink']

LinkOptions = ['link','symlink']

WriteOptions = ['WriteData','WriteDataMmap', 'pwrite']

# Constants
FILE_ATTR_KEY = "user.xattr1"
FILE_ATTR_VALUE = "val1"
WRITE_VALUE = "0x22"
MMAPWRITE_VALUE = "0x33"
DWRITE_VALUE = "0x44"

# Because the filenaming convention in the high-level j-lang language 
# seems to be arbitrary, we hard code the translation here to ensure
# that any inconsistency will raise a clear, easy-to-debug error.
jlang_files_to_xfs = {
    "test"  : "test"   ,
    "A"     : "A"      ,
    "AC"    : "A/C"    ,
    "B"     : "B"      ,
    "foo"   : "foo"    , 
    "bar"   : "bar"    ,
    "Afoo"  : "A/foo"  , 
    "Abar"  : "A/bar"  ,
    "Bfoo"  : "B/foo"  , 
    "Bbar"  : "B/bar"  ,
    "ACfoo" : "A/C/foo",
    "ACbar" : "A/C/bar"
}

used_files = set()

def translate_filename(filename):
    global used_files

    filename = "$SCRATCH_MNT/" + jlang_files_to_xfs[filename]

    used_files.add(filename)
    return filename

def build_parser():
    parser = argparse.ArgumentParser(description='Workload Generator for XFSMonkey v1.0')

    # global args
    parser.add_argument('--base_file', '-b', required=True, help='Base test file to generate workload')
    parser.add_argument('--test_file', '-t', required=True, help='J lang test skeleton to generate workload')

    # crash monkey args
    parser.add_argument('--target_path', '-p', required=True, help='Directory to save the generated test files')

    parser.add_argument('--test_number', '-n', required=True, help='The test number following xfstest convention. Will generate <test_number> and <test_number>.out')
    return parser


def print_setup(parsed_args):
    print '\n{: ^50s}'.format('XFSMonkey Workload generatorv0.1\n')
    print '='*20, 'Setup' , '='*20, '\n'
    print '{0:20}  {1}'.format('Base test file', parsed_args.base_file)
    print '{0:20}  {1}'.format('Test skeleton', parsed_args.test_file)
    print '{0:20}  {1}'.format('Target directory', parsed_args.target_path)
    print '{0:20}  {1}'.format('Test number', parsed_args.test_number)
    print '\n', '='*48, '\n'
    

def create_dir(dir_path):
    try: 
        os.makedirs(dir_path)
    except OSError:
        if not os.path.isdir(dir_path):
            raise
def translate_link(line, method):
    parts = line.split(" ")
    assert parts[0] == "link"
    assert len(parts) == 3, "Must have 2 args. Usage: link <src> <target>."

    src, target = parts[1:]
    src, target = translate_filename(src), translate_filename(target)
    return ["ln {} {}".format(src, target)]

def translate_symlink(line, method):
    parts = line.split(" ")
    assert parts[0] == "symlink"
    assert len(parts) == 3, "Must have 2 args. Usage: symlink <src> <target>."

    src, target = parts[1:]
    src, target = translate_filename(src), translate_filename(target)
    return ["ln -s {} {}".format(src, target)]

def translate_remove(line, method):
    parts = line.split(" ")
    assert parts[0] == "remove" or parts[0] == "unlink"
    assert len(parts) == 2, "Must have 1 arg. Usage: remove/unlink <filename>."

    filename = parts[1]
    filename = translate_filename(filename)

    return ["rm {}".format(filename)]

def translate_fsetxattr(line, method):
    parts = line.split(" ")
    assert parts[0] == "fsetxattr"
    assert len(parts) == 2, "Must have 1 arg. Usage: fsetxattr <filename>."

    filename = parts[1]
    filename = translate_filename(filename)

    return ["attr -s {} -V {} {} > /dev/null".format(FILE_ATTR_KEY,
        FILE_ATTR_VALUE, filename)]

def translate_removexattr(line, method):
    parts = line.split(" ")
    assert parts[0] == "removexattr"
    assert len(parts) == 2, "Must have 1 arg. Usage: removexattr <filename>."

    filename = parts[1]
    filename = translate_filename(filename)

    return ["attr -r {} {}".format(FILE_ATTR_KEY, filename)]

def translate_truncate(line, method):
    parts = line.split(" ")
    assert parts[0] == "truncate"
    assert len(parts) == 3, "Must have 2 args. Usage: truncate <filename> <new size>."

    filename, size = parts[1:]
    filename = translate_filename(filename)

    return ["truncate {} -s {}".format(filename, size)]

def translate_dwrite(line, method):
    parts = line.split(" ")
    assert parts[0] == "dwrite"
    assert len(parts) == 4, "Must have 3 args. Usage: dwrite <filename> <offset> <size>"

    filename, offset, size = parts[1:]
    filename = translate_filename(filename)

    # From common/rc in xfstest
    return ["_dwrite_byte {} {} {} {} \"\" > /dev/null".format(
        DWRITE_VALUE,
        offset,
        size,
        filename)]

def translate_write(line, method):
    parts = line.split(" ")
    assert parts[0] == "write"
    assert len(parts) == 4, "Must have 3 args. Usage: write <filename> <offset> <size>"

    filename, offset, size = parts[1:]
    filename = translate_filename(filename)

    # From common/rc in xfstest
    return ["_pwrite_byte {} {} {} {} \"\" > /dev/null".format(
        WRITE_VALUE,
        offset,
        size,
        filename)]

def translate_mmapwrite(line, method):
    parts = line.split(" ")
    assert parts[0] == "mmapwrite"
    assert len(parts) == 4, "Must have 3 args. Usage: mmapwrite <filename> <offset> <size>"

    filename, offset, size = parts[1:]
    filename = translate_filename(filename)

    mmap_offset = int(offset) + int(size)

    # From common/rc in xfstest
    # Calling mmapwrte to an offset that has not yet been 
    # allocated causes a BUS_IO error.
    return ["$XFS_IO_PROG -f -c \"falloc 0 {}\" {}".format(
                mmap_offset,
                filename),
            "_mwrite_byte_and_msync {} {} {} {} {}".format(
                MMAPWRITE_VALUE,
                offset,
                size,
                mmap_offset,
                filename),
            "check_consistency {}".format(filename)]

def translate_open(line, method):
    parts = line.split(" ")
    assert parts[0] == "open"
    assert len(parts) == 4, "Must have 3 args. Usage: open <filename> <flags> <mode>."

    # If O_CREAT is not passed, it's unclear what the 
    # 'mode' bits are for. Thus, we assume that O_CREAT
    # will be passed.
    filename, flags, mode = parts[1:]
    filename = translate_filename(filename)
    assert "O_CREAT" in flags, "flags must contain O_CREAT for open"

    return ["touch {}".format(filename), 
            "chmod {} {}".format(mode, filename)]

def translate_mkdir(line, method):
    parts = line.split(" ")
    assert parts[0] == "mkdir"
    assert len(parts) == 3, "Must have 2 args. Usage: mkdir <directory> <mode>."

    filename, umask = parts[1:]
    filename = translate_filename(filename)
    return ["mkdir {} -p -m {}".format(filename, umask)]

def translate_opendir(line, method):
    parts = line.split(" ")
    assert parts[0] == "opendir"
    assert len(parts) == 3, "Must have 2 args. Usage: opendir <directory> <mode>."

    filename, umask = parts[1:]
    filename = translate_filename(filename)
    return ["mkdir {} -p -m {}".format(filename, umask)]

def translate_fsync(line, method):
    parts = line.split(" ")
    assert parts[0] == "fsync"
    assert len(parts) == 2, "Must have 1 arg. Usage: fsync <file or directory>."

    filename = parts[1]
    filename = translate_filename(filename)
    return ["$XFS_IO_PROG -c \"fsync\" {}".format(filename),
            "check_consistency {}".format(filename)]

def translate_fdatasync(line, method):
    parts = line.split(" ")
    assert parts[0] == "fdatasync"
    assert len(parts) == 2, "Must have 1 arg. Usage: fdatasync <file or directory>."

    filename = parts[1]
    filename = translate_filename(filename)
    return ["$XFS_IO_PROG -c \"fdatasync\" {}".format(filename),
            "check_consistency --data-only {}".format(filename)]

def translate_sync(line, method):
    parts = line.split(" ")
    assert parts[0] == "sync"
    assert len(parts) == 1, "sync does not allow any arguments"

    return ["sync", "check_consistency " + " ".join(used_files)]


def translate_rename(line, method):
    parts = line.split(" ")
    assert parts[0] == "rename"
    assert len(parts) == 3, "Must have 2 args. Usage: rename <old name> <new name>."

    old, new = parts[1:]
    old, new = translate_filename(old), translate_filename(new)

    return ["mv {} {}".format(old, new)]

def translate_rmdir(line, method):
    parts = line.split(" ")
    assert parts[0] == "rmdir"
    assert len(parts) == 2, "Must have 1 arg. Usage: rmdir <directory>."

    directory = parts[1]
    directory = translate_filename(directory)

    return ["rm -rf {}".format(directory)]

def translate_falloc(line, method):
    parts = line.split(" ")
    assert parts[0] == "falloc"
    assert len(parts) == 5, "Must have 4 args. Usage: falloc <file> <mode> <offset> <length>."

    filename, mode, offset, length = parts[1:]
    filename = translate_filename(filename)

    return [FallocOptions[mode].format(filename=filename, offset=offset, length=length)]

# Translate a line of the high-level j-lang language
# into a list of lines of bash for xfstest
def translate_functions(line, method):
    # Note that order is important here. Commands that are
    # prefixes of each other, i.e. open and opendir, must
    # have the longer string appear first.

    if line.startswith("mkdir"):
        return translate_mkdir(line, method)
    elif line.startswith("opendir"):
        return translate_opendir(line, method)
    elif line.startswith("open"):
        return translate_open(line, method)
    elif line.startswith("fsync"):
        return translate_fsync(line, method)
    elif line.startswith("fdatasync"):
        return translate_fdatasync(line, method)
    elif line.startswith("sync"):
        return translate_sync(line, method)
    elif line.startswith("rename"):
        return translate_rename(line, method)
    elif line.startswith("falloc"):
        return translate_falloc(line, method)
    elif line.startswith("close"):
        return []
    elif line.startswith("rmdir"):
        return translate_rmdir(line, method)
    elif line.startswith("truncate"):
        return translate_truncate(line, method)
    elif line.startswith("checkpoint"):
        return []
    elif line.startswith("fsetxattr"):
        return translate_fsetxattr(line, method)
    elif line.startswith("removexattr"):
        return translate_removexattr(line, method)
    elif line.startswith("remove"):
        return translate_remove(line, method)
    elif line.startswith("unlink"):
        return translate_remove(line, method)
    elif line.startswith("link"):
        return translate_link(line, method)
    elif line.startswith("symlink"):
        return translate_symlink(line, method)
    elif line.startswith("dwrite"):
        return translate_dwrite(line, method)
    elif line.startswith("mmapwrite"):
        return translate_mmapwrite(line, method)
    elif line.startswith("write"):
        return translate_write(line, method)
    elif line.startswith("none"):
        return []
    else:
        raise ValueError("Unknown function", method, line)

def main():
    # Parse input args
    parsed_args = build_parser().parse_args()

    # Print the test setup - just for sanity
    #print_setup(parsed_args)
    
    # Check if test file exists
    if not os.path.exists(parsed_args.test_file) or not os.path.isfile(parsed_args.test_file):
        print parsed_args.test_file + ' : No such test file\n'
        exit(1)

    # Check that test number is a valid number/has no extension
    if not parsed_args.test_number.isdigit():
        print("'{}' is not a valid test number".format(parsed_args.test_number))
        exit(1)
    
    # Create the target directory
    create_dir(parsed_args.target_path)

    base_file = parsed_args.base_file
    test_file = parsed_args.test_file
    test_number = parsed_args.test_number
    generated_test = os.path.join(parsed_args.target_path, test_number)
    generated_test_output = os.path.join(parsed_args.target_path, test_number + ".out")
    
    # Template parameters and helper functions for populating the base file
    template_params = [("$CURRENT_YEAR", str(datetime.datetime.now().year)),
                       ("$TEST_NUMBER", test_number)]

    def filter_line(line):
        for param, value in template_params:
            if param in line:
                line = line.replace(param, value)
        return line

    # Find index to insert tests cases, this region is marked by
    # a line containing the comment '# Test cases'. Also, replace
    # template parameters.
    with open(base_file, 'r') as f:
        base_contents = f.readlines()
        base_contents = list(map(filter_line, base_contents))
        test_cases_index = base_contents.index("# Test cases\n") + 1
    f.close()

    copyfile(base_file, generated_test)

    # Iterate through test file and create list of lines to add
    lines_to_add = ["_mount_flakey"]
    with open(test_file, 'r') as f:
        for line in f:
            # Ignore newlines
            if line.split(' ')[0] == '\n':
                continue
            
            # Remove leading, trailing spaces
            line = line.strip()
            
            # If the line starts with #, it indicates which region of 
            # base file to populate so save it and skip this line
            if line.split(' ')[0] == '#' :
                method = line.strip().split()[-1]
                continue
            
            if method in ['declare', 'define']:
                continue

            elif method in ['setup', 'run']: 
                lines_to_add.extend(translate_functions(line, method))
                #insertFunctions(line, new_file, new_index_map, method)
    f.close()
    lines_to_add.append("clean_dir")

    # Write output files
    lines_to_add = list(map(lambda x: x + "\n", lines_to_add))
    output_contents = base_contents[:test_cases_index] + lines_to_add + base_contents[test_cases_index:]
    with open(generated_test, 'w+') as f:
        f.writelines(output_contents)

    out_lines = ["QA output created by {}\n".format(test_number), "Silence is golden\n"]
    with open(generated_test_output, 'w+') as f:
        f.writelines(out_lines)

if __name__ == '__main__':
	main()
