//
// Created by Fábio Domingues on 20/07/17.
//

#ifndef CRASHMONKEY_BIO_ALIAS_H
#define CRASHMONKEY_BIO_ALIAS_H

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0) \
  && LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)

#define BI_RW                   bi_rw
#define BI_DISK                 bi_bdev->bd_disk
#define BI_SIZE                 bi_size
#define BI_SECTOR               bi_sector
#define BIO_ENDIO(bio, err)     bio_endio(bio, err)
#define BIO_IO_ERR(bio, err)    bio_endio(bio, err)
#define BIO_DISCARD_FLAG        REQ_DISCARD
#define BIO_IS_WRITE(bio)       (!!(bio_rw(bio) & REQ_WRITE))

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0) \
  && LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)

#define BI_RW                   bi_rw
#define BI_DISK                 bi_bdev->bd_disk
#define BI_SIZE                 bi_iter.bi_size
#define BI_SECTOR               bi_iter.bi_sector
#define BIO_ENDIO(bio, err)     bio_endio(bio, err)
#define BIO_IO_ERR(bio, err)    bio_endio(bio, err)
#define BIO_DISCARD_FLAG        REQ_DISCARD
#define BIO_IS_WRITE(bio)       (!!(bio_rw(bio) & REQ_WRITE))


#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0) \
  && LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)

#define BI_RW                   bi_rw
#define BI_DISK                 bi_bdev->bd_disk
#define BI_SIZE                 bi_iter.bi_size
#define BI_SECTOR               bi_iter.bi_sector
#define BIO_ENDIO(bio, err)     bio_endio(bio)
#define BIO_IO_ERR(bio, err)    bio_io_error(bio)
#define BIO_DISCARD_FLAG        REQ_DISCARD
#define BIO_IS_WRITE(bio)       (!!(bio_rw(bio) & REQ_WRITE))

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0) \
  && LINUX_VERSION_CODE < KERNEL_VERSION(4, 17, 0)

#define BI_RW                   bi_opf
#define BI_DISK                 bi_disk
#define BI_SIZE                 bi_iter.bi_size
#define BI_SECTOR               bi_iter.bi_sector
#define BIO_ENDIO(bio, err)     bio_endio(bio)
#define BIO_IO_ERR(bio, err)    bio_io_error(bio)
#define BIO_DISCARD_FLAG        REQ_OP_DISCARD
#define BIO_IS_WRITE(bio)       op_is_write(bio_op(bio))

#else
#error "Unsupported kernel version: CrashMonkey has not been tested with " \
  "your kernel version."
#endif

#endif //CRASHMONKEY_BIO_ALIAS_H
