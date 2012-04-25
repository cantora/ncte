/* 
 * Copyright 2012 anthony cantor
 * This file is part of ncte.
 *
 * ncte is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ncte is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with ncte.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "opt.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>

#define NCTE_QUOTE(_EXP) #_EXP
#define NCTE_EXPAND_QUOTE(_EXP) NCTE_QUOTE(_EXP)

struct opt_desc {
	const char *name;
	const char *usage;
	const char *const desc[3];
	const void *default_val;
	const struct option lopt; 	/* getopt long option struct */
};

char opt_err_msg[64];
static const char *default_argv[] = NCTE_DEFAULT_ARGV;
static const int default_argc = (sizeof(default_argv)/sizeof(char *)) - 1;

#define LONG_ONLY_VAL(_index) ((1 << 11) + _index)

static const struct opt_desc ncte_options[] = {
	{
#define OPT_TERM "term"
#define OPT_TERM_INDEX 0
		.name = OPT_TERM,
		.usage = " TERMNAME",
		.desc = {"sets the TERM environment variable to TERMNAME", 
				 "\tdefault: the current value of TERM in the environment", NULL},
		.default_val = NULL, /* when .default_val is NULL, use the value of TERM in current environment */
		.lopt = {OPT_TERM, 1, 0, LONG_ONLY_VAL(OPT_TERM_INDEX)}
	},
	{
#define OPT_NCTERM "ncterm"
#define OPT_NCTERM_INDEX 1
		.name = OPT_NCTERM,
		.usage = " TERMNAME",
		.desc = {"sets the terminal profile used by ncurses",
				 "\tdefault: the current value of TERM in the environment", NULL},
		.default_val = NULL, /* use current TERM environment variable */
		.lopt = {OPT_NCTERM, 1, 0, LONG_ONLY_VAL(OPT_NCTERM_INDEX)}
	},
	{
#define OPT_DEBUG "debug"
#define OPT_DEBUG_INDEX 2
		.name = OPT_DEBUG,
		.usage = " FILENAME",
		.desc = {"collect debug messages into FILENAME", NULL},
		.default_val = NULL, /* redirect stderr to /dev/null */
		.lopt = {OPT_DEBUG, 1, 0, 'd'}
	},
	{
#define OPT_HELP "help"
#define OPT_HELP_INDEX 3
		.name = OPT_HELP,
		.usage = NULL,
		.desc = {"display this message", NULL},
		.default_val = NULL, 
		.lopt = {OPT_HELP, 0, 0, 'h'}
	}
}; /* ncte_options */
#define NCTE_OPTLEN 4

static void init_long_options(struct option *long_options, char *optstring) {
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
	const char *shell;

#define DEFAULT_VAL(_name) ncte_options[OPT_##_name##_INDEX].default_val
	conf->term = DEFAULT_VAL(TERM);
	conf->ncterm = DEFAULT_VAL(NCTERM);
	conf->debug_file = DEFAULT_VAL(DEBUG);
	
	shell = getenv("SHELL");
	if(shell != NULL) {
		default_argv[0] = shell;
		default_argv[1] = NULL;
		conf->cmd_argc = 1;
	}
	else {
		conf->cmd_argc = default_argc;
	}
	conf->cmd_argv = default_argv;
}

void opt_print_usage(int argc, const char *const argv[]) {
	fprintf(stdout, "usage: %s [OPTIONS] [CMD [ARG1, ...]]\n", (argc > 0)? argv[0] : "");
}



void opt_print_help(int argc, const char *const argv[]) {
	int i, k, f1_amt;
#define F1_SIZE 32
#define F1_WIDTH 26
	char f1[F1_SIZE];
	
	opt_print_usage(argc, argv);
	
	fprintf(stdout, "\n%-" NCTE_EXPAND_QUOTE(F1_WIDTH) "s%s\n", "  CMD", "run CMD with arguments instead of the default");
	fprintf(stdout, "%-" NCTE_EXPAND_QUOTE(F1_WIDTH) "s\tdefault: the value of SHELL in the environment\n", "");
	fprintf(stdout, "%-" NCTE_EXPAND_QUOTE(F1_WIDTH) "s\tor if SHELL undefined: ", "");
	for(k = 0; k < default_argc; k++)
		fprintf(stdout, "%s ", default_argv[k]);
	
	fprintf(stdout, "\nOPTIONS:\n");
	for(i = 0; i < NCTE_OPTLEN; i++) {
		f1_amt = 0;
		f1_amt += snprintf(f1, F1_SIZE, "  --%s", ncte_options[i].name);
		if(f1_amt < F1_SIZE && ncte_options[i].lopt.val >= 0 && ncte_options[i].lopt.val < 256) 
			f1_amt += snprintf(f1+f1_amt, F1_SIZE-f1_amt, "|-%c", ncte_options[i].lopt.val);
		
		if(f1_amt < F1_SIZE && ncte_options[i].lopt.has_arg && ncte_options[i].usage != NULL)
			snprintf(f1+f1_amt, F1_SIZE-f1_amt, "%s", ncte_options[i].usage);

		/*desc = ncte_options[i].desc;
		do {
			fprintf(stdout, "%-" NCTE_EXPAND_QUOTE(F1_WIDTH) "s%s\n", f1, *desc);
		} while( *(desc++) != NULL );*/
		for(k = 0; ncte_options[i].desc[k] != NULL; k++) 
			fprintf(stdout, "%-" NCTE_EXPAND_QUOTE(F1_WIDTH) "s%s\n", (k == 0? f1 : ""), ncte_options[i].desc[k]);
	}
	
	fputc('\n', stdout);
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
			snprintf(opt_err_msg, 64, "option '%s' missing argument", argv[optind-2]);
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

		case 'h':
			errno = OPT_ERR_HELP;
			goto fail;
			break;
			
		case '?':
			snprintf(opt_err_msg, 64, "unknown option '%s'", argv[optind-1]);
			errno = OPT_ERR_UNKNOWN_OPTION;
			goto fail;
			break;
		
		case ':': 
			snprintf(opt_err_msg, 64, "option '%s' missing argument", argv[optind-1]);
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
