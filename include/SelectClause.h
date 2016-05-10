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
#pragma once

#include <list>
#include <iostream>

#include "Xpr.h"
#include "Ivy_pgm.h"

class Matcher;
class LUN;
class Select;

class SelectClauseValue
{
public:
    yy::location bookmark; // The beginning/ending location in the source file - provided by the Bison parser
    Xpr* pXpr; // Parser provides a pointer to a "string_x", an Xpr that evaluates to a string.

// methods
    SelectClauseValue(const yy::location& l, Xpr*p) : bookmark(l), pXpr(p) {};

    std::string value() // returns the result of evaluating the string expression.
        { if (pXpr == nullptr) return ""; std::string v; pXpr->evaluate((void*)(&v)); return v; }

    void display(const std::string& /*indent*/, std::ostream&); // used to implement trace for parser, evaluate
};

void display_SelectClauseValue_pointer_list(const std::string& indent, std::list<SelectClauseValue*>*, std::ostream&);

class SelectClause
{
public:
    yy::location bookmark; // The beginning/ending location in the source file - provided by the Bison parser
	Xpr* p_attribute_name_Xpr;
	std::list<SelectClauseValue*>* p_SelectClauseValue_pointer_list {nullptr};
	std::string logfile; // because ivyslave doesn't have m_s

// methods
	SelectClause(const yy::location& l, Xpr* p_a, std::list<SelectClauseValue*>* p_vl, const std::string& lf)
        : bookmark(l), p_attribute_name_Xpr(p_a), p_SelectClauseValue_pointer_list(p_vl), logfile(lf) {};

    std::string attribute_name_token();
    std::list<std::string> attribute_value_tokens();

    void display(const std::string& /*indent*/, std::ostream&);

	bool matches(LUN*);

	bool ldev_matches(LUN*);
	bool host_matches(LUN*);
	bool pg_matches(LUN*);
	bool default_matches(LUN*);
	void complain_pg(std::string s);
};

