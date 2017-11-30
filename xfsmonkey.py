#!/usr/bin/env python3
import os
import re
import sys
import stat
import subprocess
import argparse


def build_parser():
    parser = argparse.ArgumentParser(description='Run xfstests through crashmonkey.')

    # global args
    parser.add_argument('--fs-type', '-f', default=None)

    # crash monkey args
    parser.add_argument('--crashmonkey-root', '-c', default="./crashmonkey")
    parser.add_argument('--dev-size', '-s', default=1048576, type=int)

    # xfstest args
    parser.add_argument('--xfstest-root', '-x', default="./xfstests")
    parser.add_argument('--primary-dev', choices=['TEST', 'SCR'], default='TEST')
    parser.add_argument('--secondary-mnt-data', '-t', nargs=2, default=None)
    parser.add_argument('--xfstests-args', '-e', default="", nargs=argparse.REMAINDER)
    return parser


def remove_comments(lines):
    return filter(lambda x: len(x.strip()) != 0 and not str.startswith(x.strip(), '#'), lines)


def dev_mnt_pair(dev, sec_args=None):
    dev_mnt_names = {
            "TEST": ("TEST_DEV", "TEST_DIR"),
            "SCR": ("SCRATCH_DEV", "SCRATCH_MNT"),
            "primary": ("/dev/hwm", "/mnt/snapshot")
    }
    if sec_args is not None:
        sec_args = tuple(sec_args)
    return dev_mnt_names["TEST" if dev == "TEST" else "SCR"], dev_mnt_names["SCR" if dev == "TEST" else "TEST"], dev_mnt_names["primary"], sec_args


def init_script(src, parsed_args):
    primary_dev, secondary_dev, primary_args, secondary_args = dev_mnt_pair(parsed_args.primary_dev, parsed_args.secondary_mnt_data)

    with open(src, "r") as init_script:
        extracted_lines = init_script.readlines()

    r_lines = remove_comments(extracted_lines)
    export_func = None
    env_exports = []

    for r_line in r_lines:
        r_line = r_line.strip()
        if not (r_line.startswith('export') or r_line.startswith('setenv')):
            print("Only environment exports allowed on a single line. Unknown script instruction {}. Early exit.".format(r_line))
            exit(0)
        if export_func is None:
            export_func = r_line[:6]
        env_exports.append(
            tuple(re.split("(\s+|=|;)*", r_line)[1:3])
        )
    env_exports = dict(env_exports)
    print("Old environment", env_exports)

    # Primary device
    next_env_exports = dict(zip(primary_dev, primary_args))

    # Secondary device
    if secondary_args is None:
        if parsed_args.primary_dev == "TEST":
            if "SCRATCH_DEV" in env_exports and "SCRATCH_MNT" in env_exports:
                parsed_args.scratch_dev = [env_exports["SCRATCH_DEV"], env_exports["SCRATCH_MNT"]]
        elif parsed_args.primary_dev == "SCR":
            if "TEST_DEV" in env_exports and "TEST_DIR" in env_exports:
                parsed_args.scratch_dev = [env_exports["TEST_DEV"], env_exports["TEST_DIR"]]
    if secondary_args is not None:
        next_env_exports = {**next_env_exports, **dict(zip(secondary_dev, secondary_args))}

    # File system type
    if parsed_args.fs_type is None:
        if "FSTYP" in env_exports:
            parsed_args.fs_type = env_exports["FSTYP"]
        else:
            parsed_args.fs_type = "ext4"
    next_env_exports["FSTYP"] = parsed_args.fs_type

    print("Generated environment", next_env_exports)

    with open(src, "w") as init_script:
        # write new script
        if export_func is None:
            export_func = "export"
        init_script.truncate(0)
        for key in next_env_exports:
            print("{} {}={}\n".format(export_func, key, next_env_exports[key]))
            init_script.write("{} {}={}\n".format(export_func, key, next_env_exports[key]))

    return extracted_lines


def main():
    parsed_args = build_parser().parse_args()
    extracted = []
    xfs_config_path = parsed_args.xfstest_root + "/local.config"
    old_xfs_script = "\n".join(init_script(xfs_config_path, parsed_args))

    test_script_path = "./run.sh"
    with open(test_script_path, "w") as test_script:
        testing_script_data = "#!/usr/bin/env bash\nexport TEST_DEV=\"/dev/hwm\";export TEST_DIR=\"/mnt/snapshot\"\necho \"hello\"\ncd {}/build\n(sudo ./c_harness -f /dev/sda -t {} -m barrier -d /dev/cow_ram0 -e {} -b tests/echo_sub_dir.so -v -s 10)&\nsleep 2\nsudo user_tools/begin_log\ncd {}\n ./check {}; mount /dev/hwm /mnt/snapshot; \ncd {}/build\nsudo user_tools/end_log\nsudo user_tools/begin_tests\n".format(os.path.abspath(parsed_args.crashmonkey_root), parsed_args.fs_type, parsed_args.dev_size, os.path.abspath(parsed_args.xfstest_root), " ".join(parsed_args.xfstests_args), os.path.abspath(parsed_args.crashmonkey_root))
        test_script.write(testing_script_data)
    os.chmod(test_script_path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
    with subprocess.Popen(["sudo", "./run.sh"]) as running:
        running.wait()
    os.remove(test_script_path)

    with open(xfs_config_path, "w") as export_file:
        export_file.write(old_xfs_script)


if __name__ == "__main__":
    main()
