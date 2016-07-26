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

#include "IogeneratorInput.h"
#include "SubintervalOutput.h"

class Subinterval {

public:

	IogeneratorInput
		input;

	SubintervalOutput
		output;

	ivytime start_time, end_time;

	uint64_t currentblock; // Set by slave master for 1st subinterval, then carries forward
	uint64_t ioerrorcount{0};
	uint64_t
		minblock{1},
		maxblock;	// Slave master sets minblock & maxblock when preparing subinterval from LUN's max LBA
				// and from VolCoverageFractionStart and VolCoverageFractionEnd

#define IVY_SUBINTERVAL_STATUS_UNDEFINED (0)
#define IVY_SUBINTERVAL_READY_TO_RUN (1)
#define IVY_SUBINTERVAL_READY_TO_SEND (2)
#define IVY_SUBINTERVAL_STOP (3)

	int subinterval_status{IVY_SUBINTERVAL_STATUS_UNDEFINED};
	Subinterval(){input.reset();output.clear();}
};



