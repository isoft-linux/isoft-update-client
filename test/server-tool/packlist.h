#ifndef __PACKLIST_H__
#define __PACKLIST_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "md5.h"

#define DB_PACK_FNAME_LEN	256
#define DB_PACK_MD5_LEN		MD5_NUM_LEN

struct db_package {
	char name[DB_PACK_FNAME_LEN];
	unsigned char md5[DB_PACK_MD5_LEN];
};
typedef struct db_package db_pack_t;

struct db_package_list {
	db_pack_t package;
	struct db_package_list *next;
};
typedef struct db_package_list db_packlist_t;

extern struct db_package_list *db_packlist_init(void);
extern void db_packlist_destroy(struct db_package_list *list);
extern int db_packlist_add(struct db_package_list *list, const struct db_package *pack);
extern int db_packlist_del(struct db_package_list *list, const char *packname);
extern int db_packlist_search(struct db_package_list *list, struct db_package *pack);
extern int db_packlist_get_number(struct db_package_list *list);
extern int db_packlist_get_all(
		struct db_package_list *list,
		int callback(
			void *arg,
			const struct db_package *package
			),
		void *arg
		);


#endif	/* __PACKLIST_H__ */
