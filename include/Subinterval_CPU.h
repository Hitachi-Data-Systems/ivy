//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include "ivylinuxcpubusy.h"

class Subinterval_CPU
{
public:
	int rollup_count=0;
	struct avgcpubusypercent overallCPU;
	std::map<std::string, struct avgcpubusypercent> eachHostCPU;
	bool add(std::string hostname, const struct avgcpubusypercent&);  // sets rollup_count from zero to 1 as the first host entry is added.
	void rollup(const Subinterval_CPU&);  // used to make an average over a series of subintervals.
	static std::string csvTitles();
	std::string csvValuesAvgOverHosts();
	std::string csvValues(std::string hostname);
	void clear();
};


