// netlink includes
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/cache.h>

// linux and C includes 
#include <csignal>
#include <cstdlib>
#include <sys/time.h>
#include <unistd.h>

// C++ includes
#include <iostream>
#include <fstream>
#include <iomanip>
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

void collectData(const TargetVec &targets, 
                 IfaceData &data)
{
    int err = 0;
    nl_sock *sock;
    nl_cache *link_cache;
    setupNetlink(sock, link_cache);

    rtnl_link *link;
    InterfaceStat ifstat;
    timeval time;

    // end - start gives us the program time without a frame of reference
	// diff, start, and end are in seconds
    double diff, start, end;

    // offset - base tells us how long our monitoring loop took
    // so we sleep the correct amount
	// offset, base, and sleepTime are in microseconds since I am using usleep
    double offset, base, sleepTime;
    unsigned int i = 0;

    gettimeofday(&time, NULL);
    start = time.tv_sec + (time.tv_usec / StatmonConstants::MICRO);
    while(running)
    {
        gettimeofday(&time, NULL);
        base = (time.tv_sec * StatmonConstants::MICRO) + time.tv_usec;

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
            gettimeofday(&time, NULL);
            end = time.tv_sec + (time.tv_usec / 1000000.0);
            diff = end - start;
            ifstat.timeDelta = diff;
            data[target].push_back(ifstat);
        }

        ++i;
        gettimeofday(&time, NULL);
        offset = (time.tv_sec * StatmonConstants::MICRO) + time.tv_usec;
		sleepTime = StatmonConstants::SAMPLE_RATE - (offset - base);
		#ifdef DEBUG
		cout << "sleeping for " << sleepTime << " microseconds." << endl;
		#endif
		// if sleepTime is negative, we have already taken longer than sampling rate
		// and there is no need to sleep. No unlikely macro because it depends
		// what the user sets the sampling rate to.
		if (sleepTime > 0.0) 
			usleep(sleepTime);
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

void parseConfigFile(char *argv[], TargetVec &targets, const string &inputFile)
{
    string iface;
    string metric;
    MeasureTarget target;
    // we are doing file based configuration of targets, inputFile is set
    if (inputFile != "")
    {
        string line;
        ifstream in( inputFile.c_str() );
        while (in >> iface >> metric)
        {
            target.iface = iface;
            target.metric = metric;
            targets.push_back(target);
        }
        in.close();
    }
    // it is empty, we are not doing file based and only have one target pair
    else 
    {
        iface = argv[1];
        metric = argv[2];

        target.iface = iface;
        target.metric = metric;
        targets.push_back(target);
    }
}


void printUsage()
{
    cout << "Usage:\tstatmon iface metric output" << endl;
    cout << "\tstatmon -f config output" << endl;
    cout << endl;
    cout << "Examples:" << endl;
    cout << "statmon eth0 rx_packets test.csv" << endl;
    cout << "statmon -f measure.cfg test.csv" << endl; 
    cout << endl;
    cout << "    -f config: read in config file specifying iface metric pairs" << endl;
    cout << "--file config" << endl;
    cout << endl;
    cout << "iface: which interface you want to measure." << endl;
    cout << "metric: which quantity you want to measure." << endl;
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
    cout << "output: the name of the output csv file for the measurements." << endl;
    cout << endl;
}


void parseArgs(int argc, char *argv[], string &inputFile)
{
    int ch = 0;
    while ( (ch = getopt(argc, argv, "f:h")) != -1 )
    {
        switch(ch)
        {
        case 'f':
            inputFile = optarg;
            break;
        case 'h':
            printUsage();
            exit(0);
        case '?':
        default:
            printUsage();
            exit(1);
        }
    }
}


int main(int argc, char *argv[])
{
    // no matter which way it is executed, we are expecting precisely four arguments
    if (argc != 4)
    {
        printUsage();
        return StatmonConstants::INVALID_NUM_ARGS;
    }

    string inputFile = "";
    parseArgs(argc, argv, inputFile);
    string fname(argv[3]);

    TargetVec targets;
    parseConfigFile(argv, targets, inputFile);
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
    collectData(targets, data);

    ofstream out( fname.c_str() );
    out.setf(ios::fixed,ios::floatfield);
    out.precision( StatmonConstants::OUTPUT_PRECISION );

    out << "i,time,iface,metric,value" << endl;
    MeasureTarget target;
    StatVec stats;
    for (const auto &d : data)
    {
        target = d.first;
        stats = d.second;
        for (const auto &ifstat : stats)
        {
            out << ifstat.iter << "," << ifstat.timeDelta << "," << target.iface << "," 
                << target.metric << "," << ifstat.value << endl;
        }
    }
    out.close();
    return StatmonConstants::SUCCESS;
}

