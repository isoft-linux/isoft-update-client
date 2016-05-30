#ifndef __MD5_H__
#define __MD5_H__

/* -----------------  md5 core header ----------- */
#include <sys/types.h>

typedef u_int32_t uint32;

struct MD5Context {
	uint32 buf[4];
	uint32 bits[2];
	unsigned char in[64];
	int doByteReverse;
};

void MD5_Init(struct MD5Context *);
void MD5_Update(struct MD5Context *, unsigned const char *, unsigned);
void MD5_Final(unsigned char digest[16], struct MD5Context *);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */

typedef struct MD5Context MD5_CTX;


/* ------------ md5 wraper header ------------------ */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define MD5_NUM_LEN	16
#define MD5_STR_LEN	40

extern void md5_num2str(const unsigned char *md5num, char *md5str);
extern int md5_str2num(unsigned char *md5num, const char *md5str);
extern int md5_from_file(const char *fname, unsigned char *md5num);
extern int md5_from_str(const char *str, unsigned char *md5num);

#endif	/* __MD5_H__ */
