//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

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
	inline Subinterval(){input.reset();output.clear();}
};



