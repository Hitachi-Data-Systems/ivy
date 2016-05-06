//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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
