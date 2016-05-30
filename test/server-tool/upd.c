#include <stdio.h>
#include "upd.h"
#include "env.h"
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static char * parse_string(char *str, const char *needle)
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

static inline void updinfo_add_id(xmlNodePtr root, struct update *update)
{
	char content[128];

	snprintf(content, 128, "%ld", update->id);
	xmlNewChild(root, NULL, BAD_CAST"id", BAD_CAST content);
}

static inline void updinfo_add_type(xmlNodePtr root, struct update *update)
{
	xmlNewChild(root, NULL, BAD_CAST"type", BAD_CAST update->type);
}

static inline void updinfo_add_arch(xmlNodePtr root, struct update *update)
{
	xmlNewChild(root, NULL, BAD_CAST"arch", BAD_CAST update->arch);
}

static inline void updinfo_add_date(xmlNodePtr root, struct update *update)
{
	struct tm *tmbuf = NULL;
	time_t tm_sec;
	char content[128];

	time(&tm_sec);
	tmbuf = localtime(&tm_sec);
	snprintf(content, 128, "%d.%d.%d", tmbuf->tm_year + 1900,
			tmbuf->tm_mon + 1, tmbuf->tm_mday);
	xmlNewChild(root, NULL, BAD_CAST"date", BAD_CAST content);
}

static inline void updinfo_add_osversion(xmlNodePtr root, struct update *update)
{
	xmlNewChild(root, NULL, BAD_CAST"osversion", BAD_CAST update->osversion);
}

static inline int set_prop_lang(xmlNodePtr node, const char *lang)
{
	if (!strcmp(lang, "en")) {
		return 0;
	} else if (!strcmp(lang, "zh")) {
		xmlSetProp(node, BAD_CAST"xml:lang", BAD_CAST"zh_CN");
		return 0;
	} else {
		return -1;
	}
}

static void updinfo_add_summary(xmlNodePtr root, struct update *update)
{
	xmlNodePtr node = NULL;
	char *buff = NULL;
	char *p = NULL;
	char *lang = NULL;
	char *content = NULL;

	if (update->summary == NULL || '\0' == *update->summary) {
		xmlNewChild(root, NULL, BAD_CAST"summary", NULL);
		return;
	}

	buff = strdup(update->summary);
	
	if (NULL != (p = parse_string(buff, UPD_SUMMARY_NEEDLE1))) {
		lang = strtok(p, UPD_SUMMARY_NEEDLE2);
		content = strtok(NULL, UPD_SUMMARY_NEEDLE2);
		node = xmlNewChild(root, NULL, BAD_CAST"summary", BAD_CAST content);
		set_prop_lang(node, lang);
	}

	while (NULL != (p = parse_string(NULL, UPD_SUMMARY_NEEDLE1))) {
		lang = strtok(p, UPD_SUMMARY_NEEDLE2);
		content = strtok(NULL, UPD_SUMMARY_NEEDLE2);
		node = xmlNewChild(root, NULL, BAD_CAST"summary", BAD_CAST content);
		set_prop_lang(node, lang);
	}

	free(buff);
}

static inline void updinfo_add_description(xmlNodePtr root, struct update *update)
{
	xmlNodePtr node = NULL;
	char *buff = NULL;
	char *p = NULL;
	char *lang = NULL;
	char *content = NULL;
	char *item = NULL;
	
	if (update->desc == NULL || '\0' == *update->desc) {
		xmlNewChild(root, NULL, BAD_CAST"description", NULL);
		return;
	}
	
	buff = strdup(update->desc);
	
	if (NULL != (p = parse_string(buff, UPD_DESC_NEEDLE1))) {
		lang = strtok(p, UPD_DESC_NEEDLE2);
		content = strtok(NULL, UPD_DESC_NEEDLE2);
		node = xmlNewChild(root, NULL, BAD_CAST"description", NULL);
		set_prop_lang(node, lang);
		
		if (NULL != (item = strtok(content, UPD_DESC_NEEDLE3)))
			xmlNewChild(node, NULL, BAD_CAST"item", BAD_CAST item);
		while (NULL != (item = strtok(NULL, UPD_DESC_NEEDLE3)))
			xmlNewChild(node, NULL, BAD_CAST"item", BAD_CAST item);
	}

	while (NULL != (p = parse_string(NULL, UPD_DESC_NEEDLE1))) {
		lang = strtok(p, UPD_DESC_NEEDLE2);
		content = strtok(NULL, UPD_DESC_NEEDLE2);
		node = xmlNewChild(root, NULL, BAD_CAST"description", NULL);
		set_prop_lang(node, lang);
		
		if (NULL != (item = strtok(content, UPD_DESC_NEEDLE3)))
			xmlNewChild(node, NULL, BAD_CAST"item", BAD_CAST item);
		while (NULL != (item = strtok(NULL, UPD_DESC_NEEDLE3)))
			xmlNewChild(node, NULL, BAD_CAST"item", BAD_CAST item);
	}

	free(buff);
}

static inline void updinfo_add_status(xmlNodePtr root, struct update *update)
{
	char content[32];

	/* is there has prescript ? */
	update->status.prescript ? sprintf(content, "true") :
		sprintf(content, "false");
	xmlNewChild(root, NULL, BAD_CAST"prescript", BAD_CAST content);


	/* is there has postscript ? */
	update->status.postscript ? sprintf(content, "true") :
		sprintf(content, "false");
	xmlNewChild(root, NULL, BAD_CAST"postscript", BAD_CAST content);


	/* is system need reboot ? */
	update->status.reboot ? sprintf(content, "true") :
		sprintf(content, "false");
	xmlNewChild(root, NULL, BAD_CAST"needreboot", BAD_CAST content);
}

static inline void updinfo_add_packages(xmlNodePtr root, struct update *update)
{
	xmlNodePtr packs = NULL;
	xmlNodePtr rpm = NULL;
	int i;

	packs = xmlNewChild(root, NULL, BAD_CAST"packages", NULL);
	for (i = 0; i < update->packnum; i++) {
		rpm = xmlNewChild(packs, NULL, BAD_CAST"rpm", NULL);
		xmlNewChild(rpm, NULL, BAD_CAST"filename", BAD_CAST update->packages[i].name);
		xmlNewChild(rpm, NULL, BAD_CAST"md5sum", BAD_CAST update->packages[i].md5);
	}
}

int updinfo_create(struct update *update)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;

	/* create a new xmldoc */
	if (NULL == (doc = xmlNewDoc(BAD_CAST"1.0"))) {
		fprintf(stderr, "Can`t create xml\n");
		return 0xe001;
	}

	/* create and set the root node */
	root = xmlNewNode(NULL, BAD_CAST"update");
	xmlDocSetRootElement(doc, root);

	updinfo_add_id(root, update);
	updinfo_add_type(root, update);
	updinfo_add_arch(root, update);
	updinfo_add_date(root, update);
	updinfo_add_osversion(root, update);
	updinfo_add_summary(root, update);
	updinfo_add_description(root, update);
	updinfo_add_packages(root, update);
	updinfo_add_status(root, update);

	//xmlKeepBlanksDefault(1);
	//xmlIndentTreeOutput = 1;
	xmlSetCompressMode(0);
	if (-1 == chdir(ENV_TOP_DIR)) {
		perror("chdir");
		return -1;
	}
	if (-1 == xmlSaveFormatFileEnc(ENV_INFO_FILE, doc, "utf-8", 1)) {
		fprintf(stderr, "Can`t save xml\n");
		return 0xe002;
	}
	if (-1 == chdir("..")) {
		perror("chdir");
		return -1;
	}
	xmlFreeDoc(doc);

	return 0;
}



#if 0
node = xmlNewNode(NULL, BAD_CAST name);
xmlNodeAddContent(node, BAD_CAST content);
xmlAddChild(root, node);
#endif
