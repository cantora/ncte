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

#ifndef NCTE_VTERM_UTIL
#define NCTE_VTERM_UTIL

#include "vterm.h"

static inline int vterm_color_equal(const VTermColor *a, const VTermColor *b) {
	return (a->red == b->red) && (a->green == b->green) && (a->blue == b->blue);
}

static inline int vterm_color_dist_sq(const VTermColor *x, const VTermColor *y) {
	short r,g,b;
	
	r = (x->red - y->red);
	g = (x->green - y->green);
	b = (x->blue - y->blue);

	return r*r + g*g + b*b;
}

#endif /* NCTE_VTERM_UTIL */