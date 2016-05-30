#include "update-create.h"
#include "update-demo.h"
#include "update-check.h"
#include "update-gen-repos.h"

int main(int argc, const char **argv)
{
	if (argc == 3) {

		if (!strcmp(argv[1], "-demo"))
			update_demo(argv[2]);
		else if (!strcmp(argv[1], "-gen"))
			update_gen_repos(argv[2]);
		else
			update_usage();
	} else if (argc == 4) {
		if (!strcmp(argv[1], "-check"))
			update_check(argv[2], argv[3]);
		else if (!strcmp(argv[1], "-create"))
			update_create(argv[2], argv[3]);
		else
			update_usage();
	} else {
		update_usage();
	}

	return 0;
}
