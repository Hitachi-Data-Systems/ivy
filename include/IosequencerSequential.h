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
#include "Iosequencer.h"

class IosequencerSequential : public Iosequencer {
public:
	IosequencerSequential(LUN* pL, logger& lf, std::string wID, WorkloadThread* pWT, TestLUN* p_tl, Workload* p_w)
	    : Iosequencer(pL, lf, wID, pWT, p_tl, p_w) {}
    ~IosequencerSequential(){}
	bool generate(Eyeo&) override;
	void setFrom_IosequencerInput(IosequencerInput*) override;
	bool isRandom() { return false; }
	std::string instanceType() { return std::string("sequential"); }
	long long int lastIOblockNumber=0;  // default of zero means block 1 will be the first one read or written.

	// blocks_generated & wrapping used by sequential I/O sequencer to detect wraparound for sequential_fill=on
    long long int blocks_generated {-1};
    bool wrapping {true};
};


// The [Go!] parameter "sequential_fill=on" keeps running another subinterval no matter what else
// including either satisfying the criteria for a successful measurement or else timing out.
//
// From an implementation point of view, the problem is that it is only individual
// iosequencer threads that know if they have wrapped around yet, but the decision to
// "continue" and run another subinterval is a global thing, and the setting of the
// "sequential_fill = on" is itself as a [Go!] statement parameter is a global setting.

// So the way this works is a two part system, one part is a "from the ground up"
// rollup of whether there are any sequential I/O sequencers that have not yet wrapped around,
// and the other part is an intervention if "sequential_fill = on" at the point where
// we would otherwise either declare "success" or "timeout", and instead  say "continue",
// if there are any sequential I/O sequencers running that have not wrapped around yet.

// The reason this overrides the "timeout" setting is that it makes sequential_fill=on
// easier / more reliable to use.

// When ivydriver gets "go", it resets to false "wrapping".
//
// Each IosequencerSequential instance keeps separate boolean flags to track
// whether they have wrapped around yet separately for wrapping around max_LBA
// and wrapping around the starting LBA for the first I/O in subinterval zero.
//
// Each
