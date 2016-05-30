#ifndef __ARCHIVE_H__
#define __ARCHIVE_H__

#include <libtar.h>
#include <lzma.h>

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "utils.h"

#define ARCHIVE_SMEAR_MAGIC	'A'

extern int tar_verbose;
extern int xz_verbose;
extern int archive_verbose;

extern int xz_compress(const char  *infile, const char *outfile);
extern int xz_decompress(const char *infile, const char *outfile);
extern int tar_decompress(const char *tarfile, const char *outdir);
extern int archive_pack(const char *infile, const char *tarfile);
extern int archive_unpack(const char *tarfile, const char *outdir);

#endif	/* __ARCHIVE_H__ */
