
%define api.prefix {xy_}
%language "C++"
%defines
%locations
%define parser_class_name {hosts}

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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>, Kumaran Subramaniam <kumaran.subramaniam@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.

#include <iostream>
#include <sstream>
#include <cassert>
#include <list>

#include "hosts_list.h"

using namespace std;

bool trace_hosts_parser {false};

%}

%parse-param { hosts_list&  context_ref }
%lex-param { hosts_list&  context_ref }

%union {
	std::string* p_str;
};

/* declare tokens */

%token <p_str> IPV4_DOTTED_QUAD HOSTNAME UNSIGNED_INTEGER
%token END_OF_FILE

%type <not_used> host_list

%{
extern int yylex(xy_::hosts::semantic_type *xy_lval, xy_::hosts::location_type* xy_lloc,  hosts_list&  context_ref);
void myout(int val, int radix);
%}

%initial-action {
 // Nothing for now
}

%%

program:

	{
        if (trace_hosts_parser) std::cout << "hosts_list parser starting" << std::endl;
	}

	host_list END_OF_FILE
        {
            if (trace_hosts_parser) std::cout << "hosts_list parser program: statements - now going to YYACCEPT." << std::endl;

            if (!context_ref.unsuccessful_compile) context_ref.successful_compile = true;

            YYACCEPT;
        }
;

host_list:
        {
            if (trace_hosts_parser) std::cout << "hosts_list parser empty host_list" << std::endl;
        }
    | host_list ','
        {
            if (trace_hosts_parser) std::cout << "hosts_list parser host_list \',\'" << std::endl;
        }
    | host_list HOSTNAME '[' UNSIGNED_INTEGER '-' UNSIGNED_INTEGER ']'
        {
            if (trace_hosts_parser) {std::cout << "hosts_list parser host_list HOSTNAME=" << (*($2)) << " \'[\' UNSIGNED_INTEGER=" << (*($4)) << " \'-\' UNSIGNED_INTEGER=" << (*($6)) << " \']\'" << std::endl; }

            {
                std::string& base = *($2);  // we delete the strings after the life of these references
                std::string& from = *($4);
                std::string& to   = *($6);

                unsigned int from_length = from.size();
                    // there may be leading zeros, if so, we make sure the generated numbers have at least the same number of digits as the "from" value by adding leading zeros as necessary

                unsigned int first, last;
                {
                    std::istringstream i(from);
                    i >> first;
                }
                {
                    std::istringstream i(to);
                    i >> last;
                }
                if (first >= last)
                {
                    context_ref.unsuccessful_compile=true;
                    std::ostringstream o;
                    o << "<Error> Invalid numbered hostname range - in \"" << base << "[" << from << "-" << to << "]\", the starting host number must be smaller than the ending number." << std::endl;
                    context_ref.message += o.str();
                }

                for (unsigned int i=first; i<=last; i++)
                {
                    {  // extra block to get fresh newly initialized variables.

                        std::ostringstream o;
                        o << i;
                        std::string suffix = o.str();
                        while (suffix.size() < from_length)
                        {
                            suffix = std::string("0") + suffix;
                        }

                        context_ref.hosts.push_back(base+suffix);
                    }
                }
            }
            context_ref.has_hostname_range = true;
            delete $2; delete $4; delete $6;
        }
    | host_list HOSTNAME
        {
            if (trace_hosts_parser) std::cout<<"hosts_list parser host_list HOSTNAME=" << (*($2)) << std::endl;
            context_ref.hosts.push_back(*($2));
            delete $2;
        }
    | host_list IPV4_DOTTED_QUAD
        {
            if (trace_hosts_parser) std::cout<<"hosts_list parser host_list IPV4_DOTTED_QUAD=" << (*($2)) << std::endl;
            context_ref.hosts.push_back(*($2));
            delete $2;
        }
;

%%

// C++ code section of parser

namespace xy_
{
	void hosts::error(location const &loc, const std::string& s)
	{
		std::cout << "<Error> at " << loc << " invalid list of hostnames - \"" << s << "\"" << std::endl << std::endl;

		context_ref.unsuccessful_compile = true;
	}
}
