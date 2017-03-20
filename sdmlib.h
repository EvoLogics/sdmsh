#ifndef SDM_LIB
#define SDM_LIB

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h> /* FILE* */


typedef struct modem_t{
  char name[];
  int socketID;
  int state;
} modem_t;

int tx(modem_t *modem, char *data, int timeToTx);
int rx(modem_t *modem,int nsamples, int type);
int config(modem_t *modem, int txPower, int gain, int rxThreshold);
int setRef(modem_t *modem, char *data);
int open(modem_t *modem);
int release(modem_t *modem);

#endif
