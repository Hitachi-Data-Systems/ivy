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

#include <tuple>
#include <vector>

#include "RunningStat.h"
#include "ivydefines.h"

extern std::vector<std::tuple<std::string,ivy_float, ivy_float>> io_time_clip_levels;

ivy_float histogram_bucket_scale_factor(unsigned int);

class Accumulators_by_io_type {
public:
// variables
	RunningStat<double,long int> rs_array[2] /* 0 = random, 1 = sequential */
	                                     [2] /* 0 = read, 1 = write */
	                                     [io_time_buckets];  // defined in ivydefines.h, to match io_time_clip_levels in Accumulators_by_io_type.cpp
// methods
/*0*/	RunningStat<ivy_float,ivy_int> overall() const;
/*1*/	RunningStat<ivy_float,ivy_int> random() const ;
/*2*/	RunningStat<ivy_float,ivy_int> sequential() const;
/*3*/	RunningStat<ivy_float,ivy_int> read() const;
/*4*/	RunningStat<ivy_float,ivy_int> write() const;
/*5*/	RunningStat<ivy_float,ivy_int> randomRead() const;
/*6*/	RunningStat<ivy_float,ivy_int> randomWrite() const;
/*7*/	RunningStat<ivy_float,ivy_int> sequentialRead() const;
/*8*/	RunningStat<ivy_float,ivy_int> sequentialWrite() const;

	RunningStat<ivy_float,ivy_int> getRunningStatByCategory(int) const;

	constexpr static int max_category_index() { return 8; }

	static std::string getRunningStatTitleByCategory(int);

	static std::string getCategories();

	static int get_bucket_index(ivy_float io_time);
};

