#include "opt.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>

struct opt_desc {
	const char *name;
	const char *usage;
	const char *desc;
	const void *default_val;
	const struct option lopt; 	/* getopt long option struct */
};

char opt_err_msg[64];
static const char *default_argv[] = NCTE_DEFAULT_ARGV;

#define LONG_ONLY_VAL(_index) ((1 << 11) + _index)

static const struct opt_desc ncte_options[] = {
	{
#define OPT_TERM "term"
#define OPT_TERM_INDEX 0
		.name = OPT_TERM,
		.usage = " TERMNAME",
		.desc = "sets the TERM environment variable to TERMNAME",
		.default_val = NULL, /* when .default_val is NULL, use the value of TERM in current environment */
		.lopt = {OPT_TERM, 1, 0, LONG_ONLY_VAL(OPT_TERM_INDEX)}
	},
	{
#define OPT_NCTERM "ncterm"
#define OPT_NCTERM_INDEX 1
		.name = OPT_NCTERM,
		.usage = " TERMNAME",
		.desc = "sets the terminal profile used by NCurses",
		.default_val = NULL, /* use current TERM environment variable */
		.lopt = {OPT_NCTERM, 1, 0, LONG_ONLY_VAL(OPT_NCTERM_INDEX)}
	},
	{
#define OPT_DEBUG "debug"
#define OPT_DEBUG_INDEX 2
		.name = OPT_DEBUG,
		.usage = " FILENAME",
		.desc = "collect debug messages into FILENAME",
		.default_val = NULL, /* redirect stdout to /dev/null */
		.lopt = {OPT_DEBUG, 1, 0, 'd'}
	}
}; /* ncte_options */
#define NCTE_OPTLEN 3

void init_long_options(struct option *long_options, char *optstring) {
	int i, os_i;

	os_i = 0;
	optstring[os_i++] = '+'; /* stop processing after first non-option */
	optstring[os_i++] = ':'; /* return : for missing arg */

	for(i = 0; i < NCTE_OPTLEN; i++) {
		long_options[i] = ncte_options[i].lopt;
		if(ncte_options[i].lopt.val > 0 && ncte_options[i].lopt.val < 256) {
			optstring[os_i++] = ncte_options[i].lopt.val;
			switch(ncte_options[i].lopt.has_arg) {
			case 2:
				optstring[os_i++] = ':';
				/* fall through */
			case 1: 
				optstring[os_i++] = ':';
				break;
			case 0:
				break; /* do nothing */
			default:
				assert(0); /* shouldnt get here */
			}				
		}
	}
	
	long_options[i].name = NULL;
	long_options[i].has_arg = 0;
	long_options[i].flag = NULL;
	long_options[i].val = 0;	
	optstring[os_i++] = '\0'; 
}

void opt_init(struct ncte_conf *conf) {
#define DEFAULT_VAL(_name) ncte_options[OPT_##_name##_INDEX].default_val
	conf->term = DEFAULT_VAL(TERM);
	conf->ncterm = DEFAULT_VAL(NCTERM);
	conf->debug_file = DEFAULT_VAL(DEBUG);
	conf->cmd_argc = 1;
	conf->cmd_argv = default_argv;
}

int opt_parse(int argc, char *const argv[], struct ncte_conf *conf) {
	int index, c;
	char optstring[64];
	struct option long_options[NCTE_OPTLEN+1];
		
	/* dont print error message */
	opterr = 0;
	init_long_options(long_options, optstring);
	
	index = 0;
	while( (c = getopt_long(argc, argv, optstring, long_options, &index)) != -1 ) {
		if(optarg && optarg[0] == '-') {
			if(optopt == 0) 
				snprintf(opt_err_msg, 64, "option '%s' missing argument", argv[optind-2]);
			else
				snprintf(opt_err_msg, 64, "option '-%c' missing argument'", optopt); 

			errno = OPT_ERR_MISSING_ARG;
			goto fail;
		}
			
		switch (c) {
		case 'd': /* debug file */
			conf->debug_file = optarg;
			break;

		case LONG_ONLY_VAL(OPT_TERM_INDEX):
			conf->term = optarg;
			break;

		case LONG_ONLY_VAL(OPT_NCTERM_INDEX):
			conf->ncterm = optarg;
			break;
			
		case '?':
			if(optopt == 0)
				snprintf(opt_err_msg, 64, "unknown option '%s'", argv[optind-1]);
			else
				snprintf(opt_err_msg, 64, "unknown option '-%c'", optopt);

			errno = OPT_ERR_UNKNOWN_OPTION;
			goto fail;
			break;
		
		case ':': 
			if(optopt == 0) 
				snprintf(opt_err_msg, 64, "option '%s' missing argument", argv[optind-1]);
			else
				snprintf(opt_err_msg, 64, "option '-%c' missing argument'", optopt); 

			errno = OPT_ERR_MISSING_ARG;
			goto fail;
			break;

		default:
			printf("getopt returned char '%c' (%d)\n", c, c);
			assert(0); /* shouldnt get here */
		}
	}

	/* non-default command was passed (as non-option args) */
	if(optind < argc) {
		conf->cmd_argc = argc - optind;
		conf->cmd_argv = (const char *const *) &argv[optind];
	}	

	return 0;
fail:
	return -1;			   
}
