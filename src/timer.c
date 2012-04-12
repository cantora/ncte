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
#include <sys/time.h>
#include <time.h>

#include "ts_util.h"

typedef struct timer_t {
	struct timespec start;
};

void timer_init(timer_t *timer) {
	clock_gettime(CLOCK_MONOTONIC, &timer->start);
}

void timer_cmp(timer_t *timer, uint32_t sec, uint32_t nsec) {
	timespec ts_amt, ts_now;

	ts_amt.tv_sec = sec;
	ts_amt.tv_nsec = nsec;

	clock_gettime(CLOCK_MONOTONIC, &ts_now);
	return ts_compare(ts_subtract(ts_now, timer->start), ts_amt);
}

