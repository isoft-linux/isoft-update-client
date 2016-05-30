#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include "utils.h"
#include "packlist.h"
#include "md5.h"

#define CONFIG_CONF_FILE	"update.ini"
#define CONFIG_TOP_DIR		"update"
#define CONFIG_PACK_DIR		"update/packages"
#define CONFIG_BILL_FILE	"update/update.xml"
#define CONFIG_SCRIPT_PRE	"update/prescript"
#define CONFIG_SCRIPT_POST	"update/postscript"

#define CONFIG_TYPE_LEN		32
#define CONFIG_ARCH_LEN		32
#define CONFIG_OSVER_LEN	64
#define CONFIG_SUMMARY_LEN	(1 << 16)  /* 64K */
#define CONFIG_KEYVAL_MAX_LEN	(1 << 18)  /* 256K */
#define CONFIG_DESC_LEN		CONFIG_KEYVAL_MAX_LEN

#define CONFIG_SUMMARY_OUT_NEEDLE1	"###"
#define CONFIG_SUMMARY_OUT_NEEDLE2	"@"
#define CONFIG_DESC_OUT_NEEDLE1		"###"
#define CONFIG_DESC_OUT_NEEDLE2		"@"
#define CONFIG_DESC_OUT_NEEDLE3		"&"

#define CONFIG_DESC_IN_NEEDLE1		CONFIG_DESC_OUT_NEEDLE3

extern const char *config_type[];
extern const char *config_arch[];
extern const char *config_lang[];
extern const char *config_status[];

extern char *config_get_key_value(const char *file, const char *key);
extern long config_get_id(const char *file);
extern int config_get_type(const char *file, char *type);
extern int config_get_arch(const char *file, char *arch);
extern int config_get_osversion(const char *file, char *osversion);
extern int config_get_summary(const char *file, char *summary);
extern int config_get_description(const char *file, char *desc);
extern bool config_get_status_reboot(const char *file);
extern bool config_get_status_prescript(const char *dir);
extern bool config_get_status_postscript(const char *dir);
extern struct db_package_list *config_get_packages(const char *path);

#endif /* __CONFIG_H__ */
