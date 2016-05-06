//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <string>
#include <list>

// SORRY ABOUT THE PRIMITIVE 1970s-STYLE PARSER.

// Regular expressions are broken in g++ and I didn't want to go through the hassle of figuring out if it's OK to use Boost.

// Later when we have time, or it's open source and others have time, build a proper lex/yacc-style parser or whatever animal is in use today
// for an ivyscript language with the usual programming language conveniences and constructs.

// Well, actually, what I am going to leave for now as a "to do" on this list is to retrofit the use of this
// parser in Select, from whence the NameEqualsValueList parser originally came.  Also go through the verbage and
// separate out a description of how things parse from the semantics of selecting LUNs and selecting rollups.

// Some valid initializer strings are
//
// LDEV = 00:00
// LDEV = "00:00, 01:00-01:FF"
// LDEV = {00:00, 01:00-01:FF}
// LDEV = {00:00 01:00-01:FF}
// LDEV={ 00:00 01:00-01:FF }
// LDEV = 00:00-00:FF, Port = (1A,3A,5A,7A}  PG={ 1:3-*, 15-15 }
// host="sun159 cb28-31"
// host = {sun159 cb28-31}
// serial_number+Port = 410123+1A

// The example "LDEV = 00:00-00:FF, Port = (1A,3A,5A,7A}  PG={ 1:3-*, 15-15 }" parses into
//
// ListOfNameEqualsValueList "LDEV = 00:00-00:FF, Port = (1A,3A,5A,7A}  PG={ 1:3-*, 15-15 }"
//       NameEqualsValueList "LDEV = 00:00-00:FF"
//           name token      "LDEV"
//           value token list
//                value token "00:00-00:FF"
//       NameEqualsValueList "Port = (1A,3A,5A,7A}"
//           name token      "Port"
//           value token list
//                value token "1A"
//                value token "3A"
//                value token "5A"
//                value token "7A"
//       NameEqualsValueList "PG={ 1:3-*, 15-15 }"
//           name token      "PG"
//           value token list
//                value token "1:3-*"
//                value token "15-15"

// Edit the isUnquotedOKin[Name/Value]Tokens functions to define what characters are OK in tokens without needing to be enclosed in quotes.

// NOTE: The parser doesn't try to figure out if the name tokens or value tokens are meaningful or valid.
// Thus "+_+ = {cross face }" parses just fine.

class NameEqualsValueList
{
public:
	std::string text {"<text not set yet>"};
	std::string name_token {"<name_token not set yet>"};
	std::list<std::string> value_tokens;

	NameEqualsValueList(){}
	std::string toString();
};


