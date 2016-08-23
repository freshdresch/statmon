#!/bin/bash

if [ $# -ne 4 ]; then
	echo "Usage: `basename $0` sampleRate sleepTime input.cfg output.csv"
	exit 1
fi

SAMPLE_RATE=$1
SLEEP_TIME=$2
INFILE=$3
OUTFILE=$4

./statmon -f $INFILE $SAMPLE_RATE $OUTFILE &
sleep $SLEEP_TIME
killall --signal SIGINT statmon
