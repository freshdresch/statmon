// netlink includes
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/cache.h>

// linux and C includes 
#include <csignal>
#include <sys/time.h>
#include <unistd.h>

// C++ includes
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

// use likely and unlikely for my error codes where I care about timing
#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

using namespace std;

static volatile int running = 1;

struct InterfaceStat
{
	double timeDelta;
	unsigned int value;
};

void exitHandler(int dummy) 
{
    running = 0;
}

void teardown(struct nl_sock *sock, struct nl_cache *link_cache)
{
	nl_close(sock);
	nl_cache_free(link_cache);
	nl_socket_free(sock);
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

void printUsage()
{
	cout << "Usage: statmon iface metric output" << endl;
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

int main(int argc, char *argv[])
{
	if (argc != 4)
	{
		printUsage();
		return 1;
	}

	string nic(argv[1]);
	string metric(argv[2]);
	string fname(argv[3]);

	rtnl_link_stat_id_t statId;
	if ( !parseMetric(metric, statId) )
	{
		cerr << "[ERROR] provided metric is invalid." << endl;
		cerr << endl;
		printUsage();
		return 2;
	}

	signal(SIGINT, exitHandler);
	signal(SIGTERM, exitHandler);

	struct nl_sock *sock;
	struct nl_cache *link_cache;
	struct rtnl_link *link;

	uint64_t rtn_stat = 0;
	int err = 0;
	
	sock = nl_socket_alloc();
	if (!sock) 
	{
		cerr << "[ERROR] Unable to allocate netlink socket." << endl;
		return 3;
	}
	
	err = nl_connect(sock, NETLINK_ROUTE);
	if (err < 0) 
	{
		nl_perror(err, "Unable to connect socket");
		nl_socket_free(sock);
		return 4;
	}
	
	err = rtnl_link_alloc_cache(sock, AF_UNSPEC, &link_cache);
	if (err < 0)
    {
		nl_perror(err, "Unable to allocate cache");
		nl_socket_free(sock);
		return 5;
    }

	link = rtnl_link_get_by_name(link_cache, nic.c_str());
	if (!link)
    {
		cerr << "[ERROR] rtnl: failed to get the link by name." << endl;
		teardown(sock, link_cache);
		return 6;
    }

	struct InterfaceStat ifstat;
	vector<InterfaceStat> data;
	struct timeval time;

	// end - start gives us the program time without a frame of reference
	double diff, start, end;
	// offset - base tells us how long our monitoring loop took
	// so we sleep the correct amount
	double offset, base;
	const double MICRO = 1000000.0;
	const unsigned int SAMPLE_RATE = 100000;

	gettimeofday(&time, NULL);
	start = time.tv_sec + (time.tv_usec / MICRO);
	while(running)
	{
		gettimeofday(&time, NULL);
		base = time.tv_usec;

		err = nl_cache_resync(sock, link_cache, NULL, NULL);
		if ( unlikely(err < 0) )
		{
			nl_perror(err, "Unable to resync cache");
			teardown(sock, link_cache);
			return 7;
		}

		link = rtnl_link_get_by_name(link_cache, nic.c_str());
		if ( unlikely(!link) )
		{
			cerr << "[ERROR] rtnl: failed to get the link by name." << endl;
			teardown(sock, link_cache);
			return 6;
		}

		rtn_stat = rtnl_link_get_stat(link, statId);
		ifstat.value = rtn_stat;

		gettimeofday(&time, NULL);
		end = time.tv_sec + (time.tv_usec / 1000000.0);
		diff = end - start;
		ifstat.timeDelta = diff;
		data.push_back(ifstat);

		gettimeofday(&time, NULL);
		offset = time.tv_usec;
		usleep( SAMPLE_RATE - (offset - base) ); // sleep every 0.1 seconds
	}

	cout << endl; // just for clean CTRL-C aftermath

	ofstream out( fname.c_str() );
    out.setf(ios::fixed,ios::floatfield);
    out.precision(3);

	out << "time," << metric << endl;
	vector<InterfaceStat>::iterator it;
	for (it = data.begin(); it != data.end(); ++it)
	{
		ifstat = *it;
		out << ifstat.timeDelta << "," << ifstat.value << endl;
	}
	out.close();

	#ifdef DEBUG
	cout.setf(ios::fixed,ios::floatfield);
    cout.precision(3);
	for (it = data.begin(); it != data.end(); ++it)
	{
		ifstat = *it;
		cout << "Time: " << ifstat.timeDelta << "\t" << nic << " " 
			 << metric << ": " << ifstat.value << endl;
	}
	cout << endl;
	#endif
	// terminate responsibly
	teardown(sock, link_cache);
	return 0;
}
