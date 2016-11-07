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

class IosequencerRandom : public Iosequencer {
	// this is the base class of random_steady and random_independent.
protected:
//        size_t seed_hash;
	std::uniform_int_distribution<uint64_t>* p_uniform_int_distribution {NULL};
	std::uniform_real_distribution<ivy_float>* p_uniform_real_distribution_0_to_1{NULL};
        std::default_random_engine deafrangen;

public:
	IosequencerRandom(LUN* pL, std::string lf, std::string tK, iosequencer_stuff* p_is, WorkloadThread* pWT) : Iosequencer(pL, lf, tK, p_is, pWT) {}

	virtual std::string instanceType()=0;
	virtual bool isRandom()=0;  // This is used to plug the I/O statistics into "random" and "sequential" categories.
	bool setFrom_IosequencerInput(IosequencerInput*);
	bool generate(Eyeo& slang);
};
