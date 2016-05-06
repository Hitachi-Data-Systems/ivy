//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <iostream>
#include <sstream>

#include "ivydefines.h"
#include "ivyhelpers.h"
#include "LUN.h"
#include "WorkloadID.h"
#include "IogeneratorInput.h"
#include "WorkloadTracker.h"

WorkloadTracker::WorkloadTracker(std::string ID, std::string iogenerator_name, LUN* pL) : workloadID(ID)
{
	workloadLUN.copyOntoMe(pL);
	workloadLUN.attributes[std::string("workloadid")] = ID;
	workloadLUN.attributes[std::string("iogenerator_name")] = iogenerator_name;

	WorkloadID workloadID(ID);
	if (workloadID.isWellFormed) 
	{
		workloadLUN.attributes[std::string("workload")] = workloadID.getWorkloadPart();
	}
	else
	{
		std::ostringstream o; o << "<WorkloadTracker::WorkloadTracker() - internal programming error - workloadID \"" << ID << "\" is malformed.";
		workloadLUN.attributes[std::string("workload")] = o.str();
	}

//*debug*/std::cout << "WorkloadTracker::WorkloadTracker - workloadLUN = " << workloadLUN.toString() << std::endl;

	return;
}

std::string WorkloadTracker::toString()
{
	std::ostringstream o;
	o << "WorkloadTracker workloadID = \"" << workloadID.workloadID << "\", LUN = " << workloadLUN.toString();
	return o.str();
}




