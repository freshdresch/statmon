// netlink includes
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/cache.h>

// linux and C includes 
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <unistd.h>

// C++ includes
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <stdexcept>
#include <vector>
#include <unordered_map>

// header file with constants and struct definitions
#include "statmon.hpp"


using namespace std;

typedef vector<MeasureTarget> TargetVec;
typedef vector<InterfaceStat> StatVec;
typedef unordered_map<MeasureTarget, StatVec, MeasureTargetHash, MeasureTargetEqual> IfaceData;

static volatile int running = 1;

void exitHandler(int dummy) 
{
    running = 0;
}

void teardownNetlink(nl_sock *&sock, nl_cache *&link_cache)
{
    nl_close(sock);
    nl_cache_free(link_cache);
    nl_socket_free(sock);
    sock = NULL;
    link_cache = NULL;
}

void setupNetlink(nl_sock *&sock, nl_cache *&link_cache)
{
    int err = 0;
    sock = nl_socket_alloc();
    if (!sock) 
    {
        cerr << "[ERROR] Unable to allocate netlink socket." << endl;
        exit( StatmonConstants::FAILED_ALLOC_NETLINK_SOCK );
    }
    
    err = nl_connect(sock, NETLINK_ROUTE);
    if (err < 0) 
    {
        nl_perror(err, "Unable to connect socket");
        nl_socket_free(sock);
        exit( StatmonConstants::FAILED_CONN_NETLINK_SOCK );
    }
    
    err = rtnl_link_alloc_cache(sock, AF_UNSPEC, &link_cache);
    if (err < 0)
    {
        nl_perror(err, "Unable to allocate cache");
        nl_socket_free(sock);
        exit( StatmonConstants::FAILED_ALLOC_LINK_CACHE );
    }
}

// taken from https://gist.github.com/BinaryPrison/1112092
static inline uint32_t __iter_div_u64_rem(uint64_t dividend, uint32_t divisor, uint64_t *remainder)
{
	uint32_t ret = 0;
	while (dividend >= divisor) {
		/* The following asm() prevents the compiler from
		   optimising this loop into a modulo operation.  */
		asm("" : "+rm"(dividend));
		dividend -= divisor;
		ret++;
	}
	*remainder = dividend;
	return ret;
}

static inline void timespec_add_ns(struct timespec *a, uint64_t ns)
{
	a->tv_sec += __iter_div_u64_rem(a->tv_nsec + ns, StatmonConstants::NANO, &ns);
	a->tv_nsec = ns;
}

static inline uint64_t timespec_to_ns(struct timespec *t)
{
	return (StatmonConstants::NANO * t->tv_sec) + t->tv_nsec;
}

static inline uint64_t timespec_diff(struct timespec *larger, struct timespec *smaller)
{
	return StatmonConstants::NANO * (larger->tv_sec - smaller->tv_sec) + larger->tv_nsec - smaller->tv_nsec;
}

void collectData(const TargetVec &targets, 
                 IfaceData &data,
				 const uint64_t sampleRate)
{
    int err = 0;
    nl_sock *sock;
    nl_cache *link_cache;
    setupNetlink(sock, link_cache);

    rtnl_link *link;
    InterfaceStat ifstat;
    timespec base, loop, next;
	uint64_t diff, offset, sleepTime;

	/*
    // end - start gives us the program time without a frame of reference
	// diff, start, and end are in seconds
    double diff, start, end;

    // offset - base tells us how long our monitoring loop took
    // so we sleep the correct amount
	// offset, base, and sleepTime are in microseconds since I am using usleep
    double offset, base, sleepTime;
    unsigned int i = 0;
	*/

    unsigned int i = 0;
	clock_gettime(CLOCK_MONOTONIC, &base);
    while(running)
    {
        clock_gettime(CLOCK_MONOTONIC, &loop);
        err = nl_cache_resync(sock, link_cache, NULL, NULL);
        if ( unlikely(err < 0) )
        {
            nl_perror(err, "Unable to resync cache");
            teardownNetlink(sock, link_cache);
            exit( StatmonConstants::FAILED_RESYNC_CACHE );
        }

        for (const auto &target : targets)
        {
            link = rtnl_link_get_by_name( link_cache, target.iface.c_str() );
            if ( unlikely(!link) )
            {
                cerr << "[ERROR] rtnl: failed to get the link " << target.iface 
                     << " by name." << endl;
                teardownNetlink(sock, link_cache);
                exit( StatmonConstants::FAILED_GET_LINK_BY_NAME );
            }

            ifstat.iter = i;
            ifstat.value = rtnl_link_get_stat(link, target.statId);

			clock_gettime(CLOCK_MONOTONIC, &next);
            ifstat.time = timespec_diff( &next, &base);
            data[target].push_back(ifstat);
        }

        ++i;
		clock_gettime(CLOCK_MONOTONIC, &next);
		sleepTime = sampleRate - timespec_diff( &next, &loop );
		#ifdef DEBUG
		cout << "sleeping for " << sleepTime << " microseconds." << endl;
		#endif
		// if sleepTime is negative, we have already taken longer than sampling rate
		// and there is no need to sleep. No unlikely macro because it depends
		// what the user sets the sampling rate to.
		if (sleepTime > 0) 
		{
			timespec interval = { 0, 0 };
			timespec_add_ns( &interval, sleepTime );
			// try normal nanosleep too
			clock_nanosleep(CLOCK_MONOTONIC, 0, &interval, NULL);
		}
    }
    // terminate responsibly
    teardownNetlink(sock, link_cache);
    cout << endl; // just for clean CTRL-C aftermath
}

bool parseMetric(const string &metric, rtnl_link_stat_id_t &statId)
{
    bool valid = true;
    if (metric == "rx_packets") statId = RTNL_LINK_RX_PACKETS;
    else if (metric == "tx_packets") statId = RTNL_LINK_TX_PACKETS;
    else if (metric == "rx_bytes") statId = RTNL_LINK_RX_BYTES;
    else if (metric == "tx_bytes") statId = RTNL_LINK_TX_BYTES;
    else if (metric == "rx_errors") statId = RTNL_LINK_RX_ERRORS;
    else if (metric == "tx_errors") statId = RTNL_LINK_TX_ERRORS;
    else if (metric == "rx_dropped") statId = RTNL_LINK_RX_DROPPED;
    else if (metric == "tx_dropped") statId = RTNL_LINK_TX_DROPPED;
    else if (metric == "rx_fifo_errors") statId = RTNL_LINK_RX_FIFO_ERR;
    else if (metric == "tx_fifo_errors") statId = RTNL_LINK_TX_FIFO_ERR;
    else valid = false;
    return valid;
}

void parseConfigFile(const string &inputFile, TargetVec &targets)
{
    string iface, metric;
    MeasureTarget target;
	ifstream in( inputFile.c_str() );
	while (in >> iface >> metric)
	{
		target.iface = iface;
		target.metric = metric;
		targets.push_back(target);
	}
	in.close();
}

void printUsage()
{
    cout << "Usage:\tstatmon <rate> <config> <outfile> " << endl;
    cout << endl;
	cout << "Where:" << endl;
    cout << "    rate        the rate at which the program samples the specified metrics (in MICROseconds)" << endl;
    cout << "    outfile     the name of the output csv file for the measurements." << endl;
	cout << "    config      the input configuration file specifying the iface metric pairs." << endl;
	cout << endl;
    cout << "Example: statmon 250000 measure.cfg results.csv" << endl; 
    cout << endl;
	cout << "Valid metrics for the configuration file:" << endl;
    cout << "    rx_packets" << endl; 
    cout << "    tx_packets" << endl; 
    cout << "    rx_bytes" << endl;
    cout << "    tx_bytes" << endl;
    cout << "    rx_errors" << endl;
    cout << "    tx_errors" << endl;
    cout << "    rx_dropped" << endl;
    cout << "    tx_dropped" << endl;
    cout << "    rx_fifo_errors" << endl;
    cout << "    tx_fifo_errors" << endl;
	cout << endl;
	cout << "Example entries in configuration file:" << endl;
	cout << endl;
	cout << "eth0 rx_packets" << endl;
	cout << "eth0 tx_packets" << endl;
	cout << "eth1 rx_packets" << endl;
	cout << endl;
	cout << "As you can see above, each entry in the config file is space delimited and one pair per line." << endl;
	cout << endl;
	cout << "Important note:" << endl;
	cout << "    Time resolution is machine dependent. You should be aware that sampling rates of too fine" << endl;
	cout << "    granularity may not work on your machine." << endl;
	cout << endl;
}

int main(int argc, char *argv[])
{
	// parse arguments 
    if (argc != 4)
    {
        printUsage();
        return StatmonConstants::INVALID_NUM_ARGS;
    }

	uint64_t sampleRate;
	try {
		sampleRate = stoul( argv[1] ) * 1000L;
	} catch (...) {
		cerr << "The argument for sample rate is not a valid unsigned integer." << endl;
		return StatmonConstants::INVALID_SAMPLE_RATE;
	}
	if ( access(argv[2], F_OK) == -1 )
	{
		cerr << "The provided input file does not exist." << endl;
		return StatmonConstants::INVALID_INPUT_FILE;
	}
	string inputFile(argv[2]);
    string fname(argv[3]);

    TargetVec targets;
    parseConfigFile(inputFile, targets);
    rtnl_link_stat_id_t statId;
    for (auto &target : targets)
    {
        if ( !parseMetric(target.metric, statId) )
        {
            cerr << "[ERROR] provided metric is invalid." << endl;
            cerr << endl;
            printUsage();
            return StatmonConstants::INVALID_METRIC;
        }
        target.statId = statId;
    }

    signal(SIGINT, exitHandler);
    signal(SIGTERM, exitHandler);

    IfaceData data;
    collectData(targets, data, sampleRate);

    ofstream out( fname.c_str() );
	out.setf(ios::fixed);
    out << "i,time,iface,metric,value" << endl;
    MeasureTarget target;
    StatVec stats;
    for (const auto &d : data)
    {
        target = d.first;
        stats = d.second;
        for (const auto &ifstat : stats)
        {
            out << ifstat.iter << "," << ifstat.time << "," << target.iface << "," 
                << target.metric << "," << ifstat.value << endl;
        }
    }
    out.close();
    return StatmonConstants::SUCCESS;
}

