//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
//NameEqualsValueList.cpp

#include <cctype> // for isalpha(), etc.
#include <iostream>
#include <sstream>

#include "ivyhelpers.h"
#include "NameEqualsValueList.h"


std::string NameEqualsValueList::toString()
{
	std::ostringstream o;
	o	<< "NameEqualsValueList:" << std::endl
		<< "    text = \"" << text << "\"" << std::endl
		<< "    name_token = \"" << name_token << "\"" << std::endl
		;
	for (auto& s : value_tokens)
	{
		o 
		<< "    value_token = \"" << s << "\"" << std::endl;
	}
	return o.str();
}

