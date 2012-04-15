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

static const int BUF_SIZE = 2048*4;

/* globals */
static struct {
	struct sigaction winch_act, prev_winch_act; /* structs for handing window size change */
	int master;		/* master pty */
	VTerm *vt;		/* libvterm virtual terminal pointer */
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

void loop(VTerm *vt, int master) {
	fd_set in_fds;
	ssize_t n_read, total_read;	
	int ch, buflen, status, force_refresh;
	char buf[BUF_SIZE]; /* IO buffer */
	struct timer_t timer;
	struct timeval tv_select;

	timer_init(&timer);
	while(1) {
		FD_ZERO(&in_fds);
		FD_SET(STDIN_FILENO, &in_fds);
		FD_SET(master, &in_fds);
		tv_select.tv_sec = 0;
		tv_select.tv_usec = 5000;

		/* most of this process's time is spent waiting for
		 * select's timeout, so we want to handle all
		 * SIGWINCH signals here
		 */
		unblock_winch(); 
		if(select(master + 1, &in_fds, NULL, NULL, &tv_select) == -1) {
			if(errno == EINTR) {
				block_winch();
				continue;
			}
			else
				err_exit(errno, "select");
		}
		block_winch();

		if(FD_ISSET(master, &in_fds) ) {
			total_read = 0;
			do {
				/*if(total_read > 0) 
					fprintf(stderr, "read %d/%d bytes\n", (int) total_read, BUF_SIZE);*/
				n_read = read(master, buf + total_read, 512);
			} while(n_read > 0 && ( (total_read += n_read) + 512 <= BUF_SIZE) );
			
			if(n_read == 0) { 
				return; /* the master pty is closed */
			}
			else if(n_read < 0 && errno != EAGAIN) {
				err_exit(errno, "error reading from pty master");
			}

			/*fprintf(stderr, "push_bytes: start\n");*/
			vterm_push_bytes(vt, buf, total_read);
			/*fprintf(stderr, "push_bytes: stop\n");*/
			timer_init(&timer);
		}

		if(force_refresh == 1 || (status = timer_thresh(&timer, 0, 10000) ) ) {
			screen_damage_win();
			screen_refresh();
			timer_init(&timer);
			force_refresh = 0;
		}
		else if(status < 0)
			err_exit(errno, "timer error");

		if(FD_ISSET(STDIN_FILENO, &in_fds) ) {
			while(vterm_output_get_buffer_remaining(vt) > 0 && screen_getch(&ch) == 0 ) {
				vterm_input_push_char(vt, VTERM_MOD_NONE, (uint32_t) ch);
			}
	
			buflen = vterm_output_get_buffer_current(vt);
			if(buflen > 0) {
				char buffer[buflen];
				buflen = vterm_output_bufferread(vt, buffer, buflen);
				write_n_or_exit(master, buffer, buflen);
			}

			force_refresh = 1;
		}
	}
}

void err_exit_cleanup(int error) {
	(void)(error);

	screen_free();
}

int main() {
	VTermScreen *vts;
	pid_t child;
	struct winsize size;
	
	/* block winch right off the bat
	 * because we want to defer 
	 * processing of it until 
	 * curses and vterm are set up
	 */
	block_winch();

	g.winch_act.sa_handler = handler_winch;
	sigemptyset(&g.winch_act.sa_mask);
	g.winch_act.sa_flags = 0;

	VTermScreenCallbacks screen_cbs = {
		//.damage = screen_damage,
		.movecursor = screen_movecursor,
		.bell = screen_bell,
		.settermprop = screen_settermprop
	};

	if(freopen("/tmp/ncte_log", "w", stderr) == NULL) {
		err_exit(errno, "redirect stderr");
	}
	
	setlocale(LC_ALL,"");
	putenv("TERM=screen");
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

	child = forkpty(&g.master, NULL, NULL, &size);
	if(child == 0) {
		char *shell = getenv("SHELL");
		char *args[2] = { shell, NULL };
		unblock_winch();
		execvp(shell, args);
		err_exit(errno, "cannot exec %s", shell);
	}

	if(set_nonblocking(g.master) != 0)
		err_exit(errno, "failed to set master fd to non-blocking");
	
	if(sigaction(SIGWINCH, &g.winch_act, &g.prev_winch_act) != 0)
		err_exit(errno, "sigaction failed");
	
	loop(g.vt, g.master);

cleanup:
	vterm_free(g.vt);
	screen_free();

	return 0;
}
