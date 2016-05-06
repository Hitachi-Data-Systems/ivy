//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once 

class WorkloadID
{
public:
// variables
	std::string workloadID, ivyscript_hostname, LUN_name, workload_name;
	bool isWellFormed {false};  // set() turns this on if ID looks like "sun159+/dev/sdxy+charlie"
		// well, at least exactly 2 "+" signs and something non-blank on either side of them.

// methods
	bool set(std::string workloadID); // true if workloadID is well-formed.
	WorkloadID (std::string ID) { set(ID); return; }
	WorkloadID(){}
	std::string getHostPart();
	std::string getLunPart();
	std::string getWorkloadPart();
};



