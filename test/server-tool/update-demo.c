#include "update-demo.h"

void update_usage()
{
	char *mesg = 
"usage:\n\
\t1. to create a update demo:\n\
\t\t./main -demo demo\n\
\t2. to build the update package:\n\
\t\t./main -create demo update.xsd\n\
\t3. to check the update package:\n\
\t\t./main -check demo/update-xxx-xxx.upt update.xsd\n\
\t4. to generate the updates file:\n\
\t\t./main -gen upts/\n";
	puts(mesg);
}

int update_demo(const char *path)
{
	FILE *fp = NULL;
	char *format = NULL;
	char *cwd = NULL;
	char buff[8192];
	int i;

	/* backup the current work dirctory before we change dir */
	cwd = getcwd(NULL, 0);
	
	if (-1 == mkdir(path, 0755) && EEXIST != errno)
		error_exit("mkdir");
	if (-1 == access(path, W_OK)) 
		error_exit("access");
	if (-1 == chdir(path))
		error_exit("chdir");

	if (-1 == mkdir(CONFIG_TOP_DIR, 0755))
		error_exit("mkdir");
	if (-1 == mkdir(CONFIG_PACK_DIR, 0755))
		error_exit("mkdir");

	if (NULL == (fp = fopen(CONFIG_CONF_FILE, "w")))
		error_exit("fopen");

	/* configure id */
	fprintf(fp, "#本次更新的ID号。\nId=00001\n\n");

	/* configure type */
	/* get all type */
	strcpy(buff, config_type[0]);
	for (i = 1; NULL != config_type[i]; i++) {
		strcat(buff, "/");
		strcat(buff, config_type[i]);
	}
	format = "#更新类型，可选参数有：%s。\n";
	fprintf(fp, format, buff);
	format = "Type=%s\n\n";
	fprintf(fp, format, config_type[0]);

	/* configure arch */
	/* get all arch */
	strcpy(buff, config_arch[0]);
	for (i = 1; NULL != config_arch[i]; i++) {
		strcat(buff, "/");
		strcat(buff, config_arch[i]);
	}
	format = "#本包适用的架构体系，可选参数有：%s。\n";
	fprintf(fp, format, buff);
	format = "Arch=%s\n\n";
	fprintf(fp, format, config_arch[0]);

	/* configure OsVersion */
	fprintf(fp, "#版本号。\nOsVersion=v4.0\n\n");

	/* configure Summary */
	format = 
"#本次更新的英文简介。\n\
Summary_en=this is summary for update.\n\n\
#本次更新的中文简介。\n\
Summary_zh=这是关于本次更新的综述。\n\n";
	fputs(format, fp);

	/* configure Description_ */
	format = "#本次更新概要，英文描述；每条描述之间必须用'%s'隔开！\n";
	fprintf(fp, format, CONFIG_DESC_IN_NEEDLE1);

	format = "Description_en=fix some very inmportant bug.%supdate some software.%soptimize system feature.\n\n";
	fprintf(fp, format, CONFIG_DESC_IN_NEEDLE1, CONFIG_DESC_IN_NEEDLE1);

	format = "#本次更新概要，中文描述；每条描述之间必须用'%s'隔开！\n";
	fprintf(fp, format, CONFIG_DESC_IN_NEEDLE1);

	format = "Description_zh=修复一些漏洞%s更新部分软件。%s优化了系统特性。\n\n";
	fprintf(fp, format, CONFIG_DESC_IN_NEEDLE1, CONFIG_DESC_IN_NEEDLE1);

	/*  configure reboot */
	/* get all status */
	strcpy(buff, config_status[0]);
	for (i = 1; NULL != config_status[i]; i++) {
		strcat(buff, "/");
		strcat(buff, config_status[i]);
	}
	format = "#本次更新完是否需要重启，可选参数有：%s。\n";
	fprintf(fp, format, buff);
	format = "Reboot=%s\n";
	fprintf(fp, format, config_status[0]);

	fclose(fp);
	
	/* recover the previous wd */
	if (-1 == chdir(cwd))
		error_exit("chdir");
	free(cwd);

	return 0;
}
