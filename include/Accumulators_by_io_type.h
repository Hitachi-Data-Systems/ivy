//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <tuple>
#include <vector>

#include "RunningStat.h"

extern std::vector<std::tuple<std::string,ivy_float, ivy_float>> io_time_clip_levels;

ivy_float histogram_bucket_scale_factor(unsigned int);

class Accumulators_by_io_type {
public:
// variables
	RunningStat<ivy_float,ivy_int> rs_array[2] /* 0 = random, 1 = sequential */
	                                       [2] /* 0 = read, 1 = write */
	                                       [io_time_buckets];  // defined in ivydefines.h, to match io_time_clip_levels in Accumulators_by_io_type.cpp
// methods
/*0*/	RunningStat<ivy_float,ivy_int> overall();
/*1*/	RunningStat<ivy_float,ivy_int> random();
/*2*/	RunningStat<ivy_float,ivy_int> sequential();
/*3*/	RunningStat<ivy_float,ivy_int> read();
/*4*/	RunningStat<ivy_float,ivy_int> write();
/*5*/	RunningStat<ivy_float,ivy_int> randomRead();
/*6*/	RunningStat<ivy_float,ivy_int> randomWrite();
/*7*/	RunningStat<ivy_float,ivy_int> sequentialRead();
/*8*/	RunningStat<ivy_float,ivy_int> sequentialWrite();

	RunningStat<ivy_float,ivy_int> getRunningStatByCategory(int);

	static int max_category_index() { return 8; }

	static std::string getRunningStatTitleByCategory(int);

	static std::string getCategories();

	static int get_bucket_index(ivy_float io_time);

};

