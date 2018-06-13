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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#pragma once

#include <string>
#include <vector>
#include <map>
#include <list>

// a well-formed ivyscript line looks like "[section1] section 1 text [section2] section 2 text"
// Where all the [section] tokens don't contain white space and where the section name is an identifier
// starting with an alphabetic and continuing with alphanumerics and underscores.


class IvyscriptLine
{
	typedef struct IvyscriptLineSection
	{
		std::string sectionNameWithBrackets;
		std::string sectionText;
	} IvyscriptLineSection;

public:
// variables
	bool wellFormed {false};
	std::vector<IvyscriptLineSection> sections;
	std::map<std::string /*lowercasesectionname*/, int /*index_into_vector*/> section_indexes_by_name;
	std::string parse_error_message;

	std::string text;
	unsigned int cursor = 0;
	char next_char;

// methods
	IvyscriptLine () {}
	IvyscriptLine (std::string s) {parse(s);}

	inline bool haveAnotherCharacter() { return cursor < text.length(); }
	inline void consume_character() { cursor++; if (cursor<text.length()) next_char = text[cursor]; }

	static bool isValidSectionNameWithBrackets(std::string);

	bool parse(std::string); // returns true if well-formed.

		// [section1] section1 text [section2] section2 text

		// Where there are one or more "sections" of the form "[sectionName]sectionText",
		// where the [sectionName] part has no whitespace and has an identifier token
		// starting with an alphabetic and continuing with alphanumerics & underscores,
		// and where no [sectionName] occurs more than once.

		// Result of last parse() is remembered in wellFormed.


	inline bool isNull() { return 0 == text.length(); }  // if after removing comments what remains is at most whitespace

	inline bool isWellFormed() {return wellFormed; }

	std::string getParseErrorMessage() { return parse_error_message; }
	std::string getText(){ return text; }

	int numberOfSections() { return sections.size(); }

	bool containsSection(std::string sectionNameWithBrackets);  // lookups and matches are case insensitive so [host] and [Host] are the same.

	bool getSectionTextBySectionName(std::string callers_error_message, std::string sectionNameWithBrackets, std::string& referenceToPutText);
		// returns true on success

	bool getSectionNameBySubscript(std::string callers_error_message, int, std::string& referenceToPutText);
		// returns true on success

	bool getSectionTextBySubscript(std::string callers_error_message, int, std::string& referenceToPutText);
		// returns true on success

	bool containsOnly(std::string& callers_error_message, std::list<std::string>& sectionNamesWithBrackets);
	// returns true if we contain only section names in the list
		// in C++11 hopefully you can say if xxxx.containsOnly(std::list<std::string>{"[hosts]", "[select]"}) ...

};
