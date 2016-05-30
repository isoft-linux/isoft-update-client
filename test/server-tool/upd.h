#ifndef __UPDATE_H__
#define __UPDATE_H__

#include <stdlib.h>
#include <strings.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#define UPD_TYPE_LEN		64
#define UPD_ARCH_LEN		64
//#define UPD_DATE_LEN		32
#define UPD_OSVER_LEN		128
#define UPD_SUMMARY_LEN		1024
#define UPD_DESC_ITEM_LEN	1024
#define UPD_HAVE_SCRIPT_YES	1
#define UPD_HAVE_SCRIPT_NO	0
#define UPD_PACK_FNAME_LEN	128
#define UPD_PACK_MD5_LEN	36

#define UPD_SUMMARY_NEEDLE1	"###"
#define UPD_SUMMARY_NEEDLE2	"@"
#define UPD_DESC_NEEDLE1	"###"
#define UPD_DESC_NEEDLE2	"@"
#define UPD_DESC_NEEDLE3	"&&&"

/*------------- summary ----------------------*/
/*format:
 * lang@content###lang@content###
 * ....
 * lang@content###lang@content###\0
 * */

/*---------------- description ------------------- */
/* format:
 * lang@item&&&item&&&item&&& ...###
 * lang@item&&&item&&&item&&& ...###
 * ....
 * lang@item&&&item&&&item&&& ...###\0
 * */
struct update_package {
	char name[UPD_PACK_FNAME_LEN];
	char md5[UPD_PACK_MD5_LEN];
};

struct update {
	long id;
	char type[UPD_TYPE_LEN];
	char arch[UPD_ARCH_LEN];
	long date;
	char osversion[UPD_OSVER_LEN];
	char *summary; /* all of the data must be in heap ! */
	char *desc; /* all of the data must be in heap ! */
	struct status {
		char prescript:1;
		char postscript:1;
		char reboot:1;
	}status;
	short packnum; 
	struct update_package *packages; /* all of the data must be in heap ! */
};

typedef struct update update_t;

#define updstc_create() (\
{		\
	int _m;	\
	update_t *update = NULL;	\
	_m = sizeof(update_t);	\
	update = malloc(_m);	\
	update ? (bzero(update, _m), update) : NULL;		\
})

#define updstc_destroy(update) (	\
{	\
	update_t *_p  = update;	\
	NULL == _p ? 0 : (\
		NULL == _p->summary ? 0 : free(_p->summary),	\
		NULL == _p->desc ? 0 : free(_p->desc), 	\
		NULL == _p->packages ? 0 : free(_p->packages) 	\
	);	\
	free(_p);		\
})

extern int updinfo_create(struct update *data);

#endif	/* __UPDATE_H__ */
