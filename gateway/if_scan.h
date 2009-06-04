#ifndef __if_scan_h__
#define __if_scan_h__

#include <pthread.h>

#ifndef AF_CAN
# define AF_CAN 29
#endif
#ifndef PF_CAN
# define PF_CAN AF_CAN
#endif

#define ADDLSTSIZE 1000 /* assume we don't have more than 1000 nodes on one CAN bus */
#define TXBUFLEN   5000 /* maxumum number of mambanet messages in the send buffer */
#define TXDELAY    1024 /* delay between each CAN frame transmit in us (with bursts for MambaNet messages) */
#define HWPARTIMEOUT 10 /* timeout for receiving the hardware parent, in seconds */

struct can_ifaddr;

struct can_queue {
  int length, canid;
  unsigned char *buf;
};

struct can_data {
  int sock;
  int ifindex;
  pthread_t rxthread, txthread;
  pthread_mutex_t *txmutex;
  int txstart;
  struct can_ifaddr *addrs[ADDLSTSIZE];
  struct can_queue *tx[TXBUFLEN];
};

struct can_ifaddr {
  int addr;
  int seq; /* next sequence ID we should receive */
  unsigned char buf[MBN_MAX_MESSAGE_SIZE+8]; /* fragmented MambaNet message */
  struct can_data *lnk; /* so we have access to the addrs list */
  int lnkindex; /* so we know where in the list we are */
};

int scan_init(struct mbn_interface *, char *);
int scan_hwparent(int, unsigned short *, char *);
void scan_free(struct mbn_interface *);
void scan_free_addr(void *);
void *scan_receive(void *);
void *scan_send(void *);
int scan_transmit(struct mbn_interface *, unsigned char *, int, void *, char *);

struct mbn_interface * MBN_EXPORT mbnCANOpen(char *, unsigned short *, char *);

#endif
