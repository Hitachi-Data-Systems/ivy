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

#include "IosequencerInput.h"
#include "SubintervalOutput.h"
#include "ivytime.h"

// Each WorkloadThread has an array of two subinterval objects, each with a SubintervalInput
// object and a SubintervalOutput object.

// Once the Workload thread is running, it switches back and forth between the two subinterval objects.

// When the lock on the WorkloadThread is not held, one of the subintervals is the current running Subinterval,
// meaning as I/O operations complete, their data are being recorded in the current running Subinterval's
// SubintervalOutput object.

// When the lock on the WorkloadThread is not held, the other, not-running, Subinterval object can be in several states.

// subinterval_state::ready_to_run means ivyslave has finished preparing the next subinterval by setting
// the parameter values in the SubintervalInput object, and has cleared the contents of the SubintervalOutput
// object so it's ready to record events in the next subinterval.

// subinterval_state::ready_to_send means the subinterval data is for the previous subinterval that just ended,
// and that the WorkloadThread has declared the Subinterval ready for ivyslave to pick up and send to ivymaster.

// The ivyslave cycle starts with ivyslave waiting to the scheduled subinterval end time.
// Then it harvests the CPU core busy values from Linux and sends a [CPU] line to ivymaster.

// Then it waits for "catnap" seconds into the subinterval, before it tries to
// check to see if WorkloadThreads have posted the subinterval "ready to send".

// When ivyslave harvests a subinterval, it gets the lock and checks the subinterval's status.

// If it shows as subinterval_state::running, this means that the WorkloadThread is running a bit late,
// and hasn't marked it as complete yet.

// In this case, ivyslave will wait for a quarter-catnap and try to get the lock again to see if in the mean time
// the WorkloadThread has marked the subinterval as subinterval_state::ready_to_send.

// If at least one WorkloadThread was running late past "catnap" to post a subinterval,
// we write a log record showing the longest WorkloadThread post delay after the subinterval end.
// If that delay is more than 2x "catnap" we also write that log record to the console.

// There is a "post_limit_time" in seconds parameter (2 seconds at time of writing) where
// if a WorkloadThread hasn't posted switching over to the other subinterval within that
// time limit, an <Error> condition is recognized.

// As a WorkloadThread is running, issuing I/O operations, when it reaches the scheduled end of
// a subinterval, it attempts to get the lock.  This is tried a couple of times, as if for no
// other reason the attempt to get the lock can fail spuriously.  But after two tries, the
// WorkloadThread waits one ms at a time for a few times to get the lock, declaring an
// <Error> condition and abnormally terminating if it had to wait longer than a few ms.

// Once the WorkloadThread has the lock, it checks to make sure the "other" subinterval is
// marked "ready to run", otherise declares an <Error> and abnormally terminates.

// The WorkloadThread then switches between subintervals, marking the current running
// subinterval as subinterval_state::running and the other just-completed subinterval
// as subinterval_state::ready_to_send, and drops the lock.

// And when either ivyslave or the WorkloadThread has dropped the lock, it does a notify_all()
// to ensure the other is notified that the lock may be available.

// In the original design, I had planned to set any [EditRollup] parameter updates
// into the non-running subinterval's SubintervalInput object.  However, due to a
// serendipitous bug in the original implementation, [EditRollup] updates were
// accidentally applied to the current running Subinterval's SubintervalInput object.

// I noticed that it seemed to run fine.  My current I/O sequencer methods run OK
// even if input parameter changes are made as the sequencer is running.

// This was kept as the operating method so that dynamic feedback control updates
// could be applied almost immediately after a subinterval ended, with the reaction
// time being about a "catnap" after the subinterval switchover, plus the time
// to rollup at ivymaster, plus the time send out [EditRollup] DFC updates.

// This usually is much shorter than a subinterval.

// However, keep in mind as you write other I/O sequencers that the SubintervalInput
// parameter object can have parameter values that can change at any time while it's running.

// If you need parameter values not to change during a subinterval, or require perhaps
// atomic update of a set of parameter values, you may need to revert back to the original
// design where changes to input parameters are only made to the non-running object.


enum class subinterval_state
{
    undefined,
    ready_to_run,  // ivyslave finished setting up
    running,       // WorkloadThread is running, accumulating event data in the SubintervalOutput subobject
    ready_to_send, // subinterval collection complete, WorkloadThread is now running the "other" subinterval
    sending,
    stop
};

std::ostream& operator<< (std::ostream& o, const subinterval_state s);

class Subinterval {

public:

	IosequencerInput
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

	subinterval_state subinterval_status { subinterval_state::undefined };

	Subinterval(){input.reset();output.clear();}
};



