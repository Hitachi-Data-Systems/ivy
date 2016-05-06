//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

class linuxcpucounters {
public:
// each value is a cumulative long int counter of time spent in clock tick units (jiffies)
	uint64_t
		user,
		nice,
		system,
		idle,
		iowait,
		irq,
		softirq,
		stealtime,
		virtualguest;

	linuxcpucounters();
	linuxcpucounters(linuxcpucounters& other);
	void copy(linuxcpucounters& other);
	std::string toString();
};

struct linuxcpubusypercent{
public:
// each value is 0.0 to 100.0
	double
		user,
		nice,
		system,
		idle,
		iowait,
		irq,
		softirq,
		stealtime,
		virtualguest;
};

struct procstatcounters{
public:
	ivytime t;
	linuxcpucounters overall;
	std::list<linuxcpucounters*> eachcore;
	~procstatcounters();
	void copyFrom(struct procstatcounters& other, std::string when, std::string logfile);
	std::string toString();
};

struct cpubusypercent{
public:
	struct linuxcpubusypercent overall;
	std::list<struct linuxcpubusypercent*> eachcore;
	~cpubusypercent();
};

struct avgcpubusypercent{
public:
	int cores;
	double
		sumcores_total,  avgcore_total,  mincore_total,  maxcore_total,
		sumcores_system, avgcore_system, mincore_system, maxcore_system,
		sumcores_user,   avgcore_user,   mincore_user,   maxcore_user;
	std::string toString();
	bool fromString(std::string);
	void clear();
	avgcpubusypercent();
void	add(const avgcpubusypercent&);
	static std::string csvTitles();
	std::string csvValues(int number_of_subintervals);
};

int getprocstat(struct procstatcounters* psc, const std::string logfilename );
        // read /proc/stat to get cpu usage counters for overall and for each core
        // returns 0 on success, returns -1 with logfile entry on error

int computecpubusy(
        // compute cpu busy % from counter values in jiffies at begin/end of interval
        struct procstatcounters* psc1, // counters at start of interval
        struct procstatcounters* psc2, // counters at end of interval
        struct cpubusypercent* cpubusydetail, // this gets filled in as output
        struct avgcpubusypercent* cpubusysummary, // this gets filled in as output
	const std::string logfilename
);


