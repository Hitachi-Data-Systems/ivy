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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Stephen Morgan <stephen.morgan@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#pragma once

#include "ivytypes.h"

std::map<unsigned int /*sides*/, std::map<unsigned int /*throws*/, long double /* dedupe_ratio */>> constant_ratio_map
	{
		{ 2, {
			{ 2, 1.33333 } // compute time 0.000000 seconds.
			, { 3, 1.714286 } // compute time 0.000000 seconds.
			, { 4, 2.133333 } // compute time 0.000000 seconds.
			, { 5, 2.580645 } // compute time 0.000000 seconds.
			, { 6, 3.047619 } // compute time 0.000000 seconds.
			, { 7, 3.527559 } // compute time 0.000000 seconds.
			, { 8, 4.015686 } // compute time 0.000000 seconds.
			, { 9, 4.508806 } // compute time 0.000000 seconds.
			, { 10, 5.004888 } // compute time 0.000000 seconds.
			, { 11, 5.502687 } // compute time 0.000000 seconds.
			, { 12, 6.001465 } // compute time 0.001000 seconds.
			, { 13, 6.500794 } // compute time 0.002000 seconds.
			, { 14, 7.000427 } // compute time 0.008000 seconds.
			, { 15, 7.500229 } // compute time 0.019000 seconds.
			, { 16, 8.000122 } // compute time 0.039000 seconds.
			, { 17, 8.500065 } // compute time 0.081000 seconds.
			, { 18, 9.000034 } // compute time 0.170000 seconds.
			, { 19, 9.500018 } // compute time 0.349000 seconds.
			, { 20, 10.000010 } // compute time 0.740000 seconds.
			, { 21, 10.500005 } // compute time 1.255000 seconds.
			, { 22, 11.000003 } // compute time 2.646000 seconds.
			, { 23, 11.500001 } // compute time 4.920000 seconds.
			, { 24, 12.000001 } // compute time 10.181000 seconds.
			, { 25, 12.500000 } // compute time 21.151000 seconds.
			, { 26, 13.000000 } // compute time 43.208000 seconds.
			, { 27, 13.500000 } // compute time 89.521000 seconds.
		} } // side compute time 174.303431 seconds.
		, { 3, {
			{ 2, 1.200000 } // compute time 0.000000 seconds.
			, { 3, 1.421053 } // compute time 0.000000 seconds.
			, { 4, 1.661538 } // compute time 0.000000 seconds.
			, { 5, 1.919431 } // compute time 0.000000 seconds.
			, { 6, 2.192481 } // compute time 0.000000 seconds.
			, { 7, 2.478388 } // compute time 0.000000 seconds.
			, { 8, 2.774941 } // compute time 0.001000 seconds.
			, { 9, 3.080121 } // compute time 0.007000 seconds.
			, { 10, 3.392159 } // compute time 0.025000 seconds.
			, { 11, 3.709553 } // compute time 0.075000 seconds.
			, { 12, 4.031069 } // compute time 0.214000 seconds.
			, { 13, 4.355714 } // compute time 0.749000 seconds.
			, { 14, 4.682707 } // compute time 2.534000 seconds.
			, { 15, 5.011444 } // compute time 8.040000 seconds.
			, { 16, 5.341465 } // compute time 25.939000 seconds.
			, { 17, 5.672424 } // compute time 74.169000 seconds.
		} } // side compute time 111.757873 seconds.
		, { 4, {
			{ 2, 1.142857 } // compute time 0.000000 seconds.
			, { 3, 1.297297 } // compute time 0.000000 seconds.
			, { 4, 1.462857 } // compute time 0.000000 seconds.
			, { 5, 1.638924 } // compute time 0.000000 seconds.
			, { 6, 1.824770 } // compute time 0.001000 seconds.
			, { 7, 2.019582 } // compute time 0.005000 seconds.
			, { 8, 2.222501 } // compute time 0.020000 seconds.
			, { 9, 2.432655 } // compute time 0.091000 seconds.
			, { 10, 2.649185 } // compute time 0.382000 seconds.
			, { 11, 2.871268 } // compute time 1.654000 seconds.
			, { 12, 3.098138 } // compute time 7.125000 seconds.
			, { 13, 3.329090 } // compute time 35.177000 seconds.
			, { 14, 3.563494 } // compute time 143.377000 seconds.
		} } // side compute time 187.835949 seconds.
		, { 5, {
			{ 2, 1.111111 } // compute time 0.000000 seconds.
			, { 3, 1.229508 } // compute time 0.000000 seconds.
			, { 4, 1.355014 } // compute time 0.000000 seconds.
			, { 5, 1.487387 } // compute time 0.000000 seconds.
			, { 6, 1.626334 } // compute time 0.004000 seconds.
			, { 7, 1.771513 } // compute time 0.021000 seconds.
			, { 8, 1.922550 } // compute time 0.125000 seconds.
			, { 9, 2.079045 } // compute time 0.688000 seconds.
			, { 10, 2.240580 } // compute time 3.738000 seconds.
			, { 11, 2.406737 } // compute time 19.775000 seconds.
			, { 12, 2.577097 } // compute time 110.425000 seconds.
		} } // side compute time 134.780316 seconds.
		, { 6, {
			{ 2, 1.090909 } // compute time 0.000000 seconds.
			, { 3, 1.186813 } // compute time 0.000000 seconds.
			, { 4, 1.287630 } // compute time 0.000000 seconds.
			, { 5, 1.393249 } // compute time 0.002000 seconds.
			, { 6, 1.503529 } // compute time 0.014000 seconds.
			, { 7, 1.618306 } // compute time 0.091000 seconds.
			, { 8, 1.737396 } // compute time 0.582000 seconds.
			, { 9, 1.860596 } // compute time 3.644000 seconds.
			, { 10, 1.987690 } // compute time 23.310000 seconds.
			, { 11, 2.118451 } // compute time 157.146000 seconds.
		} } // side compute time 184.792960 seconds.
		, { 7, {
			{ 2, 1.076923 } // compute time 0.000000 seconds.
			, { 3, 1.157480 } // compute time 0.000000 seconds.
			, { 4, 1.241629 } // compute time 0.000000 seconds.
			, { 5, 1.329310 } // compute time 0.004000 seconds.
			, { 6, 1.420450 } // compute time 0.035000 seconds.
			, { 7, 1.514960 } // compute time 0.264000 seconds.
			, { 8, 1.612741 } // compute time 1.989000 seconds.
			, { 9, 1.713680 } // compute time 14.971000 seconds.
			, { 10, 1.817656 } // compute time 111.055000 seconds.
		} } // side compute time 128.323369 seconds.
		, { 8, {
			{ 2, 1.066667 } // compute time 0.000000 seconds.
			, { 3, 1.136095 } // compute time 0.000000 seconds.
			, { 4, 1.208260 } // compute time 0.001000 seconds.
			, { 5, 1.283128 } // compute time 0.009000 seconds.
			, { 6, 1.360656 } // compute time 0.084000 seconds.
			, { 7, 1.440794 } // compute time 0.717000 seconds.
			, { 8, 1.523482 } // compute time 6.215000 seconds.
			, { 9, 1.608655 } // compute time 52.413000 seconds.
			, { 10, 1.696239 } // compute time 441.204000 seconds.
		} } // side compute time 500.647661 seconds.
	}; // run compute time 1422.441000seconds.
	std::map<double, std::pair<unsigned int /*sides of die*/, unsigned int /*throws*/>> table_by_dedupe_ratio
	{
		{ 1.066667, { 8, 2} }
		, { 1.076923, { 7, 2} }
		, { 1.090909, { 6, 2} }
		, { 1.111111, { 5, 2} }
		, { 1.136095, { 8, 3} }
		, { 1.142857, { 4, 2} }
		, { 1.157480, { 7, 3} }
		, { 1.186813, { 6, 3} }
		, { 1.200000, { 3, 2} }
		, { 1.208260, { 8, 4} }
		, { 1.229508, { 5, 3} }
		, { 1.241629, { 7, 4} }
		, { 1.283128, { 8, 5} }
		, { 1.287630, { 6, 4} }
		, { 1.297297, { 4, 3} }
		, { 1.329310, { 7, 5} }
		, { 1.333333, { 2, 2} }
		, { 1.355014, { 5, 4} }
		, { 1.360656, { 8, 6} }
		, { 1.393249, { 6, 5} }
		, { 1.420450, { 7, 6} }
		, { 1.421053, { 3, 3} }
		, { 1.440794, { 8, 7} }
		, { 1.462857, { 4, 4} }
		, { 1.487387, { 5, 5} }
		, { 1.503529, { 6, 6} }
		, { 1.514960, { 7, 7} }
		, { 1.523482, { 8, 8} }
		, { 1.608655, { 8, 9} }
		, { 1.612741, { 7, 8} }
		, { 1.618306, { 6, 7} }
		, { 1.626334, { 5, 6} }
		, { 1.638924, { 4, 5} }
		, { 1.661538, { 3, 4} }
		, { 1.696239, { 8, 10} }
		, { 1.713680, { 7, 9} }
		, { 1.714286, { 2, 3} }
		, { 1.737396, { 6, 8} }
		, { 1.771513, { 5, 7} }
		, { 1.817656, { 7, 10} }
		, { 1.824770, { 4, 6} }
		, { 1.860596, { 6, 9} }
		, { 1.919431, { 3, 5} }
		, { 1.922550, { 5, 8} }
		, { 1.987690, { 6, 10} }
		, { 2.019582, { 4, 7} }
		, { 2.079045, { 5, 9} }
		, { 2.118451, { 6, 11} }
		, { 2.133333, { 2, 4} }
		, { 2.192481, { 3, 6} }
		, { 2.222501, { 4, 8} }
		, { 2.240580, { 5, 10} }
		, { 2.406737, { 5, 11} }
		, { 2.432655, { 4, 9} }
		, { 2.478388, { 3, 7} }
		, { 2.577097, { 5, 12} }
		, { 2.580645, { 2, 5} }
		, { 2.649185, { 4, 10} }
		, { 2.774941, { 3, 8} }
		, { 2.871268, { 4, 11} }
		, { 3.047619, { 2, 6} }
		, { 3.080121, { 3, 9} }
		, { 3.098138, { 4, 12} }
		, { 3.329090, { 4, 13} }
		, { 3.392159, { 3, 10} }
		, { 3.527559, { 2, 7} }
		, { 3.563494, { 4, 14} }
		, { 3.709553, { 3, 11} }
		, { 4.015686, { 2, 8} }
		, { 4.031069, { 3, 12} }
		, { 4.355714, { 3, 13} }
		, { 4.508806, { 2, 9} }
		, { 4.682707, { 3, 14} }
		, { 5.004888, { 2, 10} }
		, { 5.011444, { 3, 15} }
		, { 5.341465, { 3, 16} }
		, { 5.502687, { 2, 11} }
		, { 5.672424, { 3, 17} }
		, { 6.001465, { 2, 12} }
		, { 6.500794, { 2, 13} }
		, { 7.000427, { 2, 14} }
		, { 7.500229, { 2, 15} }
		, { 8.000122, { 2, 16} }
		, { 8.500065, { 2, 17} }
		, { 9.000034, { 2, 18} }
		, { 9.500018, { 2, 19} }
		, { 10.000010, { 2, 20} }
		, { 10.500005, { 2, 21} }
		, { 11.000003, { 2, 22} }
		, { 11.500001, { 2, 23} }
		, { 12.000001, { 2, 24} }
		, { 12.500000, { 2, 25} }
		, { 13.000000, { 2, 26} }
		, { 13.500000, { 2, 27} }
	};
