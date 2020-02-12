
%define api.prefix {xx_}
%language "C++"
%defines
%locations
%define api.parser.class {select}

%{

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

#include <iostream>
#include <cassert>

#include "JSON_select.h"

using namespace std;

bool trace_select_parser {false};

%}

%parse-param { JSON_select&  context_ref }
%lex-param { JSON_select&  context_ref }

%union {
	std::string* p_str;
	JSON_select_clause* p_JSON_select_clause;
	JSON_select_value* p_JSON_select_value;
    std::list<JSON_select_value*>* p_JSON_select_value_pointer_list;
};

/* declare tokens */

%token <p_str> QUOTED_STRING BARE_JSON_NUMBER
%token KW_TRUE KW_FALSE KW_NULL
%token END_OF_FILE

%type <p_JSON_select_value_pointer_list> zero_or_more_values
%type <not_used> optional_enclosing_braces select_clause_list select_clause
%type <p_JSON_select_value> value

%{
extern int yylex(xx_::select::semantic_type *xx_lval, xx_::select::location_type* xx_lloc,  JSON_select&  context_ref);
void myout(int val, int radix);
%}

%initial-action {
 // Nothing for now
}

%%

program:

	{
        if (trace_select_parser) std::cout << "select parser - startup.";
	}

	optional_enclosing_braces END_OF_FILE
        {
            if (trace_select_parser) std::cout << "select parser - now going to YYACCEPT." << std::endl;

            if (!context_ref.unsuccessful_compile) context_ref.successful_compile = true;

            YYACCEPT;
        }
;

optional_enclosing_braces:
    select_clause_list
        {
            if (trace_select_parser) std::cout << "select parser - optional_enclosing_braces matching on bare select_clause_list." << std::endl;
        }
    | '{' select_clause_list '}'
        {
            if (trace_select_parser) std::cout << "select parser - optional_enclosing_braces matching on { select_clause_list }." << std::endl;
        }
;

select_clause_list:
     /* nothing */
        {
            if (trace_select_parser) std::cout << "select parser - select_clause_list matching on nothing." << std::endl;

        }
    | select_clause_list ','
        {
            if (trace_select_parser) std::cout << "select parser - select_clause_list matching on select_clause_list \',\'." << std::endl;
        }
    | select_clause_list select_clause
        {
            if (trace_select_parser) std::cout << "select parser - select_clause_list matching on select_clause_list select_clause." << std::endl;
        }
;

select_clause:
    QUOTED_STRING ':' value
        {
            if (trace_select_parser) std::cout << "select parser - select_clause matching QUOTED_STRING =>" << *($1) << "<= value." << std::endl;
            auto p = new std::list<JSON_select_value*>;
            p->push_back($3);

            context_ref.JSON_select_clause_pointer_list.push_back(new JSON_select_clause($1,p));
        }
    | QUOTED_STRING ':' '[' zero_or_more_values ']'
        {
            if (trace_select_parser) std::cout << "select parser - select_clause matching QUOTED_STRING =>" << *($1) << "<= : [ zero_or_more_values ]." << std::endl;

            context_ref.JSON_select_clause_pointer_list.push_back(new JSON_select_clause($1,$4));
        }
;

zero_or_more_values:
    /* nothing */
        {
            if (trace_select_parser) std::cout << "select parser - zero_or_more_values matching nothing.." << std::endl;
            $$ = new std::list<JSON_select_value*>;
        }
    | zero_or_more_values ','
        {
            if (trace_select_parser) std::cout << "select parser - zero_or_more_values matching one_or_more_values \',\'." << std::endl;
            $$ = $1;
        }
    | zero_or_more_values value
        {
            if (trace_select_parser) std::cout << "select parser - zero_or_more_values matching one_or_more_values value." << std::endl;
            $1->push_back($2);
            $$ = $1;
        }
;

value:
    KW_TRUE
        {
            if (trace_select_parser) std::cout << "select parser - value matching KW_TRUE." << std::endl;

            $$ = new JSON_select_value(JSON_select_value_type::true_keyword);
        }
    | KW_FALSE
        {
            if (trace_select_parser) std::cout << "select parser - value matching KW_FALSE." << std::endl;
            $$ = new JSON_select_value(JSON_select_value_type::false_keyword);
        }
    | KW_NULL
        {
            if (trace_select_parser) std::cout << "select parser - value matching KW_NULL." << std::endl;
            $$ = new JSON_select_value(JSON_select_value_type::null_keyword);
        }
    | QUOTED_STRING
        {
            if (trace_select_parser) std::cout << "select parser - value matching QUOTED_STRING =>" << *($1) << "<=" << std::endl;
            $$ = new JSON_select_value(JSON_select_value_type::quoted_string,$1);
        }
    | BARE_JSON_NUMBER
        {
            if (trace_select_parser) std::cout << "select parser - value matching bare_JSON_number =>" << *($1) << "<=" << std::endl;
            $$ = new JSON_select_value(JSON_select_value_type::bare_JSON_number,$1);
        }
;

%%

// C++ code section of parser

namespace xx_
{
	void select::error(location const &loc, const std::string& s)
	{
		std::cout << "<Error> at " << loc << " invalid LUN attribute select expression - " << s << std::endl;
		context_ref.unsuccessful_compile = true;
	}
}
