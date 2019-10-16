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

