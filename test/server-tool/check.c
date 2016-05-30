#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

void parse_rpm(xmlDocPtr doc, xmlNodePtr cur)
{
	xmlChar* key;
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (!xmlStrcmp(cur->name, (const xmlChar *)"filename")) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			printf("filename: %s\n", key);
			xmlFree(key);
			
		} else if (!xmlStrcmp(cur->name, (const xmlChar *)"md5sum")) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			printf("md5sum: %s\n", key);
			xmlFree(key);
		}
		cur=cur->next; /* 下一个子节点 */
	}

	return;
}

/* 解析packages节点，打印rpm节点的内容 */
void parse_packages(xmlDocPtr doc, xmlNodePtr cur){
	cur=cur->xmlChildrenNode;
	while(cur != NULL){
		if(!xmlStrcmp(cur->name, (const xmlChar *)"rpm")) {
			parse_rpm(doc, cur);
		}
		/* 找到rpm子节点 */
		//printf("%d\n", i++);
		
		cur=cur->next; /* 下一个子节点 */
	}

	return;
}

/* 解析文档 */
static void parse_doc(char *docname){
	/* 定义文档和节点指针 */
	xmlDocPtr doc;
	xmlNodePtr cur;
	
	/* 进行解析，如果没成功，显示一个错误并停止 */
	//doc = xmlParseFile(docname);
	doc = xmlReadFile(docname, NULL, XML_PARSE_NOBLANKS);

	if(doc == NULL){
		fprintf(stderr, "Document not parse successfully. \n");
		return;
	}

	/* 获取文档根节点，若无内容则释放文档树并返回 */
	cur = xmlDocGetRootElement(doc);
	if(cur == NULL){
		fprintf(stderr, "empty document\n");
		xmlFreeDoc(doc);
		return;
	}

	/* 确定根节点名是否为update，不是则返回 */
	if(xmlStrcmp(cur->name, (const xmlChar *)"update")){
		fprintf(stderr, "document of the wrong type, root node != update");
		xmlFreeDoc(doc);
		return;
	}

	/* 遍历文档树 */
	cur = cur->xmlChildrenNode;
	while(cur != NULL){
		/* 找到packages子节点 */
		if(!xmlStrcmp(cur->name, (const xmlChar *)"packages")){
			printf("Name: %s\n", cur->name);
			parse_packages(doc, cur); /* 解析packages子节点 */
		}
//		puts(cur->name);
		cur = cur->next; /* 下一个子节点 */
	}

	xmlFreeDoc(doc); /* 释放文档树 */
	return;
}

int check_rpm(const char *xmlfile){
	parse_doc((char *)xmlfile);
	return 1;
}
