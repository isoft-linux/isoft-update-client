#include "update-gen-repos.h"

int update_gen_repos(const char *dirpath)
{
	char *cwd = NULL;
	struct dirent *entry = NULL;
	DIR *dp = NULL;
	char *p = NULL;
	char tmpdir[256];
	char buff[1024];
	int i, n;
	unsigned char md5sum[MD5_NUM_LEN];

	/* backup the current work dirctory before we change dir */
	cwd = getcwd(NULL, 0);
	
	if (-1 == chdir(dirpath))
		error_exit(dirpath);

	if (NULL == (dp = opendir(".")))
		error_exit(dirpath);
	
	if (NULL == tmpnam(tmpdir))
		error_exit("tmpnam");
	
	/* calculate the number of upt */
	n = 0;
	while (NULL != (entry = readdir(dp))) {
		if (entry->d_name[0] == '.')
			continue;
		p = entry->d_name + strlen(entry->d_name) - strlen(".upt");
		if (!strcmp(p, ".upt"))
			n++;
	}

	i = 0;
	rewinddir(dp);
	while (NULL != (entry = readdir(dp))) {
		if (entry->d_name[0] == '.')
			continue;
		p = entry->d_name + strlen(entry->d_name) - strlen(".upt");
		if (strcmp(p, ".upt"))
			continue;
		printf("processing updates.xml ... ... [\e[1;33m%d\e[0m/\e[1;34m%d\e[0m]\r", ++i, n);
		fflush(stdout);
		
		md5_from_file(entry->d_name, md5sum);

		tar_verbose = xz_verbose = 0;
		if (-1 == archive_unpack(entry->d_name, tmpdir))
			error_exit("archive_unpack");
		
		sprintf(buff, "%s/%s", tmpdir, CONFIG_BILL_FILE);
	
		if (-1 == bill_alter_updates("updates.xml", buff, md5sum))
			error_exit("bill_alter_updates");
		utils_remove_dir(tmpdir);
	}
	puts("");

	/* compress the xml use xz */
	if (-1 == xz_compress("updates.xml", "updates.xml.xz"))
		error_exit("xz_compress");
	remove("updates.xml");
	
	/* create md5 file */
	utils_create_md5file("updates.xml.xz");
		
	/* recover the previous wd */
	if (-1 == chdir(cwd))
		error_exit("chdir");
	free(cwd);

	return 0;
}
