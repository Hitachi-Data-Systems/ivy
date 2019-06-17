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

extern std::default_random_engine deafrangen;

class IosequencerRandom : public Iosequencer {
	// this is the base class of random_steady and random_independent.
protected:
//        size_t seed_hash;
	std::uniform_int_distribution<uint64_t>*   p_uniform_int_distribution         {nullptr};
	std::uniform_real_distribution<ivy_float>* p_uniform_real_distribution_0_to_1 {nullptr};


	std::uniform_int_distribution<uint64_t>* p_hot_zone_block_distribution        {nullptr};
	std::uniform_int_distribution<uint64_t>* p_other_area_block_distribution      {nullptr};

	long long int   hot_zone_coverageStartBlock {0},   hot_zone_coverageEndBlock {0},   hot_zone_numberOfCoverageBlocks {0};

public:
	IosequencerRandom(LUN* pL, std::string lf, std::string wID, WorkloadThread* pWT, TestLUN* p_tl, Workload* p_w)
	    : Iosequencer(pL, lf, wID, pWT, p_tl, p_w) {}
    ~IosequencerRandom() {}
	virtual std::string instanceType()=0;
	virtual bool isRandom()=0;  // This is used to plug the I/O statistics into "random" and "sequential" categories.
	bool setFrom_IosequencerInput(IosequencerInput*);
	bool set_hot_zone_parameters (IosequencerInput*);
	bool generate(Eyeo& slang) override;
};
