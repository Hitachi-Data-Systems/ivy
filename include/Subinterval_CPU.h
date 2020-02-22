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
#pragma once

#include "ivytypes.h"
#include "ivylinuxcpubusy.h"

class Subinterval_CPU
{
public:
	int rollup_count=0;
	struct avgcpubusypercent overallCPU;
	std::map<std::string, struct avgcpubusypercent> eachHostCPU;

// methods
	bool add(std::string hostname, const struct avgcpubusypercent&);  // sets rollup_count from zero to 1 as the first host entry is added.
	void rollup(const Subinterval_CPU&);  // used to make an average over a series of subintervals.
	static std::string csvTitles();
	std::string csvValuesAvgOverHosts();
	std::string csvValues(const std::string& hostname);
	void clear();
	double active_core_average_busy_overall();
	double active_core_average_busy_by_host(const std::string& hostname);
};

class Subinterval_CPU_temp
{
public:
	int rollup_count=0;
	RunningStat<double,long> overall_CPU_temp;
	std::map<std::string, RunningStat<double,long>> each_host_CPU_temp;

// methods
	bool add(std::string hostname, const RunningStat<double,long>&);  // sets rollup_count from zero to 1 as the first host entry is added.
	void rollup(const Subinterval_CPU_temp&);  // used to make an average over a series of subintervals.
	static std::string csvTitles();
	std::string csvValuesAvgOverHosts();
	std::string csvValues(std::string hostname);
	void clear();
};





