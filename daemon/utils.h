#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <dirent.h>
#include <errno.h>
#include <error.h>
#include "md5.h"
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

/*
* to write logs
*/
#define LOG_MAX_LINE_SIZE 1024
#define LOG_MAX_FILE_SIZE (1024*1024*10)
#define LOG_FILE_PATH     "/var/log/isoft-update"
#define UPT_RECORD_LOG_FILE_PATH     "/etc/update/updates"
#define INSTALLED_LOG_FILE     UPT_RECORD_LOG_FILE_PATH"/updated.log"
#define NOT_INSTALLED_LOG_FILE     UPT_RECORD_LOG_FILE_PATH"/updating.log"
#define LOG_FILE_FILTER     "|||"

#define PK_OFFLINE_TRIGGER_FILENAME "/system-update"
#define PK_OFFLINE_PKGS_PATH "/system-update-doing"
#define PK_OFFLINE_PKGS_CFG_FILE  ".record.cfg" // == /system-update-doing/.record.cfg


#define error_exit(_errmsg_)	error(EXIT_FAILURE, errno, \
		"%s:%d -> %s\n", __FILE__, __LINE__, _errmsg_)	

extern char *utils_split_string(char *str, const char *needle);
extern bool utils_find_file(const char *path, const char *name);
extern bool utils_check_number(const char *str);
extern int utils_copy_file(const char *src, const char *dest);
extern int utils_remove_dir(const char *dir);
extern int utils_create_md5file(const char *file);

/*
* write_log usage:
* 1.int _write_log(const char *fmt,...); default:LOG_DEBUG
* 2.int write_log(int log_level,const char *fmt,...);
* 3.int write_error_log(const char *fmt,...); only for ERROR
* 4.int write_debug_log(const char *fmt,...); only for DEBUG
* 5.int set_write_log_level(int log_level); set default level to @log_level
*/
extern int set_write_log_level(int log_level);
extern int set_program_name(const char *name);
extern int _write_log(const char *fmt,...);
extern int write_log(int log_level,const char *fmt,...);
extern int write_error_log(const char *fmt,...);
extern int write_debug_log(const char *fmt,...);

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
extern int write_upt_reocord_file(const char *upt_name,const char *type,char action,int state,
                                  const char *summary,const char *desc);

void
get_xml_node_value(const char *upt_xml_file,const char *pType,char retValue[512]);
void
get_desc_node(xmlDocPtr  doc,
             xmlNodePtr cur,
             char desc_buffer[512]);
#endif /* __UTILS_H__ */
