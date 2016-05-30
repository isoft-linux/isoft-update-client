#include "packlist.h"

struct db_package_list *db_packlist_init(void)
{
	struct db_package_list *list = NULL;

	if (NULL == (list = malloc(sizeof(*list))))
		return NULL;

	memset(list, 0, sizeof(*list));

	return list;
}

void db_packlist_destroy(struct db_package_list *list)
{
	db_packlist_t *p = list;
	db_packlist_t *tmp = NULL;
	
	while (NULL != p) {
		tmp = p;
		p = p->next;
		free(tmp);
	}
}

int db_packlist_add(struct db_package_list *list, const struct db_package *pack)
{
	db_packlist_t *node = NULL;

	/* create new node */
	if (NULL == (node = malloc(sizeof(*node))))
		return -1;
	node->package = *pack;
	node->next = NULL;

	/* insert newnode to list */
	node->next = list->next;
	list->next = node;

	return 0;

}

/* the locate funtion return the previous address of target node */
static struct db_package_list *_db_packlist_locate_(struct db_package_list *list, const struct db_package *pack)
{
	db_packlist_t *p = NULL;
	
	if (NULL == list || NULL == pack)
		return NULL;
	
	p = list;

	while (NULL != p->next) {
		if (!strcmp(pack->name, p->next->package.name))
			return p;
		else if (!memcmp(pack->md5, p->next->package.md5, DB_PACK_MD5_LEN))
			return p;
		p = p->next;
	}
	return NULL;
}

int db_packlist_del(struct db_package_list *list, const char *packname)
{
	db_packlist_t *p = NULL;
	db_packlist_t *tmp = NULL;
	db_pack_t pack;
	
	strncpy(pack.name, packname, DB_PACK_FNAME_LEN);
	if (NULL == (p = _db_packlist_locate_(list, &pack)))
		return -1;
	tmp = p->next;
	p->next = tmp->next;
	free(tmp);
	return 0;
}

int db_packlist_search(struct db_package_list *list, struct db_package *pack)
{
	db_packlist_t *p = NULL;
	
	if (NULL == (p = _db_packlist_locate_(list, pack)))
		return -1;
	memcpy(pack, &p->next->package, sizeof(*pack));
	
	return 0;
}

int db_packlist_get_number(struct db_package_list *list)
{
	db_packlist_t *p = NULL;
	int n = 0;

	if (NULL == list)
		return 0;
	p = list->next;
	
	while (NULL != p) {
		n++;
		p = p->next;
	}

	return n;
}

int db_packlist_get_all(
		struct db_package_list *list, 
		int callback(
			void *arg, 
			const struct db_package *pack),
		void *arg)
{
	db_packlist_t *p = NULL;
	int n = 0;

	if (NULL == list || NULL == callback)
		return 0;

	p = list->next;
	while (NULL != p) {
		n++;
		if (0 != callback(arg, &p->package))
			break;
		p = p->next;
	}
	
	return n;
}

void _db_packlist_debug_(db_packlist_t *list)
{
	db_packlist_t *p = list->next;
	char md5str[MD5_STR_LEN];

	while (NULL != p) {
		md5_num2str(p->package.md5, md5str);
		printf("%s  -->  ", md5str);
		printf("%s\n", p->package.name);
		p = p->next;
	}
	
}
