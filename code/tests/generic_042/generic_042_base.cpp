#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>

#include <string>

#include "generic_042_base.h"
#include "../../user_tools/api/workload.h"
#include "../../user_tools/api/actions.h"

namespace fs_testing {
namespace tests {

using fs_testing::tests::DataTestResult;
using fs_testing::user_tools::api::WriteData;
using fs_testing::user_tools::api::Checkpoint;

using std::string;

namespace {

static constexpr char kTestFileName[] = "foo";

static const int kTestFileFlags = O_WRONLY | O_CREAT | O_TRUNC;
static const mode_t kTestFilePerms = (mode_t) (S_IRWXU | S_IRWXG | S_IRWXO);

static const unsigned int kWriteDataChunk = 4096;

}

Generic042Base::Generic042Base(unsigned int start_file_size,
    unsigned int falloc_offset, unsigned int falloc_len, int mode) :
  start_file_size_(start_file_size), falloc_offset_(falloc_offset),
  falloc_len_(falloc_len), falloc_mode_(mode) {}

int Generic042Base::setup() {
  // Fill the entire file system with data so that when we make a file later
  // on it will use data blocks that have already been written.
  const string path(mnt_dir_ + "/" + kTestFileName);

  const int fd = open(path.c_str(), kTestFileFlags, kTestFilePerms);
  if (fd < 0) {
    return -1;
  }
  unsigned int data_written = 0;
  while (1) {
    // Keep writing chunks of data to the file until we get an error. If the
    // error is that the disk is full, then we've accomplished what we want
    // and we can continue on. Otherwise, we should abort because something
    // else went wrong.
    const int write_res = WriteData(fd, data_written, kWriteDataChunk);
    if (write_res == ENOSPC) {
      break;
    } else if (write_res < 0) {
      close(fd);
      return -2;
    }

    data_written += kWriteDataChunk;
  }
  // Make sure data is written out.
  syncfs(fd);

  // Delete the file we made so that only the used data blocks are left
  // behind.
  if (unlink(path.c_str()) != 0) {
    close(fd);
    return -3;
  }
  syncfs(fd);
  close(fd);

  // Just so that we will always have a file in the crash state.
  const int fd2 = open(path.c_str(), kTestFileFlags, kTestFilePerms);
  if (fd2 < 0) {
    return -4;
  }
  syncfs(fd2);
  close(fd2);

  return 0;
}

int Generic042Base::run() {
  // Write 64k of data to test file.
  const string file_path(mnt_dir_ + "/" + kTestFileName);

  const int fd_file = open(file_path.c_str(), O_WRONLY, kTestFilePerms);
  if (fd_file < 0) {
    return -1;
  }

  char data[4096];
  memset(data, 0xff, 4096);
  unsigned int written_data = 0;
  while (written_data < start_file_size_) {
    const int to_write = (4096 < start_file_size_ - written_data) ?
      4096 :
      start_file_size_ - written_data;
    const int res = write(fd_file, data, to_write);
    if (res < 0) {
      close(fd_file);
      return -1;
    }
    written_data += res;
  }

  // If we fsync here, it resolves the delayed allocation
  // fsync(fd_file);

  if (fallocate(fd_file, falloc_mode_, falloc_offset_, falloc_len_) != 0) {
    close(fd_file);
    return -2;
  }

  fsync(fd_file);

  //Any crash beyond this point should have persisted both the write and the appropriate fallocate
  Checkpoint();

  close(fd_file);

  return 0;
}

int Generic042Base::CheckBase(DataTestResult *test_result) {
  const string file_path(mnt_dir_ + "/" + kTestFileName);
  const string command("hexdump -C " + file_path);
  struct stat file_stats;

  if (stat(file_path.c_str(), &file_stats) < 0) {
    // Problem stat-ing file -> file not found?
    test_result->SetError(DataTestResult::kFileMissing);
    test_result->error_description = ": file " + file_path + " is missing";
    return -1;
  }

  if (file_stats.st_size == start_file_size_) {
    // Replay may be mostly complete, so we should have a file of
    // start_file_size_ bytes and it should have the data written in run.
    return 1;
  } else if (file_stats.st_size != 0) {
    // File is partially written.
    test_result->SetError(DataTestResult::kFileDataCorrupted);
    test_result->error_description = ": file " + file_path +
      " has size " + std::to_string(file_stats.st_size) + " instead of 0\n\n";

    string file_cont;
    HexdumpFile(file_path, file_cont);
    test_result->error_description += file_cont;
    return -1;
  }

  // File size is zero.
  return 0;
};

int Generic042Base::CheckDataNoZeros(const unsigned int offset,
    const unsigned int len, DataTestResult *test_result) {
  const string file_path(mnt_dir_ + "/" + kTestFileName);
  const string command("hexdump -C " + file_path);

  const int fd_file = open(file_path.c_str(), O_RDONLY);
  if (fd_file < 0) {
    test_result->SetError(DataTestResult::kOther);
    test_result->error_description = ": error opening file";
    return -1;
  }

  uint8_t bit_and;
  uint8_t bit_or;
  // Check from falloc_offset_ for falloc_len_ bytes.
  int res = ReadData(test_result, fd_file, offset, len, &bit_and, &bit_or);
  if (res < 0) {
    close(fd_file);
    return res;
  }

  close(fd_file);

  if (bit_and != 0xff) {
    // If somewhere along the way a byte wasn't 0xff then bit_and will no longer
    // be 0xffffffff by the & in ReadData.
    test_result->SetError(DataTestResult::kFileDataCorrupted);
    test_result->error_description = ": stale data was leaked at offset :" + std::to_string(offset) + "\n\n";

    string file_cont;
    HexdumpFile(file_path, file_cont);
    test_result->error_description += file_cont;
    return -1;
  }

  return 0;
};

int Generic042Base::CheckDataWithZeros(const unsigned int offset,
    const unsigned int len, DataTestResult *test_result) {
  const string file_path(mnt_dir_ + "/" + kTestFileName);

  const int fd_file = open(file_path.c_str(), O_RDONLY);
  if (fd_file < 0) {
    test_result->SetError(DataTestResult::kOther);
    test_result->error_description = ": error opening file";
    return -1;
  }

  uint8_t bit_and;
  uint8_t bit_or;
  // Check from falloc_offset_ for falloc_len_ bytes.
  int res = ReadData(test_result, fd_file, offset, len, &bit_and, &bit_or);
  if (res < 0) {
    close(fd_file);
    return res;
  }

  close(fd_file);

  if (bit_or != 0) {
    // If somewhere along the way a byte wasn't 0xff then bit_or will no longer
    // be 0 by the | in ReadData.
    test_result->SetError(DataTestResult::kFileDataCorrupted);
    test_result->error_description = ": stale data was leaked at offset :" + std::to_string(offset) + "\n\n";
    string file_cont;
    HexdumpFile(file_path, file_cont);
    test_result->error_description += file_cont;
    return -1;
  }

  return 0;
};

int Generic042Base::ReadData(DataTestResult *test_result, int fd,
    const unsigned int offset, const unsigned int len, uint8_t *bit_and,
    uint8_t *bit_or) {
  *bit_and = 0xff;
  *bit_or = 0;
  char data[kWriteDataChunk];
  unsigned int read_data = 0;

  if (lseek(fd, offset, SEEK_SET) < 0) {
    test_result->SetError(DataTestResult::kOther);
    test_result->error_description = ": error seeking in file";
    return -1;
  }
  // Read 4k of data and make sure that all of the bytes in it are 0xff.
  while (read_data < len) {
    const unsigned int chunk_size =
      (kWriteDataChunk < len - read_data) ?
      kWriteDataChunk :
      len - read_data;
    unsigned int page_data = 0;
    memset(data, 0, chunk_size);
    while (page_data < chunk_size) {
      const int res = read(fd, data + page_data, chunk_size - page_data);
      if (res < 0) {
        test_result->SetError(DataTestResult::kOther);
        test_result->error_description = ": error reading file";
        return -1;
      }
      page_data += res;
      read_data += res;
    }
    // Loop through all values in the page we just read.
    for (unsigned int i = 0; i < chunk_size; ++i) {
      *bit_and &= data[i];
      *bit_or |= data[i];
    }
  }

  return 0;
}

int Generic042Base::HexdumpFile(const string &path, string &output) {
  const string command("hexdump -C " + path);

  FILE *pipe = popen(command.c_str(), "r");
  char tmp[128];
  if (!pipe) {
    return -1;
  }
  while (!feof(pipe)) {
    char *r = fgets(tmp, 128, pipe);
    // NULL can be returned on error.
    if (r != NULL) {
      output += tmp;
    }
  }
  int res = pclose(pipe);
  if (WIFEXITED(res)) {
    return WEXITSTATUS(res);
  }
  return -1;
}

}  // namespace tests
}  // namespace fs_testing
