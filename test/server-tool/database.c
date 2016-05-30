#include "database.h"

update_db_t *db_init(void)
{
	update_db_t *db = NULL;

	if (NULL == (db = malloc(sizeof(*db))))
		return NULL;
	memset(db, 0, sizeof(*db));

	return db;
}

void db_destroy(update_db_t *db)
{
	if (NULL == db)
		return;
	if (NULL != db->packlist)
		db_packlist_destroy(db->packlist);
	if (NULL != db->summary)
		free(db->summary);
	if (NULL != db->desc)
		free(db->desc);
	free(db);
}

inline int db_set_id(update_db_t *db, long id)
{
	db->id = id;
	return 0;
}

inline long db_get_id(update_db_t *db)
{
	return db->id;
}

inline int db_set_type(update_db_t *db, const char *type)
{
	strncpy(db->type, type, DB_TYPE_LEN);
	return 0;
}

inline int db_get_type(update_db_t *db, char *type)
{
	strncpy(type, db->type, DB_TYPE_LEN);
	return 0;
}

inline int db_set_arch(update_db_t *db, const char *arch)
{
	strncpy(db->arch, arch, DB_ARCH_LEN);
	return 0;
}

inline int db_get_arch(update_db_t *db, char *arch)
{
	strncpy(arch, db->arch, DB_ARCH_LEN);
	return 0;
}

inline int db_set_date(update_db_t *db, long date)
{
	db->date = date;
	return 0;
}

inline long db_get_date(update_db_t *db)
{
	return db->date;
}

inline int db_set_osversion(update_db_t *db, const char *osversion)
{
	strncpy(db->osversion, osversion, DB_OSVER_LEN);
	return 0;
}

inline int db_get_osversion(update_db_t *db, char *osversion)
{
	strncpy(osversion, db->osversion, DB_OSVER_LEN);
	return 0;
}

int db_set_summary(update_db_t *db, const char *summary)
{
	int len;

	len = strlen(summary) + 1;
	len = len > DB_SUMMARY_LEN ? DB_SUMMARY_LEN : len;
	if (NULL == (db->summary = malloc(len)))
		return -1;
	strncpy(db->summary, summary, len);

	return 0;
}

int db_get_summary(
		update_db_t *db, 
		int callback(void *arg, const char *lang, const char *content),
		void *arg
		)
{
	char *summary = NULL;
	const char *lang = NULL;
	const char *content = NULL;
	char *p = NULL;

	if (NULL == db->summary)
		return -1;

	if (NULL == (summary = strdup(db->summary)))
		return -1;
	
	if (NULL != (p = utils_split_string(summary, DB_SUMMARY_NEEDLE1))) {
		lang = strtok(p, DB_SUMMARY_NEEDLE2);
		content = strtok(NULL, DB_SUMMARY_NEEDLE2);
		if (0 != callback(arg, lang, content)) {
			free(summary);
			return -1;
		}
	}
	
	while (NULL != (p = utils_split_string(NULL, DB_SUMMARY_NEEDLE1))) {
		lang = strtok(p, DB_SUMMARY_NEEDLE2);
		content = strtok(NULL, DB_SUMMARY_NEEDLE2);
		if (0 != callback(arg, lang, content)) {
			free(summary);
			return -1;
		}
	}
	
	free(summary);
	return 0;
}

int db_set_description(update_db_t *db, const char *description)
{
	int len;

	len = strlen(description) + 1;
	len = len > DB_DESC_LEN ? DB_DESC_LEN : len;
	if (NULL == (db->desc = malloc(len)))
		return -1;
	strncpy(db->desc, description, len);

	return 0;
}

int db_get_description(
		update_db_t *db, 
		int callback(void *arg, const char *lang, const char *content),
		void *arg)
{
	char *desc = NULL;
	const char *lang = NULL;
	const char *content = NULL;
	char *p = NULL;

	if (NULL == db->desc)
		return -1;

	if (NULL == (desc = strdup(db->desc)))
		return -1;
	
	if (NULL != (p = utils_split_string(desc, DB_DESC_NEEDLE1))) {
		lang = strtok(p, DB_DESC_NEEDLE2);
		content = strtok(NULL, DB_DESC_NEEDLE2);
		if (0 != callback(arg, lang, content)) {
			free(desc);
			return -1;
		}
	}
	
	while (NULL != (p = utils_split_string(NULL, DB_DESC_NEEDLE1))) {
		lang = strtok(p, DB_DESC_NEEDLE2);
		content = strtok(NULL, DB_DESC_NEEDLE2);
		if (0 != callback(arg, lang, content)) {
			free(desc);
			return -1;
		}
	}

	return 0;
}

inline int db_set_status_prescript(update_db_t *db, bool statu)
{
	db->status.prescript = statu ? true : false;
	return 0;
}

inline bool db_get_status_prescript(update_db_t *db)
{
	return db->status.prescript;
}

inline int db_set_status_postscript(update_db_t *db, bool statu)
{
	db->status.postscript = statu ? true : false;
	return 0;
}

inline bool db_get_status_postscript(update_db_t *db)
{
	return db->status.postscript;
}

inline int db_set_status_reboot(update_db_t *db, bool statu)
{
	db->status.reboot = statu ? true : false;
	return 0;
}

inline bool db_get_status_reboot(update_db_t *db)
{
	return db->status.reboot;
}

inline int db_set_packages(update_db_t *db, db_packlist_t *packlist)
{
	if (NULL == db || NULL == packlist)
		return -1;

	db->packlist = packlist;
	return 0;
}

inline int db_get_packages(
		update_db_t *db,
		int callback(
			void *arg,
			const struct db_package *package),
		void *arg)
{
	db_packlist_get_all(db->packlist, callback, arg);
	return 0;
}

update_db_t *db_create(const char *path)
{
	update_db_t *db = NULL;
	long id;
	char *buff = NULL;
	char pathbuf[1024];

	if (NULL == (buff = malloc(CONFIG_KEYVAL_MAX_LEN)))
		return NULL;


	if (NULL == (db = db_init())) {
		free(buff);
		return NULL;
	}

	sprintf(pathbuf, "%s/%s", path, CONFIG_CONF_FILE);
	/* set id */
	if (-1 == (id = config_get_id(pathbuf)))
		goto Error_Handler;
	db_set_id(db, id);

	/* set date */
	db_set_date(db, time(NULL));

	/* set type */
	if (-1 == config_get_type(pathbuf, buff))
		goto Error_Handler;
	db_set_type(db, buff);

	/* set arch */
	if (-1 == config_get_arch(pathbuf, buff))
		goto Error_Handler;
	db_set_arch(db, buff);

	/* set osversion */
	if (-1 == config_get_osversion(pathbuf, buff))
		goto Error_Handler;
	db_set_osversion(db, buff);

	/* set summary */
	if (-1 == config_get_summary(pathbuf, buff))
		goto Error_Handler;
	db_set_summary(db, buff);

	/* set description */
	if (-1 == config_get_description(pathbuf, buff))
		goto Error_Handler;
	db_set_description(db, buff);


	/* set status */
	db_set_status_reboot(db, 
			config_get_status_reboot(pathbuf));

	sprintf(pathbuf, "%s/%s", path, CONFIG_TOP_DIR);

	db_set_status_prescript(db, 
			config_get_status_prescript(pathbuf));
	db_set_status_postscript(db, 
			config_get_status_postscript(pathbuf));

	/* set packages */
	sprintf(pathbuf, "%s/%s", path, CONFIG_PACK_DIR);
	db_set_packages(db, config_get_packages(pathbuf));

	free(buff);

	return db;

Error_Handler:
	db_destroy(db);
	free(buff);
	return NULL;
}

void _db_debug(update_db_t *db)
{
	db_packlist_t *p = NULL;
	char md5str[MD5_STR_LEN];
	
	printf("Id:\t%ld\n", db->id);
	printf("Type:\t%s\n", db->type);
	printf("Arch:\t%s\n", db->arch);
	printf("Date:\t%ld\n", db->date);
	printf("Osver:\t%s\n", db->osversion);
	printf("Summary:\t%s\n", db->summary);
	printf("Desc:\t%s\n", db->desc);
	puts("Packages:");
	p = db->packlist->next;
	while (NULL != p) {
		putchar('\t');
		md5_num2str(p->package.md5, md5str);
		printf("%s", md5str);
		printf("  ->  ");
		puts(p->package.name);
		p = p->next;
	}
}

