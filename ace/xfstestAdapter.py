#!/usr/bin/env python

# To run : python xfstestAdapter.py -b ../code/tests/ace-base/base_xfstest.sh -t <jlang-file> \
#               -p <output_directory> -n <test_number> -f <filesystem type>
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
# The format is:
# <jlang filename> | <bash filename> | <parent jlang filename> | <file type>
JLANG_FILES = [
    ( ""      , ""        , ""  , "dir"  ),
    ( "test"  , "test"    , ""  , "file" ), 
    ( "A"     , "A"       , ""  , "dir"  ), 
    ( "AC"    , "A/C"     , "A" , "dir"  ), 
    ( "B"     , "B"       , ""  , "dir"  ), 
    ( "foo"   , "foo"     , ""  , "file" ), 
    ( "bar"   , "bar"     , ""  , "file" ), 
    ( "Afoo"  , "A/foo"   , "A" , "file" ), 
    ( "Abar"  , "A/bar"   , "A" , "file" ), 
    ( "Bfoo"  , "B/foo"   , "B" , "file" ), 
    ( "Bbar"  , "B/bar"   , "B" , "file" ), 
    ( "ACfoo" , "A/C/foo" , "AC", "file" ), 
    ( "ACbar" , "A/C/bar" , "AC", "file" )
]

prefix = "$SCRATCH_MNT/"

def get_row_from_filename(filename):
    # Strip prefix if present
    if filename.startswith(prefix):
        filename = filename[len(prefix):]

    for row in JLANG_FILES:
        if filename == row[0] or filename == row[1]:
            return row
    return None

def is_file_or_dir(name):
    return get_row_from_filename(name) != None


def translate_filename(filename):
    translated_filename = get_row_from_filename(filename)[1]
    return prefix + translated_filename

def parent(filename):
    jlang_parent = get_row_from_filename(filename)[2]
    return translate_filename(jlang_parent)

def is_dir(filename):
    get_row_from_filename(filename)[3] == "dir"

def find_basefile(version):
    cwd = os.path.dirname(os.path.realpath(__file__))
    fname = "base_xfstest.sh" if version == 1 else "base_xfstest_concise.sh"
    possible_locs = ["../code/tests/ace-base",
                     "."]

    for loc in possible_locs:
        f = os.path.join(cwd, os.path.join(loc, fname))
        if os.path.exists(f):
            return f
            print("Found base file : '{}'".format(f))
    print("Cannot find {} in any of {}".format(fname, possible_locs))
    sys.exit(1)

def build_parser():
    parser = argparse.ArgumentParser(description='Workload Generator for XFSMonkey v1.0')

    # global args
    parser.add_argument('--test_file', '-t', required=True, help='J lang test skeleton to generate workload')

    # crash monkey args
    parser.add_argument('--target_path', '-p', required=True, help='Directory to save the generated test files')

    parser.add_argument('--test_number', '-n', required=True, help='The test number following xfstest convention. Will generate <test_number> and <test_number>.out')
    parser.add_argument('--filesystem_type', '-f', default="generic", help='The filesystem type for the test (i.e. generic, ext4, btrfs, xfs, f2fs, etc.)')
    return parser


def print_setup(parsed_args):
    print '\n{: ^50s}'.format('XFSMonkey Workload generatorv0.1\n')
    print '='*20, 'Setup' , '='*20, '\n'
    print '{0:20}  {1}'.format('Test skeleton', parsed_args.test_file)
    print '{0:20}  {1}'.format('Target directory', parsed_args.target_path)
    print '{0:20}  {1}'.format('Test number', parsed_args.test_number)
    print '{0:20}  {1}'.format('Filesystem type', parsed_args.filesystem_type)
    print '\n', '='*48, '\n'
    

def create_dir(dir_path):
    try: 
        os.makedirs(dir_path)
    except OSError:
        if not os.path.isdir(dir_path):
            raise


class State():
    """
    State contains two elements:
    - opened_files is a set containing the currently modified
      files that should be written back to disk upon a call
      to 'sync'
    - data_synced_files is a set that contains the files are 
      currently synced with fdatasync
    - synced_files is a strict subset of data_synced_files 
      containing the files that should currently have their 
      metadata synced and therefore be consistent through a 
      crash
    """

    def __init__(self):
        self.opened_files = set()
        self.synced_files = set()
        self.data_synced_files = set()

    def _open_file(self, filename):
        self.opened_files.add(filename)

    def _unopen_file(self, filename):
        if filename in self.opened_files:
            self.opened_files.remove(filename)

    def _sync_file(self, filename):
        self.synced_files.add(filename)
        self.data_synced_files.add(filename)

    def _sync_file_data(self, filename):
        self.data_synced_files.add(filename)

    def _unsync_file(self, filename):
        if filename in self.synced_files:
            self.synced_files.remove(filename)

        if filename in self.data_synced_files:
            self.data_synced_files.remove(filename)

    def _unsync_file_metadata(self, filename):
        if filename in self.synced_files:
            self.synced_files.remove(filename)

    def _check_internal(self):
        assert all([f in self.data_synced_files for f in self.synced_files])
        assert all([f in self.opened_files for f in self.data_synced_files])

    def modify_file(self, filename):
        self._open_file(filename)
        self._unsync_file(filename)
        self._check_internal()

    def modify_file_metadata(self, filename):
        self._open_file(filename)
        self._unsync_file_metadata(filename)
        self._check_internal()

    def remove_file(self, filename):
        parent_dir = parent(filename)

        self._unopen_file(filename)
        self._unsync_file(filename)

        self._open_file(parent_dir)
        self._unsync_file(parent_dir)
        self._check_internal()

    def add_file(self, filename):
        parent_dir = parent(filename)

        self._open_file(filename)
        self._unsync_file(filename)

        self._open_file(parent_dir)
        self._unsync_file(parent_dir)
        self._check_internal()

    def rename_file(self, old, new):
        self.remove_file(old)
        self.add_file(new)

        # Special case for directory renames: 
        # If we rename A -> B, we must rename Afoo -> Bfoo
        jlang_old, jlang_new = get_row_from_filename(old)[0], get_row_from_filename(new)[0]

        dirs = ["A", "B", "AC"]
        files = ["foo", "bar"]

        if jlang_old in dirs and jlang_new in dirs:
            for child in files:
                old_child = translate_filename(jlang_old + child)
                new_child = translate_filename(jlang_new + child)

                if old_child in self.opened_files:
                    self._unopen_file(old_child) # special case where we don't want to open parent directory
                    self._unsync_file(old_child)
                    self.add_file(new_child)

    def sync_file(self, filename):
        self._sync_file(filename)
        self._check_internal()

    def sync_file_data(self, filename):
        self._sync_file_data(filename)
        self._check_internal()

    def sync_all_files(self):
        self.synced_files = self.synced_files.union(self.opened_files)
        self.data_synced_files = self.data_synced_files.union(self.opened_files)
        self._check_internal()

    def get_check_consistency_command(self):
        command = "check_consistency"

        for f in self.synced_files:
            command += " " + f

        for f in [f for f in self.data_synced_files if f not in self.synced_files]:
            command += " --data-only " + f
        return command


## Functions for translating jlang instructions into bash

def translate_link(line, state):
    parts = line.split()
    assert parts[0] == "link"
    assert len(parts) == 3, "Must have 2 args. Usage: link <src> <target>."

    src, target = parts[1:]
    src, target = translate_filename(src), translate_filename(target)

    state.modify_file_metadata(src) 
    state.add_file(target)

    return ["ln {} {}".format(src, target)], state

def translate_symlink(line, state):
    parts = line.split()
    assert parts[0] == "symlink"
    assert len(parts) == 3, "Must have 2 args. Usage: symlink <src> <target>."

    src, target = parts[1:]
    src, target = translate_filename(src), translate_filename(target)

    state.add_file(target)

    return ["ln -s {} {}".format(src, target)], state

def translate_remove(line, state):
    parts = line.split()
    assert parts[0] == "remove" or parts[0] == "unlink"
    assert len(parts) == 2, "Must have 1 arg. Usage: remove/unlink <filename>."

    filename = parts[1]
    filename = translate_filename(filename)

    state.remove_file(filename)

    return ["rm {}".format(filename)], state

def translate_fsetxattr(line, state):
    parts = line.split()
    assert parts[0] == "fsetxattr"
    assert len(parts) == 2, "Must have 1 arg. Usage: fsetxattr <filename>."

    filename = parts[1]
    filename = translate_filename(filename)

    state.modify_file(filename)

    return ["attr -s {} -V {} {} > /dev/null".format(FILE_ATTR_KEY,
        FILE_ATTR_VALUE, filename)], state

def translate_removexattr(line, state):
    parts = line.split()
    assert parts[0] == "removexattr"
    assert len(parts) == 2, "Must have 1 arg. Usage: removexattr <filename>."

    filename = parts[1]
    filename = translate_filename(filename)

    state.modify_file(filename)

    return ["attr -r {} {}".format(FILE_ATTR_KEY, filename)], state

def translate_truncate(line, state):
    parts = line.split()
    assert parts[0] == "truncate"
    assert len(parts) == 3, "Must have 2 args. Usage: truncate <filename> <new size>."

    filename, size = parts[1:]
    filename = translate_filename(filename)

    state.modify_file(filename)

    return ["truncate {} -s {}".format(filename, size)], state

def translate_dwrite(line, state):
    parts = line.split()
    assert parts[0] == "dwrite"
    assert len(parts) == 4, "Must have 3 args. Usage: dwrite <filename> <offset> <size>"

    filename, offset, size = parts[1:]
    filename = translate_filename(filename)

    state.modify_file(filename)

    # From common/rc in xfstest
    return ["_dwrite_byte {} {} {} {} \"\" > /dev/null".format(
        DWRITE_VALUE,
        offset,
        size,
        filename)], state

def translate_write(line, state):
    parts = line.split()
    assert parts[0] == "write"
    assert len(parts) == 4, "Must have 3 args. Usage: write <filename> <offset> <size>"

    filename, offset, size = parts[1:]
    filename = translate_filename(filename)

    state.modify_file(filename)

    # From common/rc in xfstest
    return ["_pwrite_byte {} {} {} {} \"\" > /dev/null".format(
        WRITE_VALUE,
        offset,
        size,
        filename)], state

def translate_mmapwrite(line, state):
    parts = line.split()
    assert parts[0] == "mmapwrite"
    assert len(parts) == 4, "Must have 3 args. Usage: mmapwrite <filename> <offset> <size>"

    filename, offset, size = parts[1:]
    filename = translate_filename(filename)

    state.modify_file(filename)
    state.sync_file(filename)

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
                filename)], state

def translate_open(line, state):
    parts = line.split()
    assert parts[0] == "open"
    assert len(parts) == 4, "Must have 3 args. Usage: open <filename> <flags> <mode>."

    # If O_CREAT is not passed, it's unclear what the 
    # 'mode' bits are for. Thus, we assume that O_CREAT
    # will be passed.
    filename, flags, mode = parts[1:]
    filename = translate_filename(filename)
    assert "O_CREAT" in flags, "flags must contain O_CREAT for open"

    state.add_file(filename)

    return ["touch {}".format(filename), 
            "chmod {} {}".format(mode, filename)], state

def translate_mkdir(line, state):
    parts = line.split()
    assert parts[0] == "mkdir" or parts[0] == "opendir"
    assert len(parts) == 3, "Must have 2 args. Usage: mkdir/opendir <directory> <mode>."

    filename, umask = parts[1:]
    filename = translate_filename(filename)

    state.add_file(filename)

    return ["mkdir {} -p -m {}".format(filename, umask)], state

def translate_fsync(line, state):
    parts = line.split()
    assert parts[0] == "fsync"
    assert len(parts) == 2, "Must have 1 arg. Usage: fsync <file or directory>."

    filename = parts[1]
    filename = translate_filename(filename)

    state.sync_file(filename)

    return ["$XFS_IO_PROG -c \"fsync\" {}".format(filename)], state

def translate_fdatasync(line, state):
    parts = line.split()
    assert parts[0] == "fdatasync"
    assert len(parts) == 2, "Must have 1 arg. Usage: fdatasync <file or directory>."

    filename = parts[1]
    filename = translate_filename(filename)

    state.sync_file_data(filename)

    return ["$XFS_IO_PROG -c \"fdatasync\" {}".format(filename)], state

def translate_sync(line, state):
    parts = line.split()
    assert parts[0] == "sync"
    assert len(parts) == 1, "sync does not allow any arguments"

    state.sync_all_files()

    return ["sync"], state


def translate_rename(line, state):
    parts = line.split()
    assert parts[0] == "rename"
    assert len(parts) == 3, "Must have 2 args. Usage: rename <old name> <new name>."

    old, new = parts[1:]
    old, new = translate_filename(old), translate_filename(new)

    state.rename_file(old, new)

    return ["rename {} {}".format(old, new)], state

def translate_rmdir(line, state):
    parts = line.split()
    assert parts[0] == "rmdir"
    assert len(parts) == 2, "Must have 1 arg. Usage: rmdir <directory>."

    directory = parts[1]
    directory = translate_filename(directory)

    state.remove_file(directory)

    return ["rm -rf {}".format(directory)], state

def translate_falloc(line, state):
    parts = line.split()
    assert parts[0] == "falloc"
    assert len(parts) == 5, "Must have 4 args. Usage: falloc <file> <mode> <offset> <length>."

    filename, mode, offset, length = parts[1:]
    filename = translate_filename(filename)

    state.modify_file(filename)

    return [FallocOptions[mode].format(filename=filename, offset=offset, length=length)], state

# Translate a line of the high-level j-lang language
# into a list of lines of bash for xfstest
def translate_functions(line, state):
    # Note that order is important here. Commands that are
    # prefixes of each other, i.e. open and opendir, must
    # have the longer string appear first.

    if line.startswith("mkdir"):
        res = translate_mkdir(line, state)
    elif line.startswith("opendir"):
        res = translate_mkdir(line, state)
    elif line.startswith("open"):
        res = translate_open(line, state)
    elif line.startswith("fsync"):
        res = translate_fsync(line, state)
    elif line.startswith("fdatasync"):
        res = translate_fdatasync(line, state)
    elif line.startswith("sync"):
        res = translate_sync(line, state)
    elif line.startswith("rename"):
        res = translate_rename(line, state)
    elif line.startswith("falloc"):
        res = translate_falloc(line, state)
    elif line.startswith("close"):
        return [], state
    elif line.startswith("rmdir"):
        res = translate_rmdir(line, state)
    elif line.startswith("truncate"):
        res = translate_truncate(line, state)
    elif line.startswith("checkpoint"):
        return [], state
    elif line.startswith("fsetxattr"):
        res = translate_fsetxattr(line, state)
    elif line.startswith("removexattr"):
        res = translate_removexattr(line, state)
    elif line.startswith("remove"):
        res = translate_remove(line, state)
    elif line.startswith("unlink"):
        res = translate_remove(line, state)
    elif line.startswith("link"):
        res = translate_link(line, state)
    elif line.startswith("symlink"):
        res = translate_symlink(line, state)
    elif line.startswith("dwrite"):
        res = translate_dwrite(line, state)
    elif line.startswith("mmapwrite"):
        res = translate_mmapwrite(line, state)
    elif line.startswith("write"):
        res = translate_write(line, state)
    elif line.startswith("none"):
        return [], state
    else:
        raise ValueError("Unknown function", line)

    # Unpack tuple
    line, state = res
    return line, state

# Creates a test file for J-lang V1 files.
def build_test_v1(parsed_args):
    base_file = find_basefile(1)
    test_file = parsed_args.test_file
    test_number = parsed_args.test_number
    filesystem_type = parsed_args.filesystem_type
    generated_test = os.path.join(parsed_args.target_path, test_number)
    generated_test_output = os.path.join(parsed_args.target_path, test_number + ".out")
    
    # Template parameters and helper functions for populating the base file
    template_params = [("$CURRENT_YEAR", str(datetime.datetime.now().year)),
                       ("$TEST_NUMBER", test_number),
                       ("$FILESYSTEM_TYPE", filesystem_type)]

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

    copyfile(base_file, generated_test)

    # Iterate through test file and get lines to translate.
    lines_to_translate = []
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
            elif method in ['setup', 'run']: 
                lines_to_translate.append(line)

    # Translate lines into xfstest format.
    lines_to_add = ["_mount_flakey"]
    state = State()
    for line in lines_to_translate:
        new_line, state = translate_functions(line, state)
        lines_to_add.extend(new_line)
    lines_to_add.append(state.get_check_consistency_command())
    lines_to_add.append("clean_dir")

    # Insert lines into relevant location and write output files.
    lines_to_add = list(map(lambda x: x + "\n", lines_to_add))
    output_contents = base_contents[:test_cases_index] + lines_to_add + base_contents[test_cases_index:]
    with open(generated_test, 'w+') as f:
        f.writelines(output_contents)

    out_lines = ["QA output created by {}\n".format(test_number), "Silence is golden\n"]
    with open(generated_test_output, 'w+') as f:
        f.writelines(out_lines)

# Given a list of commands (i.e. "fsync ...", "dwrite ..."), create a function
# name (i.e. 'fsync_dwrite_template').
def function_name_from_commands(commands):
    functions = [c.split(" ")[0] for c in commands]
    return "{}_template".format("_".join(functions))

def build_template_function(commands, commands_with_deps, args, fname, do_fsync):
    lines = ["function {}() {{".format(fname)]
    for i, a in enumerate(args):
        lines.append("\tlocal {}=\"${}\"".format(a[0], i+1))


    if do_fsync:
        lines.append("\tlocal fsync_file=\"${}\"".format(len(args)+1))
        lines.append("\tlocal fsync_command=\"${}\"".format(len(args)+2))

    # TODO: generalize this
    # Hack for num_ops = 1, ignore link operations
    if commands[0].split(" ")[0] in ["link", "rename"]:
        lines.append("\t[[ $file1 == $file2 ]] && return")

    lines.append("\n\t_mount_flakey")

    for c in commands_with_deps:
        lines.append("\t{}".format(c))

    lines.append("\tclean_dir")
    lines.append("}\n")

    return lines

def build_for_loops(args, fname, do_fsync):
    lines, num_tabs = [], 0

    for a in args:
        lines.append('\t' * num_tabs + "for {0} in ${{{0}_options[@]}}; do".format(a[0]))
        num_tabs += 1

    fsync_files = ["\"${0}\" \"$(dirname ${0})\"".format(a[0]) for a in args if "file" in a[0]]

    # For fsync files
    lines.append('\t' * num_tabs + "fsync_files=({})".format(" ".join(fsync_files)))
    lines.append('\t' * num_tabs + "uniques=($(for v in ${fsync_files[@]}; do echo $v; done | sort -u))")
    lines.append('\t' * num_tabs + "for fsync_file in ${uniques[@]}; do")
    num_tabs += 1

    if do_fsync:
        lines.append('\t' * num_tabs + "for fsync_command in fsync fdatasync; do")
        num_tabs += 1

    function_call = fname + " " + " ".join(list(map(lambda a: "$" + a[0], args)))

    if do_fsync:
        function_call += " $fsync_file $fsync_command"

    lines.append('\t' * num_tabs + function_call)

    while num_tabs != 0:
        num_tabs -= 1
        lines.append('\t' * num_tabs + 'done')
    return lines


def build_command_dependencies(commands):
    lines = []

    create_parent_dir = lambda f : "mkdir -p $(dirname {})".format(f)
    create_file = lambda f : "touch " + f
    ensure_file_size = lambda f : "ensure_file_size_one_block " + f
    remove_if_exists = lambda f : "rm -rf " + f

    for c in commands:
        parts = c.split(" ")

        if parts[0] == "open":
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append("chmod {} {}".format(parts[3], parts[1]))
        elif parts[0] == "mkdir":
            lines.append("mkdir {} -p -m {}".format(parts[1], parts[2]))
        elif parts[0] == "falloc":
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append(ensure_file_size(parts[1]))
            lines.append("do_falloc {} {} {}".format(parts[1], parts[2], parts[3]))
        elif parts[0] == "write":
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append("[[ {} != 'append' ]] && ensure_file_size_one_block {}".format(
                parts[2], 
                parts[1]))
            lines.append("_pwrite_byte {} $(translate_range {} {}) {} > /dev/null".format(
                WRITE_VALUE,
                parts[1],
                parts[2],
                parts[1]))
        elif parts[0] == "dwrite":
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append("[[ {} != 'append' ]] && ensure_file_size_one_block {}".format(
                parts[2], 
                parts[1]))
            lines.append("_dwrite_byte {} $(translate_range {} {}) {} > /dev/null".format(
                DWRITE_VALUE,
                parts[1],
                parts[2],
                parts[1]))
        elif parts[0] == "mmapwrite":
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append("read offset size <<< $(translate_range {} {})".format(
                parts[1],
                parts[2]))
            lines.append("mmap_offset=$((offset + size))")
            lines.append("$XFS_IO_PROG -f -c \"falloc 0 $mmap_offset\" " + parts[1])
            lines.append("_mwrite_byte_and_msync {} $offset $size $mmap_offset {} > /dev/null".format(
                MMAPWRITE_VALUE,
                parts[1]))
        elif parts[0] == "link" or parts[0] == "symlink":
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append(create_parent_dir(parts[2]))
            lines.append(remove_if_exists(parts[2]))
            lines.append("ln{} {} {}".format(
                "" if parts[0] == "link" else " -s",
                parts[1],
                parts[2]))
        elif parts[0] == "unlink" or parts[0] == "remove": 
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append("rm -rf " + parts[1])
        elif parts[0] == "rename":
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append(create_parent_dir(parts[2]))
            lines.append("rename {} {}".format(parts[1], parts[2]))
        elif parts[0] == "fsetxattr":
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append("attr -s {} -V {} {} > /dev/null".format(
                FILE_ATTR_KEY,
                FILE_ATTR_VALUE,
                parts[1]))
        elif parts[0] == "removexattr":
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append("attr -s {} -V {} {} > /dev/null".format(
                FILE_ATTR_KEY,
                FILE_ATTR_VALUE,
                parts[1]))
            lines.append("attr -r {} {}".format(FILE_ATTR_KEY, parts[1]))
        elif parts[0] == "truncate":
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append("truncate {} -s {}".format(parts[1], parts[2]))
        elif parts[0] == "fdatasync":
            lines.append(create_parent_dir(parts[1]))
            lines.append(create_file(parts[1]))
            lines.append(ensure_file_size(parts[1]))
            lines.append("fdatasync " + parts[1])
    return lines

def build_array_lines(lines):
    lines_to_add = []
    for elems in lines:
        name = elems[0]
        options = sorted(list(map(lambda x: "{}".format(
            translate_filename(x) if is_file_or_dir(x) else x), 
            elems[1:])))

        lines_to_add.append("{}_options=(\"{}\")".format(name, "\" \"".join(options)))
    return lines_to_add

def add_sync_check_consistency(commands_with_deps, commands, do_fsync):
    if do_fsync:
        commands_with_deps.append("do_fsync_check $fsync_file $fsync_command")
    else:
        # For mmapwrite and fdatasync, the file is the first argument.
        elems = commands[0].split(" ")
        commands_with_deps.append("check_consistency " + elems[1])
    return commands_with_deps


# Creates a test file for J-lang V2 files.
def build_test_v2(parsed_args):
    base_file = find_basefile(2)
    test_file = parsed_args.test_file
    test_number = parsed_args.test_number
    filesystem_type = parsed_args.filesystem_type
    generated_test = os.path.join(parsed_args.target_path, test_number)
    generated_test_output = os.path.join(parsed_args.target_path, test_number + ".out")

    # Template parameters and helper functions for populating the base file
    template_params = [("$CURRENT_YEAR", str(datetime.datetime.now().year)),
                       ("$TEST_NUMBER", test_number),
                       ("$FILESYSTEM_TYPE", filesystem_type)]

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

    copyfile(base_file, generated_test)


    args = []
    commands = []
    with open(test_file, 'r') as f:
        # Ignore first line
        f.readline()

        for line in f:
            line = line.strip()

            if line.startswith("file") or line.startswith("option"):
                args.append(line.split(" "))
            else:
                commands.append(line)

    assert len(commands) == 1
    do_fsync = commands[0].split(" ")[0] != "mmapwrite"

    fname = function_name_from_commands(commands)
    commands_with_deps = build_command_dependencies(commands)
    commands_with_deps = add_sync_check_consistency(commands_with_deps, commands, do_fsync)

    template_lines = build_template_function(commands, commands_with_deps, args, fname, do_fsync)
    array_lines = build_array_lines(args)
    loop_lines = build_for_loops(args, fname, do_fsync)

    lines_to_add = template_lines + array_lines + loop_lines

    # Insert lines into relevant location and write output files.
    lines_to_add = list(map(lambda x: x + "\n", lines_to_add))
    output_contents = base_contents[:test_cases_index] + lines_to_add + base_contents[test_cases_index:]
    with open(generated_test, 'w+') as f:
        f.writelines(output_contents)

    out_lines = ["QA output created by {}\n".format(test_number), "Silence is golden\n"]
    with open(generated_test_output, 'w+') as f:
        f.writelines(out_lines)

def main():
    # Parse input args
    parsed_args = build_parser().parse_args()

    # Print the test setup - just for sanity
    #print_setup(parsed_args)
    
    # Check if test file exists
    if not os.path.exists(parsed_args.test_file) or not os.path.isfile(parsed_args.test_file):
        print parsed_args.test_file + ' : No such test file\n'
        exit(1)

    # Get J-lang version number
    jlang_version = 1
    with open(parsed_args.test_file, "r") as f:
        if "J2-Lang" in f.readline().strip():
            jlang_version = 2

    # Check that test number is a valid number/has no extension
    if not parsed_args.test_number.isdigit():
        print("'{}' is not a valid test number".format(parsed_args.test_number))
        exit(1)
    
    # Create the target directory
    create_dir(parsed_args.target_path)

    if jlang_version == 1:
        build_test_v1(parsed_args)
    elif jlang_version == 2:
        build_test_v2(parsed_args)


if __name__ == '__main__':
    main()
