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

#include "IosequencerInputRollup.h"

class SubintervalRollup
{
public:
// variables
	IosequencerInputRollup inputRollup;
	SubintervalOutput outputRollup;
	ivytime startIvytime, endIvytime;
	RunningStat<ivy_float,ivy_int> IOPS_series        [ 1 + Accumulators_by_io_type::max_category_index()] {};
	RunningStat<ivy_float,ivy_int> service_time_series[ 1 + Accumulators_by_io_type::max_category_index()] {};

//methods
	SubintervalRollup(ivytime subinterval_start, ivytime subinterval_end) : startIvytime(subinterval_start), endIvytime(subinterval_end) {}
	SubintervalRollup() {startIvytime = ivytime(0); endIvytime = ivytime(0);}
	void clear();
	void addIn(const SubintervalRollup& other);
	ivy_float durationSeconds() { ivytime dur = endIvytime - startIvytime; return dur.getlongdoubleseconds();}
};


