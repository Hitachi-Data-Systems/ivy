//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <iostream>
#include <sstream>
#include <list>
#include <map>
#include <algorithm>
#include <set>

#include "ivyhelpers.h"
#include "LUN.h"
#include "Select.h"
#include "LUNpointerList.h"

//#define IVY_TRACE_LUNPOINTERLIST

bool LUNpointerList::clear_and_set_filtered_version_of(LUNpointerList& source,Select* pSelect)
{

	LUNpointers.clear();

	if (pSelect == nullptr) return false;

	for (LUN*& p_LUN : source.LUNpointers)
	{
		if (pSelect->matches(p_LUN))
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
		for (auto& pear : pLUN->attributes)
		{
			const std::string& attribute = pear.first;

			if (!stringCaseInsensitiveEquality(std::string("all"),attribute)) column_headers.insert(attribute);
		}
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

			auto att_it = pLUN->attributes.find(header);
			if (pLUN->attributes.end() != att_it)
			{
				o << quote_wrap_except_number((*att_it).second);
			}
		}
		o << std::endl;
	}
}
