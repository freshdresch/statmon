#!/usr/bin/env python
import numpy as np
import sys

def generateStringRange(first, last):
	# expects first and last to be strings
	decimalFound = first.find('.')
	# slice number after decimal to get number of digits, which is the precision
	precision = len( first[decimalFound+1:] )
	start = int( float(first) * (10**precision) )
	stop = int( float(last) * (10**precision) )
	numStrings = []
	# now that I have scaled the numbers to integers, the increment is just 1
	for i in range(start, stop+1):
		temp = float(i) / (10**precision) 
		numString = "{1:.{0}f}".format(precision, temp)
		numStrings.append( numString )
	return numStrings

def initializeCounts(counts, first, last):
	# first and last should be strings
	# counts is a dictionary with the time strings as the keys
	times = generateStringRange(first, last)
	for time in times:
		counts[time] = 0

def initializeIterations(counts, first, last):
	start = int(first)
	stop = int(last)
	for i in range(start, stop+1):
		counts[ str(i) ] = 0

def tallySamples(timeCounts, iterationCounts, lines):
	# use set for faster membership test
	timeSet = set( timeCounts.keys() )
	iterationSet = set( iterationCounts.keys() )
	for line in lines:
		chunks = line.split(',')
		it = chunks[0]
		time = chunks[1]
		# increment time, add if need be (though it shouldn't need to happen)
		if time in timeSet:
			timeCounts[time] += 1
		else:
			print( "[WARNING] timeCount: why wasn't {0} in there? Adding..".format(time) )
			timeCounts[time] = 1
			timeSet.add(time)
		# increment that iteration, add if need be (though it shouldn't need to happen)
		if it in iterationSet:
			iterationCounts[it] += 1
		else:
			print( "[WARNING] iterationCount: why wasn't {0} in there? Adding..".format(it) )
			iterationCounts[it] = 1
			iterationSet.add(it)

def countNotSampled(counts):
	# for time in times:
	zeroes = []
	for val, count in counts.items():
		if count == 0:
			zeroes.append(val)
	return zeroes

def printStatistics(counts):
	mean = np.mean( counts.values() )
	stdev = np.std( counts.values() )
	numSampleIntervals = len( counts.values() )
	totalNumSamples = sum( counts.values() )
	mostSampled = max( counts.values() )
	leastSampled = min( counts.values() )
	unsampled = countNotSampled( counts )

	print("mean numbers: {0}".format(mean))	
	print("standard deviation: {0}".format(stdev))	
	print("number of sample intervals: {0}".format(numSampleIntervals))		
	print("total number of samples: {0}".format(totalNumSamples))	
	print("highest number of samples in sample interval: {0}".format(mostSampled))
	print("lowest number of samples in sample interval: {0}".format(leastSampled))
	print("there were {0} values with zero samples".format( len(unsampled) )) 
	if len(unsampled) > 0:
		print(unsampled)
	print("minimum: {0}".format(list(counts.keys())[list(counts.values()).index(leastSampled)]) )
	print("maximum: {0}".format(list(counts.keys())[list(counts.values()).index(mostSampled)]) )

if len(sys.argv) != 2:
	print("Usage: analyzeMeasure results.csv")
	sys.exit(1)

fname = sys.argv[1]
with open(fname, "r") as f:
	lines = f.readlines()
lines = [ line.strip() for line in lines ]
# remove header line in csv
lines.pop(0)

# get first and last iteration of sampling loop
temp = lines[0].split(',')
firstIter = temp[0]
firstTime = temp[1]
temp = lines[len(lines)-1].split(',')
lastIter = temp[0]
lastTime = temp[1]

# populate iterations and zero their counters
timeCounts = {}
iterationCounts = {}
initializeCounts(timeCounts, firstTime, lastTime)
initializeIterations(iterationCounts, firstIter, lastIter)

tallySamples(timeCounts, iterationCounts, lines)

"""
print( iterationCounts)
print("\n"*5)
print( timeCounts )
"""


# delete first and last time,
# because they could be partial samples based on timing
# just gives cleaner data
del timeCounts[firstTime]
del timeCounts[lastTime]
del iterationCounts[firstIter]
del iterationCounts[lastIter]

# display iteration statistics
print( "{0} iteration counts {1}".format("*"*20, "*"*20) )
printStatistics(iterationCounts)

print("")

# display time statistics
print( "{0} time counts {1}".format("*"*20, "*"*20) )
printStatistics(timeCounts)

