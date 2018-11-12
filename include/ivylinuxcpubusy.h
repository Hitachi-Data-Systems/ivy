//Copyright (c) 2016, 2017, 2018 Hitachi Vantara Corporation
//All Rights Reserved.
//
//   Licensed under the Apache License, Version 2.0 (the "License"); you may
//   not use this file except in compliance with the License. You may obtain
//   a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
//   License for the specific language governing permissions and limitations
//   under the License.
//
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
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
    inline double total() const { return user + system; }
};

std::ostream& operator<<(std::ostream&, const struct linuxcpubusypercent&);

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

std::ostream& operator<<(std::ostream&, const struct cpubusypercent&);


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

unsigned int core_count(const std::string& /*logfilename*/);
    // reads /proc/stat to get CPU core count.


