#!/usr/bin/env python

import numpy as np
import sys
from statsmodels.distributions.empirical_distribution import ECDF


if len(sys.argv) != 4:
	print("Usage: analyzeResults.py <sample rate> <input config file> <output results csv>")
	sys.exit(1)

titles = { "rx_packets"     : "RX Packets",
		   "tx_packets"     : "TX Packets",
		   "rx_bytes"       : "RX Bytes",
		   "tx_bytes"       : "TX Bytes",
		   "rx_dropped"     : "RX Dropped",
		   "tx_dropped"     : "TX Dropped",
		   "rx_fifo_errors" : "RX FIFO Errors",
		   "tx_fifo_errors" : "TX FIFO Errors" }

""" Unfortunately there is a bit of an art to setting tolerances.
These are used to determine whether our network tests have started or not.
If the measured value of our metrics is within the tolerance level, we believe that to be
outside noise (for example, ARP packets). 
However, if it is above our tolerance level, we now assume the test has started.
Once it drops back below the tolerance level, we assume the test is over.
"""
# tolerance will be of the form metric --> tolerance value
tolerances = { "rx_packets"     : 3,
			   "tx_packets"     : 3,
			   "rx_bytes"       : 200,
			   "tx_bytes"       : 200,
			   "rx_dropped"     : 0,
			   "tx_dropped"     : 0,
			   "rx_fifo_errors" : 0,
			   "tx_fifo_errors" : 0 }

# data will be of the form iface --> metric --> time   --> list
#                                           --> values --> list
data = {}
scaled_data = {}
# base will be of the form iface --> metric --> tuple( time, baseVal )
base = {}

sampleRate = int( sys.argv[1] )

# parse measure configuration file
with open( sys.argv[2], "r" ) as f:
	for line in f:
		iface, metric = line.strip().split(' ')
		if iface not in data:
			data[iface] = {}
			scaled_data[iface] = {}
			base[iface] = {}
		if metric not in data[iface]:
			data[iface][metric] = { "times" : [ ], "values" : [ ] }
			scaled_data[iface][metric] = { "times" : [ ], "values" : [ ] }
			base[iface][metric] = (-1.0, -1.0)

# parse results file
with open( sys.argv[3], "r" ) as f:
	lines = f.readlines()
lines = [ line.strip() for line in lines ]
# remove header line in csv
lines.pop(0)


for line in lines:
	_, time, iface, metric, val = line.split(',')
	if base[iface][metric][1] == -1.0:
		base[iface][metric] = ( float(time), float(val) )
		continue
	# this will eliminate values before we start
	if (float(val) - base[iface][metric][1]) <= tolerances[metric]:
		base[iface][metric] = ( float(time), float(val) )
		continue
	# otherwise, it is actually started and we should add to data
	# add the revised base value before the new data values
	# this should only execute once per iface, metric pair
	if len( data[iface][metric]["values"] ) == 0:
		data[iface][metric]["times"].append( base[iface][metric][0] )
		data[iface][metric]["values"].append( base[iface][metric][1] )
	# add the general data values
	data[iface][metric]["times"].append( float(time) )
	data[iface][metric]["values"].append( float(val) )

# eliminate any samples on the end that are after the test has finished
for iface in data:
	for metric in data[iface]:
		if len( data[iface][metric]["values"] ) == 0:
			continue
		times = data[iface][metric]["times"]
		vals = data[iface][metric]["values"]
		i = len(vals) - 1
		while vals[i] - vals[i-1] <= tolerances[metric]:
			del vals[i]
			del times[i]
			i-=1
		data[iface][metric]["times"] = times
		data[iface][metric]["values"] = vals

""" We are shaving off a particular number of samples on each end of the list.
We remove the first and last valid sample in order to avoid partial samples.
For example, if we are sampling every 0.25 seconds, and the packet generator 
started 0.2 seconds into the current sample, it would only get 0.05 seconds
of results but it thinks it is for the whole sample interval.

In addition, we are shaving off 1 seconds' worth of samples at the beginning.
This is to smooth away any oddities with the testing environment.
In particular, sometimes dpdk pktgen has a "ramp up" period.

NOTE: the sampleRate argument is in microseconds.
"""
# this is a formula to get the number of samples in one second
numSamples = int( round( (1e6) / sampleRate ) )
for iface in data:
	for metric in data[iface]:
		if len( data[iface][metric]["values"] ) == 0:
			continue
		times = data[iface][metric]["times"]
		vals = data[iface][metric]["values"]
		# remove potential partial sample on the end
		times.pop()
		vals.pop()
		# remove potential partial sample on beginning, plus 1 seconds worth of ramp up time
		# assuming there is anything still in the list
		for i in range( numSamples + 1 ):
			if len(times) > 0:
				times.pop(0)
			if len(vals) > 0:
				vals.pop(0)
		data[iface][metric]["times"] = times
		data[iface][metric]["values"] = vals

ns2sec=1000000000.0
for iface in data:
	for metric in data[iface]:
		if len( data[iface][metric]["values"] ) == 0:
			print("There were no " + titles[metric] + "\n")
			continue
		times = data[iface][metric]["times"]
		vals = data[iface][metric]["values"]
		valRange = vals[ len(vals) - 1 ] - vals[0]
		timeRange = ( times[ len(times) - 1] - times[0] ) / ns2sec

		print( "Total sampled time for {0} on {1}: {2:0.2f} seconds".format(titles[metric], iface, timeRange) )
		print( "Number of samples: {0}".format( len(vals) ) )
		print( "{0} total on {1}: {2}".format( titles[metric], iface, valRange ) )
		print( "Time average of {0} on {1}: {2:0.2f}".format( titles[metric], iface, float(valRange) / timeRange ) )
		
		"""
		spread = []
		for i in range( 1, len(scaled_vals) ):
			raw = ( scaled_vals[i] - scaled_vals[i-1] ) / scaled_vals[i-1]
			spread.append(raw)

		print( "Mean spread for {0} per second on {1}: {2:0.2f}".format( titles[metric], iface, np.mean(spread) ) )
		xvals = np.linspace( min(spread), max(spread), 100 )
		ecdf = ECDF(spread)
		spread.sort()
		print(spread)
		print("")
		print( ecdf(spread) )
		"""

