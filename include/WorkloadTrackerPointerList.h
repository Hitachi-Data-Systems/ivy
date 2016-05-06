//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

class WorkloadTrackerPointerList
{
private:
	std::string logfilename;
public:
// variables
	std::list<WorkloadTracker*> workloadTrackerPointers;

// methods

	WorkloadTrackerPointerList() {}

	std::string toString();

};

