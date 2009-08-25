#include "backup.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>

FILE *backupfd = NULL;
char backup_file[500];
pthread_t backup_thread;
backup_info_struct backup_info;
backup_info_struct old_backup_info;

void backup_open(void *buffer, size_t length) {
  if(backupfd != NULL)
    fclose(backupfd);
  if((backupfd = fopen(backup_file, "r")) == NULL) {
    log_write("No backup file: %s", strerror(errno));
  }
  else
  {
    backup_info.buffer = buffer;
    backup_info.length = length;
    old_backup_info.buffer = calloc(length, 1);
    old_backup_info.length = length;

    size_t readed_length = backup_read(old_backup_info.buffer, length);
    if (readed_length == 1)
    {
      log_write("Correct size, use backup");
      memcpy(buffer, old_backup_info.buffer, length);
    }
  }

  if(backupfd != NULL)
    fclose(backupfd);
  if((backupfd = fopen(backup_file, "w+")) == NULL) {
    fprintf(stderr, "Couldn't open backup file: %s\n", strerror(errno));
    exit(1);
  }

  log_write("Start backup thread...");
  pthread_create(&backup_thread, NULL, backup_thread_loop, (void *)&backup_info);
}

void backup_close() {
  if(backupfd != NULL)
    fclose(backupfd);
  backupfd = NULL;

  free(old_backup_info.buffer);
}

size_t backup_read(void *buffer, size_t length)
{
  size_t readed_length = 0;
  if (backupfd != NULL)
  {
    rewind(backupfd);
    readed_length = fread(buffer, length, 1, backupfd);
    if (readed_length == 0)
    {
      log_write("ferror returns %d", ferror(backupfd));
    }
  }
  return readed_length;
}

void backup_write(const void *buffer, size_t length)
{
  if (backupfd != NULL)
  {
    log_write("write backup...");
    rewind(backupfd);
    fwrite(buffer, length, 1, backupfd);
    fflush(backupfd);
  }
}

void *backup_thread_loop(void *arg)
{
  backup_info_struct *bi = (backup_info_struct *)arg;
  while (!main_quit)
  {
    sleep(30);
    pthread_testcancel();

    if (backup_info.length == old_backup_info.length)
    {
      if (memcmp(old_backup_info.buffer, backup_info.buffer, old_backup_info.length) != 0)
      {
        backup_write(bi->buffer, bi->length);
        memcpy(old_backup_info.buffer, backup_info.buffer, old_backup_info.length);
      }
    }
    else
    {
      log_write("old and new backup buffer have a different length!");
    }
  }
  backup_close();
  log_write("exit backup thread, remove %s", backup_file);
  remove(backup_file);
  pthread_exit(NULL);
  return NULL;
}

