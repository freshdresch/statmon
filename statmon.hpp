#ifndef STATMON_HPP
#define STATMON_HPP

#include <string>

namespace StatmonConstants
{
    /******** Time conversion constants. ********/
    const uint64_t MICRO = 1000000L;
	const uint64_t NANO = 1000000000L;
    /************************************/

    /******** main program return values ********/
    const int SUCCESS                   = 0;
    const int INVALID_NUM_ARGS          = -1;
	const int INVALID_INPUT_FILE        = -2;
	const int INVALID_SAMPLE_RATE       = -3;
    const int INVALID_METRIC            = -4;
    const int FAILED_ALLOC_NETLINK_SOCK = -5;
    const int FAILED_CONN_NETLINK_SOCK  = -6;
    const int FAILED_ALLOC_LINK_CACHE   = -7;
    const int FAILED_RESYNC_CACHE       = -8;
    const int FAILED_GET_LINK_BY_NAME   = -9;
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
    uint64_t time;
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
