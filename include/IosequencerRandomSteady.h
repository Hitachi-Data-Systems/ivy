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

#include "ivytypes.h"
#include "IosequencerRandom.h"

class IosequencerRandomSteady : public IosequencerRandom {
public:
	IosequencerRandomSteady(LUN* pL, logger& lf, std::string wID, WorkloadThread* pWT, TestLUN* p_tl, Workload* p_w)
	    : IosequencerRandom(pL, lf, wID, pWT, p_tl, p_w) {}
    ~IosequencerRandomSteady(){}
	std::string instanceType() { return std::string("random_steady"); }
	bool isRandom() { return true; }
	bool generate(Eyeo&);
};

