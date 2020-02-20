//Copyright (c) 2015-2020 Hitachi Vantara Corporation
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
#pragma once

#include <set>

//prerequisites
//SelectClause.h

class LUN;
class Select;
class Matcher
{
protected:
	bool compileSuccessful=false;
	SelectClause* p_SelectClause;
	std::string logfilename;
	std::string compileErrorMessage {"Matcher compile error."};

public:
	virtual bool matches(LUN* p_LUN) = 0;
	bool compiledOK() { return compileSuccessful; }
	std::string getMatcherCompileErrorMessage() { return compileErrorMessage; }
	Matcher(SelectClause* p_SC, std::string lfn) : p_SelectClause(p_SC), logfilename(lfn) {};
};

class MatcherColumnHeader : public Matcher
{
public:
	void compile();
	MatcherColumnHeader(SelectClause* p_SC, std::string logfilename) : Matcher(p_SC, logfilename) {compile();}

	bool matches(LUN* p_LUN);
};

class Matcher_ivyscript_hostname : public Matcher
{
private:
	std::set<std::string> hostname_set;

public:
	void compile();
	Matcher_ivyscript_hostname(SelectClause* p_SC, std::string logfilename) : Matcher(p_SC, logfilename) {compile();}

	bool matches(LUN* p_LUN);
};


class MatcherLDEV : public Matcher
{
private:
	LDEVset overallLDEVset;
	LDEVset thisTokenLDEVset;

public:
	void compile();
	MatcherLDEV(SelectClause* p_SC, std::string logfilename) : Matcher(p_SC, logfilename) {compile();}

	bool matches(LUN* p_LUN);
};



class Matcher_PG_compiled_match_token
{
private:
	std::string text;
	std::string integer_text;
	unsigned int cursor{0};
	char next_char;

	bool compiledWellFormedToken {false};

	inline bool next_char_is_available() { return cursor < text.length(); }
	void consume();

public:
//variables
	bool firstNumberIsWildcard, secondNumberIsWildcard;
	int firstNumberRangeStart, firstNumberRangeEnd, secondNumberRangeStart, secondNumberRangeEnd;
//methods
	Matcher_PG_compiled_match_token(std::string token, std::string logfilename);

	bool matches(int firstNumber, int secondNumber);
	inline bool compiledOK() { return compiledWellFormedToken;}
	std::string toString();
};


class MatcherPG : public Matcher
{
	// For matching, the LUN object's "Parity Group" is decoded into a first integer before the hyphen and a second integer after the hyphen.
private:
	std::list<Matcher_PG_compiled_match_token*> compiled_token_list;
	std::string text;
	std::string integer_text;
	unsigned int cursor;
	char next_char;

public:
//variables
	bool compiledOK {false};
//methods
	MatcherPG(SelectClause* p_SC, std::string logfilename) : Matcher(p_SC, logfilename) {compile();}
	~MatcherPG() { for (auto& p : compiled_token_list) delete p; }

	void compile();
	bool matches(LUN* p_LUN);
	void consume();
	inline bool next_char_available() {return cursor < text.length();}
};

