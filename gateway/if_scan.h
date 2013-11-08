#ifndef __if_scan_h__
#define __if_scan_h__

struct mbn_interface * MBN_EXPORT mbnCANOpen(char *, unsigned short *, char *);

//Required here to determine tty mode for setting RTS = extern clock on/off
#include <pthread.h>

#define ADDLSTSIZE    1000 /* assume we don't have more than 1000 nodes on one CAN bus */
#define TXBUFLEN      8000 /* maxumum number of mambanet messages in the send buffer */
#define CIRBUFLENGTH  8192 /* Length of serial decoding buffer */

struct can_data {
  unsigned char tty_mode;
  int txdly;
  int fd;
  int sock;
  int ifindex;
  pthread_t rxthread, txthread;
  pthread_mutex_t *txmutex;
  int txstart;
  struct can_ifaddr *addrs[ADDLSTSIZE];
  struct can_queue *tx[TXBUFLEN];
  unsigned char msgbuf[13];
  unsigned char msgbufi;
  unsigned char cirbuf[CIRBUFLENGTH];
  unsigned int cirbufb, cirbuft;
  unsigned short *parent;
};

#endif
