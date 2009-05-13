

#ifndef _main_h
#define _main_h

#include <mbn.h>
#include <pthread.h>

extern struct mbn_handler *mbn;
extern pthread_mutex_t lock;

void writelog(char *, ...);
unsigned long hex2int(const char *, int);

#endif
