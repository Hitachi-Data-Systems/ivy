//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <list>
#include <string>

class ListOfWorkloadIDs
{
	// this makes it easy to send a list of WorkloadIDs in text form over the ivymaster-ivyslave pipe.
private:
	bool valid; // retains return code from last call to fromString()

public:
	std::list<WorkloadID> workloadIDs;

//methods
	bool isValid() {return valid;}
	std::string toString();  // '<', a list of WorkloadIDs separated by commas, and '>'
	bool fromString(std::string*); // true if the string parses as '<', a list of well-formed WorkloadIDs separated by commas, and '>'
	void addAtEndAllFromOther(ListOfWorkloadIDs& other);
	void clear() {workloadIDs.clear();}
};

