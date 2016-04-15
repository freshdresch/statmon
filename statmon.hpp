#ifndef STATMON_HPP
#define STATMON_HPP

#include <string>

namespace StatmonConstants
{
	/******** user set constants ********/
	// how frequently to sample the metric
	// given in microseconds
	const unsigned int SAMPLE_RATE      = 100000;
	const unsigned int OUTPUT_PRECISION = 3;
	/************************************/

	/******** program set codes ********/
	// return values for main program
	const int SUCCESS                   = 0;
	const int INVALID_NUM_ARGS          = 1;
	const int INVALID_METRIC            = 2;
	const int FAILED_ALLOC_NETLINK_SOCK = 3;
	const int FAILED_CONN_NETLINK_SOCK  = 4;
	const int FAILED_ALLOC_LINK_CACHE   = 5;
	const int FAILED_RESYNC_CACHE       = 6;
	const int FAILED_GET_LINK_BY_NAME   = 7;

	// number of microseconds in one second
	// type is double for division casting purposes
	const double MICRO = 1000000.0;
	/************************************/
}

struct MeasureTarget
{
	std::string iface;
	std::string metric;
	rtnl_link_stat_id_t statId;
};

struct MeasureTargetHash 
{
	size_t operator()(const MeasureTarget& target) const
	{
		return std::hash<std::string>()(target.iface) ^
            (std::hash<std::string>()(target.metric) << 1);
	}
};

struct MeasureTargetEqual 
{
	bool operator()(const MeasureTarget& lhs, 
					const MeasureTarget& rhs) const
	{
		return (lhs.iface == rhs.iface) && (lhs.metric == rhs.metric);
	}
};

struct InterfaceStat
{
	double timeDelta;
	unsigned int value;
};

// use likely and unlikely for my error codes where I care about timing
#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif


#endif
