#ifndef STATMON_HPP
#define STATMON_HPP

#include <string>

namespace StatmonConstants
{
    /******** general values ********/
    const unsigned int OUTPUT_PRECISION = 2;

    /* How frequently to sample the metric, given in microseconds.
	 * Constant is no longer accurate, but it is not worth the fix. 
	 */
    unsigned long sampleRate = 100000;

    /* Number of microseconds in one second.
     * It is a double for division casting purposes.
	 */
    const double MICRO = 1000000.0;
    /************************************/

    /******** main program return values ********/
    const int SUCCESS                   = 0;
    const int INVALID_NUM_ARGS          = -1;
	const int INVALID_SAMPLE_RATE       = -2;
    const int INVALID_METRIC            = -3;
    const int FAILED_ALLOC_NETLINK_SOCK = -4;
    const int FAILED_CONN_NETLINK_SOCK  = -5;
    const int FAILED_ALLOC_LINK_CACHE   = -6;
    const int FAILED_RESYNC_CACHE       = -7;
    const int FAILED_GET_LINK_BY_NAME   = -8;
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
    unsigned int iter;
    unsigned int value;
    double timeDelta;
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
