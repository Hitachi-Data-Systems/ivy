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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
//#include "time.h"
#include <stdint.h> // for unit64_t
#include <unistd.h> // for sysconf(_SC_CLK_TCK) which returns clock ticks per second

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <list>
#include <algorithm> // for find_if
#include <map>


#include "ivytime.h"
#include "ivydefines.h"
#include "ivyhelpers.h"
#include "ivylinuxcpubusy.h"
#include "Subinterval_CPU.h"
#include "ivydriver.h"

//#define IVY_LINUX_CPU_BUSY_DETAIL

// In the code below, it is taken as given that the number of cores does not change while running the program.
// Thus when we copy or when we set new values and there is an existing set of counters for each core, we assume
// that we still have the same number of cores and the per-core objects are reused.

linuxcpucounters::linuxcpucounters(linuxcpucounters& other)
{
	user=other.user;
	nice=other.nice;
	system=other.system;
	idle=other.idle;
	iowait=other.iowait;
	irq=other.irq;
	softirq=other.softirq;
	stealtime=other.stealtime;
	virtualguest=other.virtualguest;
}

void linuxcpucounters::copy(const linuxcpucounters& other)
{
	user=other.user;
	nice=other.nice;
	system=other.system;
	idle=other.idle;
	iowait=other.iowait;
	irq=other.irq;
	softirq=other.softirq;
	stealtime=other.stealtime;
	virtualguest=other.virtualguest;
}

std::string linuxcpucounters::toString()
{
    std::ostringstream o;
    o << "user = " << user << ", nice = " << nice << ", system = " << system << ", idle = " << idle << ", iowait = " << iowait
        << ", irq = " << irq << ", softirq = " << softirq << ", stealtime = " << stealtime << ", virtualguest = " << virtualguest;
    return o.str();
}


void procstatcounters::copy(const procstatcounters& other)
{
	t = other.t;

	overall.copy(other.overall);

    eachcore.clear();

    for (const auto& pear : other.eachcore)
    {
        struct linuxcpucounters* p = new linuxcpucounters(*pear.second);
        eachcore[pear.first] = p;
    }
}

std::string procstatcounters::toString()
{
    std::ostringstream o;
    o << "procstatcounters { t = " << t.format_as_datetime_with_ns() << ", overall = " << overall.toString() << std::endl;

    for (auto& pear : eachcore)
    {
        o << "   core " << pear.first << ": " << pear.second->toString() << std::endl;
    }

    return o.str();
}

void avgcpubusypercent::clear()
{
	cores = 0;
	sumcores_total = 0.;
	avgcore_total = 0.;
	mincore_total = 0.;
	maxcore_total = 0.;
	sumcores_system = 0.;
	avgcore_system = 0.;
	mincore_system = 0.;
	maxcore_system = 0.;
	sumcores_user = 0.;
	avgcore_user = 0.;
	mincore_user = 0.;
	maxcore_user = 0.;

	activecores = 0;
	sum_activecores_total = 0.;
	avg_activecore_total = 0.;
	min_activecore_total = 0.;
	max_activecore_total = 0.;
	sum_activecores_system = 0.;
	avg_activecore_system = 0.;
	min_activecore_system = 0.;
	max_activecore_system = 0.;
	sum_activecores_user = 0.;
	avg_activecore_user = 0.;
	min_activecore_user = 0.;
	max_activecore_user = 0.;
}

void	avgcpubusypercent::add(const avgcpubusypercent& other)
{
	if (0 == cores)
	{
		cores = other.cores;

		sumcores_total = other.sumcores_total;
		avgcore_total = other.avgcore_total;
		mincore_total = other.mincore_total;
		maxcore_total = other.maxcore_total;

		sumcores_system = other.sumcores_system;
		avgcore_system = other.avgcore_system;
		mincore_system = other.mincore_system;
		maxcore_system = other.maxcore_system;

		sumcores_user = other.sumcores_user;
		avgcore_user = other.avgcore_user;
		mincore_user = other.mincore_user;
		maxcore_user = other.maxcore_user;
	}
	else if (0 != other.cores)
	{
		cores += other.cores;

		sumcores_total += other.sumcores_total;
		avgcore_total = sumcores_total/((ivy_float)cores);
		if (mincore_total > other.mincore_total) mincore_total = other.mincore_total;
		if (maxcore_total < other.maxcore_total) maxcore_total = other.maxcore_total;

		sumcores_system += other.sumcores_system;
		avgcore_system = sumcores_system/((ivy_float)cores);
		if (mincore_system > other.mincore_system) mincore_system = other.mincore_system;
		if (maxcore_system < other.maxcore_system) maxcore_system = other.maxcore_system;

		sumcores_user += other.sumcores_user;
		avgcore_user = sumcores_user / ((ivy_float)cores);
		if (mincore_user > other.mincore_user) mincore_user = other.mincore_user;
		if (maxcore_user < other.maxcore_user) maxcore_user = other.maxcore_user;
	}

	if (0 == activecores)
	{
		activecores = other.activecores;

		sum_activecores_total = other.sum_activecores_total;
		avg_activecore_total = other.avg_activecore_total;
		min_activecore_total = other.min_activecore_total;
		max_activecore_total = other.max_activecore_total;

		sum_activecores_system = other.sum_activecores_system;
		avg_activecore_system = other.avg_activecore_system;
		min_activecore_system = other.min_activecore_system;
		max_activecore_system = other.max_activecore_system;

		sum_activecores_user = other.sum_activecores_user;
		avg_activecore_user = other.avg_activecore_user;
		min_activecore_user = other.min_activecore_user;
		max_activecore_user = other.max_activecore_user;
	}
	else if (0 != other.activecores)
	{
		activecores += other.activecores;

		sum_activecores_total += other.sum_activecores_total;
		avg_activecore_total = sum_activecores_total/((ivy_float)activecores);
		if (min_activecore_total > other.min_activecore_total) min_activecore_total = other.min_activecore_total;
		if (max_activecore_total < other.max_activecore_total) max_activecore_total = other.max_activecore_total;

		sum_activecores_system += other.sum_activecores_system;
		avg_activecore_system = sum_activecores_system/((ivy_float)activecores);
		if (min_activecore_system > other.min_activecore_system) min_activecore_system = other.min_activecore_system;
		if (max_activecore_system < other.max_activecore_system) max_activecore_system = other.max_activecore_system;

		sum_activecores_user += other.sum_activecores_user;
		avg_activecore_user = sum_activecores_user / ((ivy_float)activecores);
		if (min_activecore_user > other.min_activecore_user) min_activecore_user = other.min_activecore_user;
		if (max_activecore_user < other.max_activecore_user) max_activecore_user = other.max_activecore_user;
	}
}


std::string avgcpubusypercent::toString()
{
	std::ostringstream o;
	o <<"<"
	<< cores
	<< "," << sumcores_total
	<< "," << avgcore_total
	<< "," << mincore_total
	<< "," << maxcore_total
	<< "," << sumcores_system
	<< "," << avgcore_system
	<< "," << mincore_system
	<< "," << maxcore_system
	<< "," << sumcores_user
	<< "," << avgcore_user
	<< "," << mincore_user
	<< "," << maxcore_user
	<< "," << activecores
	<< "," << sum_activecores_total
	<< "," << avg_activecore_total
	<< "," << min_activecore_total
	<< "," << max_activecore_total
	<< "," << sum_activecores_system
	<< "," << avg_activecore_system
	<< "," << min_activecore_system
	<< "," << max_activecore_system
	<< "," << sum_activecores_user
	<< "," << avg_activecore_user
	<< "," << min_activecore_user
	<< "," << max_activecore_user
	<< ">";
	return o.str();
}

bool avgcpubusypercent::fromString(std::string s)
{
	std::istringstream is(s);
	char c;
	is >> c; if (is.fail() || ('<' != c)) {clear(); return false;}

	is >> cores; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> sumcores_total; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> avgcore_total; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> mincore_total ; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> maxcore_total; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> sumcores_system; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> avgcore_system; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> mincore_system ; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> maxcore_system; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> sumcores_user; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> avgcore_user; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> mincore_user ; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> maxcore_user; if (is.fail()) {clear(); return false;}

	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> activecores; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> sum_activecores_total; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> avg_activecore_total; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> min_activecore_total ; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> max_activecore_total; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> sum_activecores_system; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> avg_activecore_system; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> min_activecore_system ; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> max_activecore_system; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> sum_activecores_user; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> avg_activecore_user; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> min_activecore_user ; if (is.fail()) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> max_activecore_user; if (is.fail()) {clear(); return false;}

	is >> c; if (is.fail() || ('>' != c)) {clear(); return false;}
	is >> c; if (!is.fail()) {clear(); return false;}
	return true;
}


// read linux's /proc/stat to get overall system CPU usage info


//    $ cat /proc/stat
//    cpu  167898 1946 24030 7415559 20916 0 366 0 0 0
//    cpu0 10200 334 1474 461689 683 0 93 0 0 0
//    cpu1 11449 246 1794 462755 836 0 40 0 0 0
//    cpu2 9561 356 1280 465095 759 0 21 0 0 0
//    cpu3 8280 308 1341 466634 533 0 14 0 0 0
//    cpu4 19825 29 2505 454051 601 0 20 0 0 0
//    cpu5 17670 98 2472 456428 363 0 19 0 0 0
//    cpu6 19337 191 2295 454959 256 0 13 0 0 0
//    cpu7 22097 277 2524 451753 311 0 22 0 0 0
//    cpu8 4797 0 853 471095 208 0 6 0 0 0
//    cpu9 5220 2 1012 469995 932 0 2 0 0 0
//    cpu10 4789 39 780 471329 221 0 2 0 0 0
//    cpu11 4784 2 866 471381 130 0 30 0 0 0
//    cpu12 7066 0 1011 468544 299 0 55 0 0 0
//    cpu13 6321 1 1162 469520 161 0 3 0 0 0
//    cpu14 5769 2 951 470307 137 0 4 0 0 0
//    cpu15 10725 52 1701 450017 14479 0 15 0 0 0
//    intr 7139097 1012304 0 0 0 0 0 0 0 1 1 0 0 0 0 0 0 0 0 30 57 0 0 0 16477 0 0 0 0 0 0 0 0 0 0 74580 0 0 1 75995 6291 12929 7280 2606 2609 61621 11141 0 0 1 15344 2596 2582 2534 2371 2371 2371 2371 0 0 2371 2371 2371 2371 2371 2371 2371 2371 0 0 2371 2371 2371 2371 2371 2371 2371 2371 35 221 0 4 0 0 35 745 0 4 0 0 2 0 2 0 2 0 2 2 2 2 2 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
//    ctxt 14904001
//    btime 1555006116
//    processes 5629
//    procs_running 2
//    procs_blocked 0
//    softirq 4483094 10 2615773 975 370196 87400 0 17513 797030 0 594197

int getprocstat(struct procstatcounters* psc, const std::string logfilename ){
        // read /proc/stat to get cpu usage counters for overall and for each core
        // returns 0 on success, returns -1 with logfile entry on error
	psc->t.setToNow();
	std::ifstream procstat("/proc/stat");
	if (!procstat.good()){
		fileappend(logfilename,"Failed opening /proc/stat.\n");
		return -1;
	}
	std::string inputline;
	unsigned int cpu_number;
	int line=0;

	while (procstat.good()){
		getline(procstat,inputline);
		if (!procstat.good()) break;
		line++;
		trim(inputline); // removes whitespace at beginning and end

		if (1==line) {
			if (inputline.substr(0,4)!=std::string("cpu ")) {
				fileappend(logfilename,"first line from /proc/stat was not overall cpu line.\n");
				procstat.close();
				return -1;
			}
			std::istringstream iss(inputline.substr(4,inputline.size()-4));
			iss >> psc->overall.user >> psc->overall.nice >> psc->overall.system
				>> psc->overall.idle >> psc->overall.iowait >> psc->overall.irq
				>> psc->overall.softirq >> psc->overall.stealtime >> psc->overall.virtualguest;
		} else {
			if (1<line) {
				if (inputline.substr(0,5)==std::string("intr ")) {
					procstat.close();
					return 0;
				}
			}
			if (inputline.substr(0,3)!=std::string("cpu")) {
				fileappend(logfilename,"Did not find expected \"cpu\" detail line.\n");
				procstat.close();
				return -1;
			}
			{
				std::istringstream iss(inputline.substr(3,inputline.size()-3));

				iss >> cpu_number;
				if(iss.fail())
				{
				    std::ostringstream o; o << "<Error> internal error - in getprocstat(), failed reading cpu number in what is supposed to be a cpu detail line - \""
				        << inputline << "\".  Occurred at " << __FILE__ << " line " << __LINE__ << ".";
                    fileappend (logfilename,o.str());
                    return -1;
				}

                struct linuxcpucounters* p;

                std::map<unsigned int /*processor number*/, struct linuxcpucounters*>::iterator it = psc->eachcore.find(cpu_number);

                if (it == psc->eachcore.end())
                {
                    psc->eachcore[cpu_number] = p = new struct linuxcpucounters();
                }
                else
                {
                    p = it->second;
                }

				iss	>> p->user >> p->nice >> p->system
					>> p->idle >> p->iowait >> p->irq
					>> p->softirq >> p->stealtime >> p->virtualguest;
			}
		}
	}
	fileappend(logfilename,"Hit end-of-file prematurely on /proc/stat.\n");
	procstat.close();
	return -1;
}




int computecpubusy(
    // compute cpu busy % from counter values in jiffies at begin/end of interval
    struct procstatcounters* psc1, // counters at start of interval
    struct procstatcounters* psc2, // counters at end of interval
    struct cpubusypercent* p_cpubusydetail, // this gets filled in as output
    struct avgcpubusypercent* p_cpubusysummary, // this gets filled in as output
    std::map<unsigned int,bool>& active_hyperthreads,
	const std::string logfilename
){
    ivy_float duration_seconds=psc2->t.getlongdoubleseconds() - psc1->t.getlongdoubleseconds();

	if (psc1->eachcore.size()==0 || psc1->eachcore.size()!=psc2->eachcore.size()) {
		{std::ostringstream o; o<< "Starting procstatcounters is size " << (psc1->eachcore).size() << ", ending procstatcounters is size " << (psc2->eachcore).size() << ".\n"; fileappend(logfilename,o.str());}
		fileappend(logfilename,"computecpubusy(): number of cores was zero or number of cores at beginning and end not the same.\n");
		return -1;
	}
	if (p_cpubusydetail->eachcore.size()!=0) {
		fileappend(logfilename,"computecpubusy(): cpubusydetail was not empty to start.\n");
		return -1;
	}
	int jiffiespersecond=sysconf(_SC_CLK_TCK);

	if (-1==jiffiespersecond)
	{
		fileappend(logfilename,std::string("computecpubusy(): sysconf(_SC_CLK_TCK) failed.\n"));
		return -1;
	}
	double secondsperjiffy=1.0/((double)jiffiespersecond);
	p_cpubusydetail->overall.user        =100.*((double)(psc2->overall.user        -psc1->overall.user        ))*secondsperjiffy / duration_seconds;
	p_cpubusydetail->overall.nice        =100.*((double)(psc2->overall.nice        -psc1->overall.nice        ))*secondsperjiffy / duration_seconds;
	p_cpubusydetail->overall.system      =100.*((double)(psc2->overall.system      -psc1->overall.system      ))*secondsperjiffy / duration_seconds;
	p_cpubusydetail->overall.idle        =100.*((double)(psc2->overall.idle        -psc1->overall.idle        ))*secondsperjiffy / duration_seconds;
	p_cpubusydetail->overall.iowait      =100.*((double)(psc2->overall.iowait      -psc1->overall.iowait      ))*secondsperjiffy / duration_seconds;
	p_cpubusydetail->overall.irq         =100.*((double)(psc2->overall.irq         -psc1->overall.irq         ))*secondsperjiffy / duration_seconds;
	p_cpubusydetail->overall.softirq     =100.*((double)(psc2->overall.softirq     -psc1->overall.softirq     ))*secondsperjiffy / duration_seconds;
	p_cpubusydetail->overall.stealtime   =100.*((double)(psc2->overall.stealtime   -psc1->overall.stealtime   ))*secondsperjiffy / duration_seconds;
	p_cpubusydetail->overall.virtualguest=100.*((double)(psc2->overall.virtualguest-psc1->overall.virtualguest))*secondsperjiffy / duration_seconds;

	for (auto& pear : psc1->eachcore)
	{
		unsigned int processor = pear.first;

		struct linuxcpubusypercent* p = &(p_cpubusydetail->eachcore[processor]);
		struct linuxcpucounters* p1 = (psc1->eachcore)[processor];
		struct linuxcpucounters* p2 = (psc2->eachcore)[processor];
		p->user        =100.*((double)(p2->user        -p1->user        ))*secondsperjiffy/duration_seconds;
		p->nice        =100.*((double)(p2->nice        -p1->nice        ))*secondsperjiffy/duration_seconds;
		p->system      =100.*((double)(p2->system      -p1->system      ))*secondsperjiffy/duration_seconds;
		p->idle        =100.*((double)(p2->idle        -p1->idle        ))*secondsperjiffy/duration_seconds;
		p->iowait      =100.*((double)(p2->iowait      -p1->iowait      ))*secondsperjiffy/duration_seconds;
		p->irq         =100.*((double)(p2->irq         -p1->irq         ))*secondsperjiffy/duration_seconds;
		p->softirq     =100.*((double)(p2->softirq     -p1->softirq     ))*secondsperjiffy/duration_seconds;
		p->stealtime   =100.*((double)(p2->stealtime   -p1->stealtime   ))*secondsperjiffy/duration_seconds;
		p->virtualguest=100.*((double)(p2->virtualguest-p1->virtualguest))*secondsperjiffy/duration_seconds;
	}
	p_cpubusysummary->cores=p_cpubusydetail->eachcore.size();

	p_cpubusysummary->sumcores_total = p_cpubusydetail->overall.user + p_cpubusydetail->overall.system;
	p_cpubusysummary->avgcore_total = p_cpubusysummary->sumcores_total/p_cpubusysummary->cores;

	p_cpubusysummary->sumcores_system = p_cpubusydetail->overall.system;
	p_cpubusysummary->avgcore_system = p_cpubusydetail->overall.system/p_cpubusysummary->cores;

	p_cpubusysummary->sumcores_user = p_cpubusydetail->overall.user;
	p_cpubusysummary->avgcore_user = p_cpubusydetail->overall.user/p_cpubusysummary->cores;

	bool firstpass {true};

	bool firstactive {true};

	p_cpubusysummary->activecores = 0;

	p_cpubusysummary->sum_activecores_total = 0;
	p_cpubusysummary->sum_activecores_system = 0;
	p_cpubusysummary->sum_activecores_user = 0;

	p_cpubusysummary->avg_activecore_total = 0;
	p_cpubusysummary->avg_activecore_system = 0;
	p_cpubusysummary->avg_activecore_user = 0;

	for (auto& pear : p_cpubusydetail->eachcore)
	{
		double total = pear.second.user + pear.second.system;

		if (firstpass)
		{
		    firstpass = false;
			p_cpubusysummary->mincore_total = total;
			p_cpubusysummary->maxcore_total = total;
			p_cpubusysummary->mincore_system= pear.second.system;
			p_cpubusysummary->maxcore_system= pear.second.system;
			p_cpubusysummary->mincore_user  = pear.second.user;
			p_cpubusysummary->maxcore_user  = pear.second.user;
		} else {
			if (total<p_cpubusysummary->mincore_total)                p_cpubusysummary->mincore_total   = total;
			if (total>p_cpubusysummary->maxcore_total)                p_cpubusysummary->maxcore_total   = total;
			if (pear.second.system < p_cpubusysummary->mincore_system)p_cpubusysummary->mincore_system  = pear.second.system;
			if (pear.second.system > p_cpubusysummary->maxcore_system)p_cpubusysummary->maxcore_system  = pear.second.system;
			if (pear.second.user < p_cpubusysummary->mincore_user)    p_cpubusysummary->mincore_user    = pear.second.user;
			if (pear.second.user > p_cpubusysummary->maxcore_user)    p_cpubusysummary->maxcore_user    = pear.second.user;
		}


		if (active_hyperthreads[pear.first])
		{
		    p_cpubusysummary->activecores++;

		    p_cpubusysummary->sum_activecores_total += total;
		    p_cpubusysummary->sum_activecores_system += pear.second.system;
		    p_cpubusysummary->sum_activecores_user += pear.second.user;

		    p_cpubusysummary->avg_activecore_total  = p_cpubusysummary->sum_activecores_total  / ((double) p_cpubusysummary->activecores);
		    p_cpubusysummary->avg_activecore_system = p_cpubusysummary->sum_activecores_system / ((double) p_cpubusysummary->activecores);
		    p_cpubusysummary->avg_activecore_user   = p_cpubusysummary->sum_activecores_user   / ((double) p_cpubusysummary->activecores);

		    if (firstactive)
		    {
		        firstactive = false;

                p_cpubusysummary->min_activecore_total  = total;
                p_cpubusysummary->min_activecore_system = pear.second.system;
                p_cpubusysummary->min_activecore_user   = pear.second.user;

                p_cpubusysummary->max_activecore_total  = total;
                p_cpubusysummary->max_activecore_system = pear.second.system;
                p_cpubusysummary->max_activecore_user   = pear.second.user;
		    }
		    else
		    {
                if (p_cpubusysummary->min_activecore_total  > total)              p_cpubusysummary->min_activecore_total  = total;
                if (p_cpubusysummary->min_activecore_system > pear.second.system) p_cpubusysummary->min_activecore_system = pear.second.system;
                if (p_cpubusysummary->min_activecore_user   > pear.second.user)   p_cpubusysummary->min_activecore_user   = pear.second.user;;

                if (p_cpubusysummary->max_activecore_total  < total)              p_cpubusysummary->max_activecore_total  = total;
                if (p_cpubusysummary->max_activecore_system < pear.second.system) p_cpubusysummary->max_activecore_system = pear.second.system;
                if (p_cpubusysummary->max_activecore_user   < pear.second.user)   p_cpubusysummary->max_activecore_user   = pear.second.user;
		    }
		}
	}
	return 0;
}

/*static*/ std::string avgcpubusypercent::csvTitles()
{
	return std::string
	(
		",test host cores"
		",test host avg core CPU % busy"
		",test host active cores"
		",test host active core CPU % busy"
#ifdef IVY_LINUX_CPU_BUSY_DETAIL
		",test host min active core total CPU % busy"
		",test host max active core total CPU % busy"
		",test host avg active core system CPU % busy"
		",test host min active core system CPU % busy"
		",test host max active core system CPU % busy"
		",test host avg active core user CPU % busy"
		",test host min active core user CPU % busy"
		",test host max active core user CPU % busy"
#endif // IVY_LINUX_CPU_BUSY_DETAIL
	);
}

std::string avgcpubusypercent::csvValues(int number_of_subintervals)
{
	std::ostringstream o;

	if (0 >= number_of_subintervals)
	{
		o << "Error calculating CPU busy numbers - avgcpubusypercent::csvValues(int number_of_subintervals=" << number_of_subintervals << ") - number_of_subintervals must be a positive number.";
		return o.str();
	}

	o 	<< (cores/number_of_subintervals)
		<< ',' << std::fixed << std::setprecision(1) << avgcore_total << "%"
		<< ',' << (activecores/number_of_subintervals)
		<< ',' << std::fixed << std::setprecision(1) << avg_activecore_total << "%"
#ifdef IVY_LINUX_CPU_BUSY_DETAIL
		<< ',' << std::fixed << std::setprecision(1) << min_activecore_total << "%"
		<< ',' << std::fixed << std::setprecision(1) << max_activecore_total << "%"
		<< ',' << std::fixed << std::setprecision(1) << avg_activecore_system << "%"
		<< ',' << std::fixed << std::setprecision(1) << min_activecore_system << "%"
		<< ',' << std::fixed << std::setprecision(1) << max_activecore_system << "%"
		<< ',' << std::fixed << std::setprecision(1) << avg_activecore_user << "%"
		<< ',' << std::fixed << std::setprecision(1) << min_activecore_user << "%"
		<< ',' << std::fixed << std::setprecision(1) << max_activecore_user << "%"
#endif // IVY_LINUX_CPU_BUSY_DETAIL
		;
	return o.str();
}

/*static*/ std::string Subinterval_CPU::csvTitles()
{
	return avgcpubusypercent::csvTitles();
}

void Subinterval_CPU::clear()
{
	rollup_count=0;
	overallCPU.clear();
	eachHostCPU.clear();
}

bool Subinterval_CPU::add(std::string hostname, const avgcpubusypercent& a)
{
	// sets rollup_count from zero to 1 as the first host entry is added.
	if (0 == rollup_count)
	{
		rollup_count=1;
	}
	if (1 < rollup_count)
		{ clear(); return false;} // fromString may not be used on completed individal subinterval objects with all the host data in place that are being rolled up.

	overallCPU.add(a);

	eachHostCPU[hostname]=a;

	return true;
}


void Subinterval_CPU::rollup(const Subinterval_CPU& other)
{
	// used to make an average over a series of subintervals.
	// The assumption is made that over each subinterval, the set of hosts is the same.

	rollup_count += other.rollup_count;
	overallCPU.add(other.overallCPU);
	for (auto& pear : other.eachHostCPU)
	{
		eachHostCPU[pear.first].add(pear.second);
	}
}


std::string Subinterval_CPU::csvValuesAvgOverHosts()
{
	if (0 == rollup_count)
	{
		return std::string("SubintervalCPU::csvValuesAvgOverHosts() is being called for an empty SubintervalCPU object.");
	}

	return overallCPU.csvValues(rollup_count);
}


std::string Subinterval_CPU::csvValues(std::string hostname)
{
	if (0 == rollup_count)
	{
		return std::string("SubintervalCPU::csvValues(hostname=\"") + hostname + std::string("\") is being called for an empty SubintervalCPU object.");
	}

	std::map<std::string, avgcpubusypercent>::iterator it = eachHostCPU.find(hostname);
	if (eachHostCPU.end() == it)
	{
		return std::string("SubintervalCPU::csvValues(hostname=\"") + hostname + std::string("\") is being called for an invalid hostname.");
	}

	return (*it).second.csvValues(rollup_count);
}


std::regex cpu_line_regex( "cpu[0-9]+ [ 0-9]+" );

unsigned int core_count(const std::string& logfilename)
{
    // read /proc/stat to get CPU core count.

    //cpu  950690554 589052403 469908370 17009911838 47148691 0 18196600 0 0 0
    //cpu0 80923480 45164263 39063505 961249444 4673263 0 12111457 0 0 0
    //cpu1 60738717 38087730 29195607 1061391545 4973075 0 2625995 0 0 0
    //cpu2 51342786 43908415 22505328 1072156314 5019881 0 1274456 0 0 0
    //...
    //intr ...

	std::ifstream procstat("/proc/stat");
	if (!procstat.good()){
	    std::ostringstream o;
	    o << "<Error> Internal programming error - failed opening /proc/stat at line " << __LINE__ << " of " << __FILE__;
		throw std::runtime_error(o.str());
	}

	std::string inputline;

	unsigned int cores {0};

	while (procstat.good()){
		getline(procstat,inputline);
		if (!procstat.good()) break;
		trim(inputline); // removes whitespace at beginning and end
		if (std::regex_match(inputline,cpu_line_regex))
		{
		    cores++;
		}
	}
	return cores;
}


std::ostream& operator<<(std::ostream& o, const struct linuxcpubusypercent& c)
{
    o << "total (user+system) = " << c.total() << "%"
        << "%, user = "           << c.user << "%"
        << "%, nice = "           << c.nice << "%"
        << "%, system = "         << c.system << "%"
        << "%, idle = "           << c.idle << "%"
        << "%, iowait = "         << c.iowait << "%"
        << "%, irq = "            << c.irq << "%"
        << "%, softirq = "        << c.softirq << "%"
        << "%, stealtime = "      << c.stealtime << "%"
        << "%, virtualguest = "   << c.virtualguest << "%"
        ;
    return o;
}


std::ostream& operator<<(std::ostream&o, const struct cpubusypercent& c)
{
    o << "overall: " << c.overall << std::endl;

    for (const auto& pear : c.eachcore)
    {
        o << "hyperthread " << pear.first << ": " << pear.second << std::endl;
    }

    return o;
}


























