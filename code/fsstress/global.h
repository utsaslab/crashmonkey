/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#ifndef GLOBAL_H
#define GLOBAL_H

#include "config.h"

#ifdef HAVE_XFS_XFS_H
#include <xfs/xfs.h>
#endif

#ifdef HAVE_XFS_LIBXFS_H
#include <xfs/libxfs.h>
#endif

#ifdef HAVE_XFS_JDM_H
#include <xfs/jdm.h>
#endif

#ifdef HAVE_ATTR_ATTRIBUTES_H
#include <attr/attributes.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_SYS_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_SYS_ATTRIBUTES_H
#include <sys/attributes.h>
#endif

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif

#ifdef STDC_HEADERS
#include <signal.h>
#include <stdint.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_SYS_SYSSGI_H
#include <sys/syssgi.h>
#endif

#ifdef HAVE_SYS_UUID_H
#include <sys/uuid.h>
#endif

#ifdef HAVE_SYS_FS_XFS_FSOPS_H
#include <sys/fs/xfs_fsops.h>
#endif

#ifdef HAVE_SYS_FS_XFS_ITABLE_H
#include <sys/fs/xfs_itable.h>
#endif

#ifdef HAVE_BSTRING_H
#include <bstring.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_LINUX_FALLOC_H
#include <linux/falloc.h>

#ifndef FALLOC_FL_KEEP_SIZE
#define FALLOC_FL_KEEP_SIZE		0x01
#endif

#ifndef FALLOC_FL_PUNCH_HOLE
#define FALLOC_FL_PUNCH_HOLE		0x02
#endif

#ifndef FALLOC_FL_NO_HIDE_STALE
#define FALLOC_FL_NO_HIDE_STALE		0x04
#endif

#ifndef FALLOC_FL_COLLAPSE_RANGE
#define FALLOC_FL_COLLAPSE_RANGE	0x08
#endif

#ifndef FALLOC_FL_ZERO_RANGE
#define FALLOC_FL_ZERO_RANGE		0x10
#endif

#ifndef FALLOC_FL_INSERT_RANGE
#define FALLOC_FL_INSERT_RANGE		0x20
#endif

#endif /* HAVE_LINUX_FALLOC_H */

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#endif /* GLOBAL_H */
