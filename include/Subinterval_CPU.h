//Copyright (c) 2016 Hitachi Data Systems, Inc.
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
//Author: Allart Ian Vogelesang <ian.vogelesang@hds.com>
//
//Support:  "ivy" is not officially supported by Hitachi Data Systems.
//          Contact me (Ian) by email at ian.vogelesang@hds.com and as time permits, I'll help on a best efforts basis.
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


