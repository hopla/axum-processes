#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <linux/if_arp.h>
#include <linux/can.h>

#include <termios.h>

#include "mbn.h"
#include "if_scan.h"

void *(*scan_receive)(void *) = NULL;
void *(*scan_send)(void *) = NULL;

struct mbn_interface * MBN_EXPORT mbnCANOpen(char *ifname, unsigned short *parent, char *err) {
  struct mbn_interface *itf;
  struct can_data *dat;
  int error = 0;

  itf = (struct mbn_interface *)calloc(1, sizeof(struct mbn_interface));
  dat = (struct can_data *)calloc(1, sizeof(struct can_data));

  if (strchr(ifname,'/') != NULL) {
    dat->tty_mode = 1;
  }

  if(dat->tty_mode) {
    if(scan_open_tty(ifname, dat, err))
      error++;
    scan_receive = scan_tty_receive;
    scan_send = scan_tty_send;
  }
  else {
    if(scan_open_sock(ifname, dat, err))
      error++;
    scan_receive = scan_can_receive;
    scan_send = scan_can_send;
  }

  /* wait for hardware parent */
  if(!error && parent != NULL) {
    if(scan_hwparent(dat->sock, parent, err))
      error++;
  }

  if(error) {
    free(dat);
    free(itf);
    return NULL;
  }

  itf->data = (void *)dat;
  itf->cb_init = scan_init;
  itf->cb_free = scan_free;
  itf->cb_transmit = scan_transmit;
  itf->cb_free_addr = scan_free_addr;
  return itf;
}

int scan_open_sock(char *ifname, struct can_data *dat, char *err) {
  struct sockaddr_can addr;
  struct ifreq ifr;

  /* create socket */
  if((dat->sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
    sprintf(err, "socket(): %s", strerror(errno));
    return 1;
  }

  /* get interface index */
  strcpy(ifr.ifr_name, ifname);
  if(ioctl(dat->sock, SIOCGIFINDEX, &ifr) < 0) {
    sprintf(err, "Couldn't find can interface: %s", strerror(errno));
    return 1;
  } else
    dat->ifindex = ifr.ifr_ifindex;

  /* bind */
  addr.can_family = AF_CAN;
  addr.can_ifindex = dat->ifindex;
  if(bind(dat->sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_can)) < 0) {
    sprintf(err, "Couldn't bind socket: %s", strerror(errno));
    return 1;
  }
  return 0;
}

int scan_open_tty(char *ifname, struct can_data *dat, char *err) {
  struct termios tio;
  int fd;

  /* open serial device */
  dat->fd = open(ifname, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK); 
  if (dat->fd<0) {
    sprintf(err, "Couldn't open port: %s", strerror(errno));
    return 1;
  }
  
  /* set the FNDELAY for this device */
  if (fcntl(dat->fd, F_SETFL, FNDELAY | O_NONBLOCK) == -1) {
    sprintf(err, "Couldn't not set FNDELAY: %s", strerror(errno));
    close(dat->fd);
    return 1;
  }

  /* get the attributes */
  if (tcgetattr(fd, &tio) != 0) {
    sprintf(err, "Couldn't get attribs: %s", strerror(errno));
    close(dat->fd);
    return 1;
  }

  /* change the atrributes to use a baudrate of 500000 */
  if (cfsetspeed(&tio, B250000) != 0) {
    sprintf(err, "could not set baudrate to 250000: %s", strerror(errno));
    close(dat->fd);
    return 1;
  }

  /* Change the atrributes to disable local echo, line processing etc. */
  tio.c_cflag &= ~CRTSCTS;
  tio.c_cflag |= (CLOCAL | CREAD);
  tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  tio.c_oflag &= ~OPOST;
  tio.c_cc[VMIN] = 0;
  tio.c_cc[VTIME] = 10;

  /* make raw */
  tio.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);

  /* set the attributes. */
  if (tcsetattr(fd, TCSANOW, &tio) != 0) {
    sprintf(err, "Couldn't set attribs: %s", strerror(errno));
    close(dat->fd);
    return 1;
  }
  return 0;
}

int scan_init(struct mbn_interface *itf, char *err) {
  struct can_data *dat = (struct can_data *)itf->data;
  int i;

  if ((scan_receive == NULL) && (scan_send == NULL)) {
    sprintf(err, "scan rcv or send functions are not assigned");
    return 1;
  }

  dat->txmutex = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(dat->txmutex, NULL);  
  if((i = pthread_create(&(dat->rxthread), NULL, scan_receive, (void *)itf)) != 0) {
    sprintf(err, "Can't create rxthread: %s (%d)", strerror(i), i);
    return 1;
  }
  if((i = pthread_create(&(dat->txthread), NULL, scan_send, (void *)itf)) != 0) {
    sprintf(err, "Can't create txthread: %s (%d)", strerror(i), i);
    return 1;
  }
  return 0;
}

int scan_hwparent(int sock, unsigned short *par, char *err) {
  struct can_frame frame;
  struct timeval tv, end;
  int n;
  fd_set rd;

  gettimeofday(&end, NULL);
  end.tv_sec += HWPARTIMEOUT;
  tv.tv_sec = 0;
  tv.tv_usec = 500000;

  while(1) {
    FD_ZERO(&rd);
    FD_SET(sock, &rd);
    n = select(sock+1, &rd, NULL, NULL, &tv);
    /* handle errors */
    if(n < 0) {
      sprintf(err, "Checking read state: %s", strerror(errno));
      return 1;
    }
    /* received frame, check for ID */
    if(n > 0) {
      n = read(sock, &frame, sizeof(struct can_frame));
      if(n < 0 || n != (int)sizeof(struct can_frame)) {
        sprintf(err, "Reading from network: %s", strerror(errno));
        return 1;
      }
      if((frame.can_id & CAN_ERR_MASK & 0xFFFF000F) == 0x0FFF0001) {
        par[0] = ((unsigned short)frame.data[0]<<8) | frame.data[1];
        par[1] = ((unsigned short)frame.data[2]<<8) | frame.data[3];
        par[2] = ((unsigned short)frame.data[4]<<8) | frame.data[5];
        return 0;
      }
    }
    /* no parent found, check for timeout and try again */
    gettimeofday(&tv, NULL);
    if(tv.tv_sec > end.tv_sec || (tv.tv_sec == end.tv_sec && tv.tv_usec > end.tv_usec)) {
      sprintf(err, "Timeout in getting hardware parent");
      return 1;
    }
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
  }
  return 0;
}


void scan_free(struct mbn_interface *itf) {
  struct can_data *dat = (struct can_data *)itf->data;
  pthread_cancel(dat->rxthread);
  pthread_cancel(dat->txthread);
  pthread_join(dat->rxthread, NULL);
  pthread_join(dat->txthread, NULL);
  pthread_mutex_destroy(dat->txmutex);
  free(dat);
  free(itf);
}


void scan_free_addr(void *ptr) {
  struct can_ifaddr *adr = (struct can_ifaddr *)ptr;
  adr->lnk->addrs[adr->lnkindex] = NULL;
  free(ptr);
}


void *scan_can_receive(void *ptr) {
  struct mbn_interface *itf = (struct mbn_interface *)ptr;
  struct can_data *dat = (struct can_data *)itf->data;
  struct can_frame frame;
  char err[MBN_ERRSIZE];
  int n, ai, bcast, src, dest, seq;

  while((n = read(dat->sock, &frame, sizeof(struct can_frame))) >= 0 && n == (int)sizeof(struct can_frame)) {
    /* ignore flags - assume all incoming frames are correct */
    frame.can_id &= CAN_ERR_MASK;

    /* parse CAN id */
    bcast = (frame.can_id>>28) & 0x0001;
    dest  = (frame.can_id>>16) & 0x0FFF;
    src   = (frame.can_id>> 4) & 0x0FFF;
    seq   =  frame.can_id      & 0x000F;

    /* ignore if it's not for us */
    if(!(dest == 1 || (bcast && dest == 0)))
      continue;

    /* look for existing ifaddr struct */
    n = -2;
    for(ai=0;ai<ADDLSTSIZE;ai++) {
      if(dat->addrs[ai] != NULL && dat->addrs[ai]->addr == src)
        break;
      else if(n == -2 && dat->addrs[ai] == NULL)
        n = ai;
    }
    /* not found, create new one */
    if(ai == ADDLSTSIZE) {
      if(n == -2)
        break;
      dat->addrs[n] = malloc(sizeof(struct can_ifaddr));
      dat->addrs[n]->lnk = dat;
      dat->addrs[n]->lnkindex = n;
      dat->addrs[n]->addr = src;
      dat->addrs[n]->seq = 0;
      ai = n;
    }

    /* check sequence ID */
    if(seq > 15 || dat->addrs[ai]->seq != seq) {
      printf("Incorrect sequence ID (%d == %d)\n", seq, dat->addrs[ai]->seq);
      continue;
    }

    /* fill buffer */
    memcpy((void *)&(dat->addrs[ai]->buf[seq*8]), (void *)frame.data, 8);

    /* check for completeness of the message */
    for(n=0;n<8;n++)
      if(frame.data[n] == 0xFF)
        break;
    if(n == 8) {
      dat->addrs[ai]->seq++;
    } else {
      dat->addrs[ai]->seq = 0;
      mbnProcessRawMessage(itf, dat->addrs[ai]->buf, seq*8+n+1, (void *)dat->addrs[ai]);
    }
  }

  sprintf(err, "Read from CAN failed: %s",
    n == -1 ? strerror(errno) : n == -2 ? "Too many nodes on the bus" : "Incorrect CAN frame size");
  mbnInterfaceReadError(itf, err);
  return NULL;
}


void *scan_can_send(void *ptr) {
  struct mbn_interface *itf = (struct mbn_interface *)ptr;
  struct can_data *dat = (struct can_data *)itf->data;
  struct can_frame frame;
  struct can_queue *q;
  struct timeval tv;
  int i;

  tv.tv_sec = 0;
  tv.tv_usec = 10000;
  while(1) {
    pthread_mutex_lock(dat->txmutex);
    q = dat->tx[dat->txstart];
    if(q != NULL) {
      frame.can_id = q->canid | CAN_EFF_FLAG;
      frame.can_dlc = 8;
      for(i=0; i<=q->length/8; i++) {
        frame.can_id &= ~0xF;
        frame.can_id |= i;
        memset((void *)frame.data, 0, 8);
        memcpy((void *)frame.data, &(q->buf[i*8]), i*8+8 > q->length ? q->length-i*8 : 8);
        if(write(dat->sock, (void *)&frame, sizeof(struct can_frame)) < (int)sizeof(struct can_frame))
          fprintf(stderr, "send: %s", strerror(errno));
      }
      dat->tx[dat->txstart] = NULL;
      if(++dat->txstart >= TXBUFLEN)
        dat->txstart = 0;
      free(q->buf);
      free(q);
      tv.tv_sec = 0;
      tv.tv_usec = TXDELAY*i;
    } else {
      tv.tv_sec = 0;
      tv.tv_usec = 10000;
    }
    pthread_mutex_unlock(dat->txmutex);
    select(0, NULL, NULL, NULL, &tv);
  }
  return NULL;
}


int scan_transmit(struct mbn_interface *itf, unsigned char *buffer, int length, void *ifaddr, char *err) {
  struct can_data *dat = (struct can_data *)itf->data;
  int i;

  pthread_mutex_lock(dat->txmutex);
  for(i=dat->txstart; i<TXBUFLEN; i++)
    if(dat->tx[i] == NULL)
      break;
  if(i == TXBUFLEN)
    for(i=0; i<dat->txstart; i++)
      if(dat->tx[i] == NULL)
        break;
  if(dat->tx[i] != NULL) {
    pthread_mutex_unlock(dat->txmutex);
    sprintf(err, "Buffer overrun");
    return 1;
  }

  dat->tx[i] = malloc(sizeof(struct can_queue));
  dat->tx[i]->buf = malloc(length);
  memcpy(dat->tx[i]->buf, buffer, length);
  dat->tx[i]->canid = ifaddr ? (0x00000010 | (((struct can_ifaddr *)ifaddr)->addr << 16)) : 0x10000010;
  dat->tx[i]->length = length;
  pthread_mutex_unlock(dat->txmutex);
  return 0;
}

void *scan_tty_receive(void *ptr) {
  return NULL;
}


void *scan_tty_send(void *ptr) {
  return NULL;
}

