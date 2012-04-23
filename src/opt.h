#ifndef NCTE_OPT_H
#define NCTE_OPT_H

struct ncte_conf {
	const char *ncterm;
	const char *term;
	const char *debug_file;
	int cmd_argc;
	const char *const *cmd_argv;
};

enum opt_err {
	OPT_ERR_NONE = 0,
	OPT_ERR_UNKNOWN_OPTION,
	OPT_ERR_MISSING_ARG
};

extern char opt_err_msg[];
void opt_init(struct ncte_conf *options);
int opt_parse(int argc, char *const argv[], struct ncte_conf *options);
//void opt_usage(struct ncte_conf *options);
//void opt_help(struct ncte_conf *options);

#endif /* NCTE_OPT_H */