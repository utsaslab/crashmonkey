//
// Created by Fábio Domingues on 20/07/17.
//

#ifndef CRASHMONKEY_BIO_DEFINES_H
#define CRASHMONKEY_BIO_DEFINES_H

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 16, 0)

#define BI_SIZE                 bi_size
#define BI_SECTOR               bi_sector
#define BIO_ENDIO(bio, err)     bio_endio(bio, err)

#else

#define BI_SIZE                 bi_iter.bi_size
#define BI_SECTOR               bi_iter.bi_sector
#define BIO_ENDIO(bio, err)     bio->bi_error = err; bio_endio(bio)

#endif

#endif //CRASHMONKEY_BIO_DEFINES_H
