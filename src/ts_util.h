#ifndef NCTE_TS_UTIL_H
#define NCTE_TS_UTIL_H

int ts_compare(struct timespec time1, struct timespec time2);
struct timespec ts_subtract(struct timespec time1, struct timespec time2);

#endif /* NCTE_TS_UTIL_H */