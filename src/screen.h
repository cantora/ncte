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

#ifndef NCTE_SCREEN_H
#define NCTE_SCREEN_H

#include "vterm.h"

enum screen_err {
	SCN_ERR_NONE = 0,
	SCN_ERR_INIT,
	SCN_ERR_COLOR_PAIRS,
	SCN_ERR_COLOR_COLORS,
	SCN_ERR_COLOR_NOT_SUPPORTED,
	SCN_ERR_COLOR_NO_DEFAULT,
	SCN_ERR_COLOR_PAIR_INIT

};

int screen_init();
void screen_free();
void screen_set_term(VTerm *term);
void screen_dims(unsigned short *rows, unsigned short *cols);
int screen_getch(int *ch);
/*void screen_err_msg(int error, char **msg);*/
int screen_color_start();
int screen_damage(VTermRect rect, void *user);
int screen_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user);
int screen_bell(void *user);
int screen_settermprop(VTermProp prop, VTermValue *val, void *user);
void screen_resize();
#endif /* NCTE_SCREEN_H */