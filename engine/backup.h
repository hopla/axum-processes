#ifndef _backup_h
#define _backup_h

#include <unistd.h>
#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

struct backup_info_struct {
  void *buffer;
  size_t length;
};
/* Logging functions. */
extern char backup_file[500];
size_t backup_read(void *buffer, size_t length);
void backup_write(const void *buffer, size_t length);
void backup_close(unsigned char remove_file);
size_t backup_open(void *buffer, size_t length, unsigned char write_only);
void *backup_thread_loop(void *arg);

extern volatile int main_quit;

#ifdef __cplusplus
}
#endif

#endif
