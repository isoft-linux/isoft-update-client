#include "update-check.h"

int check_packages_callback(void *arg, const db_pack_t *pack)
{
	db_packlist_t *list = arg;
	db_pack_t pack2 = *pack;
	
	if (-1 == db_packlist_search(list, &pack2))
		return -1;

	if (strcmp(pack->name, pack2.name))
		return -1;
	if (memcmp(pack->md5, pack2.md5, MD5_NUM_LEN))
		return -1;

	return 0;

}

int check_packages(const char *path)
{
	db_packlist_t *list1 = NULL;
	db_packlist_t *list2 = NULL;
	int n1,n2;
	char buff[1024];

	sprintf(buff, "%s/%s", path, CONFIG_PACK_DIR);
	list1 = config_get_packages(buff);

	sprintf(buff, "%s/%s", path, CONFIG_BILL_FILE);
	list2 = bill_read_packages(buff);
	n1 = db_packlist_get_number(list1);
	n2 = db_packlist_get_number(list2);

	if (n1 != n2)
		return -1;

	if (n1 != db_packlist_get_all(list1, check_packages_callback, list2))
		return -1;
	
#if 0
	puts("check package:");
	printf("total: %d\n", db_packlist_get_number(list1));
	_db_packlist_debug_(list1);
	
	puts("check package:");
	printf("total: %d\n", db_packlist_get_number(list2));
	_db_packlist_debug_(list2);
#endif
	
	return 0;
}

int check_md5(const char *filename)
{
	unsigned char md5sum[MD5_NUM_LEN];
	char md5str[MD5_STR_LEN];
	char buff[MD5_STR_LEN+4];
	FILE *fp;

	md5_from_file(filename, md5sum);
	md5_num2str(md5sum, md5str);

	strcpy(buff, filename);
	strcat(buff, ".md5");

	if (NULL == (fp = fopen(buff, "r"))) {
		perror("can`t find or read md5 file");
		return -1;
	}
	if (NULL == fgets(buff, sizeof(buff), fp)) {
		perror("fgets");
		return -1;
	}
	fclose(fp);
	
	/* ignore '\n' */
	if (buff[strlen(buff)-1] == '\n')
		buff[strlen(buff)-1] = '\0';
	if (strcmp(md5str, buff))
		return -1;
	else
		return 0;
}

int check_xsd(const char *xml, const char *xsd)
{
	return bill_validate_xsd(xml, xsd);	
}

int update_check(const char *filename, const char *xsdfile)
{
	char tmpdir[1024];
	char buff[1024];
	int ret = 0;

	if (-1 == check_md5(filename)) {
		fprintf(stderr, "check MD5sum ... ... [\e[1;31mFALSE\e[0m]\n");
		return -1;
	}
	puts("check MD5sum ... ... [\e[1;32mOK\e[0m]");


	/* decompress package to /tmp */
	if (NULL == tmpnam(tmpdir)) {
		perror("tmpnam");
		return -1;
	}

	archive_verbose = 1;
	if (-1 == archive_unpack(filename, tmpdir)) {
		fprintf(stderr, "can`t unpack archive!\n");
		return -1;
	}

	sprintf(buff, "%s/%s", tmpdir, CONFIG_BILL_FILE);
	if (-1 == check_xsd(buff, xsdfile)) {
		fprintf(stderr, "validate xsd ... ... [\e[1;31mFALSE\e[0m]\n");
		ret = -1;
		puts(getcwd(NULL, 0));
		puts(buff);
		getchar();
		goto Error_Handler;
	}
	puts("validate xsd ... ... [\e[1;32mOK\e[0m]");

	if (-1 == check_packages(tmpdir)) {
		fprintf(stderr, "check packages ... ... [\e[1;31mFALSE\e[0m]\n");
		ret = -1;
		goto Error_Handler;
	}
	puts("check packages ... ... [\e[1;32mOK\e[0m]");
	
Error_Handler:
	utils_remove_dir(tmpdir);

	return ret;
}

