#ifndef __DATABASE_H__
#define __DATABASE_H__

#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "packlist.h"
#include "utils.h"
#include "config.h"

#define DB_TYPE_LEN		CONFIG_TYPE_LEN
#define DB_ARCH_LEN		CONFIG_ARCH_LEN
#define DB_OSVER_LEN		CONFIG_OSVER_LEN
#define DB_SUMMARY_LEN		CONFIG_SUMMARY_LEN	/* 64k */
#define DB_SUMMARY_NEEDLE1	CONFIG_SUMMARY_OUT_NEEDLE1
#define DB_SUMMARY_NEEDLE2	CONFIG_SUMMARY_OUT_NEEDLE2
#define DB_DESC_LEN		CONFIG_KEYVAL_MAX_LEN	/* 256k */
#define DB_DESC_NEEDLE1		CONFIG_DESC_OUT_NEEDLE1
#define DB_DESC_NEEDLE2		CONFIG_DESC_OUT_NEEDLE2
#define DB_DESC_NEEDLE3		CONFIG_DESC_OUT_NEEDLE3


/*------------- summary ----------------------*/
/*format:
 * lang@content###lang@content###
 * ....
 * lang@content###lang@content###\0
 * */

/*---------------- description ------------------- */
/* format:
 * lang@item\nitem\nitem\n ...###
 * lang@item\nitem\nitem\n ...###
 * ....
 * lang@item\nitem\nitem\n ...###\0
 * */


struct update_database {
	long id;
	char type[DB_TYPE_LEN];
	char arch[DB_ARCH_LEN];
	long date;
	char osversion[DB_OSVER_LEN];
	char *summary; /* all of the data must be in heap ! */
	char *desc; /* all of the data must be in heap ! */
	struct status {
		char prescript:1;
		char postscript:1;
		char reboot:1;
	}status;
	struct db_package_list *packlist;
};

typedef struct update_database update_db_t;

extern update_db_t *db_create(const char *path);
extern void db_destroy(update_db_t *db);

extern int db_set_id(update_db_t *db, long id);
extern long db_get_id(update_db_t *db);

extern int db_set_type(update_db_t *db, const char *type);
extern int db_get_type(update_db_t *db, char *type);

extern int db_set_arch(update_db_t *db, const char *arch);
extern int db_get_arch(update_db_t *db, char *arch);

extern int db_set_date(update_db_t *db, long date);
extern long db_get_date(update_db_t *db);

extern int db_set_osversion(update_db_t *db, const char *osversion);
extern int db_get_osversion(update_db_t *db, char *osversion);

extern int db_set_summary(update_db_t *db, const char *summary);
extern int db_get_summary(
		update_db_t *db, 
		int callback(void *arg, const char *lang, const char *content),
		void *arg
		);

extern int db_set_description(update_db_t *db, const char *description);
extern int db_get_description(
		update_db_t *db, 
		int callback(void *arg, const char *lang, const char *content),
		void *arg);

extern int db_set_packages(update_db_t *db, db_packlist_t *packlist);
extern int db_get_packages(
		update_db_t *db,
		int callback(
			void *arg,
			const struct db_package *package),
		void *arg);


extern int db_set_status_prescript(update_db_t *db, bool statu);
extern bool db_get_status_prescript(update_db_t *db);
extern int db_set_status_postscript(update_db_t *db, bool statu);
extern bool db_get_status_postscript(update_db_t *db);
extern int db_set_status_reboot(update_db_t *db, bool statu);
extern bool db_get_status_reboot(update_db_t *db);


#endif	/* __DATABASE_H__ */
