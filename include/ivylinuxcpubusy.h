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

#include <map>

struct linuxcpucounters {
public:
// each value is a cumulative long int counter of time spent in clock tick units (jiffies)
	uint64_t
		user {0},
		nice {0},
		system {0},
		idle {0},
		iowait {0},
		irq {0},
		softirq {0},
		stealtime {0},
		virtualguest {0};

	linuxcpucounters(){};
	linuxcpucounters(linuxcpucounters& other);
	void copy(const linuxcpucounters& other);
	std::string toString();
};

struct linuxcpubusypercent{
public:
// each value is 0.0 to 100.0
	double
		user {0},
		nice {0},
		system {0},
		idle {0},
		iowait {0},
		irq {0},
		softirq {0},
		stealtime {0},
		virtualguest {0};
    inline double total() const { return user + system; }
};

std::ostream& operator<<(std::ostream&, const struct linuxcpubusypercent&);

struct procstatcounters{
public:
	ivytime t;
	linuxcpucounters overall;
	std::map<unsigned int /*processor number*/, struct linuxcpucounters*> eachcore;
	~procstatcounters() {for (auto& pear : eachcore) delete pear.second;}
	void copy(const procstatcounters&);
	std::string toString();
};

struct cpubusypercent{
public:
	struct linuxcpubusypercent overall {};
	std::map<unsigned int /*cpu number*/, struct linuxcpubusypercent> eachcore;
	cpubusypercent(){};
	~cpubusypercent(){};
};

std::ostream& operator<<(std::ostream&, const struct cpubusypercent&);


struct avgcpubusypercent{
public:
	int cores {0};
	double
		sumcores_total {0},  avgcore_total {0},  mincore_total {0},  maxcore_total {0},
		sumcores_system {0}, avgcore_system {0}, mincore_system {0}, maxcore_system {0},
		sumcores_user {0},   avgcore_user {0},   mincore_user {0},   maxcore_user {0};
	int activecores {0};
	double
		sum_activecores_total {0},  avg_activecore_total {0},  min_activecore_total {0},  max_activecore_total {0},
		sum_activecores_system {0}, avg_activecore_system {0}, min_activecore_system {0}, max_activecore_system {0},
		sum_activecores_user {0},   avg_activecore_user {0},   min_activecore_user {0},   max_activecore_user {0};
	std::string toString();
	bool fromString(std::string);
	void clear();
	avgcpubusypercent(){};
    void add(const avgcpubusypercent&);
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
    std::map<unsigned int,bool>& active_hyperthreads,
	const std::string logfilename
);

unsigned int core_count(const std::string& /*logfilename*/);
    // reads /proc/stat to get CPU core count.


