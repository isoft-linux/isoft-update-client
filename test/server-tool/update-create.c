#include "update-create.h"

static int create_upt_name(update_db_t *db, char *fname)
{
	struct tm *tmbuf = NULL;
	time_t tm_sec;
	char date[128];
	long id;
	char arch[DB_ARCH_LEN];

	/* date */
	tm_sec = db_get_date(db);
	tmbuf = localtime(&tm_sec);
	snprintf(date, 128, "%d-%02d-%02d", tmbuf->tm_year + 1900,
			tmbuf->tm_mon + 1, tmbuf->tm_mday);

	/* id */
	id = db_get_id(db);

	/* arch */
	db_get_arch(db, arch);
	
	sprintf(fname, "update-%s-%ld-%s.upt", date, id, arch);

	return 0;
}

int update_create(const char *path, const char *xsdfile)
{
	update_db_t *db = NULL;
	char filename[256];
	char buff[1024];
	char *cwd = NULL;

	/* backup the current work dirctory before we change dir */
	cwd = getcwd(NULL, 0);

	if (-1 == chdir(path))
		error_exit(path);

	/* create database */
	if (NULL == (db = db_create("."))) {
		puts("create database ... ... [\e[1;31mFALSE\e[0m]");
		return -1;
	}
	puts("create database ... ... [\e[1;32mOK\e[0m]");


	/* create bill */
	if (-1 == bill_create(db, CONFIG_BILL_FILE)) {
		puts("create update.xml ... ... [\e[1;31mFALSE\e[0m]");
		return -1;
	}
	puts("create update.xml ... ... [\e[1;32mOK\e[0m]");

	/* validate xsd */
	sprintf(buff, "%s/%s", cwd, xsdfile);
	if (-1 == bill_validate_xsd(CONFIG_BILL_FILE, buff)) {
		fprintf(stderr, "validate xsd ... ... [\e[1;31mFALSE\e[0m]\n");
		return -1;
	}
	puts("validate xsd ... ... [\e[1;32mOK\e[0m]");

	/* create archive name */
	create_upt_name(db, filename);

	/* create archive */
	archive_verbose = 1;
	if (-1 == archive_pack(CONFIG_TOP_DIR, filename))
		return -1;

	/* create md5file of  update package */
	utils_create_md5file(filename);
	puts("create md5 file ... ... [\e[1;32mOK\e[0m]");

	/* finished */
	db_destroy(db);
	
	/* recover the previous wd */
	if (-1 == chdir(cwd))
		error_exit("chdir");
	free(cwd);

	return 0;
}
