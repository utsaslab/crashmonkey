/*
 *	Copyright (C) 1991, NeXT Computer, Inc.  All Rights Reserverd.
 *
 *	File:	fsx.c
 *	Author:	Avadis Tevanian, Jr.
 *
 *	File system exerciser. 
 *
 *	Rewritten 8/98 by Conrad Minshall.
 *
 *	Small changes to work under Linux -- davej.
 *
 *	Checks for mmap last-page zero fill.
 */

#include "global.h"

#include <limits.h>
#include <time.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <stdbool.h>
#ifdef HAVE_ERR_H
#include <err.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#ifdef AIO
#include <libaio.h>
#endif

#ifndef MAP_FILE
# define MAP_FILE 0
#endif

#define NUMPRINTCOLUMNS 32	/* # columns of data to print on each line */

/* Operation flags */

enum opflags { FL_NONE = 0, FL_SKIPPED = 1, FL_CLOSE_OPEN = 2, FL_KEEP_SIZE = 4 };

/*
 *	A log entry is an operation and a bunch of arguments.
 */

struct log_entry {
	int	operation;
	int	args[3];
	enum opflags flags;
};

#define	LOGSIZE	10000

struct log_entry	oplog[LOGSIZE];	/* the log */
int			logptr = 0;	/* current position in log */
int			logcount = 0;	/* total ops */

/*
 * The operation matrix is complex due to conditional execution of different
 * features. Hence when we come to deciding what operation to run, we need to
 * be careful in how we select the different operations. The active operations
 * are mapped to numbers as follows:
 *
 *			lite	!lite	integrity
 * READ:		0	0	0
 * WRITE:		1	1	1
 * MAPREAD:		2	2	2
 * MAPWRITE:		3	3	3
 * TRUNCATE:		-	4	4
 * FALLOCATE:		-	5	5
 * PUNCH HOLE:		-	6	6
 * ZERO RANGE:		-	7	7
 * COLLAPSE RANGE:	-	8	8
 * FSYNC:		-	-	9
 *
 * When mapped read/writes are disabled, they are simply converted to normal
 * reads and writes. When fallocate/fpunch calls are disabled, they are
 * skipped.
 *
 * Because of the "lite" version, we also need to have different "maximum
 * operation" defines to allow the ops to be selected correctly based on the
 * mode being run.
 */

/* common operations */
#define	OP_READ		0
#define OP_WRITE	1
#define OP_MAPREAD	2
#define OP_MAPWRITE	3
#define OP_MAX_LITE	4

/* !lite operations */
#define OP_TRUNCATE		4
#define OP_FALLOCATE		5
#define OP_PUNCH_HOLE		6
#define OP_ZERO_RANGE		7
#define OP_COLLAPSE_RANGE	8
#define OP_INSERT_RANGE	9
#define OP_MAX_FULL		10

/* integrity operations */
#define OP_FSYNC		10
#define OP_MAX_INTEGRITY	11

#undef PAGE_SIZE
#define PAGE_SIZE       getpagesize()
#undef PAGE_MASK
#define PAGE_MASK       (PAGE_SIZE - 1)

char	*original_buf;			/* a pointer to the original data */
char	*good_buf;			/* a pointer to the correct data */
char	*temp_buf;			/* a pointer to the current data */
char	*fname;				/* name of our test file */
char	*bname;				/* basename of our test file */
char	*logdev;			/* -i flag */
char	*logid;				/* -j flag */
char	dname[1024];			/* -P flag */
char	goodfile[1024];
int	dirpath = 0;			/* -P flag */
int	fd;				/* fd for our test file */

blksize_t	block_size = 0;
off_t		file_size = 0;
off_t		biggest = 0;
unsigned long	testcalls = 0;		/* calls to function "test" */

unsigned long	simulatedopcount = 0;	/* -b flag */
int	closeprob = 0;			/* -c flag */
int	debug = 0;			/* -d flag */
unsigned long	debugstart = 0;		/* -D flag */
char	filldata = 0;			/* -g flag */
int	flush = 0;			/* -f flag */
int	do_fsync = 0;			/* -y flag */
unsigned long	maxfilelen = 256 * 1024;	/* -l flag */
int	sizechecks = 1;			/* -n flag disables them */
int	maxoplen = 64 * 1024;		/* -o flag */
int	quiet = 0;			/* -q flag */
unsigned long progressinterval = 0;	/* -p flag */
int	readbdy = 1;			/* -r flag */
int	style = 0;			/* -s flag */
int	prealloc = 0;			/* -x flag */
int	truncbdy = 1;			/* -t flag */
int	writebdy = 1;			/* -w flag */
long	monitorstart = -1;		/* -m flag */
long	monitorend = -1;		/* -m flag */
int	lite = 0;			/* -L flag */
long	numops = -1;			/* -N flag */
int	randomoplen = 1;		/* -O flag disables it */
int	seed = 1;			/* -S flag */
int     mapped_writes = 1;              /* -W flag disables */
int     fallocate_calls = 1;            /* -F flag disables */
int     keep_size_calls = 1;            /* -K flag disables */
int     punch_hole_calls = 1;           /* -H flag disables */
int     zero_range_calls = 1;           /* -z flag disables */
int	collapse_range_calls = 1;	/* -C flag disables */
int	insert_range_calls = 1;		/* -I flag disables */
int 	mapped_reads = 1;		/* -R flag disables it */
int	integrity = 0;			/* -i flag */
int	fsxgoodfd = 0;
int	o_direct;			/* -Z */
int	aio = 0;
int	mark_nr = 0;

int page_size;
int page_mask;
int mmap_mask;
#ifdef AIO
int aio_rw(int rw, int fd, char *buf, unsigned len, unsigned offset);
#define READ 0
#define WRITE 1
#define fsxread(a,b,c,d)	aio_rw(READ, a,b,c,d)
#define fsxwrite(a,b,c,d)	aio_rw(WRITE, a,b,c,d)
#else
#define fsxread(a,b,c,d)	read(a,b,c)
#define fsxwrite(a,b,c,d)	write(a,b,c)
#endif

const char *replayops = NULL;
const char *recordops = NULL;
FILE *	fsxlogf = NULL;
FILE *	replayopsf = NULL;
char opsfile[1024];
int badoff = -1;
int closeopen = 0;

static void *round_ptr_up(void *ptr, unsigned long align, unsigned long offset)
{
	unsigned long ret = (unsigned long)ptr;

	ret = ((ret + align - 1) & ~(align - 1));
	ret += offset;
	return (void *)ret;
}

void
vwarnc(int code, const char *fmt, va_list ap)
{
	if (logid)
		fprintf(stderr, "%s: ", logid);
	fprintf(stderr, "fsx: ");
	if (fmt != NULL) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, ": ");
	}
	fprintf(stderr, "%s\n", strerror(code));
}

void
warn(const char * fmt, ...)  {
	va_list ap;
	va_start(ap, fmt);
	vwarnc(errno, fmt, ap);
	va_end(ap);
}

void
prt(const char *fmt, ...)
{
	va_list args;

	if (logid)
		fprintf(stdout, "%s: ", logid);
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	if (fsxlogf) {
		va_start(args, fmt);
		vfprintf(fsxlogf, fmt, args);
		va_end(args);
	}
}

void
prterr(const char *prefix)
{
	prt("%s%s%s\n", prefix, prefix ? ": " : "", strerror(errno));
}


static const char *op_names[] = {
	[OP_READ] = "read",
	[OP_WRITE] = "write",
	[OP_MAPREAD] = "mapread",
	[OP_MAPWRITE] = "mapwrite",
	[OP_TRUNCATE] = "truncate",
	[OP_FALLOCATE] = "fallocate",
	[OP_PUNCH_HOLE] = "punch_hole",
	[OP_ZERO_RANGE] = "zero_range",
	[OP_COLLAPSE_RANGE] = "collapse_range",
	[OP_INSERT_RANGE] = "insert_range",
	[OP_FSYNC] = "fsync",
};

static const char *op_name(int operation)
{
	if (operation >= 0 &&
	    operation < sizeof(op_names) / sizeof(op_names[0]))
		return op_names[operation];
	return NULL;
}

static int op_code(const char *name)
{
	int i;

	for (i = 0; i < sizeof(op_names) / sizeof(op_names[0]); i++)
		if (op_names[i] && strcmp(name, op_names[i]) == 0)
			return i;
	return -1;
}

void
log4(int operation, int arg0, int arg1, enum opflags flags)
{
	struct log_entry *le;

	le = &oplog[logptr];
	le->operation = operation;
	if (closeopen)
		flags |= FL_CLOSE_OPEN;
	le->args[0] = arg0;
	le->args[1] = arg1;
	le->args[2] = file_size;
	le->flags = flags;
	logptr++;
	logcount++;
	if (logptr >= LOGSIZE)
		logptr = 0;
}


void
logdump(void)
{
	FILE	*logopsf;
	int	i, count, down;
	struct log_entry	*lp;

	prt("LOG DUMP (%d total operations):\n", logcount);

	logopsf = fopen(opsfile, "w");
	if (!logopsf)
		prterr(opsfile);

	if (logcount < LOGSIZE) {
		i = 0;
		count = logcount;
	} else {
		i = logptr;
		count = LOGSIZE;
	}
	for ( ; count > 0; count--) {
		bool overlap;
		int opnum;

		opnum = i+1 + (logcount/LOGSIZE)*LOGSIZE;
		prt("%d(%3d mod 256): ", opnum, opnum%256);
		lp = &oplog[i];

		overlap = badoff >= lp->args[0] &&
			  badoff < lp->args[0] + lp->args[1];

		if (lp->flags & FL_SKIPPED) {
			prt("SKIPPED (no operation)");
			goto skipped;
		}

		switch (lp->operation) {
		case OP_MAPREAD:
			prt("MAPREAD  0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t***RRRR***");
			break;
		case OP_MAPWRITE:
			prt("MAPWRITE 0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t******WWWW");
			break;
		case OP_READ:
			prt("READ     0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t***RRRR***");
			break;
		case OP_WRITE:
			prt("WRITE    0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (lp->args[0] > lp->args[2])
				prt(" HOLE");
			else if (lp->args[0] + lp->args[1] > lp->args[2])
				prt(" EXTEND");
			overlap = (badoff >= lp->args[0] ||
				   badoff >=lp->args[2]) &&
				  badoff < lp->args[0] + lp->args[1];
			if (overlap)
				prt("\t***WWWW");
			break;
		case OP_TRUNCATE:
			down = lp->args[1] < lp->args[2];
			prt("TRUNCATE %s\tfrom 0x%x to 0x%x",
			    down ? "DOWN" : "UP", lp->args[2], lp->args[1]);
			overlap = badoff >= lp->args[1 + !down] &&
				  badoff < lp->args[1 + !!down];
			if (overlap)
				prt("\t******WWWW");
			break;
		case OP_FALLOCATE:
			/* 0: offset 1: length 2: where alloced */
			prt("FALLOC   0x%x thru 0x%x\t(0x%x bytes) ",
				lp->args[0], lp->args[0] + lp->args[1],
				lp->args[1]);
			if (lp->args[0] + lp->args[1] <= lp->args[2])
				prt("INTERIOR");
			else if (lp->flags & FL_KEEP_SIZE)
				prt("PAST_EOF");
			else
				prt("EXTENDING");
			if (overlap)
				prt("\t******FFFF");
			break;
		case OP_PUNCH_HOLE:
			prt("PUNCH    0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t******PPPP");
			break;
		case OP_ZERO_RANGE:
			prt("ZERO     0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t******ZZZZ");
			break;
		case OP_COLLAPSE_RANGE:
			prt("COLLAPSE 0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t******CCCC");
			break;
		case OP_INSERT_RANGE:
			prt("INSERT 0x%x thru 0x%x\t(0x%x bytes)",
			    lp->args[0], lp->args[0] + lp->args[1] - 1,
			    lp->args[1]);
			if (overlap)
				prt("\t******IIII");
			break;
		case OP_FSYNC:
			prt("FSYNC");
			break;
		default:
			prt("BOGUS LOG ENTRY (operation code = %d)!",
			    lp->operation);
			continue;
		}

	    skipped:
		if (lp->flags & FL_CLOSE_OPEN)
			prt("\n\t\tCLOSE/OPEN");
		prt("\n");
		i++;
		if (i == LOGSIZE)
			i = 0;

		if (logopsf) {
			if (lp->flags & FL_SKIPPED)
				fprintf(logopsf, "skip ");
			fprintf(logopsf, "%s 0x%x 0x%x 0x%x",
				op_name(lp->operation),
				lp->args[0], lp->args[1], lp->args[2]);
			if (lp->flags & FL_KEEP_SIZE)
				fprintf(logopsf, " keep_size");
			if (lp->flags & FL_CLOSE_OPEN)
				fprintf(logopsf, " close_open");
			if (overlap)
				fprintf(logopsf, " *");
			fprintf(logopsf, "\n");
		}
	}

	if (logopsf) {
		if (fclose(logopsf) != 0)
			prterr(opsfile);
		else
			prt("Log of operations saved to \"%s\"; "
			    "replay with --replay-ops\n",
			    opsfile);
	}
}


void
save_buffer(char *buffer, off_t bufferlength, int fd)
{
	off_t ret;
	ssize_t byteswritten;

	if (fd <= 0 || bufferlength == 0)
		return;

	if (bufferlength > SSIZE_MAX) {
		prt("fsx flaw: overflow in save_buffer\n");
		exit(67);
	}
	if (lite) {
		off_t size_by_seek = lseek(fd, (off_t)0, SEEK_END);
		if (size_by_seek == (off_t)-1)
			prterr("save_buffer: lseek eof");
		else if (bufferlength > size_by_seek) {
			warn("save_buffer: .fsxgood file too short... will save 0x%llx bytes instead of 0x%llx\n", (unsigned long long)size_by_seek,
			     (unsigned long long)bufferlength);
			bufferlength = size_by_seek;
		}
	}

	ret = lseek(fd, (off_t)0, SEEK_SET);
	if (ret == (off_t)-1)
		prterr("save_buffer: lseek 0");
	
	byteswritten = write(fd, buffer, (size_t)bufferlength);
	if (byteswritten != bufferlength) {
		if (byteswritten == -1)
			prterr("save_buffer write");
		else
			warn("save_buffer: short write, 0x%x bytes instead of 0x%llx\n",
			     (unsigned)byteswritten,
			     (unsigned long long)bufferlength);
	}
}


void
report_failure(int status)
{
	logdump();
	
	if (fsxgoodfd) {
		if (good_buf) {
			save_buffer(good_buf, file_size, fsxgoodfd);
			prt("Correct content saved for comparison\n");
			prt("(maybe hexdump \"%s\" vs \"%s\")\n",
			    fname, goodfile);
		}
		close(fsxgoodfd);
	}
	exit(status);
}


#define short_at(cp) ((unsigned short)((*((unsigned char *)(cp)) << 8) | \
				        *(((unsigned char *)(cp)) + 1)))

void
mark_log(void)
{
	char command[256];
	int ret;

	snprintf(command, 256, "dmsetup message %s 0 mark %s.mark%d", logdev,
		 bname, mark_nr);
	ret = system(command);
	if (ret) {
		prterr("dmsetup mark failed");
		exit(211);
	}
}

void
dump_fsync_buffer(void)
{
	char fname_buffer[1024];
	int good_fd;

	if (!good_buf)
		return;

	snprintf(fname_buffer, 1024, "%s%s.mark%d", dname,
		 bname, mark_nr);
	good_fd = open(fname_buffer, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (good_fd < 0) {
		prterr(fname_buffer);
		exit(212);
	}

	save_buffer(good_buf, file_size, good_fd);
	close(good_fd);
	prt("Dumped fsync buffer to %s\n", fname_buffer + dirpath);
}

void
check_buffers(unsigned offset, unsigned size)
{
	unsigned char c, t;
	unsigned i = 0;
	unsigned n = 0;
	unsigned op = 0;
	unsigned bad = 0;

	if (memcmp(good_buf + offset, temp_buf, size) != 0) {
		prt("READ BAD DATA: offset = 0x%x, size = 0x%x, fname = %s\n",
		    offset, size, fname);
		prt("OFFSET\tGOOD\tBAD\tRANGE\n");
		while (size > 0) {
			c = good_buf[offset];
			t = temp_buf[i];
			if (c != t) {
			        if (n < 16) {
					bad = short_at(&temp_buf[i]);
				        prt("0x%05x\t0x%04x\t0x%04x", offset,
				            short_at(&good_buf[offset]), bad);
					op = temp_buf[offset & 1 ? i+1 : i];
				        prt("\t0x%05x\n", n);
					if (op)
						prt("operation# (mod 256) for "
						  "the bad data may be %u\n",
						((unsigned)op & 0xff));
					else
						prt("operation# (mod 256) for "
						  "the bad data unknown, check"
						  " HOLE and EXTEND ops\n");
				}
				n++;
				badoff = offset;
			}
			offset++;
			i++;
			size--;
		}
		report_failure(110);
	}
}


void
check_size(void)
{
	struct stat	statbuf;
	off_t	size_by_seek;

	if (fstat(fd, &statbuf)) {
		prterr("check_size: fstat");
		statbuf.st_size = -1;
	}
	size_by_seek = lseek(fd, (off_t)0, SEEK_END);
	if (file_size != statbuf.st_size || file_size != size_by_seek) {
		prt("Size error: expected 0x%llx stat 0x%llx seek 0x%llx\n",
		    (unsigned long long)file_size,
		    (unsigned long long)statbuf.st_size,
		    (unsigned long long)size_by_seek);
		report_failure(120);
	}
}


void
check_trunc_hack(void)
{
	struct stat statbuf;
	off_t offset = file_size + (off_t)100000;

	if (ftruncate(fd, file_size))
		goto ftruncate_err;
	if (ftruncate(fd, offset))
		goto ftruncate_err;
	fstat(fd, &statbuf);
	if (statbuf.st_size != offset) {
		prt("no extend on truncate! not posix!\n");
		exit(130);
	}
	if (ftruncate(fd, file_size)) {
ftruncate_err:
		prterr("check_trunc_hack: ftruncate");
		exit(131);
	}
}

void
doflush(unsigned offset, unsigned size)
{
	unsigned pg_offset;
	unsigned map_size;
	char    *p;

	if (o_direct == O_DIRECT)
		return;

	pg_offset = offset & mmap_mask;
	map_size  = pg_offset + size;

	if ((p = (char *)mmap(0, map_size, PROT_READ | PROT_WRITE,
			      MAP_FILE | MAP_SHARED, fd,
			      (off_t)(offset - pg_offset))) == (char *)-1) {
		prterr("doflush: mmap");
		report_failure(202);
	}
	if (msync(p, map_size, MS_INVALIDATE) != 0) {
		prterr("doflush: msync");
		report_failure(203);
	}
	if (munmap(p, map_size) != 0) {
		prterr("doflush: munmap");
		report_failure(204);
	}
}

void
doread(unsigned offset, unsigned size)
{
	off_t ret;
	unsigned iret;

	offset -= offset % readbdy;
	if (o_direct)
		size -= size % readbdy;
	if (size == 0) {
		if (!quiet && testcalls > simulatedopcount && !o_direct)
			prt("skipping zero size read\n");
		log4(OP_READ, offset, size, FL_SKIPPED);
		return;
	}
	if (size + offset > file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping seek/read past end of file\n");
		log4(OP_READ, offset, size, FL_SKIPPED);
		return;
	}

	log4(OP_READ, offset, size, FL_NONE);

	if (testcalls <= simulatedopcount)
		return;

	if (!quiet &&
		((progressinterval && testcalls % progressinterval == 0)  ||
		(debug &&
		       (monitorstart == -1 ||
			(offset + size > monitorstart &&
			(monitorend == -1 || offset <= monitorend))))))
		prt("%lu read\t0x%x thru\t0x%x\t(0x%x bytes)\n", testcalls,
		    offset, offset + size - 1, size);
	ret = lseek(fd, (off_t)offset, SEEK_SET);
	if (ret == (off_t)-1) {
		prterr("doread: lseek");
		report_failure(140);
	}
	iret = fsxread(fd, temp_buf, size, offset);
	if (iret != size) {
		if (iret == -1)
			prterr("doread: read");
		else
			prt("short read: 0x%x bytes instead of 0x%x\n",
			    iret, size);
		report_failure(141);
	}
	check_buffers(offset, size);
}


void
check_eofpage(char *s, unsigned offset, char *p, int size)
{
	unsigned long last_page, should_be_zero;

	if (offset + size <= (file_size & ~page_mask))
		return;
	/*
	 * we landed in the last page of the file
	 * test to make sure the VM system provided 0's 
	 * beyond the true end of the file mapping
	 * (as required by mmap def in 1996 posix 1003.1)
	 */
	last_page = ((unsigned long)p + (offset & page_mask) + size) & ~page_mask;

	for (should_be_zero = last_page + (file_size & page_mask);
	     should_be_zero < last_page + page_size;
	     should_be_zero++)
		if (*(char *)should_be_zero) {
			prt("Mapped %s: non-zero data past EOF (0x%llx) page offset 0x%x is 0x%04x\n",
			    s, file_size - 1, should_be_zero & page_mask,
			    short_at(should_be_zero));
			report_failure(205);
		}
}


void
domapread(unsigned offset, unsigned size)
{
	unsigned pg_offset;
	unsigned map_size;
	char    *p;

	offset -= offset % readbdy;
	if (size == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero size read\n");
		log4(OP_MAPREAD, offset, size, FL_SKIPPED);
		return;
	}
	if (size + offset > file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping seek/read past end of file\n");
		log4(OP_MAPREAD, offset, size, FL_SKIPPED);
		return;
	}

	log4(OP_MAPREAD, offset, size, FL_NONE);

	if (testcalls <= simulatedopcount)
		return;

	if (!quiet &&
		((progressinterval && testcalls % progressinterval == 0) ||
		       (debug &&
		       (monitorstart == -1 ||
			(offset + size > monitorstart &&
			(monitorend == -1 || offset <= monitorend))))))
		prt("%lu mapread\t0x%x thru\t0x%x\t(0x%x bytes)\n", testcalls,
		    offset, offset + size - 1, size);

	pg_offset = offset & PAGE_MASK;
	map_size  = pg_offset + size;

	if ((p = (char *)mmap(0, map_size, PROT_READ, MAP_SHARED, fd,
			      (off_t)(offset - pg_offset))) == (char *)-1) {
	        prterr("domapread: mmap");
		report_failure(190);
	}
	memcpy(temp_buf, p + pg_offset, size);

	check_eofpage("Read", offset, p, size);

	if (munmap(p, map_size) != 0) {
		prterr("domapread: munmap");
		report_failure(191);
	}

	check_buffers(offset, size);
}


void
gendata(char *original_buf, char *good_buf, unsigned offset, unsigned size)
{
	while (size--) {
		if (filldata) {
			good_buf[offset] = filldata;
		} else {
			good_buf[offset] = testcalls % 256;
			if (offset % 2)
				good_buf[offset] += original_buf[offset];
		}
		offset++;
	}
}


void
dowrite(unsigned offset, unsigned size)
{
	off_t ret;
	unsigned iret;

	offset -= offset % writebdy;
	if (o_direct)
		size -= size % writebdy;
	if (size == 0) {
		if (!quiet && testcalls > simulatedopcount && !o_direct)
			prt("skipping zero size write\n");
		log4(OP_WRITE, offset, size, FL_SKIPPED);
		return;
	}

	log4(OP_WRITE, offset, size, FL_NONE);

	gendata(original_buf, good_buf, offset, size);
	if (file_size < offset + size) {
		if (file_size < offset)
			memset(good_buf + file_size, '\0', offset - file_size);
		file_size = offset + size;
		if (lite) {
			warn("Lite file size bug in fsx!");
			report_failure(149);
		}
	}

	if (testcalls <= simulatedopcount)
		return;

	if (!quiet &&
		((progressinterval && testcalls % progressinterval == 0) ||
		       (debug &&
		       (monitorstart == -1 ||
			(offset + size > monitorstart &&
			(monitorend == -1 || offset <= monitorend))))))
		prt("%lu write\t0x%x thru\t0x%x\t(0x%x bytes)\n", testcalls,
		    offset, offset + size - 1, size);
	ret = lseek(fd, (off_t)offset, SEEK_SET);
	if (ret == (off_t)-1) {
		prterr("dowrite: lseek");
		report_failure(150);
	}
	iret = fsxwrite(fd, good_buf + offset, size, offset);
	if (iret != size) {
		if (iret == -1)
			prterr("dowrite: write");
		else
			prt("short write: 0x%x bytes instead of 0x%x\n",
			    iret, size);
		report_failure(151);
	}
	if (do_fsync) {
		if (fsync(fd)) {
			prt("fsync() failed: %s\n", strerror(errno));
			report_failure(152);
		}
	}
	if (flush) {
		doflush(offset, size);
	}
}


void
domapwrite(unsigned offset, unsigned size)
{
	unsigned pg_offset;
	unsigned map_size;
	off_t    cur_filesize;
	char    *p;

	offset -= offset % writebdy;
	if (size == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero size write\n");
		log4(OP_MAPWRITE, offset, size, FL_SKIPPED);
		return;
	}
	cur_filesize = file_size;

	log4(OP_MAPWRITE, offset, size, FL_NONE);

	gendata(original_buf, good_buf, offset, size);
	if (file_size < offset + size) {
		if (file_size < offset)
			memset(good_buf + file_size, '\0', offset - file_size);
		file_size = offset + size;
		if (lite) {
			warn("Lite file size bug in fsx!");
			report_failure(200);
		}
	}

	if (testcalls <= simulatedopcount)
		return;

	if (!quiet &&
		((progressinterval && testcalls % progressinterval == 0) ||
		       (debug &&
		       (monitorstart == -1 ||
			(offset + size > monitorstart &&
			(monitorend == -1 || offset <= monitorend))))))
		prt("%lu mapwrite\t0x%x thru\t0x%x\t(0x%x bytes)\n", testcalls,
		    offset, offset + size - 1, size);

	if (file_size > cur_filesize) {
	        if (ftruncate(fd, file_size) == -1) {
		        prterr("domapwrite: ftruncate");
			exit(201);
		}
	}
	pg_offset = offset & PAGE_MASK;
	map_size  = pg_offset + size;

	if ((p = (char *)mmap(0, map_size, PROT_READ | PROT_WRITE,
			      MAP_FILE | MAP_SHARED, fd,
			      (off_t)(offset - pg_offset))) == (char *)-1) {
	        prterr("domapwrite: mmap");
		report_failure(202);
	}
	memcpy(p + pg_offset, good_buf + offset, size);
	if (msync(p, map_size, MS_SYNC) != 0) {
		prterr("domapwrite: msync");
		report_failure(203);
	}

	check_eofpage("Write", offset, p, size);

	if (munmap(p, map_size) != 0) {
		prterr("domapwrite: munmap");
		report_failure(204);
	}
}


void
dotruncate(unsigned size)
{
	int oldsize = file_size;

	size -= size % truncbdy;
	if (size > biggest) {
		biggest = size;
		if (!quiet && testcalls > simulatedopcount)
			prt("truncating to largest ever: 0x%x\n", size);
	}

	log4(OP_TRUNCATE, 0, size, FL_NONE);

	if (size > file_size)
		memset(good_buf + file_size, '\0', size - file_size);
	file_size = size;

	if (testcalls <= simulatedopcount)
		return;
	
	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      size <= monitorend)))
		prt("%lu trunc\tfrom 0x%x to 0x%x\n", testcalls, oldsize, size);
	if (ftruncate(fd, (off_t)size) == -1) {
	        prt("ftruncate1: %x\n", size);
		prterr("dotruncate: ftruncate");
		report_failure(160);
	}
}

#ifdef FALLOC_FL_PUNCH_HOLE
void
do_punch_hole(unsigned offset, unsigned length)
{
	unsigned end_offset;
	int max_offset = 0;
	int max_len = 0;
	int mode = FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE;

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length punch hole\n");
		log4(OP_PUNCH_HOLE, offset, length, FL_SKIPPED);
		return;
	}

	if (file_size <= (loff_t)offset) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping hole punch off the end of the file\n");
		log4(OP_PUNCH_HOLE, offset, length, FL_SKIPPED);
		return;
	}

	end_offset = offset + length;

	log4(OP_PUNCH_HOLE, offset, length, FL_NONE);

	if (testcalls <= simulatedopcount)
		return;

	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      end_offset <= monitorend))) {
		prt("%lu punch\tfrom 0x%x to 0x%x, (0x%x bytes)\n", testcalls,
			offset, offset+length, length);
	}
	if (fallocate(fd, mode, (loff_t)offset, (loff_t)length) == -1) {
		prt("punch hole: 0x%x to 0x%x\n", offset, offset + length);
		prterr("do_punch_hole: fallocate");
		report_failure(161);
	}


	max_offset = offset < file_size ? offset : file_size;
	max_len = max_offset + length <= file_size ? length :
			file_size - max_offset;
	memset(good_buf + max_offset, '\0', max_len);
}

#else
void
do_punch_hole(unsigned offset, unsigned length)
{
	return;
}
#endif

#ifdef FALLOC_FL_ZERO_RANGE
void
do_zero_range(unsigned offset, unsigned length, int keep_size)
{
	unsigned end_offset;
	int mode = FALLOC_FL_ZERO_RANGE;

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length zero range\n");
		log4(OP_ZERO_RANGE, offset, length, FL_SKIPPED |
		     (keep_size ? FL_KEEP_SIZE : FL_NONE));
		return;
	}

	end_offset = keep_size ? 0 : offset + length;

	if (end_offset > biggest) {
		biggest = end_offset;
		if (!quiet && testcalls > simulatedopcount)
			prt("zero_range to largest ever: 0x%x\n", end_offset);
	}

	/*
	 * last arg matches fallocate string array index in logdump:
	 * 	0: allocate past EOF
	 * 	1: extending prealloc
	 * 	2: interior prealloc
	 */
	log4(OP_ZERO_RANGE, offset, length,
	     keep_size ? FL_KEEP_SIZE : FL_NONE);

	if (testcalls <= simulatedopcount)
		return;

	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      end_offset <= monitorend))) {
		prt("%lu zero\tfrom 0x%x to 0x%x, (0x%x bytes)\n", testcalls,
			offset, offset+length, length);
	}
	if (fallocate(fd, mode, (loff_t)offset, (loff_t)length) == -1) {
		prt("zero range: 0x%x to 0x%x\n", offset, offset + length);
		prterr("do_zero_range: fallocate");
		report_failure(161);
	}

	memset(good_buf + offset, '\0', length);
}

#else
void
do_zero_range(unsigned offset, unsigned length, int keep_size)
{
	return;
}
#endif

#ifdef FALLOC_FL_COLLAPSE_RANGE
void
do_collapse_range(unsigned offset, unsigned length)
{
	unsigned end_offset;
	int mode = FALLOC_FL_COLLAPSE_RANGE;

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length collapse range\n");
		log4(OP_COLLAPSE_RANGE, offset, length, FL_SKIPPED);
		return;
	}

	end_offset = offset + length;
	if ((loff_t)end_offset >= file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping collapse range behind EOF\n");
		log4(OP_COLLAPSE_RANGE, offset, length, FL_SKIPPED);
		return;
	}

	log4(OP_COLLAPSE_RANGE, offset, length, FL_NONE);

	if (testcalls <= simulatedopcount)
		return;

	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      end_offset <= monitorend))) {
		prt("%lu collapse\tfrom 0x%x to 0x%x, (0x%x bytes)\n", testcalls,
			offset, offset+length, length);
	}
	if (fallocate(fd, mode, (loff_t)offset, (loff_t)length) == -1) {
		prt("collapse range: 0x%x to 0x%x\n", offset, offset + length);
		prterr("do_collapse_range: fallocate");
		report_failure(161);
	}

	memmove(good_buf + offset, good_buf + end_offset,
		file_size - end_offset);
	file_size -= length;
}

#else
void
do_collapse_range(unsigned offset, unsigned length)
{
	return;
}
#endif

#ifdef FALLOC_FL_INSERT_RANGE
void
do_insert_range(unsigned offset, unsigned length)
{
	unsigned end_offset;
	int mode = FALLOC_FL_INSERT_RANGE;

	if (length == 0) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping zero length insert range\n");
		log4(OP_INSERT_RANGE, offset, length, FL_SKIPPED);
		return;
	}

	if ((loff_t)offset >= file_size) {
		if (!quiet && testcalls > simulatedopcount)
			prt("skipping insert range behind EOF\n");
		log4(OP_INSERT_RANGE, offset, length, FL_SKIPPED);
		return;
	}

	log4(OP_INSERT_RANGE, offset, length, FL_NONE);

	if (testcalls <= simulatedopcount)
		return;

	end_offset = offset + length;
	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      end_offset <= monitorend))) {
		prt("%lu insert\tfrom 0x%x to 0x%x, (0x%x bytes)\n", testcalls,
			offset, offset+length, length);
	}
	if (fallocate(fd, mode, (loff_t)offset, (loff_t)length) == -1) {
		prt("insert range: 0x%x to 0x%x\n", offset, offset + length);
		prterr("do_insert_range: fallocate");
		report_failure(161);
	}

	memmove(good_buf + end_offset, good_buf + offset,
		file_size - offset);
	memset(good_buf + offset, '\0', length);
	file_size += length;
}

#else
void
do_insert_range(unsigned offset, unsigned length)
{
	return;
}
#endif

#ifdef HAVE_LINUX_FALLOC_H
/* fallocate is basically a no-op unless extending, then a lot like a truncate */
void
do_preallocate(unsigned offset, unsigned length, int keep_size)
{
	unsigned end_offset;

        if (length == 0) {
                if (!quiet && testcalls > simulatedopcount)
                        prt("skipping zero length fallocate\n");
                log4(OP_FALLOCATE, offset, length, FL_SKIPPED |
		     (keep_size ? FL_KEEP_SIZE : FL_NONE));
                return;
        }

	end_offset = keep_size ? 0 : offset + length;

	if (end_offset > biggest) {
		biggest = end_offset;
		if (!quiet && testcalls > simulatedopcount)
			prt("fallocating to largest ever: 0x%x\n", end_offset);
	}

	/*
	 * last arg matches fallocate string array index in logdump:
	 * 	0: allocate past EOF
	 * 	1: extending prealloc
	 * 	2: interior prealloc
	 */
	log4(OP_FALLOCATE, offset, length,
	     keep_size ? FL_KEEP_SIZE : FL_NONE);

	if (end_offset > file_size) {
		memset(good_buf + file_size, '\0', end_offset - file_size);
		file_size = end_offset;
	}

	if (testcalls <= simulatedopcount)
		return;
	
	if ((progressinterval && testcalls % progressinterval == 0) ||
	    (debug && (monitorstart == -1 || monitorend == -1 ||
		      end_offset <= monitorend)))
		prt("%lu falloc\tfrom 0x%x to 0x%x (0x%x bytes)\n", testcalls,
				offset, offset + length, length);
	if (fallocate(fd, keep_size ? FALLOC_FL_KEEP_SIZE : 0, (loff_t)offset, (loff_t)length) == -1) {
	        prt("fallocate: 0x%x to 0x%x\n", offset, offset + length);
		prterr("do_preallocate: fallocate");
		report_failure(161);
	}
}
#else
void
do_preallocate(unsigned offset, unsigned length, int keep_size)
{
	return;
}
#endif

void
writefileimage()
{
	ssize_t iret;

	if (lseek(fd, (off_t)0, SEEK_SET) == (off_t)-1) {
		prterr("writefileimage: lseek");
		report_failure(171);
	}
	iret = write(fd, good_buf, file_size);
	if ((off_t)iret != file_size) {
		if (iret == -1)
			prterr("writefileimage: write");
		else
			prt("short write: 0x%x bytes instead of 0x%llx\n",
			    iret, (unsigned long long)file_size);
		report_failure(172);
	}
	if (lite ? 0 : ftruncate(fd, file_size) == -1) {
	        prt("ftruncate2: %llx\n", (unsigned long long)file_size);
		prterr("writefileimage: ftruncate");
		report_failure(173);
	}
}


void
docloseopen(void)
{ 
	if (testcalls <= simulatedopcount)
		return;

	if (debug)
		prt("%lu close/open\n", testcalls);
	if (close(fd)) {
		prterr("docloseopen: close");
		report_failure(180);
	}
	fd = open(fname, O_RDWR|o_direct, 0);
	if (fd < 0) {
		prterr("docloseopen: open");
		report_failure(181);
	}
}

void
dofsync(void)
{
	int ret;

	if (testcalls <= simulatedopcount)
		return;
	if (debug)
		prt("%lu fsync\n", testcalls);
	log4(OP_FSYNC, 0, 0, 0);
	ret = fsync(fd);
	if (ret < 0) {
		prterr("dofsync");
		report_failure(210);
	}
	mark_log();
	dump_fsync_buffer();
	mark_nr++;
}

#define TRIM_OFF(off, size)			\
do {						\
	if (size)				\
		(off) %= (size);		\
	else					\
		(off) = 0;			\
} while (0)

#define TRIM_LEN(off, len, size)		\
do {						\
	if ((off) + (len) > (size))		\
		(len) = (size) - (off);		\
} while (0)

#define TRIM_OFF_LEN(off, len, size)		\
do {						\
	TRIM_OFF(off, size);			\
	TRIM_LEN(off, len, size);		\
} while (0)

void
cleanup(int sig)
{
	if (sig)
		prt("signal %d\n", sig);
	prt("testcalls = %lu\n", testcalls);
	exit(sig);
}

static int
read_op(struct log_entry *log_entry)
{
	char line[256];

	memset(log_entry, 0, sizeof(*log_entry));
	log_entry->operation = -1;

	while (log_entry->operation == -1) {
		char *str;
		int i;

		do {
			if (!fgets(line, sizeof(line), replayopsf)) {
				if (feof(replayopsf)) {
					replayopsf = NULL;
					return 0;
				}
				goto fail;
			}
			str = strtok(line, " \t\n");
		} while (!str || str[0] == '#');

		if (strcmp(str, "skip") == 0) {
			log_entry->flags |= FL_SKIPPED;
			str = strtok(NULL, " \t\n");
			if (!str)
				goto fail;
		}
		log_entry->operation = op_code(str);
		if (log_entry->operation == -1)
			goto fail;
		for (i = 0; i < 3; i++) {
			char *end;

			str = strtok(NULL, " \t\n");
			if (!str)
				goto fail;
			log_entry->args[i] = strtoul(str, &end, 0);
			if (*end)
				goto fail;
		}
		while ((str = strtok(NULL, " \t\n"))) {
			if (strcmp(str, "keep_size") == 0)
				log_entry->flags |= FL_KEEP_SIZE;
			else if (strcmp(str, "close_open") == 0)
				log_entry->flags |= FL_CLOSE_OPEN;
			else if (strcmp(str, "*") == 0)
				;  /* overlap marker; ignore */
			else
				goto fail;
		}
	}
	return 1;

fail:
	fprintf(stderr, "%s: parse error\n", replayops);
	fclose(replayopsf);
	replayopsf = NULL;
	cleanup(100);  /* doesn't return */
	return 0;
}


int
test(void)
{
	unsigned long	offset;
	unsigned long	size;
	unsigned long	rv;
	unsigned long	op;
	int		keep_size = 0;

	if (simulatedopcount > 0 && testcalls == simulatedopcount)
		writefileimage();

	testcalls++;

	if (debugstart > 0 && testcalls >= debugstart)
		debug = 1;

	if (!quiet && testcalls < simulatedopcount && testcalls % 100000 == 0)
		prt("%lu...\n", testcalls);

	if (replayopsf) {
		struct log_entry log_entry;

		while (read_op(&log_entry)) {
			if (log_entry.flags & FL_SKIPPED) {
				log4(log_entry.operation,
				     log_entry.args[0], log_entry.args[1],
				     log_entry.flags);
				continue;
			}

			op = log_entry.operation;
			offset = log_entry.args[0];
			size = log_entry.args[1];
			closeopen = !!(log_entry.flags & FL_CLOSE_OPEN);
			keep_size = !!(log_entry.flags & FL_KEEP_SIZE);
			goto have_op;
		}
		return 0;
	}

	rv = random();
	if (closeprob)
		closeopen = (rv >> 3) < (1 << 28) / closeprob;

	offset = random();
	size = maxoplen;
	if (randomoplen)
		size = random() % (maxoplen + 1);

	/* calculate appropriate op to run */
	if (lite)
		op = rv % OP_MAX_LITE;
	else if (!integrity)
		op = rv % OP_MAX_FULL;
	else
		op = rv % OP_MAX_INTEGRITY;

	switch(op) {
	case OP_TRUNCATE:
		if (!style)
			size = random() % maxfilelen;
		break;
	case OP_FALLOCATE:
		if (fallocate_calls && size && keep_size_calls)
			keep_size = random() % 2;
		break;
	case OP_ZERO_RANGE:
		if (zero_range_calls && size && keep_size_calls)
			keep_size = random() % 2;
		break;
	}

have_op:

	switch (op) {
	case OP_MAPREAD:
		if (!mapped_reads)
			op = OP_READ;
		break;
	case OP_MAPWRITE:
		if (!mapped_writes)
			op = OP_WRITE;
		break;
	case OP_FALLOCATE:
		if (!fallocate_calls) {
			log4(OP_FALLOCATE, offset, size, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_PUNCH_HOLE:
		if (!punch_hole_calls) {
			log4(OP_PUNCH_HOLE, offset, size, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_ZERO_RANGE:
		if (!zero_range_calls) {
			log4(OP_ZERO_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_COLLAPSE_RANGE:
		if (!collapse_range_calls) {
			log4(OP_COLLAPSE_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}
		break;
	case OP_INSERT_RANGE:
		if (!insert_range_calls) {
			log4(OP_INSERT_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}
		break;
	}

	switch (op) {
	case OP_READ:
		TRIM_OFF_LEN(offset, size, file_size);
		doread(offset, size);
		break;

	case OP_WRITE:
		TRIM_OFF_LEN(offset, size, maxfilelen);
		dowrite(offset, size);
		break;

	case OP_MAPREAD:
		TRIM_OFF_LEN(offset, size, file_size);
		domapread(offset, size);
		break;

	case OP_MAPWRITE:
		TRIM_OFF_LEN(offset, size, maxfilelen);
		domapwrite(offset, size);
		break;

	case OP_TRUNCATE:
		dotruncate(size);
		break;

	case OP_FALLOCATE:
		TRIM_OFF_LEN(offset, size, maxfilelen);
		do_preallocate(offset, size, keep_size);
		break;

	case OP_PUNCH_HOLE:
		TRIM_OFF_LEN(offset, size, file_size);
		do_punch_hole(offset, size);
		break;
	case OP_ZERO_RANGE:
		TRIM_OFF_LEN(offset, size, file_size);
		do_zero_range(offset, size, keep_size);
		break;
	case OP_COLLAPSE_RANGE:
		TRIM_OFF_LEN(offset, size, file_size - 1);
		offset = offset & ~(block_size - 1);
		size = size & ~(block_size - 1);
		if (size == 0) {
			log4(OP_COLLAPSE_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}
		do_collapse_range(offset, size);
		break;
	case OP_INSERT_RANGE:
		TRIM_OFF(offset, file_size);
		TRIM_LEN(file_size, size, maxfilelen);
		offset = offset & ~(block_size - 1);
		size = size & ~(block_size - 1);
		if (size == 0) {
			log4(OP_INSERT_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}
		if (file_size + size > maxfilelen) {
			log4(OP_INSERT_RANGE, offset, size, FL_SKIPPED);
			goto out;
		}

		do_insert_range(offset, size);
		break;
	case OP_FSYNC:
		dofsync();
		break;
	default:
		prterr("test: unknown operation");
		report_failure(42);
		break;
	}

out:
	if (sizechecks && testcalls > simulatedopcount)
		check_size();
	if (closeopen)
		docloseopen();
	return 1;
}


void
usage(void)
{
	fprintf(stdout, "usage: %s",
		"fsx [-dknqxAFLOWZ] [-b opnum] [-c Prob] [-g filldata] [-i logdev] [-j logid] [-l flen] [-m start:end] [-o oplen] [-p progressinterval] [-r readbdy] [-s style] [-t truncbdy] [-w writebdy] [-D startingop] [-N numops] [-P dirpath] [-S seed] fname\n\
	-b opnum: beginning operation number (default 1)\n\
	-c P: 1 in P chance of file close+open at each op (default infinity)\n\
	-d: debug output for all operations\n\
	-f flush and invalidate cache after I/O\n\
	-g X: write character X instead of random generated data\n\
	-i logdev: do integrity testing, logdev is the dm log writes device\n\
	-j logid: prefix debug log messsages with this id\n\
	-k: do not truncate existing file and use its size as upper bound on file size\n\
	-l flen: the upper bound on file size (default 262144)\n\
	-m startop:endop: monitor (print debug output) specified byte range (default 0:infinity)\n\
	-n: no verifications of file size\n\
	-o oplen: the upper bound on operation size (default 65536)\n\
	-p progressinterval: debug output at specified operation interval\n\
	-q: quieter operation\n\
	-r readbdy: 4096 would make reads page aligned (default 1)\n\
	-s style: 1 gives smaller truncates (default 0)\n\
	-t truncbdy: 4096 would make truncates page aligned (default 1)\n\
	-w writebdy: 4096 would make writes page aligned (default 1)\n\
	-x: preallocate file space before starting, XFS only (default 0)\n\
	-y synchronize changes to a file\n"

#ifdef AIO
"	-A: Use the AIO system calls\n"
#endif
"	-D startingop: debug output starting at specified operation\n"
#ifdef HAVE_LINUX_FALLOC_H
"	-F: Do not use fallocate (preallocation) calls\n"
#endif
#ifdef FALLOC_FL_PUNCH_HOLE
"	-H: Do not use punch hole calls\n"
#endif
#ifdef FALLOC_FL_ZERO_RANGE
"	-z: Do not use zero range calls\n"
#endif
#ifdef FALLOC_FL_COLLAPSE_RANGE
"	-C: Do not use collapse range calls\n"
#endif
#ifdef FALLOC_FL_INSERT_RANGE
"	-I: Do not use insert range calls\n"
#endif
"	-L: fsxLite - no file creations & no file size changes\n\
	-N numops: total # operations to do (default infinity)\n\
	-O: use oplen (see -o flag) for every op (default random)\n\
	-P: save .fsxlog .fsxops and .fsxgood files in dirpath (default ./)\n\
	-S seed: for random # generator (default 1) 0 gets timestamp\n\
	-W: mapped write operations DISabled\n\
        -R: read() system calls only (mapped reads disabled)\n\
        -Z: O_DIRECT (use -R, -W, -r and -w too)\n\
	--replay-ops opsfile: replay ops from recorded .fsxops file\n\
	--record-ops[=opsfile]: dump ops file also on success. optionally specify ops file name\n\
	fname: this filename is REQUIRED (no default)\n");
	exit(90);
}


int
getnum(char *s, char **e)
{
	int ret;

	*e = (char *) 0;
	ret = strtol(s, e, 0);
	if (*e)
		switch (**e) {
		case 'b':
		case 'B':
			ret *= 512;
			*e = *e + 1;
			break;
		case 'k':
		case 'K':
			ret *= 1024;
			*e = *e + 1;
			break;
		case 'm':
		case 'M':
			ret *= 1024*1024;
			*e = *e + 1;
			break;
		case 'w':
		case 'W':
			ret *= 4;
			*e = *e + 1;
			break;
		}
	return (ret);
}

#ifdef AIO

#define QSZ     1024
io_context_t	io_ctx;
struct iocb 	iocb;

int aio_setup()
{
	int ret;
	ret = io_queue_init(QSZ, &io_ctx);
	if (ret != 0) {
		fprintf(stderr, "aio_setup: io_queue_init failed: %s\n",
                        strerror(ret));
                return(-1);
        }
        return(0);
}

int
__aio_rw(int rw, int fd, char *buf, unsigned len, unsigned offset)
{
	struct io_event event;
	static struct timespec ts;
	struct iocb *iocbs[] = { &iocb };
	int ret;
	long res;

	if (rw == READ) {
		io_prep_pread(&iocb, fd, buf, len, offset);
	} else {
		io_prep_pwrite(&iocb, fd, buf, len, offset);
	}

	ts.tv_sec = 30;
	ts.tv_nsec = 0;
	ret = io_submit(io_ctx, 1, iocbs);
	if (ret != 1) {
		fprintf(stderr, "errcode=%d\n", ret);
		fprintf(stderr, "aio_rw: io_submit failed: %s\n",
				strerror(ret));
		goto out_error;
	}

	ret = io_getevents(io_ctx, 1, 1, &event, &ts);
	if (ret != 1) {
		if (ret == 0)
			fprintf(stderr, "aio_rw: no events available\n");
		else {
			fprintf(stderr, "errcode=%d\n", -ret);
			fprintf(stderr, "aio_rw: io_getevents failed: %s\n",
				 	strerror(-ret));
		}
		goto out_error;
	}
	if (len != event.res) {
		/*
		 * The b0rked libaio defines event.res as unsigned.
		 * However the kernel strucuture has it signed,
		 * and it's used to pass negated error value.
		 * Till the library is fixed use the temp var.
		 */
		res = (long)event.res;
		if (res >= 0)
			fprintf(stderr, "bad io length: %lu instead of %u\n",
					res, len);
		else {
			fprintf(stderr, "errcode=%ld\n", -res);
			fprintf(stderr, "aio_rw: async io failed: %s\n",
					strerror(-res));
			ret = res;
			goto out_error;
		}

	}
	return event.res;

out_error:
	/*
	 * The caller expects error return in traditional libc
	 * convention, i.e. -1 and the errno set to error.
	 */
	errno = -ret;
	return -1;
}

int aio_rw(int rw, int fd, char *buf, unsigned len, unsigned offset)
{
	int ret;

	if (aio) {
		ret = __aio_rw(rw, fd, buf, len, offset);
	} else {
		if (rw == READ)
			ret = read(fd, buf, len);
		else
			ret = write(fd, buf, len);
	}
	return ret;
}

#endif

#define test_fallocate(mode) __test_fallocate(mode, #mode)

int
__test_fallocate(int mode, const char *mode_str)
{
#ifdef HAVE_LINUX_FALLOC_H
	int ret = 0;
	if (!lite) {
		if (fallocate(fd, mode, file_size, 1) && errno == EOPNOTSUPP) {
			if(!quiet)
				fprintf(stderr,
					"main: filesystem does not support "
					"fallocate mode %s, disabling!\n",
					mode_str);
		} else {
			ret = 1;
			if (ftruncate(fd, file_size)) {
				warn("main: ftruncate");
				exit(132);
			}
		}
	}
	return ret;
#endif
}

static struct option longopts[] = {
	{"replay-ops", required_argument, 0, 256},
	{"record-ops", optional_argument, 0, 255},
	{ }
};

int
main(int argc, char **argv)
{
	int	i, style, ch;
	char	*endp, *tmp;
	char logfile[1024];
	struct stat statbuf;
	int o_flags = O_RDWR|O_CREAT|O_TRUNC;

	logfile[0] = 0;
	dname[0] = 0;

	page_size = getpagesize();
	page_mask = page_size - 1;
	mmap_mask = page_mask;
	

	setvbuf(stdout, (char *)0, _IOLBF, 0); /* line buffered stdout */

	while ((ch = getopt_long(argc, argv,
				 "b:c:dfg:i:j:kl:m:no:p:qr:s:t:w:xyAD:FKHzCILN:OP:RS:WZ",
				 longopts, NULL)) != EOF)
		switch (ch) {
		case 'b':
			simulatedopcount = getnum(optarg, &endp);
			if (!quiet)
				prt("Will begin at operation %ld\n", simulatedopcount);
			if (simulatedopcount == 0)
				usage();
			simulatedopcount -= 1;
			break;
		case 'c':
			closeprob = getnum(optarg, &endp);
			if (!quiet)
				prt("Chance of close/open is 1 in %d\n", closeprob);
			if (closeprob <= 0)
				usage();
			break;
		case 'd':
			debug = 1;
			break;
		case 'f':
			flush = 1;
			break;
		case 'g':
			filldata = *optarg;
			break;
		case 'i':
			integrity = 1;
			logdev = strdup(optarg);
			if (!logdev) {
				prterr("strdup");
				exit(101);
			}
			break;
		case 'j':
			logid = strdup(optarg);
			if (!logid) {
				prterr("strdup");
				exit(101);
			}
			break;
		case 'k':
			o_flags &= ~O_TRUNC;
			break;
		case 'l':
			maxfilelen = getnum(optarg, &endp);
			if (maxfilelen <= 0)
				usage();
			break;
		case 'm':
			monitorstart = getnum(optarg, &endp);
			if (monitorstart < 0)
				usage();
			if (!endp || *endp++ != ':')
				usage();
			monitorend = getnum(endp, &endp);
			if (monitorend < 0)
				usage();
			if (monitorend == 0)
				monitorend = -1; /* aka infinity */
			debug = 1;
		case 'n':
			sizechecks = 0;
			break;
		case 'o':
			maxoplen = getnum(optarg, &endp);
			if (maxoplen <= 0)
				usage();
			break;
		case 'p':
			progressinterval = getnum(optarg, &endp);
			if (progressinterval == 0)
				usage();
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			readbdy = getnum(optarg, &endp);
			if (readbdy <= 0)
				usage();
			break;
		case 's':
			style = getnum(optarg, &endp);
			if (style < 0 || style > 1)
				usage();
			break;
		case 't':
			truncbdy = getnum(optarg, &endp);
			if (truncbdy <= 0)
				usage();
			break;
		case 'w':
			writebdy = getnum(optarg, &endp);
			if (writebdy <= 0)
				usage();
			break;
		case 'x':
			prealloc = 1;
			break;
		case 'y':
			do_fsync = 1;
			break;
		case 'A':
		        aio = 1;
			break;
		case 'D':
			debugstart = getnum(optarg, &endp);
			if (debugstart < 1)
				usage();
			break;
		case 'F':
			fallocate_calls = 0;
			break;
		case 'K':
			keep_size_calls = 0;
			break;
		case 'H':
			punch_hole_calls = 0;
			break;
		case 'z':
			zero_range_calls = 0;
			break;
		case 'C':
			collapse_range_calls = 0;
			break;
		case 'I':
			insert_range_calls = 0;
			break;
		case 'L':
		        lite = 1;
			o_flags &= ~(O_CREAT|O_TRUNC);
			break;
		case 'N':
			numops = getnum(optarg, &endp);
			if (numops < 0)
				usage();
			break;
		case 'O':
			randomoplen = 0;
			break;
		case 'P':
			strncpy(dname, optarg, sizeof(dname));
			strcat(dname, "/");
			dirpath = strlen(dname);
			break;
                case 'R':
                        mapped_reads = 0;
                        break;
		case 'S':
                        seed = getnum(optarg, &endp);
			if (seed == 0) {
				seed = time(0) % 10000;
				seed += (int)getpid();
			}
			if (!quiet)
				prt("Seed set to %d\n", seed);
			if (seed < 0)
				usage();
			break;
		case 'W':
		        mapped_writes = 0;
			if (!quiet)
				prt("mapped writes DISABLED\n");
			break;
		case 'Z':
			o_direct = O_DIRECT;
			o_flags |= O_DIRECT;
			break;
		case 255:  /* --record-ops */
			if (optarg)
				strncpy(opsfile, optarg, sizeof(opsfile));
			recordops = opsfile;
			break;
		case 256:  /* --replay-ops */
			replayops = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		usage();

	if (integrity && !dirpath) {
		fprintf(stderr, "option -i <logdev> requires -P <dirpath>\n");
		usage();
	}

	fname = argv[0];
	tmp = strdup(fname);
	if (!tmp) {
		prterr("strdup");
		exit(101);
	}
	bname = basename(tmp);

	signal(SIGHUP,	cleanup);
	signal(SIGINT,	cleanup);
	signal(SIGPIPE,	cleanup);
	signal(SIGALRM,	cleanup);
	signal(SIGTERM,	cleanup);
	signal(SIGXCPU,	cleanup);
	signal(SIGXFSZ,	cleanup);
	signal(SIGVTALRM,	cleanup);
	signal(SIGUSR1,	cleanup);
	signal(SIGUSR2,	cleanup);

	srandom(seed);
	fd = open(fname, o_flags, 0666);
	if (fd < 0) {
		prterr(fname);
		exit(91);
	}
	if (fstat(fd, &statbuf)) {
		prterr("check_size: fstat");
		exit(91);
	}
	block_size = statbuf.st_blksize;
#ifdef XFS
	if (prealloc) {
		xfs_flock64_t	resv = { 0 };
#ifdef HAVE_XFS_PLATFORM_DEFS_H
		if (!platform_test_xfs_fd(fd)) {
			prterr(fname);
			fprintf(stderr, "main: cannot prealloc, non XFS\n");
			exit(96);
		}
#endif
		resv.l_len = maxfilelen;
		if ((xfsctl(fname, fd, XFS_IOC_RESVSP, &resv)) < 0) {
			prterr(fname);
			exit(97);
		}
	}
#endif

	if (dirpath) {
		snprintf(goodfile, sizeof(goodfile), "%s%s.fsxgood", dname, bname);
		snprintf(logfile, sizeof(logfile), "%s%s.fsxlog", dname, bname);
		if (!*opsfile)
			snprintf(opsfile, sizeof(opsfile), "%s%s.fsxops", dname, bname);
	} else {
		snprintf(goodfile, sizeof(goodfile), "%s.fsxgood", fname);
		snprintf(logfile, sizeof(logfile), "%s.fsxlog", fname);
		if (!*opsfile)
			snprintf(opsfile, sizeof(opsfile), "%s.fsxops", fname);
	}
	fsxgoodfd = open(goodfile, O_RDWR|O_CREAT|O_TRUNC, 0666);
	if (fsxgoodfd < 0) {
		prterr(goodfile);
		exit(92);
	}
	fsxlogf = fopen(logfile, "w");
	if (fsxlogf == NULL) {
		prterr(logfile);
		exit(93);
	}
	unlink(opsfile);

	if (replayops) {
		replayopsf = fopen(replayops, "r");
		if (!replayopsf) {
			prterr(replayops);
			exit(93);
		}
	}

#ifdef AIO
	if (aio) 
		aio_setup();
#endif

	if (!(o_flags & O_TRUNC)) {
		off_t ret;
		file_size = maxfilelen = biggest = lseek(fd, (off_t)0, SEEK_END);
		if (file_size == (off_t)-1) {
			prterr(fname);
			warn("main: lseek eof");
			exit(94);
		}
		ret = lseek(fd, (off_t)0, SEEK_SET);
		if (ret == (off_t)-1) {
			prterr(fname);
			warn("main: lseek 0");
			exit(95);
		}
	}
	original_buf = (char *) malloc(maxfilelen);
	for (i = 0; i < maxfilelen; i++)
		original_buf[i] = random() % 256;
	good_buf = (char *) malloc(maxfilelen + writebdy);
	good_buf = round_ptr_up(good_buf, writebdy, 0);
	memset(good_buf, '\0', maxfilelen);
	temp_buf = (char *) malloc(maxoplen + readbdy);
	temp_buf = round_ptr_up(temp_buf, readbdy, 0);
	memset(temp_buf, '\0', maxoplen);
	if (lite) {	/* zero entire existing file */
		ssize_t written;

		written = write(fd, good_buf, (size_t)maxfilelen);
		if (written != maxfilelen) {
			if (written == -1) {
				prterr(fname);
				warn("main: error on write");
			} else
				warn("main: short write, 0x%x bytes instead "
					"of 0x%lx\n",
					(unsigned)written,
					maxfilelen);
			exit(98);
		}
	} else {
		ssize_t ret, len = file_size;
		off_t off = 0;

		while (len > 0) {
			ret = read(fd, good_buf + off, len);
			if (ret == -1) {
				prterr(fname);
				warn("main: error on read");
				exit(98);
			}
			len -= ret;
			off += ret;
		}

		check_trunc_hack();
	}

	if (fallocate_calls)
		fallocate_calls = test_fallocate(0);
	if (keep_size_calls)
		keep_size_calls = test_fallocate(FALLOC_FL_KEEP_SIZE);
	if (punch_hole_calls)
		punch_hole_calls = test_fallocate(FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE);
	if (zero_range_calls)
		zero_range_calls = test_fallocate(FALLOC_FL_ZERO_RANGE);
	if (collapse_range_calls)
		collapse_range_calls = test_fallocate(FALLOC_FL_COLLAPSE_RANGE);
	if (insert_range_calls)
		insert_range_calls = test_fallocate(FALLOC_FL_INSERT_RANGE);

	while (numops == -1 || numops--)
		if (!test())
			break;

	free(tmp);
	if (close(fd)) {
		prterr("close");
		report_failure(99);
	}
	prt("All %lu operations completed A-OK!\n", testcalls);
	if (recordops)
		logdump();

	exit(0);
	return 0;
}
