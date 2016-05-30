#include "env.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include "md5.h"


int env_create(const char *path)
{
	FILE *fp = NULL;
	char *str = NULL;
	
	if (-1 == mkdir(path, 0755) && EEXIST != errno)
		error_exit("mkdir");
	if (-1 == access(path, W_OK)) 
		error_exit("access");
	if (-1 == chdir(path))
		error_exit("chdir");
	if (-1 == mkdir(ENV_TOP_DIR, 0755))
		error_exit("mkdir");
	if (-1 == chdir(ENV_TOP_DIR))
		error_exit("chdir");
	if (-1 == mkdir(ENV_PACK_DIR, 0755))
		error_exit("mkdir");
	if (-1 == chdir(".."))
		error_exit("chdir");

	if (NULL == (fp = fopen(ENV_CONF_FILE, "w")))
		error_exit("fopen");

	str = 
"#本次更新的ID号。\n\
Id=00001\n\n\
#更新类型，可选参数有：bugfix/update/security三种。\n\
Type=bugfix\n\n\
#本包适用的架构体系，可选参数有：x86_64/x86/loongson/sw/mips/alpha/ppc/arm等。\n\
Arch=x86\n\n\
#版本号。\n\
OsVersion=v4.0\n\n\
#本次更新的英文简介。\n\
Summary_en=this is summary for update.\n\n\
#本次更新的中文简介。\n\
Summary_zh=这是关于本次更新的综述。\n\n\
#本次更新概要，英文描述；每条描述之间必须用'&'隔开！\n\
Description_en=fix some very inmportant bug.&update some software.&optimize system feature.\n\n\
#本次更新概要，中文描述；每条描述之间必须用'&'隔开！\n\
Description_zh=修复一些漏洞&更新部分软件。&优化了系统特性。\n\n\
#本次更新完是否需要重启，可选参数有：true/false。\n\
Reboot=true\n";
	fputs(str, fp);
	fclose(fp);

	return 0;
}

static char *get_keyvalue_from_file(const char *file, const char *key)
{
	char *p = NULL;
	char *p1 = NULL;
	static char buff[4096];
	FILE *fp = NULL;
	
	if (NULL == (fp = fopen(file, "r")))
		error_exit("fopen");
	
	rewind(fp);
	while (NULL != (p = fgets(buff, 4096, fp))) {
		p1 = strtok(buff, "=");
		if (!strcmp(p1, key)) {
			p1 = strtok(NULL, "=");
			p1[strlen(p1)-1] = '\0';
			return p1;
		}
	}
	return NULL;
}

static int find_file_from_dir(const char *path, const char *name)
{
	DIR *dp = NULL;
	struct dirent *entry = NULL;
	int ret = 0;
	
	if (NULL == (dp = opendir(path)))
		error_exit("opendir");
	
	ret = 0;
	while (NULL != (entry = readdir(dp))) {
		if (!strcmp(entry->d_name, name)) {
			ret = 1;
			break;
		}
	}
	closedir(dp);
	
	return ret;
}


static int env_get_id(const char *file, update_t *update)
{
	char *value = NULL;
	
	if (NULL == (value = get_keyvalue_from_file(file, "Id")))
		update->id = 0;
	else
		update->id = atoi(value);
	return 0;	
}

static int env_get_type(const char *file, update_t *update)
{
	char *value = NULL;
	
	if (NULL == (value = get_keyvalue_from_file(file, "Type")))
		strncpy(update->type, "bugfix", UPD_TYPE_LEN);
	else {
		strncpy(update->type, value, UPD_TYPE_LEN);
	}	
	return 0;
}

static int env_get_arch(const char *file, update_t *update)
{
	char *value = NULL;
	
	if (NULL == (value = get_keyvalue_from_file(file, "Arch")))
		strncpy(update->arch, "x86", UPD_ARCH_LEN);
	else
		strncpy(update->arch, value, UPD_ARCH_LEN);
	return 0;
}

static int env_get_osversion(const char *file, update_t *update)
{
	char *value = NULL;
	
	if (NULL == (value = get_keyvalue_from_file(file, "OsVersion")))
		strncpy(update->osversion, "v1.0", UPD_OSVER_LEN);
	else
		strncpy(update->osversion, value, UPD_OSVER_LEN);
	return 0;
}

static int env_get_summary(const char *file, update_t *update)
{
	char *value = NULL;
	
	update->summary = malloc(UPD_SUMMARY_LEN);
	update->summary[0] = '\0';
	
	if (NULL != (value = get_keyvalue_from_file(file, "Summary_en")))
		snprintf(update->summary, UPD_SUMMARY_LEN, 
				"en%s%s%s", UPD_SUMMARY_NEEDLE2, value, UPD_SUMMARY_NEEDLE1);

	if (NULL != (value = get_keyvalue_from_file(file, "Summary_zh")))
		snprintf(update->summary + strlen(update->summary), UPD_SUMMARY_LEN - strlen(update->summary), 
				"zh%s%s%s", UPD_SUMMARY_NEEDLE2, value, UPD_SUMMARY_NEEDLE1);

	return 0;
}

static int env_get_description(const char *file, update_t *update)
{
#if 0
	"en@@@fix some very inmportant bug.&&&update some software.&&&optimize system feature###\
zh@@@修复一些漏洞&&&更新部分软件。&&&优化了系统特性";
	
#endif
	char *value = NULL;
	char *p = NULL;
	char *buff = NULL;
	
	update->desc = malloc(8192);
	update->desc[0] = '\0';

	buff = update->desc;
	if (NULL != (value = get_keyvalue_from_file(file, "Description_en"))) {
		p = strtok(value, "&");
		sprintf(buff, "en%s%s%s", UPD_DESC_NEEDLE2, p, UPD_DESC_NEEDLE3);
		while (NULL != (p = strtok(NULL, "&"))) {
			strcat(buff, p);
			strcat(buff, UPD_DESC_NEEDLE3);
		}
	}
	strcat(buff, UPD_DESC_NEEDLE1);
	
	buff = update->desc + strlen(update->desc);
	if (NULL != (value = get_keyvalue_from_file(file, "Description_zh"))) {
		p = strtok(value, "&");
		sprintf(buff, "zh%s%s%s", UPD_DESC_NEEDLE2, p, UPD_DESC_NEEDLE3);
		while (NULL != (p = strtok(NULL, "&"))) {
			strcat(buff, p);
			strcat(buff, UPD_DESC_NEEDLE3);
		}
	}
	strcat(buff, UPD_DESC_NEEDLE1);

	return 0;	
}

static int env_get_status(update_t *update)
{
#if 0
	return 0;
#endif
	char *value = NULL;
	
	update->status.prescript = 0;
	update->status.postscript = 0;
	update->status.reboot = 0;

	if (NULL !=(value = get_keyvalue_from_file(ENV_CONF_FILE, "Reboot"))) {
		if (!strcmp(value, "true"))
			update->status.reboot = 1;
	}
	
	if (1 == find_file_from_dir(ENV_TOP_DIR, ENV_SCRIPT_PRE))
		update->status.prescript = 1;
		
	if (1 == find_file_from_dir(ENV_TOP_DIR, ENV_SCRIPT_POST))
		update->status.postscript = 1;
		
	return 0;
}

static int env_get_packages(update_t *update)
{
	DIR *dp = NULL;
	struct dirent *entry = NULL;
	char *dname = NULL;
	char *p = NULL;
	int i = 0;
	unsigned char md5sum[16];
	char path[256];
	int n;

	sprintf(path, "%s/%s", ENV_TOP_DIR, ENV_PACK_DIR);
	
	if (-1 == chdir(path))
		error_exit("chdir");

	if (NULL == (dp = opendir(".")))
		error_exit("opendir");
	
	/* get number of packages */
	n = 0;
	while (NULL != (entry = readdir(dp))) {
		dname = strdup(entry->d_name);
		p = dname + strlen(dname) - 4;
		if (strcmp(p, ".rpm"))
			continue;
		n++;
	}
	update->packages = malloc(sizeof(*update->packages) * n);
	printf("number: %d\n", n);
	
	/* parse info of packages */
	rewinddir(dp);
	while (NULL != (entry = readdir(dp))) {
		dname = strdup(entry->d_name);
		p = dname + strlen(dname) - 4;
		if (strcmp(p, ".rpm"))
			continue;
		strncpy(update->packages[i].name, dname, UPD_PACK_FNAME_LEN);
		md5_from_file(dname, md5sum);
		md5_num2str(md5sum, update->packages[i].md5);

		printf("%s -> %s\n", update->packages[i].md5, update->packages[i].name);
		i++;

		free(dname);
	}
	update->packnum = i;
	closedir(dp);
	
	if (-1 == chdir("../.."))
		error_exit("chdir");
	
	return 0;
}

struct update *env_scan(const char *dir)
{
	struct update *update = NULL;
	
	puts("\e[1;31m=========== scaning environment ===========\e[0m");
	
#if 1
	char cwd[1024];
	if (NULL == getcwd(cwd, 1024))
		error_exit("getcwd");
#endif
	if (0 != chdir(dir))
		error_exit("chdir");
		
	if (NULL == (update = updstc_create()))
		return NULL;

	env_get_id(ENV_CONF_FILE, update);
	env_get_type(ENV_CONF_FILE, update);
	env_get_arch(ENV_CONF_FILE, update);
	env_get_osversion(ENV_CONF_FILE, update);
	env_get_summary(ENV_CONF_FILE, update);
	env_get_description(ENV_CONF_FILE, update);
	env_get_status(update);
	env_get_packages(update);
	
	if (0 != chdir(cwd))
		error_exit("chdir");

	puts("\e[1;32m============= Done !  ============\e[0m");
	return update;
}
