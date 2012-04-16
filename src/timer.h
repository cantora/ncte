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
#ifndef NCTE_TIMER_H
#define NCTE_TIMER_H

#include <time.h>
#include <sys/time.h>

struct timer_t {
	struct timeval start;
};

int timer_init(struct timer_t *timer);
int timer_thresh(struct timer_t *timer, int sec, int usec);

#endif /* NCTE_TIMER_H */