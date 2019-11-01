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

#include "Iosequencer.h"

//extern std::default_random_engine deafrangen;

class IosequencerRandom : public Iosequencer {
	// this is the base class of random_steady and random_independent.
protected:

    bool have_hot_zone {false};
	long long int   hot_zone_coverageStartBlock {0},     hot_zone_coverageEndBlock {0},     hot_zone_numberOfCoverageBlocks {0};
	long long int other_zone_coverageStartBlock {0},   other_zone_coverageEndBlock {0},   other_zone_numberOfCoverageBlocks {0};

public:
	IosequencerRandom(LUN* pL, logger& lf, std::string wID, WorkloadThread* pWT, TestLUN* p_tl, Workload* p_w)
	    : Iosequencer(pL, lf, wID, pWT, p_tl, p_w) {}
    ~IosequencerRandom() {}
	virtual std::string instanceType()=0;
	virtual bool isRandom()=0;  // This is used to plug the I/O statistics into "random" and "sequential" categories.
	bool setFrom_IosequencerInput(IosequencerInput*);
	bool set_hot_zone_parameters (IosequencerInput*);
	bool generate(Eyeo& slang) override;

	inline uint64_t generate_hot_zone_block_number()   { return   hot_zone_coverageStartBlock + (xorshift64s(&xors) %   hot_zone_numberOfCoverageBlocks); }
	inline uint64_t generate_other_zone_block_number() { return other_zone_coverageStartBlock + (xorshift64s(&xors) % other_zone_numberOfCoverageBlocks); }
	inline long double generate_float_between_0_and_1()
	{
        static const long double divisor { ((ivy_float) 0xFFFFFFFFFFFFFFFF) + 1.0 };
        return ((long double) xorshift64s(&xors)) / divisor;
    }
};
