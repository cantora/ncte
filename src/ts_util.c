/*********************************************************************************
this code copied from http://www.geonius.com/software/index.html#libgpl
**********************************************************************************
The libraries (*) and applications in the CSOFT distribution are covered by
the MIT License (see http://www.opensource.org/licenses/mit-license.php ).

    (*) The "libxdr" library is covered by Sun's license, which is included
        at the beginning of each source file.  It is similar to the MIT
        License: (i) you are free to use and modify the software and
        (ii) the software is provided "as is" with no warranties of any
        kind from Sun.

Basically, you are free to use the software however you see fit, in commercial
or non-commerical applications.  I only ask that, if your time permits, you
report any bugs or portability problems you encounter.  Suggestions for
improvements and enhancements are welcome, but I do not guarantee I will act
upon them.

				Alex Measday
				c.a.measday@ieee.org
				http://wwww.geonius.com/

********************************************************************************

Copyright (c) 2009  Alex Measday

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*********************************************************************************/
#include <time.h>
#include <sys/time.h>


int ts_compare(struct timespec time1, struct timespec time2) {

	if (time1.tv_sec < time2.tv_sec)
		return (-1) ;				/* Less than. */
	else if (time1.tv_sec > time2.tv_sec)
		return (1) ;				/* Greater than. */
	else if (time1.tv_nsec < time2.tv_nsec)
		return (-1) ;				/* Less than. */
	else if (time1.tv_nsec > time2.tv_nsec)
		return (1) ;				/* Greater than. */
	else
		return (0) ;				/* Equal. */
}

/* subtract time2 from time1 */
struct timespec ts_subtract(struct timespec time1, struct timespec time2) {
	/* Local variables. */
	struct  timespec  result ;
	
	/* Subtract the second time from the first. */
	
	if ((time1.tv_sec < time2.tv_sec) ||
	  ((time1.tv_sec == time2.tv_sec) &&
	   (time1.tv_nsec <= time2.tv_nsec))) {		/* TIME1 <= TIME2? */
		result.tv_sec = result.tv_nsec = 0 ;
	} else {						/* TIME1 > TIME2 */
		result.tv_sec = time1.tv_sec - time2.tv_sec ;
		if (time1.tv_nsec < time2.tv_nsec) {
			result.tv_nsec = time1.tv_nsec + 1000000000L - time2.tv_nsec ;
			result.tv_sec-- ;				/* Borrow a second. */
		} else {
			result.tv_nsec = time1.tv_nsec - time2.tv_nsec ;
		}
	}
	
	return (result) ;
}