

int tx(char *modemID, char *data, int timeToTx)
{
  if (open(modemID))
  {
    int waitToTx = 1;
    int modemTx_transimt_minimal_chunks = 1;
    release(modemID);
    return 1;
  } else {
    return 0;
  }

}

char* rx(char *modemID,int nsamples, int type)
{
  open(modemID);
  if (open(modemID))
  {
    if (type == 1) // direct
      {
        char *data = rx_from_modem(nsamples);
        return data;
    }
    else if (type == 2) // filename
    {
      int write_data_to_file = 1;
    }
    release(modemID);
    return 1;
  } else {
    return 0;
  }
}

int config(char *modemID, int txPower, int gain, int rxThreshold)
{
  open(modemID);
  if (open(modemID))
  {
    int config_modem;
  }
}
int setRef(char *modemID, char *data)
{
    return 1;
}

int open(char *modemID)
{
  return 1;
}

int release(char *modemID)
{
  return 1;
}
