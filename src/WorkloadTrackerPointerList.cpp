//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <iostream>
#include <sstream>
#include <list>

#include "ivydefines.h"
#include "ivyhelpers.h"
#include "LUN.h"
#include "WorkloadID.h"
#include "IogeneratorInput.h"
#include "WorkloadTracker.h"
#include "Select.h"
#include "WorkloadTrackerPointerList.h"

std::string WorkloadTrackerPointerList::toString()
{
	std::ostringstream o;

	o << "WorkloadTrackerPointerList object contains " << workloadTrackerPointers.size() << " pointers." << std::endl;

	int i=0;
	for (auto& pWorkloadTracker : workloadTrackerPointers)
	{
		o << "WorkloadTrackerPointerList #" << i++ << " -> " << pWorkloadTracker->toString() << std::endl;
	}

	return o.str();
}
