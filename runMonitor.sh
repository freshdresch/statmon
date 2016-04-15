#!/bin/bash

if [ $# -ne 2 ]; then
	echo "Usage: $0 input.cfg output.csv"
	exit 1
fi

./statmon -f $1 $2 &
sleep 30
killall --signal SIGINT statmon
