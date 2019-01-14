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

void linuxcpucounters::copy(linuxcpucounters& other)
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

linuxcpucounters::linuxcpucounters()
{
	user=0;
	nice=0;
	system=0;
	idle=0;
	iowait=0;
	irq=0;
	softirq=0;
	stealtime=0;
	virtualguest=0;
}

std::string linuxcpucounters::toString()
{
    std::ostringstream o;
    o << "user = " << user << ", nice = " << nice << ", system = " << system << ", idle = " << idle << ", iowait = " << iowait
        << ", irq = " << irq << ", softirq = " << softirq << ", stealtime = " << stealtime << ", virtualguest = " << virtualguest;
    return o.str();
}


procstatcounters::~procstatcounters() {
	while (eachcore.size()>0) {
		linuxcpucounters* p = eachcore.front();
		eachcore.pop_front();
		delete p;
	}
};

void procstatcounters::copyFrom(struct procstatcounters& other, std::string when, std::string logfile)
{

	if (0 == eachcore.size())
		for (auto& p_linuxcpucounters: other.eachcore)
		{
			eachcore.push_back(new linuxcpucounters(*p_linuxcpucounters));
		}
	else
	{
		std::list<linuxcpucounters*>::iterator it=eachcore.begin();
		std::list<linuxcpucounters*>::iterator other_it=other.eachcore.begin();
		while (other.eachcore.end() != other_it)
		{
			(*it)->copy(**other_it);
			it++; other_it++;
		}
	}
	overall.copy(other.overall);
	t = other.t;
}

std::string procstatcounters::toString()
{
    std::ostringstream o;
    o << "procstatcounters { t = " << t.format_as_datetime_with_ns() << ", overall = " << overall.toString() << std::endl;
    int core =0;
    for (auto& p : eachcore)
    {
        o << "   core " << core << ": " << p->toString() << std::endl;
        core++;
    }
    return o.str();
}

cpubusypercent::~cpubusypercent() {
	while (eachcore.size()>0) {
		struct linuxcpubusypercent* p = eachcore.front();
		eachcore.pop_front();
		delete p;
	}
};

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

}

avgcpubusypercent::avgcpubusypercent()
{
	clear();
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
}


std::string avgcpubusypercent::toString()
{
	std::ostringstream o;
	o<<"<" << cores
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
	is >> sumcores_total; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> avgcore_total; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> mincore_total ; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> maxcore_total; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> sumcores_system; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> avgcore_system; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> mincore_system ; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> maxcore_system; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> sumcores_user; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> avgcore_user; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> mincore_user ; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> maxcore_user; if (is.fail() || (',' != c)) {clear(); return false;}
	is >> c; if (is.fail() || ('>' != c)) {clear(); return false;}
	is >> c; if (!is.fail()) {clear(); return false;}
	return true;
}

// read linux's /proc/stat to get overall system CPU usage info

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
	std::string cpuname;
	int line=0;

	std::list<linuxcpucounters*>::iterator it = psc->eachcore.begin();

	bool creating;
	if (psc->eachcore.size() == 0)
		creating=true;
	else
		creating=false;

	while (procstat.good()){
		getline(procstat,inputline);
		if (!procstat.good()) break;
		line++;
		trim(inputline); // removes whitespace at beginning and end
//{
//ostringstream o; o << std::endl << "getprocstat() input line " << line << " " << inputline << std::endl;
//fileappend(logfilename,o.str());
//}
		if (1==line) {
			if (inputline.substr(0,4)!=std::string("cpu ")) {
				fileappend(logfilename,"first line from /proc/stat was not overall cpu line.\n");
				procstat.close();
				return -1;
			}
			std::istringstream iss(inputline);
			iss >> cpuname
				>> psc->overall.user >> psc->overall.nice >> psc->overall.system
				>> psc->overall.idle >> psc->overall.iowait >> psc->overall.irq
				>> psc->overall.softirq >> psc->overall.stealtime >> psc->overall.virtualguest;
		} else {
			if (2<line) {
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
				std::istringstream iss(inputline);
				struct linuxcpucounters* p = new struct linuxcpucounters;
				if (creating)
				{
					p = new linuxcpucounters;
					psc->eachcore.push_back(p);
				}
				else
				{
					p = (*it);
					it++;
				}
				iss >> cpuname
					>> p->user >> p->nice >> p->system
					>> p->idle >> p->iowait >> p->irq
					>> p->softirq >> p->stealtime >> p->virtualguest;
//{
//ostringstream o; o << "getprocstat() pushing back cpuname=" << cpuname
//<< ", user=" << std::fixed << std::dec << p->user << " 0x" << std::fixed << std::hex << p->user
//<< ", system=" << std::fixed << std::dec << p->user << " 0x" << std::fixed << std::hex << p->user
//<< std::endl;
//fileappend(logfilename,o.str());
//}
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
        struct cpubusypercent* cpubusydetail, // this gets filled in as output
        struct avgcpubusypercent* cpubusysummary, // this gets filled in as output
	const std::string logfilename
){
        ivy_float duration_seconds=psc2->t.getlongdoubleseconds() - psc1->t.getlongdoubleseconds();
//{
//ostringstream o; o<<"computecpubusy() duration is " << duration_seconds << " seconds." << std::endl;
//fileappend(logfilename,o.str());
//}
	if (psc1->eachcore.size()==0 || psc1->eachcore.size()!=psc2->eachcore.size()) {
		{std::ostringstream o; o<< "Starting procstatcounters is size " << (psc1->eachcore).size() << ", ending procstatcounters is size " << (psc2->eachcore).size() << ".\n"; fileappend(logfilename,o.str());}
		fileappend(logfilename,"computecpubusy(): number of cores was zero or number of cores at beginning and end not the same.\n");
		return -1;
	}
	if (cpubusydetail->eachcore.size()!=0) {
		fileappend(logfilename,"computecpubusy(): cpubusydetail was not empty to start.\n");
		return -1;
	}
	int jiffiespersecond=sysconf(_SC_CLK_TCK);
//{
//ostringstream o; o<<"computecpubusy() sysconf(_SC_CLK_TCK) returned " << jiffiespersecond << " jiffies per second." << std::endl;
//fileappend(logfilename,o.str());
//}
	if (-1==jiffiespersecond) {
		fileappend(logfilename,std::string("computecpubusy(): sysconf(_SC_CLK_TCK) failed.\n"));
		return -1;
	}
	double secondsperjiffy=1.0/((double)jiffiespersecond);
	cpubusydetail->overall.user        =100.*((double)(psc2->overall.user        -psc1->overall.user        ))*secondsperjiffy / duration_seconds;
	cpubusydetail->overall.nice        =100.*((double)(psc2->overall.nice        -psc1->overall.nice        ))*secondsperjiffy / duration_seconds;
	cpubusydetail->overall.system      =100.*((double)(psc2->overall.system      -psc1->overall.system      ))*secondsperjiffy / duration_seconds;
	cpubusydetail->overall.idle        =100.*((double)(psc2->overall.idle        -psc1->overall.idle        ))*secondsperjiffy / duration_seconds;
	cpubusydetail->overall.iowait      =100.*((double)(psc2->overall.iowait      -psc1->overall.iowait      ))*secondsperjiffy / duration_seconds;
	cpubusydetail->overall.irq         =100.*((double)(psc2->overall.irq         -psc1->overall.irq         ))*secondsperjiffy / duration_seconds;
	cpubusydetail->overall.softirq     =100.*((double)(psc2->overall.softirq     -psc1->overall.softirq     ))*secondsperjiffy / duration_seconds;
	cpubusydetail->overall.stealtime   =100.*((double)(psc2->overall.stealtime   -psc1->overall.stealtime   ))*secondsperjiffy / duration_seconds;
	cpubusydetail->overall.virtualguest=100.*((double)(psc2->overall.virtualguest-psc1->overall.virtualguest))*secondsperjiffy / duration_seconds;
	std::list<struct linuxcpucounters*>::iterator psc1iter=psc1->eachcore.begin();
	std::list<struct linuxcpucounters*>::iterator psc2iter=psc2->eachcore.begin();
	for (;psc1iter!=psc1->eachcore.end();psc1iter++,psc2iter++){
		struct linuxcpubusypercent* p = new struct linuxcpubusypercent;
		p->user        =100.*((double)((*psc2iter)->user        -(*psc1iter)->user        ))*secondsperjiffy/duration_seconds;
		p->nice        =100.*((double)((*psc2iter)->nice        -(*psc1iter)->nice        ))*secondsperjiffy/duration_seconds;
		p->system      =100.*((double)((*psc2iter)->system      -(*psc1iter)->system      ))*secondsperjiffy/duration_seconds;
		p->idle        =100.*((double)((*psc2iter)->idle        -(*psc1iter)->idle        ))*secondsperjiffy/duration_seconds;
		p->iowait      =100.*((double)((*psc2iter)->iowait      -(*psc1iter)->iowait      ))*secondsperjiffy/duration_seconds;
		p->irq         =100.*((double)((*psc2iter)->irq         -(*psc1iter)->irq         ))*secondsperjiffy/duration_seconds;
		p->softirq     =100.*((double)((*psc2iter)->softirq     -(*psc1iter)->softirq     ))*secondsperjiffy/duration_seconds;
		p->stealtime   =100.*((double)((*psc2iter)->stealtime   -(*psc1iter)->stealtime   ))*secondsperjiffy/duration_seconds;
		p->virtualguest=100.*((double)((*psc2iter)->virtualguest-(*psc1iter)->virtualguest))*secondsperjiffy/duration_seconds;
		cpubusydetail->eachcore.push_back(p);
	}
	cpubusysummary->cores=cpubusydetail->eachcore.size();

	cpubusysummary->sumcores_total = cpubusydetail->overall.user + cpubusydetail->overall.system;
	cpubusysummary->avgcore_total = cpubusysummary->sumcores_total/cpubusysummary->cores;

	cpubusysummary->sumcores_system = cpubusydetail->overall.system;
	cpubusysummary->avgcore_system = cpubusydetail->overall.system/cpubusysummary->cores;

	cpubusysummary->sumcores_user = cpubusydetail->overall.user;
	cpubusysummary->avgcore_user = cpubusydetail->overall.user/cpubusysummary->cores;

	for (std::list<struct linuxcpubusypercent*>::iterator it=cpubusydetail->eachcore.begin();it!=cpubusydetail->eachcore.end();it++) {
		double total = (*it)->user+(*it)->system;
		if (it==cpubusydetail->eachcore.begin()){
			cpubusysummary->mincore_total = total;
			cpubusysummary->maxcore_total = total;
			cpubusysummary->mincore_system= (*it)->system;
			cpubusysummary->maxcore_system= (*it)->system;
			cpubusysummary->mincore_user  = (*it)->user;
			cpubusysummary->maxcore_user  = (*it)->user;
//{
//std::ostringstream o; o<<"First pass cores=" << cpubusysummary->cores << std::endl
//<< ", sumcores_total=" << cpubusysummary->sumcores_total << ", avgcore_total=" << cpubusysummary->avgcore_total << std::endl
//<< ", sumcores_system=" << cpubusysummary->sumcores_system << ", avgcore_system=" << cpubusysummary->avgcore_system << std::endl
//<< ", sumcores_user=" << cpubusysummary->sumcores_user << ", avgcore_user=" << cpubusysummary->avgcore_user << std::endl
//<< "mincore_total=" << cpubusysummary->mincore_total << ", maxcore_total=" << cpubusysummary->maxcore_total << std::endl
//<< ", mincore_system=" << cpubusysummary->mincore_system << ", maxcore_system=" << cpubusysummary->maxcore_system << std::endl
//<< ", mincore_user=" << cpubusysummary->mincore_user << ", maxcore_user=" << cpubusysummary->maxcore_user << std::endl;
//fileappend(logfilename,o.str());
//}
		} else {
			if (total<cpubusysummary->mincore_total)cpubusysummary->mincore_total=total;
			if (total>cpubusysummary->maxcore_total)cpubusysummary->maxcore_total=total;
			if ((*it)->system<cpubusysummary->mincore_system)cpubusysummary->mincore_system=(*it)->system;
			if ((*it)->system>cpubusysummary->maxcore_system)cpubusysummary->maxcore_system=(*it)->system;
			if ((*it)->user<cpubusysummary->mincore_user)cpubusysummary->mincore_user=(*it)->user;
			if ((*it)->user>cpubusysummary->maxcore_user)cpubusysummary->maxcore_user=(*it)->user;
//{
//std::ostringstream o; o<<"subsequent pass " << std::endl
//<< "mincore_total=" << cpubusysummary->mincore_total << ", maxcore_total=" << cpubusysummary->maxcore_total << std::endl
//<< ", mincore_system=" << cpubusysummary->mincore_system << ", maxcore_system=" << cpubusysummary->maxcore_system << std::endl
//<< ", mincore_user=" << cpubusysummary->mincore_user << ", maxcore_user=" << cpubusysummary->maxcore_user << std::endl;
//fileappend(logfilename,o.str());
//}
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
#ifdef IVY_LINUX_CPU_BUSY_DETAIL
		",test host min core total CPU % busy"
		",test host max core total CPU % busy"
		",test host avg core system CPU % busy"
		",test host min core system CPU % busy"
		",test host max core system CPU % busy"
		",test host avg core user CPU % busy"
		",test host min core user CPU % busy"
		",test host max core user CPU % busy"
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

	ivy_float n = (ivy_float) number_of_subintervals;
	o 	<< (cores/number_of_subintervals)
		<< ',' << std::fixed << std::setprecision(1) << (avgcore_total/n) << "%"
#ifdef IVY_LINUX_CPU_BUSY_DETAIL
		<< ',' << std::fixed << std::setprecision(1) << (mincore_total/n) << "%"
		<< ',' << std::fixed << std::setprecision(1) << (maxcore_total/n) << "%"
		<< ',' << std::fixed << std::setprecision(1) << (avgcore_system/n) << "%"
		<< ',' << std::fixed << std::setprecision(1) << (mincore_system/n) << "%"
		<< ',' << std::fixed << std::setprecision(1) << (maxcore_system/n) << "%"
		<< ',' << std::fixed << std::setprecision(1) << (avgcore_user/n) << "%"
		<< ',' << std::fixed << std::setprecision(1) << (mincore_user/n) << "%"
		<< ',' << std::fixed << std::setprecision(1) << (maxcore_user/n) << "%"
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

    unsigned int i {0};

    for (struct linuxcpubusypercent* p : c.eachcore)
    {
        o << "core " << i++ << ": " << *p << std::endl;
    }

    return o;
}


























