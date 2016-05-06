//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <string>
#include <map>
#include <list>
#include <algorithm>    // std::find_if
#include <iostream>
#include <sstream>
#include <regex>

#include "ivyhelpers.h"
#include "LUN.h"

#include "AttributeNameCombo.h"


bool isValidAttributeName(std::string& callers_error_message, std::string token, LUN* pSampleLUN)
{
	if (stringCaseInsensitiveEquality(token, std::string("all"))) return true;

	if (pSampleLUN->contains_attribute_name(token)) return true;

	{
		std::ostringstream o;
		bool needComma {false};

		o << "invalid attribute name \"" << token << "\".  Valid LUN attribute names are ";

		for (auto& pear : pSampleLUN->attributes)
		{
			if (needComma) o << ", ";
			needComma = true;
			o << "\"" << pear.first << "\"";
		}

		callers_error_message = o.str();
	}

	return false;
}


void AttributeNameCombo::clone( AttributeNameCombo& other )
{
	attributeNameComboID = other.attributeNameComboID;
	attributeNames.clear(); for (auto& name : other.attributeNames) attributeNames.push_back(name);
	isValid=other.isValid;
}


void AttributeNameCombo::clear()
{
	attributeNameComboID.clear();
	attributeNames.clear();
	isValid=false;
}


bool AttributeNameCombo::set(std::string& callers_error_message, std::string t, LUN* pSampleLUN)  //  serial_number+Port
{

	// A valid attribute name combo token consists of one or more valid LUN-lister column header attribute names,
	// or ivy knicknames yielding possibly processed versions of LUN-lister attribute values,
	// joined into a single token using plus signs as separators.

	// For example, where the LUN-lister "Parity Group" has the value "01-01", the ivy nickname PG has the value "1-1",
	// and you can use PG to create rollups or csv files instead of Parity_Group.

	// The upper/lower case of the text that the user supplies to create a rollup
	// persists and shows in output csv files, but all comparisons for matches on
	// attribute names are case-insensitive.

	// LUN-lister column header tokens are pre-processed to a "flattened" form
	// for the purposes of generating keys and for matching identity.
	//  - Characters that would not be allowed in an identifier name are converted to underscores _.
	//  - Text is translated to lower case.

	// That means that at any time in ivy, saying "LUN Name" is the same as saying lun_name.
	// In other words, you can use the actual orginal LUN-lister csv headers if you put them in quotes.


	// The job of the constructor is to parse the text
	// - split the text into chunks separated by + signs.
	// - Validate that each chunk either matches a built-in attribute like host or workload,
	//   or that it matches one of the column headers in the sample LUN provided to the constructor.

	callers_error_message.clear();
	clear();

	attributeNameComboID=t;

	trim(attributeNameComboID);  // removes leading / trailing whitespace

	if (0 == attributeNameComboID.length())
	{
		callers_error_message = "AttributeNameCombo::set() - called with null string.";
		return false;
	}

	cursor = 0;
	last_cursor = attributeNameComboID.length() -1;
	next_char = attributeNameComboID[0];

	std::string token;

	while ( true )
	{
		// at start of attribute name
		token.clear();

		while ( cursor <= last_cursor && '+' != next_char )
		{
			token.push_back(next_char);
			consume();
		}

		std::string my_error_message;

		if (isValidAttributeName(my_error_message, token, pSampleLUN))  // either a LUN-lister column heading, or an ivy nickname
		{
			attributeNames.push_back(token);
		}
		else
		{
			std::ostringstream o;
			o << "AttributeNameCombo::set() - " << my_error_message;
			callers_error_message = o.str();
			return false;
		}

		if (cursor > last_cursor) break;

		if ('+' == next_char)
		{
			consume();
		}
	}

	isValid=true;
	return true;
}





