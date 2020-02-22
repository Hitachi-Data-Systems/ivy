//Copyright (c) 2015, 2016, 2017, 2018, 2019 Hitachi Vantara Corporation
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

#include "ivytypes.h"
#include "ivyhelpers.h"
#include "Subinterval_CPU.h"

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


std::string Subinterval_CPU::csvValues(const std::string& hostname)
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

double Subinterval_CPU::active_core_average_busy_overall()
{
	if (0 == rollup_count)
	{
		throw std::runtime_error("SubintervalCPU::active_core_average_busy_overall() is being called for an empty SubintervalCPU object.");
	}

	return overallCPU.avg_activecore_total;
}


double Subinterval_CPU::active_core_average_busy_by_host(const std::string& hostname)
{
	if (0 == rollup_count)
	{
		throw std::runtime_error("SubintervalCPU::active_core_average_busy_by_host(hostname=\""s + hostname + "\") is being called for an empty SubintervalCPU object."s);
	}

	std::map<std::string, avgcpubusypercent>::iterator it = eachHostCPU.find(hostname);
	if (eachHostCPU.end() == it)
	{
		throw std::runtime_error("SubintervalCPU::active_core_average_busy_by_host(hostname=\"" + hostname + "\") is being called for an invalid hostname."s);
	}

	return (*it).second.avg_activecore_total;
}

// ==================================================================================================================

/*static*/ std::string Subinterval_CPU_temp::csvTitles()
{
	return ",CPU temp core count,avg degrees C below CPU critical temp,worst degrees C below CPU critical temp" ;
}

void Subinterval_CPU_temp::clear()
{
	rollup_count=0;
	overall_CPU_temp.clear();
	each_host_CPU_temp.clear();
}

bool Subinterval_CPU_temp::add(std::string hostname, const RunningStat<double,long>& a)
{
	// sets rollup_count from zero to 1 as the first host entry is added.
	if (0 == rollup_count)
	{
		rollup_count=1;
	}
	if (1 < rollup_count)
		{ clear(); return false;} // fromString may not be used on completed individal subinterval objects with all the host data in place that are being rolled up.

	overall_CPU_temp += a;

	each_host_CPU_temp[hostname]=a;

	return true;
}


void Subinterval_CPU_temp::rollup(const Subinterval_CPU_temp& other)
{
	// used to make an average over a series of subintervals.
	// The assumption is made that over each subinterval, the set of hosts is the same.

	rollup_count += other.rollup_count;

	overall_CPU_temp += other.overall_CPU_temp;

	for (auto& pear : other.each_host_CPU_temp)
	{
		each_host_CPU_temp[pear.first] += pear.second;
	}
}


std::string Subinterval_CPU_temp::csvValuesAvgOverHosts()
{
	if (0 == rollup_count)
	{
		return std::string("<Error> internal programming error - Subinterval_CPU_temp::csvValuesAvgOverHosts() is being called for an empty Subinterval_CPU_temp object.");
	}

	std::ostringstream o;

	o << "," << (((double) overall_CPU_temp.count())/((double)rollup_count));
	o << "," << overall_CPU_temp.avg();
	o << "," << overall_CPU_temp.min();

	return o.str();
}


std::string Subinterval_CPU_temp::csvValues(std::string hostname)
{
	if (0 == rollup_count)
	{
		return std::string("<Error> internal programming error - Subinterval_CPU_temp::csvValues(hostname=\"") + hostname + std::string("\") is being called for an empty SubintervalCPU object.");
	}

	std::map<std::string, RunningStat<double,long>>::iterator it = each_host_CPU_temp.find(hostname);
	if (each_host_CPU_temp.end() == it)
	{
		return std::string("<Error> internal programming error - Subinterval_CPU_temp::csvValues(hostname=\"") + hostname + std::string("\") is being called for an invalid hostname.");
	}

	std::ostringstream o;

	o << "," << (((double) it->second.count())/((double)rollup_count));
	o << "," << it->second.avg();
	o << "," << it->second.min();

	return o.str();
}


