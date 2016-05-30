#include "utils.h"
#include <time.h>
#include <stdarg.h>
#include <pthread.h>

char * utils_split_string(char *str, const char *needle)
{
	char *p = NULL;
	static char *head = NULL;
	char *ret = NULL;

	if (NULL != str)
		head = str;

	if ('\0' == *head)
		return NULL;

	p = strstr(head, needle);
	if (NULL == p) {
		ret = head;
		head += strlen(head);
		return ret;
	}

	ret = head;
	head[p-head] = '\0';

	head = p + strlen(needle);
	return ret;
}

bool utils_find_file(const char *path, const char *name)
{
	DIR *dp = NULL;
	struct dirent *entry = NULL;
	bool ret = false;
	
	if (NULL == (dp = opendir(path)))
		error_exit(path);
	
	ret = false;
	while (NULL != (entry = readdir(dp))) {
		if (!strcmp(entry->d_name, name)) {
			ret = true;
			break;
		}
	}
	closedir(dp);
	
	return ret;
}

bool utils_check_number(const char *str)
{
	const char *p = str;

	if (NULL == str)
		return false;

	while ('\0' != *p) {
		if (*p < '0' || *p > '9')
			return false;
		p++;
	}

	return true;
}

int utils_copy_file(const char *src, const char *dest)
{
	FILE *fp_src = NULL;
	FILE *fp_dst = NULL;
	char buff[BUFSIZ];
	int n;

	if (NULL == (fp_src = fopen(src, "r")))
		error_exit(src);
	if (NULL == (fp_dst = fopen(dest, "w")))
		error_exit(dest);

	setvbuf(fp_src, NULL, _IONBF, 0);
	setvbuf(fp_dst, NULL, _IONBF, 0);

	while (1) {
		n = fread(buff, 1, BUFSIZ, fp_src);
		if (n < 0)
			return -1;
		if (n == 0)
			break;
		if (0 >= fwrite(buff, n, 1, fp_dst))
			return -1;
	}

	fclose(fp_src);
	fclose(fp_dst);

	return 0;
}

int utils_remove_dir(const char *dir)
{
	DIR *dp = NULL;
	struct dirent *entry = NULL;
	struct stat stbuf;
	char *cwd = NULL;

	/* backup the current work dirctory before we change dir */
	cwd = getcwd(NULL, 0);
	
	if (-1 == chdir(dir))
		error_exit(dir);

	if (NULL == (dp = opendir(".")))
		error_exit("opendir");
	while (NULL != (entry = readdir(dp))) {
		if (entry->d_name[0] == '.')
			continue;
		if (-1 == lstat(entry->d_name, &stbuf))
			error_exit(entry->d_name);
		
		if (S_ISDIR(stbuf.st_mode)) {
			utils_remove_dir(entry->d_name);
		} else {
			if (-1 == remove(entry->d_name))
				error_exit(entry->d_name);
		}
	}
	if (-1 == closedir(dp))
		error_exit(dir);

	/* recover the previous wd */
	if (-1 == chdir(cwd))
		error_exit("chdir");
	free(cwd);
	if (-1 == remove(dir))
		error_exit(dir);

	return 0;
}

int utils_create_md5file(const char *file)
{
	unsigned char md5sum[MD5_NUM_LEN];
	char md5str[MD5_STR_LEN];
	char name[256];
	FILE *fp;

	md5_from_file(file, md5sum);
	md5_num2str(md5sum, md5str);

	strcpy(name, file);
	strcat(name, ".md5");

	if (NULL == (fp = fopen(name, "w")))
		error_exit("fopen");
	
	fputs(md5str, fp);
	fclose(fp);
	
	return 0;
}

static int g_log_level = LOG_NOTICE;
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_log_lock_inited = 0;
static char g_program_name[] = "isoft-update-daemon";
static char *g_get_log_level_name[] = {
    "EMERG",
    "ALERT",
    "CRITI",
    "ERROR",
    "WARNING",
    "NOTICE",
    "INFO",
    "DEBUG",
    ""
};

static int get_next_log_file(const char *date,char *dst_file,int dst_len)
{
    DIR *dirptr = NULL;
    struct dirent *entry = NULL;
    int len = 0;
    int max_id = 0;
    int id = 0;

    if (!date ) {
        if (dst_file) {
            goto cleanup;
        }

        return -1;
    }

    if ((dirptr = opendir(LOG_FILE_PATH)) == NULL) {
        goto cleanup;
    }
    else
    {
        len = strlen(date);
        while (entry = readdir(dirptr)) {
            if (entry->d_name == NULL ||
                strcmp(entry->d_name,".") == 0 ||
                strcmp(entry->d_name,"..") == 0 ) {
                continue;
            }

            struct stat info;
            memset(&info,0,sizeof(struct stat));
            stat(entry->d_name,&info);
            if (S_ISDIR(info.st_mode)) {
                continue;
            }

            if (strncmp(date,entry->d_name,len) != 0) {
                continue;
            }
            // 2015-09-09_1.log -->to get 1
            id = atoi(entry->d_name+len+1);
            if (id > max_id) {
                max_id = id;
            }
        }
    }

    closedir(dirptr);

    max_id ++;
    snprintf(dst_file,dst_len-1,"%s/%s_%d.log",LOG_FILE_PATH,date,max_id);

cleanup:

    if (max_id == 0) {
        time_t nowtime =time(NULL);
        snprintf(dst_file,dst_len-1,"%s/tmp_%d.log",LOG_FILE_PATH,(int)nowtime);
    }
    return 0;
}

/*
 * /var/log/isoft-update
 * 2015-09-09.log
 * 2015-09-09_1.log
 * 2015-09-09_2.log
 * 2015-09-09_n.log
*/
static int do_write_log(int log_level,const char *fmt,va_list args)
{
    char datestr[16];
    char timestr[16];
    char logstr[LOG_MAX_LINE_SIZE]="";
    char curlogfile[512]="";
    char nextlogfile[512]="";
    int  level = log_level % (LOG_DEBUG + 1);

    struct tm *now;
    time_t nowtime =time(NULL);
    FILE *fp;

    if( log_level > g_log_level )
            return 0;

    if (NULL== fmt|| 0==fmt[0])
        return 0;

    vsnprintf(logstr,LOG_MAX_LINE_SIZE-1,fmt,args);
    now=localtime(&nowtime);
    snprintf(datestr,sizeof(datestr),"%04d-%02d-%02d",now->tm_year+1900,now->tm_mon+1,now->tm_mday);
    snprintf(timestr,sizeof(timestr),"%02d:%02d:%02d",now->tm_hour     ,now->tm_min  ,now->tm_sec );

    snprintf(curlogfile,sizeof(curlogfile),"%s/%s.log",LOG_FILE_PATH,datestr);

    fp=fopen(curlogfile,"a");
    if (fp) {
        fprintf(fp,"%s %s (%s:%d): %s: %s\n",datestr,timestr,
                g_program_name,(int) getpid(),
                g_get_log_level_name[level],
                logstr);
        if (ftell(fp) > LOG_MAX_FILE_SIZE) {
            fclose(fp);
            get_next_log_file(datestr,nextlogfile,sizeof(nextlogfile));
            if (rename(curlogfile,nextlogfile)) {
                    remove(nextlogfile);
                    rename(curlogfile,nextlogfile);
            }
        } else {
            fclose(fp);
        }
    }
    return 0;
}

int _write_log(const char *fmt,...)
{
    va_list args;

    if (g_log_lock_inited == 0) {
        if (mkdir(LOG_FILE_PATH, 0644) != 0) {
            if (errno != EEXIST)
                return -1;
        }
        g_log_lock_inited = 1;
        pthread_mutex_init(&g_log_lock,NULL);
    }

    pthread_mutex_lock(&g_log_lock);
    va_start(args,fmt);
    do_write_log(LOG_DEBUG,fmt,args);
    va_end(args);
    pthread_mutex_unlock(&g_log_lock);
}

int set_write_log_level(int log_level)
{
    g_log_level = log_level;
    return 0;
}

int write_log(int log_level,const char *fmt,...)
{
    va_list args;
    if (g_log_lock_inited == 0) {
        if (mkdir(LOG_FILE_PATH, 0644) != 0) {
            if (errno != EEXIST)
                return -1;
        }
        g_log_lock_inited = 1;
        pthread_mutex_init(&g_log_lock,NULL);
    }

    pthread_mutex_lock(&g_log_lock);
    va_start(args,fmt);
    do_write_log(log_level,fmt,args);
    va_end(args);
    pthread_mutex_unlock(&g_log_lock);
    return 0;
}
int write_error_log(const char *fmt,...)
{
    va_list args;
    if (g_log_lock_inited == 0) {
        if (mkdir(LOG_FILE_PATH, 0644) != 0) {
            if (errno != EEXIST)
                return -1;
        }
        g_log_lock_inited = 1;
        pthread_mutex_init(&g_log_lock,NULL);
    }

    pthread_mutex_lock(&g_log_lock);
    va_start(args,fmt);
    do_write_log(LOG_ERR,fmt,args);
    va_end(args);
    pthread_mutex_unlock(&g_log_lock);
    return 0;
}
int write_debug_log(const char *fmt,...)
{
    va_list args;
    if (g_log_lock_inited == 0) {
        if (mkdir(LOG_FILE_PATH, 0644) != 0) {
            if (errno != EEXIST)
                return -1;
        }
        g_log_lock_inited = 1;
        pthread_mutex_init(&g_log_lock,NULL);
    }

    pthread_mutex_lock(&g_log_lock);
    va_start(args,fmt);
    do_write_log(LOG_DEBUG,fmt,args);
    va_end(args);
    pthread_mutex_unlock(&g_log_lock);
    return 0;
}

/*
* to write upt_name state  type  date to the following 2 files:
* /var/log/isoft-update/updated.log: installed
* /var/log/isoft-update/updating.log: no installed
*
* @upt_name:update-2015-09-10-5-x86_64.upt
* @type:bugfix/update/security
* @action:'u' -- not installed; 'i'--installed
* @state:1--installed successfully;0--failed;-1--not installed
*/
int write_upt_reocord_file(const char *upt_name,const char *type,char action,int state,
                           const char *summary,const char *desc)
{
    FILE *fp;
    char datestr[16] = "";
    char timestr[16] = "";
    struct tm *now;
    time_t nowtime =time(NULL);

    if (NULL == upt_name || 0 == upt_name[0])
        return 0;

    if (NULL == type || 0 == type[0])
        return 0;

    if (action  == 'u') {
        fp=fopen(NOT_INSTALLED_LOG_FILE,"a");
    } else {
        fp=fopen(INSTALLED_LOG_FILE,"a");
        remove(NOT_INSTALLED_LOG_FILE);
    }
    now=localtime(&nowtime);
    snprintf(datestr,sizeof(datestr),"%04d-%02d-%02d",now->tm_year+1900,now->tm_mon+1,now->tm_mday);
    snprintf(timestr,sizeof(timestr),"%02d:%02d:%02d",now->tm_hour     ,now->tm_min  ,now->tm_sec );
    if (fp) {
        fprintf(fp,"%s%s%d%s%s%s%s %s%s%s%s%s\n",
                upt_name,LOG_FILE_FILTER,
                state,LOG_FILE_FILTER,
                type,LOG_FILE_FILTER,
                datestr,timestr,LOG_FILE_FILTER,
                summary==NULL?"":summary,LOG_FILE_FILTER,
                desc);
        fclose(fp);
    }

    return 0;
}


#if 0
/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! need be recoded !!!!!!!!!!! */
char *utils_replace_string(char *str, const char *src, const char *dest)
{
	char *p = str;

	while ('\0' != *p) {
		if (*p == *src)
			*p = *dest;
		p++;
	}
	return str;
#if 0
	int n;
	char *new = NULL;
	char *old = NULL;
	char *p = NULL;
	
	if (NULL == (old = strdup(str)))
		return NULL;
	

	if (NULL == (p = utils_split_string(old, src)))
		return old;
	
	n = strlen(str) * strlen(dest);
	if (NULL == (new = malloc(n))) {
		free(old);
		return NULL;
	}

	strcpy(new, p);
	while (NULL != (p = utils_split_string(NULL, src))) {
		strcat(new, dest);
		strcat(new, p);
	}

	free(old);
	return new;
#endif
}
#endif
