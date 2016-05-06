//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include "ivytime.h"
#include "ivyhelpers.h"
#include "ivydefines.h"
#include "IogeneratorInput.h"
#include "IogeneratorInputRollup.h"
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

