//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include "Subinterval_CPU.h"


/*static*/ std::string Subinterval_CPU::csvTitles()
{
	return IvyLinuxCPU_avgcpubusypercent::csvTitles();
}

void Subinterval_CPU::clear()
{
	rollup_count=0;
	overallCPU.clear();
	eachHostCPU.clear();
}

bool Subinterval_CPU::add(std::string hostname, const IvyLinuxCPU_avgcpubusypercent& a)
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

	std::map<std::string, IvyLinuxCPU_avgcpubusypercent>::iterator it = eachHostCPU.find(hostname);
	if (eachHostCPU.end() == it)
	{
		return std::string("SubintervalCPU::csvValues(hostname=\"") + hostname + std::string("\") is being called for an invalid hostname.");
	}

	return (*it).second.csvValues(rollup_count);
}



