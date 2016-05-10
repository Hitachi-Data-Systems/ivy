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
#include <iostream>
#include <iomanip>
#include <sstream>
//#include <stdlib.h> // atoi
#include <stdexcept>
#include <vector>


#include "ivydefines.h"
#include "RunningStat.h"

#include "Accumulators_by_io_type.h"

/*static*/ std::string Accumulators_by_io_type::getCategories()
{
    std::ostringstream o;
    o << "{";
    for (int i=0; i<= max_category_index(); i++)
    {
        if (i>0) o << ',';
        o << " \"" << getRunningStatTitleByCategory(i) << "\"";
    }
    o << " }";
    return o.str();
}

std::vector< std::tuple< std::string, ivy_float, ivy_float> > io_time_clip_levels;

void initialize_io_time_clip_levels()
{

// If you change the number of entries, adjust "io_time_buckets" in ivydefines.h

io_time_clip_levels.push_back(std::make_tuple("0.0x", .1, 1e4));
io_time_clip_levels.push_back(std::make_tuple( "0.1x", .2, 1e4 ));
io_time_clip_levels.push_back(std::make_tuple( "0.2x", .3, 1e4 ));
io_time_clip_levels.push_back(std::make_tuple( "0.3x", .4, 1e4 ));
io_time_clip_levels.push_back(std::make_tuple( "0.4x", .5, 1e4 ));
io_time_clip_levels.push_back(std::make_tuple( "0.5x", .6, 1e4 ));
io_time_clip_levels.push_back(std::make_tuple( "0.6x", .7, 1e4 ));
io_time_clip_levels.push_back(std::make_tuple( "0.7x", .8, 1e4 ));
io_time_clip_levels.push_back(std::make_tuple( "0.8x", .9, 1e4 ));
io_time_clip_levels.push_back(std::make_tuple( "0.9x", 1., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "1.x", 2., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "2.x", 3., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "3.x", 4., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "4.x", 5., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "5.x", 6., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "6.x", 7., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "7.x", 8., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "8.x", 9., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "9.x", 10., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "10.x", 11., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "11.x", 12. , 1e3));
io_time_clip_levels.push_back(std::make_tuple( "12.x", 13., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "13.x", 14., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "14.x", 15., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "15.x", 16., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "16.x", 17., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "17.x", 18., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "18.x", 19., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "19.x", 20., 1e3 ));
io_time_clip_levels.push_back(std::make_tuple( "2x.", 30., 1e2 ));
io_time_clip_levels.push_back(std::make_tuple( "3x.", 40., 1e2 ));
io_time_clip_levels.push_back(std::make_tuple( "4x.", 50., 1e2 ));
io_time_clip_levels.push_back(std::make_tuple( "5x.", 60., 1e2 ));
io_time_clip_levels.push_back(std::make_tuple( "6x.", 70., 1e2 ));
io_time_clip_levels.push_back(std::make_tuple( "7x.", 80., 1e2 ));
io_time_clip_levels.push_back(std::make_tuple( "8x.", 90., 1e2 ));
io_time_clip_levels.push_back(std::make_tuple( "9x.", 100., 1e1 ));
io_time_clip_levels.push_back(std::make_tuple( "1xx.", 200., 1e1 ));
io_time_clip_levels.push_back(std::make_tuple( "2xx.", 300., 1e1 ));
io_time_clip_levels.push_back(std::make_tuple( "3xx.", 400., 1e1 ));
io_time_clip_levels.push_back(std::make_tuple( "4xx.", 500., 1e1 ));
io_time_clip_levels.push_back(std::make_tuple( "5xx.", 600., 1e1 ));
io_time_clip_levels.push_back(std::make_tuple( "6xx.", 700., 1e1 ));
io_time_clip_levels.push_back(std::make_tuple( "7xx.", 800., 1e1 ));
io_time_clip_levels.push_back(std::make_tuple( "8xx.", 900., 1e1 ));
io_time_clip_levels.push_back(std::make_tuple( "9xx.", 1000., 1e0 ));
io_time_clip_levels.push_back(std::make_tuple( "1xxx.", 2000., 1e0 ));
io_time_clip_levels.push_back(std::make_tuple( "2xxx.", 3000., 1e0 ));
io_time_clip_levels.push_back(std::make_tuple( "3xxx.", 4000., 1e0 ));
io_time_clip_levels.push_back(std::make_tuple( "4xxx.", 5000., 1e0 ));
io_time_clip_levels.push_back(std::make_tuple( "5xxx.", 6000., 1e0 ));
io_time_clip_levels.push_back(std::make_tuple( "6xxx.", 7000., 1e0 ));
io_time_clip_levels.push_back(std::make_tuple( "7xxx.", 8000., 1e0 ));
io_time_clip_levels.push_back(std::make_tuple( "8xxx.", 9000., 1e0 ));
io_time_clip_levels.push_back(std::make_tuple( "9xxx.", 10000., 1e0 ));
io_time_clip_levels.push_back(std::make_tuple( "10000+", -1., 1e0 ));

}


ivy_float histogram_bucket_scale_factor(unsigned int i)
{
    if (i < io_time_clip_levels.size()) return std::get<2>(io_time_clip_levels[i]);
    else                                return -1.;
}


/*static*/ int Accumulators_by_io_type::get_bucket_index(ivy_float io_time)
{
	if (io_time < 0.)
	{
        std::ostringstream o; o << "Accumulators_by_io_type::get_bucket_index(ivy_float io_time) called with negative I/O time = " << io_time << ".";
        throw std::invalid_argument(o.str());
	}

	ivy_float io_time_ms = io_time * 1000.;

	for (int i=0; i<(io_time_buckets - 1); i++)
		if (io_time_ms < std::get<1>(io_time_clip_levels[i]))
			return i;

	return io_time_buckets -1;
}

/*static*/ std::string Accumulators_by_io_type::getRunningStatTitleByCategory(int category_index)
{
	switch (category_index)
	{
		case 0: return std::string("Overall");
		case 1: return std::string("Random");
		case 2: return std::string("Sequential");
		case 3: return std::string("Read");
		case 4: return std::string("Write");
		case 5: return std::string("Random Read");
		case 6: return std::string("Random Write");
		case 7: return std::string("Sequential Read");
		case 8: return std::string("Sequential Write");
	}
	{
		std::ostringstream o;
		o << "Accumulators_by_io_type::getRunningStatTitleByCategory(int = "
			<< category_index
			<< ") - Invalid category index.  Valid values are from 0 to "
			<< Accumulators_by_io_type::max_category_index();

		throw std::out_of_range (o.str());
	}
}

RunningStat<ivy_float, ivy_int> Accumulators_by_io_type::getRunningStatByCategory(int category_index)
{
	switch (category_index)
	{
		case 0: return overall();
		case 1: return random();
		case 2: return sequential();
		case 3: return read();
		case 4: return write();
		case 5: return randomRead();
		case 6: return randomWrite();
		case 7: return sequentialRead();
		case 8: return sequentialWrite();
	}
	{
		std::ostringstream o;
		o << "Accumulators_by_io_type::getRunningStatByCategory(int = "
			<< category_index
			<< ") - Invalid category index.  Valid values are from 0 to "
			<< Accumulators_by_io_type::max_category_index()
			<< " representing " << getCategories();

		throw std::out_of_range (o.str());
	}
}

RunningStat<ivy_float, ivy_int> Accumulators_by_io_type::overall()
{
	RunningStat<ivy_float, ivy_int> x;

	for (int rs : {0,1})
		for (int rw : {0,1})
			for (int bucket=0; bucket<io_time_buckets; bucket++)
				x+=rs_array[rs][rw][bucket];
	return x;
}

RunningStat<ivy_float, ivy_int> Accumulators_by_io_type::randomRead()
{
	RunningStat<ivy_float, ivy_int> x;

	for (int rs : {0})
		for (int rw : {0})
			for (int bucket=0; bucket<io_time_buckets; bucket++)
				x+=rs_array[rs][rw][bucket];
	return x;
}

RunningStat<ivy_float, ivy_int> Accumulators_by_io_type::randomWrite()
{
	RunningStat<ivy_float, ivy_int> x;

	for (int rs : {0})
		for (int rw : {1})
			for (int bucket=0; bucket<io_time_buckets; bucket++)
				x+=rs_array[rs][rw][bucket];
	return x;
}

RunningStat<ivy_float, ivy_int> Accumulators_by_io_type::random()
{
	RunningStat<ivy_float, ivy_int> x;

	for (int rs : {0})
		for (int rw : {0,1})
			for (int bucket=0; bucket<io_time_buckets; bucket++)
				x+=rs_array[rs][rw][bucket];
	return x;
}


RunningStat<ivy_float, ivy_int> Accumulators_by_io_type::sequentialRead()
{
	RunningStat<ivy_float, ivy_int> x;

	for (int rs : {1})
		for (int rw : {0})
			for (int bucket=0; bucket<io_time_buckets; bucket++)
				x+=rs_array[rs][rw][bucket];
	return x;
}

RunningStat<ivy_float, ivy_int> Accumulators_by_io_type::sequentialWrite()
{
	RunningStat<ivy_float, ivy_int> x;

	for (int rs : {1})
		for (int rw : {1})
			for (int bucket=0; bucket<io_time_buckets; bucket++)
				x+=rs_array[rs][rw][bucket];
	return x;
}

RunningStat<ivy_float, ivy_int> Accumulators_by_io_type::sequential()
{
	RunningStat<ivy_float, ivy_int> x;

	for (int rs : {1})
		for (int rw : {0,1})
			for (int bucket=0; bucket<io_time_buckets; bucket++)
				x+=rs_array[rs][rw][bucket];
	return x;
}

RunningStat<ivy_float, ivy_int> Accumulators_by_io_type::read()
{
	RunningStat<ivy_float, ivy_int> x;

	for (int rs : {0,1})
		for (int rw : {0})
			for (int bucket=0; bucket<io_time_buckets; bucket++)
				x+=rs_array[rs][rw][bucket];
	return x;
}

RunningStat<ivy_float, ivy_int> Accumulators_by_io_type::write()
{
	RunningStat<ivy_float, ivy_int> x;

	for (int rs : {0,1})
		for (int rw : {1})
			for (int bucket=0; bucket<io_time_buckets; bucket++)
				x+=rs_array[rs][rw][bucket];
	return x;
}

