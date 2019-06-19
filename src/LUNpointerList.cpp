//Copyright (c) 2016, 2017, 2018 Hitachi Vantara Corporation
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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#include <iostream>
#include <sstream>
#include <list>
#include <map>
#include <algorithm>
#include <set>

#include "ivyhelpers.h"
#include "LUN.h"
#include "LUNpointerList.h"
#include "ivy_engine.h"

//#define IVY_TRACE_LUNPOINTERLIST

bool LUNpointerList::clear_and_set_filtered_version_of(LUNpointerList& source,JSON_select& select)
{

	LUNpointers.clear();

	if (select.is_null()) return false;

	for (LUN*& p_LUN : source.LUNpointers)
	{
		if (select.matches(p_LUN))
		{
#ifdef IVY_TRACE_LUNPOINTERLIST
			std::cout << "kept: " << p_LUN->toString() <<std::endl;
#endif//define IVY_TRACE_LUNPOINTERLIST
			LUNpointers.push_back(p_LUN);
		}
#ifdef IVY_TRACE_LUNPOINTERLIST
		else
		{
			std::cout << "rejected: " << p_LUN->toString() << std::endl;
		}
#endif//define IVY_TRACE_LUNPOINTERLIST
	}

	return true;
}

void LUNpointerList::clone(LUNpointerList& other)
{
	// making the argument a reference to a LUNpointerList object means you don't have to worry about "other" being valid by checking for invalid pointers.
	LUNpointers.clear();
	for (auto& pL : other.LUNpointers) LUNpointers.push_back(pL);
	return;
}


std::string LUNpointerList::toString()
{
	std::ostringstream o;

	o << "LUNpointerList object contains " << LUNpointers.size() << " pointers." << std::endl;

	int i=0;
	for (auto& pLUN : LUNpointers)
	{
		o << "LUNpointerList #" << i++ << " -> " << pLUN->toString() << std::endl << std::endl;
	}

	return o.str();
}

#define TRACE_SPLIT_CMDDEV on

void LUNpointerList::split_out_command_devices_into(LUNpointerList& cmddevlist)
{

	cmddevlist.clear();
	LUNpointerList others;

	for (auto it = LUNpointers.begin(); it != LUNpointers.end(); it++)
	{
		LUN* pLUN = (*it);
		if ( stringCaseInsensitiveEquality(std::string("OPEN-V-CM"),pLUN->attribute_value(std::string("Product"))))
		{
			cmddevlist.LUNpointers.push_back(pLUN);
		}
		else
		{
			others.LUNpointers.push_back(pLUN);
		}

	}
	LUNpointers.clear();
	clone(others);
	return;
}

void LUNpointerList::print_csv_file(std::ostream& o)
{
	// we make two passes though the list of LUNs.

	// On the first pass, we accumulate the set of all attribute names that appear in at least one of the LUNs in the list.

	// Then we print the title header line iterating over the unique attribute names.

	// Then for each LUN, for each column title, we try to retrieve the attribute from the LUN,
	// printing an empty column if the LUN doesn't have the particular attribute.

	std::set<std::string> column_headers;
	for (auto& pLUN : LUNpointers)
	{
		pLUN->push_attribute_names(column_headers);
	}

	bool need_comma{false};

	for (auto& header : column_headers)
	{
		if (need_comma)
		{
			o << ',';
		}
		need_comma = true;
		o << header;
	}
	o << std::endl;

	for (auto& pLUN : LUNpointers)
	{
		need_comma = false;
		for (auto& header : column_headers)
		{
			if (need_comma)
			{
				o << ',';
			}
			need_comma = true;

			if (pLUN->contains_attribute_name(header))
			{
				o << quote_wrap_LDEV_PG_leading_zero_number(pLUN->attribute_value(header),m_s.formula_wrapping);
			}
		}
		o << std::endl;
	}
}
