#!/usr/bin/env bash
if [ $# -ne 4 ]; then
	echo "Usage: `basename $0` <sample rate> <input config file> <output results file> <sleep time>"
	exit 1
fi

SAMPLE_RATE=$1
INFILE=$2
OUTFILE=$3
SLEEP_TIME=$4

./statmon $SAMPLE_RATE $INFILE $OUTFILE &
sleep $SLEEP_TIME
killall --signal SIGINT statmon
