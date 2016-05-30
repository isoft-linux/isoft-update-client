#include "config.h"

const char *config_type[] = {
	"update",
	"bugfix",
	"security",
	NULL,
};

const char *config_arch[] = {
	"x86",
	"x86_64",
	"loongson",
	"sw",
	"mips",
	"alpha",
	"ppc",
	"arm",
	NULL,
};

const char *config_lang[] = {
	"en",
	"zh",
	NULL,
};

const char *config_status[] = {
	"false",
	"true",
	NULL,
};


char *config_get_key_value(const char *file, const char *key)
{
	char *p = NULL;
	char *p1 = NULL;
	static char buff[CONFIG_KEYVAL_MAX_LEN];
	FILE *fp = NULL;
	
	if (NULL == (fp = fopen(file, "r")))
		error_exit(file);
	
	rewind(fp);
	while (NULL != (p = fgets(buff, CONFIG_KEYVAL_MAX_LEN, fp))) {
		p1 = strtok(buff, "=");
		if (!strcmp(p1, key)) {
			p1 = strtok(NULL, "=");
			p1[strlen(p1)-1] = '\0';
			fclose(fp);
			return p1;
		}
	}

	fclose(fp);

	return NULL;
}


long config_get_id(const char *file)
{
	char *value = NULL;
	
	printf("config_get_id: %s\n", file);
	if (NULL == (value = config_get_key_value(file, "Id")))
		return -1;

	if (!utils_check_number(value))
		return -1;

	return atoi(value);
}

int config_get_type(const char *file, char *type)
{
	char *value = NULL;
	
	if (NULL == (value = config_get_key_value(file, "Type"))) {
		strncpy(type, config_type[0], CONFIG_TYPE_LEN);
		return 0;
	}

	/* we have xsd to check validate */
#if 0
	int i;
	int flag = 0;
	/* chech validate */
	for (i = 0; NULL != config_type[i]; i++) {
		if (!strcmp(value, config_type[i]))
			flag = 1;
	}
	if (0 == flag)
		return -1;
#endif

	strncpy(type, value, CONFIG_TYPE_LEN);
	return 0;
}

int config_get_arch(const char *file, char *arch)
{
	char *value = NULL;
	
	if (NULL == (value = config_get_key_value(file, "Arch"))) {
		strncpy(arch, config_arch[0], CONFIG_ARCH_LEN);
		return 0;
	}

	/* we have xsd to check validate */
#if 0
	int i;
	int flag = 0;
	/* chech validate */
	for (i = 0; NULL != config_arch[i]; i++) {
		if (!strcmp(value, config_arch[i]))
			flag = 1;
	}
	if (0 == flag)
		return -1;
#endif

	strncpy(arch, value, CONFIG_ARCH_LEN);
	return 0;
}


int config_get_osversion(const char *file, char *osversion)
{
	char *value = NULL;
	
	if (NULL == (value = config_get_key_value(file, "OsVersion"))) {
		return -1;
	}
	
	strncpy(osversion, value, CONFIG_OSVER_LEN);
	return 0;
}

int config_get_summary(const char *file, char *summary)
{
	char *value = NULL;
	char key[128];
	int i;
	int n;
	char *p = NULL;

	summary[0] = '\0';
	p = summary;
	
	for (i = 0; NULL != config_lang[i]; i++) {
		sprintf(key, "Summary_%s", config_lang[i]);
		if (NULL == (value = config_get_key_value(file, key)))
			continue;

		n = snprintf(p, summary + CONFIG_SUMMARY_LEN - p, "%s%s%s%s",
				config_lang[i], 
				CONFIG_SUMMARY_OUT_NEEDLE2, 
				value, 
				CONFIG_SUMMARY_OUT_NEEDLE1);
		p += n;
		if (p - summary >= CONFIG_SUMMARY_LEN)
			return -1;
	}
	return 0;
}

int config_get_description(const char *file, char *desc)
{
/*	"en@fix some very inmportant bug.&update some software.&optimize system feature###\
zh@修复一些漏洞&更新部分软件。&&&优化了系统特性"; */
	
	char *value = NULL;
	char key[128];
	int i;
	int n;
	char *p = NULL;

	desc[0] = '\0';
	p = desc;
	
	for (i = 0; NULL != config_lang[i]; i++) {
		sprintf(key, "Description_%s", config_lang[i]);
		if (NULL == (value = config_get_key_value(file, key)))
			continue;
		
		n = snprintf(p, desc + CONFIG_DESC_LEN - p, "%s%s%s%s",
				config_lang[i], 
				CONFIG_DESC_OUT_NEEDLE2, 
				value, 
				CONFIG_DESC_OUT_NEEDLE1);
		p += n;
		if (p - desc >= CONFIG_DESC_LEN)
			return -1;
	}

	return 0;	
}

bool config_get_status_reboot(const char *file)
{
	char *value = NULL;
	
	if (NULL == (value = config_get_key_value(file, "Reboot")))
		return false;	/* the default if false */

	if (!strcmp(config_status[0], value))
		return false;
	else if (!strcmp(config_status[1], value))
		return true;
	else
		return false;	/* invalid key value */
}

inline bool config_get_status_prescript(const char *dir)
{
	return utils_find_file(dir, CONFIG_SCRIPT_PRE);	
}

inline bool config_get_status_postscript(const char *path)
{
	return utils_find_file(path, CONFIG_SCRIPT_POST);	
}

struct db_package_list *config_get_packages(const char *path)
{
	DIR *dp = NULL;
	struct dirent *entry = NULL;
	const char *suffix = NULL;
	struct db_package_list *list = NULL;
	db_pack_t pack;
	char *cwd = NULL;

	/* backup the current work dirctory before we change dir */
	cwd = getcwd(NULL, 0);

	if (-1 == chdir(path))
		error_exit("chdir");

	if (NULL == (dp = opendir(".")))
		error_exit("opendir");

	list = db_packlist_init();
	
	while (NULL != (entry = readdir(dp))) {
		suffix = entry->d_name + strlen(entry->d_name) - strlen(".rpm");
		if (strcmp(suffix, ".rpm"))
			continue;

		strncpy(pack.name, entry->d_name, DB_PACK_FNAME_LEN);
		md5_from_file(entry->d_name, pack.md5);
		db_packlist_add(list, &pack);
	}


	/* recover the previous wd */
	if (-1 == chdir(cwd))
		error_exit("chdir");
	free(cwd);

	return list;
}
