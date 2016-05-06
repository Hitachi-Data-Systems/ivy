//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include "Select.h"

class LUNpointerList
{

public:
// variables
	std::list<LUN*> LUNpointers;

// methods

	LUNpointerList(){}

	void split_out_command_devices_into(LUNpointerList& cmddevlist);
	std::string toString();
	void clone(LUNpointerList&);
	void clear() { LUNpointers.clear(); }
	void print_csv_file(std::ostream& o);
    bool clear_and_set_filtered_version_of(LUNpointerList&,Select*);
};

