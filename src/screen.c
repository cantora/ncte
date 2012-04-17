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

#include "screen.h"
#include <errno.h>
#include <stdint.h>
#include <assert.h>

#if defined(__APPLE__)
#	include <ncurses.h>
#else
#	include <ncursesw/curses.h>
#endif

#include "err.h"

#include "vterm_util.h"
#include "vterm_ansi_colors.h"

#define SCN_ANSI_COLORS 8
#define SCN_REQ_COLORS (SCN_ANSI_COLORS + 1) /* 8 ansi + default */
#define SCN_REQ_PAIRS SCN_REQ_COLORS*SCN_REQ_COLORS
#define SCN_TOTAL_ANSI_COLORS (SCN_ANSI_COLORS*2)

static VTerm *g_vt;

/* just a place holder for the default 
 * ansi color, not actually the color
 * 1,1,1.
 */
static const VTermColor DEFAULT_COLOR = {
	.red = 1,
	.green = 1,
	.blue = 1
};

int screen_init() {
	g_vt = NULL;

	initscr();
	if(raw() == ERR) 
		goto fail;
	if(noecho() == ERR) 
		goto fail;
	if(nodelay(stdscr, true) == ERR) 
		goto fail;
	if(keypad(stdscr, false) == ERR) /* libvterm interprets keys for us */
		goto fail;
	if(nonl() == ERR) 
		goto fail;

	return 0;
fail:
	errno = SCN_ERR_INIT;
	return -1;
}

void screen_free() {
	g_vt = NULL;

	if(endwin() == ERR)
		err_exit(0, "endwin failed!");
}

void screen_set_term(VTerm *term) {
	VTermState *state;

	g_vt = term;
	state = vterm_obtain_state(g_vt);
	/* have to cast default_color because the api isnt const correct */
	vterm_state_set_default_colors(state, (VTermColor *) &DEFAULT_COLOR, (VTermColor *) &DEFAULT_COLOR);	
}

void screen_dims(unsigned short *rows, unsigned short *cols) {
	getmaxyx(stdscr, *rows, *cols);
}

int screen_getch(int *ch) {
	*ch = getch();

	/* resize will be handled by signal
	 * so flush out all the resize keys
	 */
	if(*ch == KEY_RESIZE) 
		while(*ch == KEY_RESIZE) {
			*ch = getch();
		}

	if(*ch == ERR) 
		return -1;

	return 0;
}

/*void screen_err_msg(int error, char **msg) {
	switch(error) {
	case SCN_ERR_NONE:
		*msg = "no error";
		break;
	case SCN_ERR_INIT:
		*msg = "failed to initialize ncurses";
		break;
	case SCN_ERR_PAIRS:
		*msg = "terminal with more than SCN_REQ_PAIRS required";
		break;
	default:
		*msg = "unknown error";	
	}
} */

int screen_color_start() {
	int i,k;
	short pair, fg, bg;

	if(has_colors() == ERR) {
		errno = SCN_ERR_COLOR_NOT_SUPPORTED;
		goto fail;
	}

	if(start_color() == ERR) {
		errno = SCN_ERR_COLOR_NOT_SUPPORTED;
		goto fail;
	}
	
	if(use_default_colors() == ERR) {
		errno = SCN_ERR_COLOR_NO_DEFAULT;
		goto fail;		
	}
	
	if(COLORS < SCN_REQ_COLORS-1) {
		errno = SCN_ERR_COLOR_COLORS;
		goto fail;
	}

	/* for some reason we can just use pairs above 63 and its fine... */
	if(COLOR_PAIRS < SCN_REQ_PAIRS) {
		/*errno = SCN_ERR_COLOR_PAIRS;
		goto fail;*/
		fprintf(stderr, "warning: your terminal may not support 81 color pairs. if problems arise, try setting TERM to 'xterm-256color'\n");
	}

	for(i = 0; i < SCN_REQ_COLORS; i++) {
		for(k = 0; k < SCN_REQ_COLORS; k++) {
			pair = i*SCN_REQ_COLORS + k;
			if(pair == 0) { /* pair 0 already set to default fg on default bg */
				continue;
			}
			
			fg = ((SCN_REQ_COLORS - 1)-i > (SCN_ANSI_COLORS - 1))? -1 : (SCN_REQ_COLORS - 1)-i; /* => pair 0 = -1,-1 */
			bg = ((SCN_REQ_COLORS - 1)-k > (SCN_ANSI_COLORS - 1))? -1 : (SCN_REQ_COLORS - 1)-k;

			if(init_pair(pair, fg, bg) == ERR) {
				errno = SCN_ERR_COLOR_PAIR_INIT;
				goto fail;
			}
		}
	}
	
	return 0;
fail:
	return -1;
}

static inline int contained(int y, int x) {
	int maxx, maxy;

	getmaxyx(stdscr, maxy, maxx);	
	return (x < maxx) && (y < maxy);
}

static void color_index_to_curses_color_index(int index, short *curs_color, short *bright) {
	
	assert(index >= -1 && index < 16);
	
	if(index > 7) {
		index = index%8;
		*bright = 1;
	}
	else
		*bright = 0;

	switch(index) {
	case -1:
		*curs_color = 0;
		break;
	case VTERM_ANSI_BLACK:
		*curs_color = 8;
		break;
	case VTERM_ANSI_RED:
		*curs_color = 7;
		break;
	case VTERM_ANSI_GREEN:
		*curs_color = 6;
		break;
	case VTERM_ANSI_YELLOW:
		*curs_color = 5;
		break;
	case VTERM_ANSI_BLUE:
		*curs_color = 4;
		break;
	case VTERM_ANSI_MAGENTA:
		*curs_color = 3;
		break;
	case VTERM_ANSI_CYAN:
		*curs_color = 2;
		break;
	default: /* VTERM_ANSI_WHITE */
		*curs_color = 1;
	}
}

static int to_curses_color(VTermColor color, short *curs_color, short *bright) {
	int i;

	for(i = 0; i < SCN_TOTAL_ANSI_COLORS; i++) {
		if( vterm_color_equal(&color, &vterm_ansi_colors[i]) )
			break;
	}
	if(i == SCN_TOTAL_ANSI_COLORS) {
		if( vterm_color_equal(&color, &DEFAULT_COLOR) )
			i = -1;
		else
			goto fail;
	}
	
	color_index_to_curses_color_index(i, curs_color, bright);

	return 0;
fail:
	return -1;	
}

/* smallest dist^2 determines nearest
 * ansi color
 */
static void to_nearest_curses_color(VTermColor color, short *curs_color, short *bright) {
	int i, min_index, min_dist_sq, dist_sq;

	min_dist_sq = vterm_color_dist_sq(&color, &vterm_ansi_colors[0]);
	min_index = 0;
	for(i = 1; i < SCN_TOTAL_ANSI_COLORS; i++) {
		dist_sq = vterm_color_dist_sq(&color, &vterm_ansi_colors[i]);
		if(dist_sq <= min_dist_sq) {
			min_dist_sq = dist_sq;
			min_index = i;
		}
	}

	color_index_to_curses_color_index(min_index, curs_color, bright);
}

static void to_curses_pair(VTermColor fg, VTermColor bg, attr_t *attr, short *pair) {
	short curs_fg, curs_bg, bright_fg, bright_bg;
	
	if(to_curses_color(fg, &curs_fg, &bright_fg) != 0) {
		to_nearest_curses_color(fg, &curs_fg, &bright_fg);
		fprintf(stderr, "to_curses_pair: mapped fg color %d,%d,%d to color %d\n", fg.red, fg.green, fg.blue, curs_fg);
	}
	if(to_curses_color(bg, &curs_bg, &bright_bg) != 0) {
		to_nearest_curses_color(bg, &curs_bg, &bright_bg);
		fprintf(stderr, "to_curses_pair: mapped bg color %d,%d,%d to color %d\n", bg.red, bg.green, bg.blue, curs_bg);
	}
	
	*pair = SCN_REQ_COLORS*curs_fg + curs_bg;

	if(bright_fg)
		*attr |= A_BOLD;
}

static void to_curses_attr(const VTermScreenCell *cell, attr_t *attr, short *pair) {
	attr_t result = A_NORMAL;
	
	if(cell->attrs.bold != 0)
		result |= A_BOLD;

	if(cell->attrs.underline != 0)
		result |= A_UNDERLINE;

	if(cell->attrs.blink != 0)
		result |= A_BLINK;

	if(cell->attrs.reverse != 0)
		result |= A_REVERSE;

	to_curses_pair(cell->fg, cell->bg, &result, pair);
	*attr = result;
}

static void update_cell(VTermScreen *vts, VTermPos pos) {
	VTermScreenCell cell;
	attr_t attr;
	short pair;
	cchar_t cch;
	wchar_t *wch;
	wchar_t erasech = L' ';
	int maxx, maxy;

	getmaxyx(stdscr, maxy, maxx);

	/* sometimes this happens when
	 * a window resize recently happened
	 */
	if(!contained(pos.row, pos.col) ) {
		fprintf(stderr, "tried to update out of bounds cell at %d/%d %d/%d\n", pos.row, maxy-1, pos.col, maxx-1);
		return;
	}

	vterm_screen_get_cell(vts, pos, &cell);	
	to_curses_attr(&cell, &attr, &pair);

	wch = (cell.chars[0] == 0)? &erasech : (wchar_t *) &cell.chars[0];
	if(setcchar(&cch, wch, attr, pair, NULL) == ERR)
		err_exit(0, "setcchar failed");

	/* OMGROFLITZBAKWRDZ */
	pos.col = maxx - 1 - pos.col;

	if(move(pos.row, pos.col) == ERR)
		err_exit(0, "move failed: %d/%d, %d/%d\n", pos.row, maxy-1, pos.col, maxx-1);

	/*if(in_wch(&cur_cch) == ERR)
		err_exit(0, "in_wch failed");
	if(cch.attr == cur_cch.attr && wmemcmp(cch.chars, cur_cch.chars, CCHARW_MAX) == 0) {
		fprintf(stderr, "cur was same\n");
		return;
	}*/

	if(add_wch(&cch) == ERR && pos.row != (maxy-1) && pos.col != (maxx-1) )
		err_exit(0, "add_wch failed at %d/%d, %d/%d: ", pos.row, maxy-1, pos.col, maxx-1);

}

void screen_damage_win() {
	VTermRect win = {
		.start_row = 0,
		.start_col = 0,
		.end_row = 0,
		.end_col = 0
	};

	screen_dims((unsigned short *) &win.end_row, (unsigned short *) &win.end_col);
	screen_damage(win, NULL);
}

void screen_redraw() {
	if(redrawwin(stdscr) == ERR)
		err_exit(0, "redrawwin failed!");
}

void screen_refresh() {
	if(refresh() == ERR)
		err_exit(0, "refresh failed!");
}

int screen_damage(VTermRect rect, void *user) {
	VTermScreen *vts = vterm_obtain_screen(g_vt);
	VTermPos pos;
	int x,y,maxx,maxy;

	(void)(user); /* user not used */

	/* save cursor value */
	getyx(stdscr, y, x);
	getmaxyx(stdscr, maxy, maxx);

	for(pos.row = rect.start_row; pos.row < rect.end_row; pos.row++) {
		for(pos.col = rect.start_col; pos.col < rect.end_col; pos.col++) {
			update_cell(vts, pos);
		}
	}

	/* restore cursor (repainting shouldnt modify cursor) */
	if(move(y,x) == ERR) 
		err_exit(0, "move failed: %d/%d %d/%d", y, maxy, x, maxx);

	/*fprintf(stderr, "\tdamage: (%d,%d) (%d,%d) \n", rect.start_row, rect.start_col, rect.end_row, rect.end_col);*/

	return 1;
}

/*
int screen_moverect(VTermRect dest, VTermRect src, void *user) {
	
	//err_exit(0, "moverect called: dest={%d,%d,%d,%d}, src={%d,%d,%d,%d} (%d,%d)", dest.start_row, dest.start_col, dest.end_row, dest.end_col, src.start_row, src.start_col, src.end_row, src.end_col, maxy, maxx);
	return 0;
}*/

int screen_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user) {
	(void)(user); /* user is not used */
	(void)(oldpos); /* oldpos not used */
	(void)(visible);
		
	/* sometimes this happens when
	 * a window resize recently happened
	 */
	if(!contained(pos.row, pos.col) ) {
		fprintf(stderr, "tried to move cursor out of bounds to %d/%d %d/%d\n", pos.row, LINES-1, pos.col, COLS-1);
		return 1;
	}

	/* OMGROFLITZBAKWRDZ */
	pos.col = COLS - 1 - pos.col;

	if(move(pos.row, pos.col) == ERR)
		err_exit(0, "move failed: %d/%d %d/%d", pos.row, LINES-1, pos.col, COLS-1);

	/*fprintf(stderr, "\tmove cursor: %d, %d\n", pos.row, pos.col);*/

	return 1;
}

int screen_bell(void *user) {
	(void)(user);

	if(beep() == ERR) {
		fprintf(stderr, "bell failed\n");
	}

	/*if(flash() == ERR) {
		fprintf(stderr, "flash failed\n");
	}*/
	return 1;
}

int screen_settermprop(VTermProp prop, VTermValue *val, void *user) {
	
	(void)(user);

	/*fprintf(stderr, "settermprop: %d", prop);*/
	switch(prop) { 
	case VTERM_PROP_CURSORVISIBLE:
		/* fprintf(stderr, " (CURSORVISIBLE) = %02x", val->boolean); */
		/* will return ERR if cursor not supported, *
		 * so we dont bother checking return value  */
		curs_set(!!val->boolean); 
		break;
	case VTERM_PROP_CURSORBLINK: /* not sure if ncurses can change blink settings */
		/* fprintf(stderr, " (CURSORBLINK) = %02x", val->boolean); */
		break;
	case VTERM_PROP_REVERSE: /* this should be taken care of by update cell i think */
		/* fprintf(stderr, " (REVERSE) = %02x", val->boolean); */
		break;
	case VTERM_PROP_CURSORSHAPE: /* dont think curses can change cursor shape */
		/* fprintf(stderr, " (CURSORSHAPE) = %d", val->number); */
		break;
	default:
		;
	}

	/* fprintf(stderr, "\n"); */

	return 1;
}

/* this should be called before vterm_set_size
 * which will cause the term damage callback
 * to fully rewrite the screen.
 */
void screen_resize() {
	if(endwin() == ERR)
		err_exit(0, "endwin failed!");

	if(refresh() == ERR)
		err_exit(0, "refresh failed!");
}