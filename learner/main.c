
#include "common.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mbn.h>


#define DEFAULT_GTW_PATH  "/tmp/axum-gateway"
#define DEFAULT_ETH_DEV   "eth0"
#define DEFAULT_DB_STR    "dbname='axum' user='axum'"
#define DEFAULT_LOG_FILE  "/var/log/axum-learner.log"


struct mbn_node_info this_node = {
  0, 0,
  "MambaNet Learner",
  "Axum Learner Application",
  0x0001, 0x0010, 0x001,
  0, 0, 2, 0,
  0, 0, 0, 0,
  {0,0,0}, 0
};
struct mbn_handler *mbn;


void init(int argc, char *argv[]) {
  char dbstr[256], ethdev[50], err[MBN_ERRSIZE];
  struct mbn_interface *itf;
  int c;

  strcpy(ethdev, DEFAULT_ETH_DEV);
  strcpy(dbstr, DEFAULT_DB_STR);
  strcpy(log_file, DEFAULT_LOG_FILE);
  strcpy(hwparent_path, DEFAULT_GTW_PATH);

  while((c = getopt(argc, argv, "e:d:l:g:")) != -1) {
    switch(c) {
      case 'e':
        if(strlen(optarg) > 50) {
          fprintf(stderr, "Too long device name.\n");
          exit(1);
        }
        strcpy(ethdev, optarg);
        break;
      case 'd':
        if(strlen(optarg) > 256) {
          fprintf(stderr, "Too long database connection string!\n");
          exit(1);
        }
        strcpy(dbstr, optarg);
        break;
      case 'g':
        strcpy(hwparent_path, optarg);
        break;
      case 'l':
        strcpy(log_file, optarg);
        break;
      default:
        fprintf(stderr, "Usage: %s [-f] [-e dev] [-u path] [-d path]\n", argv[0]);
        fprintf(stderr, "  -e dev   Ethernet device for MambaNet communication.\n");
        fprintf(stderr, "  -g path  Hardware parent or path to gateway socket.\n");
        fprintf(stderr, "  -l path  Path to log file.\n");
        fprintf(stderr, "  -d str   PostgreSQL database connection options.\n");
        exit(1);

    }
  }

  daemonize();
  log_open();
  hwparent(&this_node);
  sql_open(dbstr, 0, NULL);

  if((itf = mbnEthernetOpen(ethdev, err)) == NULL) {
    fprintf(stderr, "Opening %s: %s\n", ethdev, err);
    sql_close();
    log_close();
    exit(1);
  }
  if((mbn = mbnInit(&this_node, NULL, itf, err)) == NULL) {
    fprintf(stderr, "mbnInit: %s\n", err);
    sql_close();
    log_close();
    exit(1);
  }

  daemonize_finish();
  log_write("------------------------");
  log_write("Axum Learner Initialized");
}


int main(int argc, char *argv[]) {
  init(argc, argv);

  while(!main_quit)
    sleep(1);

  log_write("Closing Learner");
  mbnFree(mbn);
  sql_close();
  log_close();
  return 0;
}

