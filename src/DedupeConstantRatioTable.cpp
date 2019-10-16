//Copyright (c) 2019 Hitachi Vantara Corporation
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
//#include "stdafx.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <chrono>
//#include <unistd.h>

using namespace std;


long double /* dedupe ratio */ brute_force(unsigned int sides, unsigned int throws)
{
    uint64_t outcomes = 1;

    for (unsigned int t = 0; t < throws; t++)
    {
        outcomes *= sides;
    }

    std::vector<unsigned int> outcomes_by_number_of_sides(sides);
    for (unsigned int& x : outcomes_by_number_of_sides) x=0;

    std::vector<unsigned int> throws_by_side_this_pass (sides);

    for (uint64_t pass = 0; pass < outcomes; pass++)
    {
        for (unsigned int& x : throws_by_side_this_pass) x=0;

        uint64_t x = pass;

        //std::cout << "For pass " << pass;
        for (unsigned int t = 0; t < throws; t++)
        {
            unsigned int side = x % sides;

            //std::cout << " throw(" << (t+1) << ")=" << (side+1);
            throws_by_side_this_pass[side]++;
            x /= sides;
        }
        // now update the totals

        unsigned int distinct_sides_this_pass = 0;

        for (unsigned int  i = 0; i < sides; i++)
        {
            if (throws_by_side_this_pass[i] > 0) distinct_sides_this_pass++;
        }

        //std::cout << " - " << distinct_sides_this_pass << " distinct sides."<< std::endl;
        outcomes_by_number_of_sides[distinct_sides_this_pass-1]++;
    }

    //std::cout << "sides on die = " << sides << ", number of throws = " << throws << ", number of possible outcomes = " << outcomes << std::endl << std::endl;

    //unsigned int total = 0;

    long double expected_distinct_values {0.0};
    long double sum_of_probability {0.0};

    for (unsigned int num_sides = 0; (num_sides < sides && num_sides < throws); num_sides++)
    {
        //std::cout << "For " << (num_sides+1) << " sides_seen, there were " << outcomes_by_number_of_sides[num_sides] << " outcomes." << std::endl;

        long double probability = ((long double)outcomes_by_number_of_sides[num_sides]) / ((long double) outcomes);

        sum_of_probability += probability;

        expected_distinct_values += ((long double) (num_sides+1)) * probability;

        //brute_force_map[sides][throws][num_sides+1] = outcomes_by_number_of_sides[num_sides];
        //total +=  outcomes_by_number_of_sides[num_sides];
    }
    //std::cout << "sum_of_probability = " << sum_of_probability << "." << std::endl;
    //std::cout << std::endl << "Total number of outcomes was " << total << std::endl;

    return ((long double) throws) / expected_distinct_values;
}


int main(int argc, char* argv[])
{
	std::map<double, std::pair<unsigned int /*sides of die*/, unsigned int /*throws*/>> table_by_dedupe_ratio;

    std::cout << "\tstd::map<unsigned int /*sides*/, std::map<unsigned int /*throws*/, long double /* dedupe_ratio */>> constant_ratio_map" << std::endl;
    std::cout << "\t{" << std::endl;

    bool first_side {true};

    auto run_start = chrono::steady_clock::now();

    for (unsigned int sides : {2, 3, 4, 5, 6, 7, 8})
    {
        std::cout << "\t\t";
        if (!first_side) std::cout << ", ";
        first_side = false;

        std::cout << "{ " << sides << ", {" << std::endl;

        bool first_throw = true;

        long double last_dedupe_ratio = 1.0;

		double run_time_seconds = 0.001;

        auto side_start = chrono::steady_clock::now();

        for (unsigned int throws = 2; run_time_seconds <= 60.0; throws++)
        {
            std::cout << "\t\t\t";

            if (!first_throw) std::cout << ", ";
            first_throw = false;

            auto start_time = chrono::steady_clock::now();

            last_dedupe_ratio = brute_force(sides, throws);

			table_by_dedupe_ratio[last_dedupe_ratio] = std::make_pair(sides, throws);

            auto end_time = chrono::steady_clock::now();

            run_time_seconds = 0.001 * ( (double) chrono::duration_cast<chrono::milliseconds>(end_time - start_time).count() );

            std::cout << "{ " << throws << ", " << last_dedupe_ratio << " } // compute time " << std::fixed << std::setprecision(6) << run_time_seconds << " seconds." << std::endl;

        }
        auto side_end = chrono::steady_clock::now();
        double side_time_seconds = 0.000001 * ( (double) chrono::duration_cast<chrono::microseconds>(side_end - side_start).count() );
        std::cout << "\t\t} } // side compute time " << std::fixed << std::setprecision(6) << side_time_seconds << " seconds." << std::endl;
    }
    auto run_end = chrono::steady_clock::now();
    double run_time_seconds = 0.001 * ( (double) chrono::duration_cast<chrono::milliseconds>(run_end - run_start).count() );

    std::cout << "\t}; // run compute time " << std::fixed << std::setprecision(6) << run_time_seconds << "seconds." << std::endl;

	std::cout << "\tstd::map<double, std::pair<unsigned int /*sides of die*/, unsigned int /*throws*/>> table_by_dedupe_ratio" << std::endl;
	std::cout << "\t{" << std::endl;

	bool premier{ true };
	for (auto pear : table_by_dedupe_ratio)
	{
		std::cout << "\t\t";
		if (!premier) std::cout << ", ";
		premier = false;
		std::cout << "{ " << std::fixed << std::setprecision(6) << pear.first;
		std::cout << ", { " << pear.second.first << ", " << pear.second.second << "} }" << std::endl;
	}
	std::cout << "\t};" << std::endl;

    return 0;
}

