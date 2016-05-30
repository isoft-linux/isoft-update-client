#ifndef __UPDATE_CHECK_H__
#define __UPDATE_CHECK_H__

#include <stdio.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "config.h"
#include "archive.h"
#include "bill.h"
#include "packlist.h"

extern int update_check(const char *uptpath, const char *xsd);

#endif /* __UPDATE_DEMO_H__ */
