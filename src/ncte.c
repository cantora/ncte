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

#include <errno.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <signal.h>
#include <fcntl.h>

#if defined(__FreeBSD__)
#	include <libutil.h>
#	include <termios.h>
#elif defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__)
#	include <termios.h>
#	include <util.h>
#else
#	include <pty.h>
#endif

#include "vterm.h"

#include "screen.h"
#include "err.h"
#include "timer.h"
#include "opt.h"

#define BUF_SIZE 2048*4

/* globals */
static struct {
	struct sigaction winch_act, prev_winch_act; /* structs for handing window size change */
	int master;		/* master pty */
	VTerm *vt;		/* libvterm virtual terminal pointer */
	char buf[BUF_SIZE]; /* IO buffer */
	struct ncte_conf conf;
} g; 

static int set_nonblocking(int fd) {
	int opts;

	opts = fcntl(fd, F_GETFL);
	if (opts < 0) 
		return -1;

	opts = (opts | O_NONBLOCK);
	if (fcntl(fd, F_SETFL, opts) < 0)
		return -1;

	return 0;
}

void change_winch(int how) {
	sigset_t set;

	if(sigemptyset(&set) != 0) 
		err_exit(errno, "sigemptyset failed"); 

	if(sigaddset(&set, SIGWINCH) != 0) 
		err_exit(errno, "sigaddset failed");

	if(sigprocmask(how, &set, NULL) != 0)
		err_exit(errno, "sigprocmask failed");
}

static inline void block_winch() {
	change_winch(SIG_BLOCK);
}

static inline void unblock_winch() {
	change_winch(SIG_UNBLOCK);
}

/* handler for SIGWINCH */
static void handler_winch(int signo) {
	struct winsize size;

	vterm_screen_flush_damage(vterm_obtain_screen(g.vt) );

	if(g.prev_winch_act.sa_handler != NULL) {
		(*g.prev_winch_act.sa_handler)(signo);
	}

	screen_resize();
	screen_dims(&size.ws_row, &size.ws_col);

	fprintf(stderr, "resize to %d,%d\n", size.ws_row, size.ws_col);
	if(ioctl(g.master, TIOCSWINSZ, &size) != 0) 
		err_exit(errno, "ioctl(TIOCSWINSZ) failed");

	/* this should cause full screen damage and screen will be repainted */
	vterm_set_size(g.vt, size.ws_row, size.ws_col);
}

static void write_n_or_exit(int fd, const void *buf, size_t n) {
	ssize_t n_write, error;

	if( (n_write = write(fd, buf, n) ) != (ssize_t) n) {
		if(n_write > 0)
			error = 0;
		else
			error = errno;
		
		err_exit(error, "partial write");
	}
}

static void process_input(VTerm *vt, int master) {
	int ch;
	size_t buflen;

	while(vterm_output_get_buffer_remaining(vt) > 0 && screen_getch(&ch) == 0 ) {
		vterm_input_push_char(vt, VTERM_MOD_NONE, (uint32_t) ch);
	}
		
	while( (buflen = vterm_output_get_buffer_current(vt) ) > 0) {
		buflen = (buflen < BUF_SIZE)? buflen : BUF_SIZE;
		buflen = vterm_output_bufferread(vt, g.buf, buflen);
		write_n_or_exit(master, g.buf, buflen);
	}
}

static int process_output(VTerm *vt, int master) {
	ssize_t n_read, total_read;

	total_read = 0;
	do {
		n_read = read(master, g.buf + total_read, 512);
	} while(n_read > 0 && ( (total_read += n_read) + 512 <= BUF_SIZE) );
	
	if(n_read == 0 || (n_read < 0 && errno == EIO) ) { 
		return -1; /* the master pty is closed, return -1 to signify */
	}
	else if(n_read < 0 && errno != EAGAIN) {
		err_exit(errno, "error reading from pty master (n_read = %d)", n_read);
	}

	vterm_push_bytes(vt, g.buf, total_read);
	
	return 0;
}

void loop(VTerm *vt, int master) {
	fd_set in_fds;
	int status, force_refresh, just_refreshed;

	struct timer_t inter_io_timer, refresh_expire;
	struct timeval tv_select;
	struct timeval *tv_select_p;

	timer_init(&inter_io_timer);
	timer_init(&refresh_expire);
	/* dont initially need to worry about inter_io_timer's need to timeout */
	just_refreshed = 1;

	while(1) {
		FD_ZERO(&in_fds);
		FD_SET(STDIN_FILENO, &in_fds);
		FD_SET(master, &in_fds);

		/* if we just refreshed the screen there
		 * is no need to timeout the select wait. 
		 * however if we havent refreshed on the last
		 * iteration we need to make sure that inter_io_timer
		 * is given a chance to timeout and cause a refresh.
		 */
		tv_select.tv_sec = 0;
		tv_select.tv_usec = 5000;
		tv_select_p = (just_refreshed == 0)? &tv_select : NULL;

		/* most of this process's time is spent waiting for
		 * select's timeout, so we want to handle all
		 * SIGWINCH signals here
		 */
		unblock_winch(); 
		if(select(master + 1, &in_fds, NULL, NULL, tv_select_p) == -1) {
			if(errno == EINTR) {
				block_winch();
				continue;
			}
			else
				err_exit(errno, "select");
		}
		block_winch();

		if(FD_ISSET(master, &in_fds) ) {
			if(process_output(vt, master) != 0)
				return;

			timer_init(&inter_io_timer);
		}

		/* this is here to make sure really long bursts dont 
		 * look like frozen I/O. the user should see characters
		 * whizzing by if there is that much output.
		 */
		if( (status = timer_thresh(&refresh_expire, 0, 300000)) ) { 
			force_refresh = 1; /* must refresh at least 3 times per second */
		}
		else if(status < 0)
			err_exit(errno, "timer error");
		
		/* if master pty is 'bursting' with I/O at a quick rate
		 * we want to let the burst finish (up to a point: see refresh_expire)
		 * and then refresh the screen, otherwise we waste a bunch of time
		 * refreshing the screen with stuff that just gets scrolled off
		 */
		if(force_refresh != 0 || (status = timer_thresh(&inter_io_timer, 0, 10000) ) == 1 ) {
			screen_refresh();
			timer_init(&inter_io_timer);
			timer_init(&refresh_expire);
			force_refresh = 0;
			just_refreshed = 1;
		}
		else if(status < 0)
			err_exit(errno, "timer error");
		else
			just_refreshed = 0; /* didnt refresh the screen on this iteration */

		if(FD_ISSET(STDIN_FILENO, &in_fds) ) {
			process_input(vt, master);

			force_refresh = 1;
		}
	}
}

void err_exit_cleanup(int error) {
	(void)(error);

	screen_free();
}

int main(int argc, char *argv[]) {
	VTermScreen *vts;
	pid_t child;
	struct winsize size;
	const char *debug_file, *env_term;
	struct termios child_termios;
	char joke[] = 
"          _     _____     _____                 _ \n\r"
"         | |   | ____|   |  _  |               | |\n\r"
"         | |__ / /   __ _| '/| |__  ___ __   __| |\n\r"
"         | `_ /\\ \\  |__' | |/  |/ /\\ \\ `_ / \\ _' |\n\r"
"         | |_( / /___. | / /_| \\ <  >| |_( | | | |\n\r"
"         |_,__\\ /____\\ |_|/___\\ \\_\\/_/_,__\\|_| |_|\n\r"
"\n\r"
"\n\r"
"                                        !ll3hs zdrawkcab ruoy yojne\n\r"
"                                lliw uoy D^ ro \"tixe\" epyt uoy nehw\n\r"
"                                  .llehs lamron ruoy otni kcab pord\n\r"
"                                   snigol erutuf no siht tneverp ot\n\r"
"                              ruoy ni redlof eliforp_hsab. eht tide\n\r"
"                                          eteled dna yrotcerid emoh\n\r"
"                                              enil \"shell/nib/\" eht\n\r";


	/* block winch right off the bat
	 * because we want to defer 
	 * processing of it until 
	 * curses and vterm are set up
	 */
	block_winch();

	g.winch_act.sa_handler = handler_winch;
	sigemptyset(&g.winch_act.sa_mask);
	g.winch_act.sa_flags = 0;
	
	opt_init(&g.conf);

	if(tcgetattr(STDIN_FILENO, &child_termios) != 0) {
		/* tcgetattr failed, we dont have a tty
		 * so just invoke bash 
		 */
		argv[0] = "/bin/bash";
		execvp(argv[0], argv);
		err_exit(errno, "cannot exec %s", argv[0]); 
	}

  	VTermScreenCallbacks screen_cbs = {
		.damage = screen_damage,  /* for now we refresh the screen at our own rate based on a timer */
		.movecursor = screen_movecursor,
		.bell = screen_bell,
		.settermprop = screen_settermprop
	};

	if(g.conf.debug_file != NULL)
		debug_file = g.conf.debug_file;
	else
		debug_file = "/dev/null";
		
	if(freopen(debug_file, "w", stderr) == NULL) {
		err_exit(errno, "redirect stderr");
	}
		
	setlocale(LC_ALL,"");
	
	env_term = getenv("TERM"); 
	/* set the PARENT terminal profile to --ncterm if supplied.
	 * have to set this before calling screen_init to affect ncurses */
	if(g.conf.ncterm != NULL)
		if(setenv("TERM", g.conf.ncterm, 1) != 0)
			err_exit(errno, "error setting environment variable: %s", g.conf.ncterm);
					
	if(screen_init() != 0)
		err_exit(0, "screen_init failure");

	err_exit_cleanup_fn(err_exit_cleanup);

	screen_dims(&size.ws_row, &size.ws_col);
	g.vt = vterm_new(size.ws_row, size.ws_col);
	screen_set_term(g.vt);

	vterm_parser_set_utf8(g.vt, 1);

	vts = vterm_obtain_screen(g.vt);
	vterm_screen_enable_altscreen(vts, 1);
	
	vterm_screen_reset(vts);

	vterm_screen_set_callbacks(vts, &screen_cbs, NULL);
	/*vterm_screen_set_damage_merge(vts, VTERM_DAMAGE_SCROLL);*/

	if(screen_color_start() != 0) {
		printf("failed to start color\n");
		goto cleanup;
	}

	child = forkpty(&g.master, NULL, &child_termios, &size);
	if(child == 0) {
		const char *child_term;
		/* set terminal profile for CHILD to supplied --term arg if exists 
		 * otherwise make sure to reset the TERM variable to the initial
		 * environment, even if it was null.
		 */
		child_term = NULL;
		if(g.conf.term != NULL) 
			child_term = g.conf.term;
		else if(env_term != NULL) 
			child_term = env_term;
			
		if(child_term != NULL) {
			if(setenv("TERM", child_term, 1) != 0)
				err_exit(errno, "error setting environment variable: %s", child_term);
		}
		else {
			if(unsetenv("TERM") != 0)
				err_exit(errno, "error unsetting environment variable: TERM");
		}

		argv[0] = "/bin/bash";
		unblock_winch();
		execvp(argv[0], argv);
		err_exit(errno, "cannot exec %s", argv[0]);
	}

	if(set_nonblocking(g.master) != 0)
		err_exit(errno, "failed to set master fd to non-blocking");
	
	if(sigaction(SIGWINCH, &g.winch_act, &g.prev_winch_act) != 0)
		err_exit(errno, "sigaction failed");

	vterm_push_bytes(g.vt, joke, sizeof(joke));
	loop(g.vt, g.master);

cleanup:
	vterm_free(g.vt);
	screen_free();

	return 0;
}
