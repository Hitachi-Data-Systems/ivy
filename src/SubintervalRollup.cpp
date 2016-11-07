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
#include "ivytime.h"
#include "ivyhelpers.h"
#include "ivydefines.h"
#include "IosequencerInput.h"
#include "IosequencerInputRollup.h"
#include "RunningStat.h"
#include "Accumulators_by_io_type.h"
#include "SubintervalOutput.h"
#include "SubintervalRollup.h"
#include "SequenceOfSubintervalRollup.h"
#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"
#include "RollupInstance.h"
#include "LUN.h"
#include "AttributeNameCombo.h"
#include "RollupType.h"

void SubintervalRollup::addIn(const SubintervalRollup& other)
{
	if (ivytime(0) == startIvytime) startIvytime=other.startIvytime;
	else if ( (!(ivytime(0) == other.startIvytime)) && other.startIvytime < startIvytime) startIvytime = other.startIvytime;

	if (ivytime(0) == endIvytime) endIvytime=other.endIvytime;
	else if ( (!(ivytime(0) == other.endIvytime)) && other.endIvytime > endIvytime) endIvytime = other.endIvytime;

	inputRollup.merge(other.inputRollup);
	outputRollup.add(other.outputRollup);
}

