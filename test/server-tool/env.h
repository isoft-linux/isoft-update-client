#ifndef __SCAN_ENV_H__
#define __SCAN_ENV_H__

#include "upd.h"
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>

#define ENV_TOP_DIR	"update"
#define ENV_PACK_DIR	"packages"
#define ENV_INFO_FILE	"update.xml"
#define ENV_CONF_FILE	"update.ini"
#define ENV_SCRIPT_PRE	"prescript"
#define ENV_SCRIPT_POST	"postscript"

#define error_exit(_errmsg_)	error(EXIT_FAILURE, errno, \
		"%s:%d -> %s\n", __FILE__, __LINE__, _errmsg_)	

struct update *env_scan(const char *dir);
int env_check(void);
int env_create(const char *path);
#if 0
int env_get_id(void);
int env_get_type(char *type);
int env_get_arch(char *arch);
int env_get_osversion(char *osversion);
int env_get_summary(const char *summary);
int env_get_description(const char *desc);
int env_get_status(void);
int env_get_packages();
#endif

#endif /* __SCAN_ENV_H__ */
