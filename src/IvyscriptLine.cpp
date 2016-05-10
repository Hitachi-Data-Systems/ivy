//Copyright (c) 2016 Hitachi Data Systems, Inc.
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
//Author: Allart Ian Vogelesang <ian.vogelesang@hds.com>
//
//Support:  "ivy" is not officially supported by Hitachi Data Systems.
//          Contact me (Ian) by email at ian.vogelesang@hds.com and as time permits, I'll help on a best efforts basis.
#include <map>
#include <string>
#include <iostream>
#include <sstream>

#include "ivyhelpers.h"
#include "IvyscriptLine.h"

bool IvyscriptLine::parse(std::string s)
{
	wellFormed = false;
	sections.clear();
	section_indexes_by_name.clear();
	parse_error_message.clear();
	cursor = 0;

	text = removeComments(s);
	trim(text);

	std::string parsedSectionName, parsedSectionText;

	if ( 0 == text.length())
	{
		std::ostringstream o;
		o	<< "IvyscriptLine::parse(\"" << text << "\")" << std::endl
			<< "                      "; for (unsigned int i = 0; i<cursor; i++) o << ' '; o << '|' << std::endl;
		o	<< " - Not well-formed.  A Null line (after possibly removing comments and leading whitespace) is not well-formed, but isNull() will return \"true\"." << std::endl;
		parse_error_message = o.str();
		return false; // leaving wellFormed == false
	}

	unsigned int last_cursor = -1 + text.length();
	next_char = text[0];

	while (cursor <= last_cursor)
	{
		// process a "[sectionName] section value" sequence

		parsedSectionName="";
		parsedSectionText="";

		// The parser doesn't look for quoted strings within a "section value" sequence,
		// so if you want to put a square bracket '[' or ']' in a "section value" string,
		// escape them like "\[" or "\]".

		if ( '[' != next_char )
		{
			std::ostringstream o;
			o	<< "IvyscriptLine::parse(\"" << text << "\")" << std::endl
				<< "                      "; for (unsigned int i = 0; i<cursor; i++) o << ' '; o << '|' << std::endl;
			o	<< " - section as in \"[sectionName] section text \" did not start with \'[\'." << std::endl;
			parse_error_message = o.str();
			return false; // leaving wellFormed == false
		}
		parsedSectionName.push_back(next_char);
		consume_character(); // consume '['

		while (true)
		{
			if (cursor > last_cursor)
			{
				std::ostringstream o;
				o	<< "IvyscriptLine::parse(\"" << text << "\")" << std::endl
					<< "                      "; for (unsigned int i = 0; i<cursor; i++) o << ' '; o << '|' << std::endl;
				o	<< " - did not find closing \']\' of section name." << std::endl;
				parse_error_message = o.str();
				return false; // leaving wellFormed == false
			}

			if (']' == next_char)
			{
				parsedSectionName.push_back(next_char);
				consume_character(); // consume ']'
				break;
			}
			parsedSectionName.push_back(next_char);
			consume_character();
		}
		if (!IvyscriptLine::isValidSectionNameWithBrackets(parsedSectionName))
		{
			std::ostringstream o;
			o	<< "IvyscriptLine::parse(\"" << text << "\")" << std::endl
				<< "                      "; for (unsigned int i = 0; i<cursor; i++) o << ' '; o << '|' << std::endl;
			o	<< " - section name \"" << parsedSectionName << "\" did not look like a valid section name like \"[sectionName]\"." << std::endl;
			parse_error_message = o.str();
			return false; // leaving wellFormed == false
		}
		// we now have a well-formed parsedSectionName

		// now we scan the text part up to but not including a '[' which would start the next section.

		while (true)
		{
			if (cursor>last_cursor) break;

			if (cursor<last_cursor && '\\' == next_char && ('[' == text[cursor+1] || ']' == text[cursor+1]))
			{
				// escaped square bracket
				consume_character();
				parsedSectionText.push_back(next_char);
				consume_character();
			}
			else if ('[' == next_char || ']' == next_char)
			{
				break;
			}
			else
			{
				parsedSectionText.push_back(next_char);
				consume_character();
			}
		}

		// finally we have parsed a well-formed section, but has the section name been seen before???

		std::map<std::string, int>::iterator it = section_indexes_by_name.find(toLower(parsedSectionName));

		if (!( section_indexes_by_name.end() == it ))
		{
			std::ostringstream o;
			o	<< "IvyscriptLine::parse(\"" << text << "\")" << std::endl
				<< "                      "; for (unsigned int i = 0; i<cursor; i++) o << ' '; o << '|' << std::endl;
			o	<< " - section name \"" << parsedSectionName << "\" may not repeat." << std::endl;
			parse_error_message = o.str();
			return false; // leaving wellFormed == false
		}

		trim(parsedSectionText);

		IvyscriptLineSection ils;
		ils.sectionNameWithBrackets = parsedSectionName;
		ils.sectionText = parsedSectionText;

		sections.push_back(ils);
		section_indexes_by_name[toLower(parsedSectionName)] = sections.size() - 1;
	}

	wellFormed=true;   // we know the line was not null, so there has to be at least one section, and no parse errors.
	return true;
}


/*static*/ bool IvyscriptLine::isValidSectionNameWithBrackets(std::string s)
{
	if (s.length()<3) return false;
	if ('[' != s[0]) return false;
	if (']' != s[-1 + s.length()]) return false;
	if (!isalpha(s[1])) return false;
	if (3 == s.length()) return true;

	for (unsigned int i=2; i<(-1+s.length()); i++)
	{
		if (!(  isalnum(s[i]) || '_' == s[i]  )) return false;
	}
	return true;
}

bool IvyscriptLine::containsSection(std::string s)  // lookups and matches are case insensitive so [host] and [Host] are the same.
{
	std::map<std::string, int>::iterator it;

	it = section_indexes_by_name.find(toLower(s));

	if (section_indexes_by_name.end() == it)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool IvyscriptLine::getSectionTextBySectionName(std::string callers_error_message, std::string sectionNameWithBrackets, std::string& referenceToPutText)
// returns true on success
{
	callers_error_message.clear();

	if (!containsSection(sectionNameWithBrackets))
	{
		std::ostringstream o;
		o << "IvyscriptLine::getSectionTextBySectionName(\"" << sectionNameWithBrackets << "\" - no such section name.";
		callers_error_message = o.str();
		referenceToPutText.clear();
		return false;
	}


	referenceToPutText = sections[section_indexes_by_name[toLower(sectionNameWithBrackets)]].sectionText;
	return true;
}


bool IvyscriptLine::getSectionNameBySubscript(std::string callers_error_message, int i, std::string& referenceToPutText)
// returns true on success
{
	callers_error_message.clear();

	if ( (0 > i) || (i >= numberOfSections()))
	{
		std::ostringstream o;
		o << "IvyscriptLine::getSectionNameBySubscript(" << i << ", ) - invalid section index " << i << ". Maximum index is " << (-1+numberOfSections());
		callers_error_message=o.str();
		referenceToPutText.clear();
		return false;
	}
	referenceToPutText = sections[i].sectionNameWithBrackets;
	return true;
}


bool IvyscriptLine::getSectionTextBySubscript(std::string callers_error_message, int i, std::string& referenceToPutText)
		// returns true on success
{
	callers_error_message.clear();

	if (i < 0 || i >= numberOfSections())
	{
		std::ostringstream o;
		o << "IvyscriptLine::getSectionTextBySubscript(" << i << ", ) - invalid section index " << i << ".  Max index is " << numberOfSections();
		referenceToPutText.clear();
		callers_error_message=o.str();
		return false;
	}
	referenceToPutText = sections[i].sectionText;
	return true;
}



bool IvyscriptLine::containsOnly(std::string& callers_error_message, std::list<std::string>& sectionNamesWithBrackets)
// returns true if we contain only section names in the list
	// in C++11 hopefully you can say if xxxx.containsOnly(my_error_message, std::list<std::string>{"[hosts]", "[select]"}) ...
	// There may be a way that avoids making a copy of the list to pass as a parameter ...

// The error message is built assuming that the first section name in sectionNamesWithBrackets
// is the name of the ivyscript statement we are processing.
{
	callers_error_message.clear();

	std::ostringstream o;

	bool goodSoFar = true;

	if (0 == sectionNamesWithBrackets.size())
	{
		callers_error_message = "IvyscriptLine::containsOnly() was called with an empty set of valid [sectionName] tokens.";
		return false;
	}

	if (0 == sections.size())
	{
		callers_error_message = "IvyscriptLine::containsOnly() was called on an IvyscriptLine object with an empty set of \"[sectionName] section text\" sections";
		return false;
	}

	for (unsigned int i=0; i<sections.size(); i++)
	{
		std::string sectionName;
		bool isOK;

		sectionName = sections[i].sectionNameWithBrackets;
		isOK=false;

		for (auto& s : 	sectionNamesWithBrackets)
		{
			if (stringCaseInsensitiveEquality(s,sectionName))
			{
				isOK=true;
				break;
			}
		}

		if (isOK)
		{
			continue;
		}
		else
		{
			if (goodSoFar)
			{
				o << "ivyscript statement may not contain section(s) ";
				goodSoFar=false;
			}
			else
			{
				o << ", ";
			}
			o << "\"" << sectionName << "\"";
		}
	}

	if (goodSoFar)
	{
		return true;
	}
	else
	{
		o << " - valid sections are ";
		bool first = true;
		for (auto& s : 	sectionNamesWithBrackets)
		{
			if (!first) o << ", ";
			first = false;
			o << s;
		}
		o << ".";
		callers_error_message=o.str();
		return false;
	}
}










































































