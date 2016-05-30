#ifndef __BILL_H__
#define __BILL_H__

#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlschemas.h>
#include <libxml/xmlsave.h>
#include "database.h"
#include "config.h"
#include "packlist.h"

/* public lib, usually used by create update */
extern void bill_add_id(xmlNodePtr root, struct update_database *update);
extern void bill_add_type(xmlNodePtr root, struct update_database *update);
extern void bill_add_arch(xmlNodePtr root, struct update_database *update);
extern void bill_add_date(xmlNodePtr root, struct update_database *update);
extern void bill_add_osversion(xmlNodePtr root, struct update_database *update);
extern void bill_add_osversion(xmlNodePtr root, struct update_database *update);
extern void bill_add_description(xmlNodePtr root, struct update_database *update);
extern void bill_add_status(xmlNodePtr root, struct update_database *update);
extern void bill_add_packages(xmlNodePtr root, struct update_database *update);
extern int bill_create(struct update_database *update, const char *billname);

/* used by check update */
extern db_packlist_t *bill_read_packages(const char *billname);

/* used by create updates */
extern int bill_alter_updates(const char *rootdoc, const char *childdoc, unsigned char *md5sum);

/* used by validate xsd */
typedef enum {
	XMLLINT_RETURN_OK = 0,	/* No error */
	XMLLINT_ERR_UNCLASS = 1,	/* Unclassified */
	XMLLINT_ERR_DTD = 2,	/* Error in DTD */
	XMLLINT_ERR_VALID = 3,	/* Validation error */
	XMLLINT_ERR_RDFILE = 4,	/* CtxtReadFile error */
	XMLLINT_ERR_SCHEMACOMP = 5,	/* Schema compilation */
	XMLLINT_ERR_OUT = 6,	/* Error writing output */
	XMLLINT_ERR_SCHEMAPAT = 7,	/* Error in schema pattern */
	XMLLINT_ERR_RDREGIS = 8,	/* Error in Reader registration */
	XMLLINT_ERR_MEM = 9,	/* Out of memory error */
	XMLLINT_ERR_XPATH = 10	/* XPath evaluation error */
} xmllintReturnCode;

extern int bill_validate_xsd(const char *xmlfile, const char *xsdfile);

#endif	/* __BILL_H__ */
