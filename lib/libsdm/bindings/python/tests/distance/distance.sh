#!/bin/sh
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-../../../..:../../../../../libstream}
export PYTHONPATH=${PYTHONPATH:-../..}
./distance.py $@
