#include <stdio.h>
#include <errno.h>
#include "opt.h"

void print_conf(struct ncte_conf *c) {
	int i;

	printf("ncterm: \t\t'%s'\n", c->ncterm);
	printf("term: \t\t\t'%s'\n", c->term);
	printf("debug_file: \t\t'%s'\n", c->debug_file);
	printf("cmd: \t\t\t[");
	for(i = 0; i < c->cmd_argc; i++) {
		printf("'%s'", c->cmd_argv[i]);
		if(i < c->cmd_argc - 1)
			printf(", ");
	}
	printf("]\n");
}

int main(int argc, char *argv[]) {
	struct ncte_conf c;

	opt_init(&c);
	printf("DEFAULT:\n");
	print_conf(&c);

	if(opt_parse(argc, argv, &c) != 0) {
		printf("error: ");
		switch(errno) {
		case OPT_ERR_MISSING_ARG:
			printf("missing arg\n");
			break;
		case OPT_ERR_UNKNOWN_OPTION:
			printf("unknown option\n");
			break;
		}
	
		printf("error msg: %s\n", opt_err_msg);
		goto done;
	}
	
	printf("AFTER PARSING:\n");
	print_conf(&c);
done:	
	return 0;
}