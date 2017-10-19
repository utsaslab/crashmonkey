#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <cstdlib>
#include <string>

#include "BaseTestCase.h"

using std::calloc;
using std::string;

using fs_testing::tests::DataTestResult;

#define TEST_FILE "test_file"
#define TEST_MNT "/mnt/snapshot"
#define TEST_DIR "test_dir"

#define TEST_FILE_PERMS  ((mode_t) (S_IRWXU | S_IRWXG | S_IRWXO))

// Text from "War and Peace", https://www.gutenberg.org/files/2600/2600-0.txt.
#define TEST_TEXT u8\
"“Well, Prince, so Genoa and Lucca are now just family estates of the " \
"Buonapartes. But I warn you, if you don’t tell me that this means war, " \
"if you still try to defend the infamies and horrors perpetrated by that " \
"Antichrist—I really believe he is Antichrist—I will have nothing " \
"more to do with you and you are no longer my friend, no longer my " \
"‘faithful slave,’ as you call yourself! But how do you do? I see I " \
"have frightened you—sit down and tell me all the news.” " \
"It was in July, 1805, and the speaker was the well-known Anna Pávlovna " \
"Schérer, maid of honor and favorite of the Empress Márya Fëdorovna. " \
"With these words she greeted Prince Vasíli Kurágin, a man of high " \
"rank and importance, who was the first to arrive at her reception. Anna " \
"Pávlovna had had a cough for some days. She was, as she said, suffering " \
"from la grippe; grippe being then a new word in St. Petersburg, used " \
"only by the elite. " \
"All her invitations without exception, written in French, and delivered " \
"by a scarlet-liveried footman that morning, ran as follows: " \
"“If you have nothing better to do, Count (or Prince), and if the " \
"prospect of spending an evening with a poor invalid is not too terrible, " \
"I shall be very charmed to see you tonight between 7 and 10—Annette " \
"Schérer.” " \
"“Heavens! what a virulent attack!” replied the prince, not in the " \
"least disconcerted by this reception. He had just entered, wearing an " \
"embroidered court uniform, knee breeches, and shoes, and had stars on " \
"his breast and a serene expression on his flat face. He spoke in that " \
"refined French in which our grandfathers not only spoke but thought, and " \
"with the gentle, patronizing intonation natural to a man of importance " \
"who had grown old in society and at court. He went up to Anna Pávlovna, " \
"kissed her hand, presenting to her his bald, scented, and shining head, " \
"and complacently seated himself on the sofa. " \
"“First of all, dear friend, tell me how you are. Set your friend’s " \
"mind at rest,” said he without altering his tone, beneath the " \
"politeness and affected sympathy of which indifference and even irony " \
"could be discerned. " \
"“Can one be well while suffering morally? Can one be calm in times " \
"like these if one has any feeling?” said Anna Pávlovna. “You are " \
"staying the whole evening, I hope?” "

// TODO(ashmrtn): Make helper function to concatenate file paths.
// TODO(ashmrtn): Pass mount path and test device names to tests somehow.
namespace fs_testing {
namespace tests {

using std::memcmp;

class rename_root_to_sub : public BaseTestCase {
 public:
  virtual int setup() override {
    // Create test directory.
    int res = mkdir(TEST_MNT "/" TEST_DIR, 0777);
    if (res < 0) {
      return -1;
    }
    const int dir = open(TEST_MNT "/" TEST_DIR, O_RDONLY);
    if (dir < 0) {
      return -1;
    }
    res = fsync(dir);
    if (res < 0) {
      return -1;
    }
    close(dir);

    // Create original file with permission etc.
    const int old_umask = umask(0000);
    const int fd = open(old_path.c_str(), O_RDWR | O_CREAT, TEST_FILE_PERMS);
    if (fd == -1) {
      umask(old_umask);
      return -1;
    }
    umask(old_umask);

    unsigned int written = 0;
    do {
      const int res = write(fd, (void*) ((char*) TEST_TEXT + written),
          strlen(TEST_TEXT) - written);
      if (res == -1) {
        close(fd);
        return -1;
      }
      written += res;
    } while (written != strlen(TEST_TEXT));
    fsync(fd);
    close(fd);

    return 0;
  }

  virtual int run() override {
    if (rename(old_path.c_str(), new_path.c_str()) < 0) {
      return -1;
    }

    return 0;
  }

  virtual int check_test(unsigned int last_checkpoint,
      DataTestResult *test_res) override {
    struct stat stats_old;
    struct stat stats_new;
    const int stat_old_res = stat(old_path.c_str(), &stats_old);
    const int errno_old = errno;
    const int stat_new_res = stat(new_path.c_str(), &stats_new);
    const int errno_new = errno;

    // Neither stat found the file, it's gone...
    if (stat_old_res < 0 && errno_old == ENOENT &&
        stat_new_res < 0 && errno_new == ENOENT) {
      test_res->SetError(DataTestResult::kFileMissing);
      test_res->error_description = old_path + ", " + new_path + " missing";
      return 0;
    }

    string check_file;
    struct stat check_stat;
    if (stat_old_res >= 0 && stat_new_res >= 0) {
      test_res->SetError(DataTestResult::kOldFilePersisted);
      test_res->error_description = old_path + " still present";
      return -1;
    } else if (stat_old_res >= 0) {
      check_file = old_path;
      check_stat = stats_old;
    } else if (stat_new_res >= 0) {
      check_file = new_path;
      check_stat = stats_new;
    } else {
      // Some other error(s) occurred.
      test_res->SetError(DataTestResult::kOther);
      test_res->error_description = "unknown error";
      return 0;
    }

    if (!S_ISREG(check_stat.st_mode)) {
      test_res->SetError(DataTestResult::kFileMetadataCorrupted);
      test_res->error_description = check_file + " has wrong file type";
      return 0;
    }
    if (((S_IRWXU | S_IRWXG | S_IRWXO) & check_stat.st_mode) !=
        TEST_FILE_PERMS) {
      test_res->SetError(DataTestResult::kFileMetadataCorrupted);
      test_res->error_description = check_file + " has wrong file permissions";
      return 0;
    }

    const int fd = open(check_file.c_str(), O_RDONLY);
    if (fd < 0) {
      test_res->SetError(DataTestResult::kOther);
      test_res->error_description = "unable to open " + check_file;
      return 0;
    }

    unsigned int bytes_read = 0;
    char* buf = (char*) calloc(strlen(TEST_TEXT), sizeof(char));
    if (buf == NULL) {
      test_res->SetError(DataTestResult::kOther);
      test_res->error_description = "out of memory";
      return 0;
    }
    do {
      const int res = read(fd, buf + bytes_read,
          strlen(TEST_TEXT) - bytes_read);
      if (res < 0) {
        free(buf);
        close(fd);
        test_res->SetError(DataTestResult::kOther);
        test_res->error_description = "error reading " + check_file;
        return 0;
      } else if (res == 0) {
        break;
      }
      bytes_read += res;
    } while (bytes_read < strlen(TEST_TEXT));
    close(fd);

    if (bytes_read != strlen(TEST_TEXT)) {
      test_res->SetError(DataTestResult::kFileDataCorrupted);
      test_res->error_description = check_file + " has truncated data";
    } else if (memcmp(TEST_TEXT, buf, strlen(TEST_TEXT)) != 0) {
      test_res->SetError(DataTestResult::kFileDataCorrupted);
      test_res->error_description = check_file + " has incorrect data";
    }

    free(buf);

    return 0;
  }
  virtual int pass(std::string mountDir, std::string filesysSize){
    std::cout << mountDir << " " << filesysSize << std::endl;
  }

 private:
    char text[strlen(TEST_TEXT)];
    const string old_path = TEST_MNT "/" TEST_FILE;
    const string new_path = TEST_MNT "/" TEST_DIR "/" TEST_FILE;
};

}  // namespace tests
}  // namespace fs_testing

extern "C" fs_testing::tests::BaseTestCase *test_case_get_instance() {
  return new fs_testing::tests::rename_root_to_sub;
}

extern "C" void test_case_delete_instance(fs_testing::tests::BaseTestCase *tc) {
  delete tc;
}
