//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include "WorkloadTracker.h"

class WorkloadTrackers
{
public:
//variables
	std::map<std::string /*workloadID*/, WorkloadTracker*> workloadTrackerPointers;

//methods
	inline void clear() { for (auto& pear : workloadTrackerPointers) delete pear.second; }
	inline ~WorkloadTrackers() { clear(); }
	void clone(WorkloadTrackers& other) {clear(); for (auto& pear : other.workloadTrackerPointers) workloadTrackerPointers[pear.first] = pear.second;}
};

