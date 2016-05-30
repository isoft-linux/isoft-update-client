#include "bill.h"

inline void bill_add_id(xmlNodePtr root, struct update_database *update)
{
	char content[128];
	long id;

	id = db_get_id(update);
	snprintf(content, 128, "%ld", id);
	xmlNewChild(root, NULL, BAD_CAST"id", BAD_CAST content);
}

inline void bill_add_type(xmlNodePtr root, struct update_database *update)
{
	char type[DB_TYPE_LEN];

	db_get_type(update, type);
	xmlNewChild(root, NULL, BAD_CAST"type", BAD_CAST type);
}

inline void bill_add_arch(xmlNodePtr root, struct update_database *update)
{
	char arch[DB_ARCH_LEN];

	db_get_arch(update, arch);
	xmlNewChild(root, NULL, BAD_CAST"arch", BAD_CAST arch);
}

inline void bill_add_date(xmlNodePtr root, struct update_database *update)
{
	struct tm *tmbuf = NULL;
	time_t tm_sec;
	char content[128];

	tm_sec = db_get_date(update);
	tmbuf = localtime(&tm_sec);
	snprintf(content, 128, "%04d-%02d-%02d", tmbuf->tm_year + 1900,
			tmbuf->tm_mon + 1, tmbuf->tm_mday);
	xmlNewChild(root, NULL, BAD_CAST"date", BAD_CAST content);
}

inline void bill_add_osversion(xmlNodePtr root, struct update_database *update)
{
	char osversion[DB_OSVER_LEN];

	db_get_osversion(update, osversion);
	xmlNewChild(root, NULL, BAD_CAST"osversion", BAD_CAST osversion);
}

static inline int set_prop_lang(xmlNodePtr node, const char *lang)
{
	if (!strcmp(lang, "en")) {
		return 0;
	} else if (!strcmp(lang, "zh")) {
		xmlSetProp(node, BAD_CAST"lang", BAD_CAST"zh_CN");
		return 0;
	} else {
		return -1;
	}
}

static int summary_callback(void *arg, const char *lang, const char *content)
{
	xmlNodePtr root = arg;
	xmlNodePtr node = NULL;

	node = xmlNewChild(root, NULL, BAD_CAST"summary", BAD_CAST content);
	set_prop_lang(node, lang);

	return 0;
}

static inline void bill_add_summary(xmlNodePtr root, struct update_database *update)
{
	db_get_summary(update, summary_callback, root);
}

static int desc_callback(void *arg, const char *lang, const char *content)
{
	xmlNodePtr root = arg;
	xmlNodePtr node = NULL;
	char *item = NULL;
	char *buff = NULL;

	if (NULL == (buff = strdup(content)))
		return -1;

	node = xmlNewChild(root, NULL, BAD_CAST"description", NULL);
	set_prop_lang(node, lang);

	if (NULL != (item = strtok(buff, DB_DESC_NEEDLE3)))
		xmlNewChild(node, NULL, BAD_CAST"item", BAD_CAST item);
	while (NULL != (item = strtok(NULL, DB_DESC_NEEDLE3)))
		xmlNewChild(node, NULL, BAD_CAST"item", BAD_CAST item);

	free(buff);
	return 0;
}

inline void bill_add_description(xmlNodePtr root, struct update_database *update)
{
	db_get_description(update, desc_callback, root);
}


void bill_add_status(xmlNodePtr root, struct update_database *update)
{
	const char *content = NULL;

	/* is there has prescript ? */
	content = db_get_status_prescript(update) ? config_status[1]: config_status[0];
	xmlNewChild(root, NULL, BAD_CAST"prescript", BAD_CAST content);


	/* is there has postscript ? */
	content = db_get_status_postscript(update) ? config_status[1]: config_status[0];
	xmlNewChild(root, NULL, BAD_CAST"postscript", BAD_CAST content);


	/* is system need reboot ? */
	content = db_get_status_reboot(update) ? config_status[1]: config_status[0];
	xmlNewChild(root, NULL, BAD_CAST"needreboot", BAD_CAST content);
}

static int package_callback(void *arg, const struct db_package *pack)
{
	xmlNodePtr root = arg;
	xmlNodePtr node = NULL;
	char md5str[MD5_STR_LEN];

	node = xmlNewChild(root, NULL, BAD_CAST"rpm", NULL);
	xmlNewChild(node, NULL, BAD_CAST"filename", BAD_CAST pack->name);
	md5_num2str(pack->md5, md5str);
	xmlNewChild(node, NULL, BAD_CAST"md5sum", BAD_CAST md5str);
	
	return 0;
}

void bill_add_packages(xmlNodePtr root, struct update_database *update)
{
	xmlNodePtr packs = NULL;

	packs = xmlNewChild(root, NULL, BAD_CAST"packages", NULL);
	db_get_packages(update, package_callback, packs);
}


int bill_create(struct update_database *update, const char *filename)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;

	/* create a new xmldoc */
	if (NULL == (doc = xmlNewDoc(BAD_CAST"1.0"))) {
		fprintf(stderr, "Can`t create xml\n");
		return -1;
	}

	/* create and set the root node */
	root = xmlNewNode(NULL, BAD_CAST"update");
	xmlDocSetRootElement(doc, root);

	bill_add_id(root, update);
	bill_add_type(root, update);
	bill_add_arch(root, update);
	bill_add_date(root, update);
	bill_add_osversion(root, update);
	bill_add_summary(root, update);
	bill_add_description(root, update);
	bill_add_packages(root, update);
	bill_add_status(root, update);

	/* if you want to compress it, set 1 */
	xmlSetCompressMode(0);

	/* if you want to add blank , set the last arg to 1 */
	if (-1 == xmlSaveFormatFileEnc(filename, doc, "utf-8", 1)) {
		fprintf(stderr, "Can`t save xml\n");
		return -1;
	}

	xmlFreeDoc(doc);

	return 0;
}


static void bill_parse_rpm(xmlDocPtr doc, xmlNodePtr cur, db_packlist_t *list)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	db_pack_t pack;

	while (cur != NULL) {

		if (!xmlStrcmp(cur->name, (const xmlChar *)"filename")) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			strncpy(pack.name, (const char *)key, DB_PACK_FNAME_LEN);
			xmlFree(key);
			
		} else if (!xmlStrcmp(cur->name, (const xmlChar *)"md5sum")) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			md5_str2num(pack.md5, (const char *)key);
			xmlFree(key);
		}
		cur=cur->next; /* 下一个子节点 */
	}
	db_packlist_add(list, &pack);

	return;
}

/* 解析packages节点，打印rpm节点的内容 */
static db_packlist_t *bill_parse_packages(xmlDocPtr doc, xmlNodePtr cur)
{
	cur = cur->xmlChildrenNode;
	db_packlist_t *list = NULL;

	if (NULL == (list = db_packlist_init()))
		return NULL;

	while(cur != NULL){
		if(!xmlStrcmp(cur->name, (const xmlChar *)"rpm")) {
			bill_parse_rpm(doc, cur, list);
		}
		cur=cur->next; /* 下一个子节点 */
	}

	return list;
}

/* 解析文档 */
db_packlist_t *bill_read_packages(const char *docname)
{
	/* 定义文档和节点指针 */
	xmlDocPtr doc;
	xmlNodePtr cur;

	db_packlist_t *list = NULL;
	
	/* 进行解析，如果没成功，显示一个错误并停止 */
	//doc = xmlParseFile(docname);
	doc = xmlReadFile(docname, NULL, XML_PARSE_NOBLANKS);

	if(doc == NULL){
		fprintf(stderr, "Document not parse successfully. \n");
		return NULL;
	}

	/* 获取文档根节点，若无内容则释放文档树并返回 */
	cur = xmlDocGetRootElement(doc);
	if(cur == NULL){
		fprintf(stderr, "empty document\n");
		xmlFreeDoc(doc);
		return NULL;
	}

	/* 确定根节点名是否为update，不是则返回 */
	if(xmlStrcmp(cur->name, (const xmlChar *)"update")){
		fprintf(stderr, "document of the wrong type, root node != update");
		xmlFreeDoc(doc);
		return NULL;
	}

	/* 遍历文档树 */
	cur = cur->xmlChildrenNode;
	while(cur != NULL){
		/* 找到packages子节点 */
		if(!xmlStrcmp(cur->name, (const xmlChar *)"packages")){
			list = bill_parse_packages(doc, cur); /* 解析packages子节点 */
		}
		cur = cur->next; /* 下一个子节点 */
	}

	xmlFreeDoc(doc); /* 释放文档树 */
	return list;
}



static xmlNodePtr bill_extract_update(const char *docname)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;
	xmlNodePtr new = NULL;
	xmlNodePtr cur = NULL;
	
	doc = xmlReadFile(docname, NULL, XML_PARSE_NOBLANKS);
	if(doc == NULL){
		fprintf(stderr, "Document not parse successfully. \n");
		return NULL;
	}

	/* 获取文档根节点，若无内容则释放文档树并返回 */
	root = xmlDocGetRootElement(doc);
	if(root == NULL){
		fprintf(stderr, "empty document\n");
		xmlFreeDoc(doc);
		return NULL;
	}

	/* 确定根节点名是否为update，不是则返回 */
	if(xmlStrcmp(root->name, (const xmlChar *)"update")){
		fprintf(stderr, "document of the wrong type, root node != update");
		xmlFreeDoc(doc);
		return NULL;
	}

	new = xmlCopyNode(root, 1);
	xmlFreeDoc(doc);

	/* free packages node */
	cur = new->xmlChildrenNode;
	while(cur != NULL){
		/* 找到packages子节点 */
		if(!xmlStrcmp(cur->name, (const xmlChar *)"packages")){
			xmlUnlinkNode(cur);
			xmlFreeNode(cur);
		}
		cur = cur->next; /* 下一个子节点 */
	}

	return new;	
}
	
static char *bill_updates_get_maxid(xmlDocPtr doc, xmlNodePtr root)
{
	static char result[32];
	int maxid = 0;
	int n;

	xmlNodePtr update = NULL;
	xmlNodePtr id = NULL;
	xmlChar *key = NULL;
	
	update = root->xmlChildrenNode;
	while(update != NULL){
		/* 找到update子节点 */
		if(!xmlStrcmp(update->name, BAD_CAST "update")){
			id = update->xmlChildrenNode;
			while (id != NULL) {
				if (!xmlStrcmp(id->name, BAD_CAST "id")) {
					key = xmlNodeListGetString(doc, id->xmlChildrenNode, 1);
					n = atoi((const char *)key);
					if (maxid < n)
						maxid = n;
					xmlFree(key);
				}
				id = id->next;
			}
		}
		update = update->next; /* 下一个子节点 */
	}
	snprintf(result, 32, "%d", maxid);
	return result;
}

int bill_alter_updates(const char *docname, const char *srcname, unsigned char *md5sum)
{
	xmlDocPtr doc = NULL;
	xmlNodePtr root = NULL;
	xmlNodePtr node = NULL;
	char *id;
	char md5str[MD5_STR_LEN];

	/* if there is not doc */
	if (0 != access(docname, F_OK | W_OK)) {
		/* create a new xmldoc */
		if (NULL == (doc = xmlNewDoc(BAD_CAST"1.0"))) {
			fprintf(stderr, "Can`t create xml\n");
			return -1;
		}
		/* create and set the root node */
		root = xmlNewNode(NULL, BAD_CAST"updates");
		xmlDocSetRootElement(doc, root);
	} else {
		doc = xmlReadFile(docname, NULL, XML_PARSE_NOBLANKS);
		if(doc == NULL){
			fprintf(stderr, "Document not parse successfully. \n");
			return -1;
		}

		/* 获取文档根节点，若无内容则释放文档树并返回 */
		root = xmlDocGetRootElement(doc);
		if(root == NULL){
			fprintf(stderr, "empty document\n");
			xmlFreeDoc(doc);
			return -1;
		}

		/* 确定根节点名是否为updates，不是则返回 */
		if(xmlStrcmp(root->name, (const xmlChar *)"updates")){
			fprintf(stderr, "document of the wrong type, root node != updates\n");
			xmlFreeDoc(doc);
			return -1;
		}
	}

	if (NULL == (node = bill_extract_update(srcname))) {
		fprintf(stderr, "can`t extract %s\n", srcname);
		xmlFreeDoc(doc);
		return -1;
	}

	md5_num2str(md5sum, md5str);
	xmlNewChild(node, NULL, BAD_CAST"checksum", BAD_CAST md5str);


	if (NULL == xmlAddChild(root, node))
		return -1;
	
	id = bill_updates_get_maxid(doc, root);
	xmlSetProp(root, BAD_CAST"latest", BAD_CAST id);

	/* if you want to compress it use gz, set 1 */
	xmlSetCompressMode(0);

	/* if you want to add blank , set the last arg to 1 */
	if (-1 == xmlSaveFormatFileEnc(docname, doc, "utf-8", 1)) {
		fprintf(stderr, "Can`t save xml\n");
		return -1;
	}
	
	xmlFreeDoc(doc);

	return 0;
}


static xmllintReturnCode parseAndPrintFile(const char *filename, xmlSchemaPtr wxschemas)
{
	xmlDocPtr doc = NULL;
	xmllintReturnCode progresult = XMLLINT_RETURN_OK;

	doc = xmlReadFile(filename, NULL, XML_PARSE_COMPACT | XML_PARSE_BIG_LINES);

	/*
	 * If we don't have a document we might as well give up.  Do we
	 * want an error message here?  <sven@zen.org> */
	if (doc == NULL) {
		progresult = XMLLINT_ERR_UNCLASS;
		return progresult;
	}

#if 0
	xmlSaveCtxtPtr ctxt = NULL;
	ctxt = xmlSaveToFd(1, NULL, 0);
	if (ctxt != NULL) {
		if (xmlSaveDoc(ctxt, doc) < 0) {
			fprintf(stderr, "failed save to stdout\n");
			progresult = XMLLINT_ERR_OUT;
		}
		xmlSaveClose(ctxt);
	} else {
		progresult = XMLLINT_ERR_OUT;
	}
#endif

	if (wxschemas != NULL) {
		xmlSchemaValidCtxtPtr ctxt;
		int ret;


		ctxt = xmlSchemaNewValidCtxt(wxschemas);
		xmlSchemaSetValidErrors(ctxt,
				(xmlSchemaValidityErrorFunc) fprintf,
				(xmlSchemaValidityWarningFunc) fprintf,
				stderr);
		ret = xmlSchemaValidateDoc(ctxt, doc);
		if (ret == 0) {
			fprintf(stderr, "%s validates\n", filename);
		} else if (ret > 0) {
			fprintf(stderr, "%s fails to validate\n", filename);
			progresult = XMLLINT_ERR_VALID;
		} else {
			fprintf(stderr, "%s validation generated an internal error\n",
					filename);
			progresult = XMLLINT_ERR_VALID;
		}
		xmlSchemaFreeValidCtxt(ctxt);
	}

	xmlFreeDoc(doc);

	return progresult;
}

int bill_validate_xsd(const char *xmlfile, const char *xsdfile)
{
	xmlSchemaPtr wxschemas = NULL;

	xmlSchemaParserCtxtPtr ctxt;

	ctxt = xmlSchemaNewParserCtxt(xsdfile);
	xmlSchemaSetParserErrors(ctxt,
			(xmlSchemaValidityErrorFunc) fprintf,
			(xmlSchemaValidityWarningFunc) fprintf,
			stderr);
	wxschemas = xmlSchemaParse(ctxt);
	if (wxschemas == NULL) {
		xmlGenericError(xmlGenericErrorContext,
				"WXS schema %s failed to compile\n", xsdfile);
		return -1;
	}
	xmlSchemaFreeParserCtxt(ctxt);

	if (0 != parseAndPrintFile(xmlfile, wxschemas))
		return -1;

	return 0;
}
