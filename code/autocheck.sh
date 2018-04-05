./c_harness -c -v -f /dev/sda -t btrfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_336.so
cat diff-at-chck*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko; rm -rf diff-at-chck*;

./c_harness -c -v -f /dev/sda -t xfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_336.so
cat diff-at-chck*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko

./c_harness -c -v -f /dev/sda -t btrfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_341.so
cat diff-at-check*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko; rm -rf diff-at-chck*;

./c_harness -c -v -f /dev/sda -t xfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_341.so
cat diff-at-check*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko; rm -rf diff-at-chck*;

./c_harness -c -v -f /dev/sda -t btrfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_342.so
cat diff-at-check*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko; rm -rf diff-at-chck*;

./c_harness -c -v -f /dev/sda -t xfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_342.so
cat diff-at-check*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko; rm -rf diff-at-chck*;

./c_harness -c -v -f /dev/sda -t btrfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_343.so
cat diff-at-check*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko; rm -rf diff-at-chck*;

./c_harness -c -v -f /dev/sda -t xfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_343.so
cat diff-at-check*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko; rm -rf diff-at-chck*;

./c_harness -c -v -f /dev/sda -t btrfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_348.so
cat diff-at-check*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko; rm -rf diff-at-chck*;

./c_harness -c -v -f /dev/sda -t xfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_348.so
cat diff-at-check*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko; rm -rf diff-at-chck*;

./c_harness -c -v -f /dev/sda -t btrfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_376.so
cat diff-at-check*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko; rm -rf diff-at-chck*;

./c_harness -c -v -f /dev/sda -t xfs -d /dev/cow_ram0 -e 102400 -s 100 tests/generic_376.so
cat diff-at-check*
umount /mnt/snapshot/; rmmod disk_wrapper.ko; rmmod cow_brd.ko; rm -rf diff-at-chck*;

