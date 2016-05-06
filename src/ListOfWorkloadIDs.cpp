//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <string>
#include <iostream>
#include <sstream>
#include <list>

#include "WorkloadID.h"
#include "ListOfWorkloadIDs.h"

std::string ListOfWorkloadIDs::toString()
{
	std::ostringstream o;
	bool need_comma=false;

	o << '<';
	for (auto& wID: workloadIDs)
	{
		if (need_comma) o << ',';
		o << wID.workloadID;
		need_comma = true;
	}
	o << '>';
	return o.str();
}


bool ListOfWorkloadIDs::fromString(std::string* pText)
{
	workloadIDs.clear();

	if ( 2 > pText->length() )
	{
		valid=false;
		return false;
	}

	int last_cursor = -1 + pText->length();

	int cursor=0;
	char next_char=(*pText)[cursor];
	std::string wID;

	if ('<' != next_char) return false;

	/* consume character */ cursor++; if (cursor<=last_cursor) next_char = (*pText)[cursor];

	// we already know the length was at least 2, so there is a valid next_char

	while (true)
	{
		// process a WorkloadID

		// step over leading whitespace and commas
		while ( cursor <= last_cursor && (isspace(next_char) || ',' == next_char ) ) 
			{/* consume character */ cursor++; if (cursor<=last_cursor) next_char = (*pText)[cursor];}

		if ( cursor > last_cursor ) return false; // did not find final '>'
	
		if ( '>' == next_char)
		{
			if (cursor == last_cursor) return true;
			else return false;
		}

		// at first character of putative workloadID.  A non-blank, non comma, non '>' character.

		wID="";
		
		while (cursor<=last_cursor && (!(isspace(next_char) || ',' == next_char || '>' == next_char  )))
		{
			wID.push_back(next_char);
			/* consume character */ cursor++; if (cursor<=last_cursor) next_char = (*pText)[cursor];
		}
		{
			WorkloadID parsedWID(wID);
			
			if (parsedWID.isWellFormed)
			{
				workloadIDs.push_back(parsedWID);
			}
			else
			{
				return false;
			}
		}
	}	
}



void ListOfWorkloadIDs::addAtEndAllFromOther(ListOfWorkloadIDs& other)
{
	for (auto& workloadID : other.workloadIDs)
	{
		workloadIDs.push_back(workloadID);
	}
}

