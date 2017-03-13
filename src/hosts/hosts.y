
%define api.prefix {xy_}
%language "C++"
%defines
%locations
%define parser_class_name {hosts}

%{

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

#include <iostream>
#include <sstream>
#include <cassert>
#include <list>

#include "hosts_list.h"

using namespace std;

bool trace_hosts_parser {false};

extern bool hostname_hyphen;

%}

%parse-param { hosts_list&  context_ref }
%lex-param { hosts_list&  context_ref }

%union {
	std::string* p_str;
};

/* declare tokens */

%token <p_str> IPV4_DOTTED_QUAD IDENTIFIER UNSIGNED_INTEGER
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
    | host_list IDENTIFIER '-' UNSIGNED_INTEGER
        {
            if (trace_hosts_parser) std::cout<<"hosts_list parser host_list IDENTIFIER=" << (*($2)) << " \'-\' UNSIGNED_INTEGER=" << (*($4)) << std::endl;

            if (hostname_hyphen)
            {
                context_ref.hosts.push_back
                (
                    (*($2))
                      +
                    std::string("-")
                      +
                    (*($4))
                );

                delete $2;
                delete $4;
            }
            else
            {

                {
                    std::string& id = *($2);  // we delete the strings after the life of these references
                    std::string& ui = *($4);

                    unsigned int suffix_length {0};
                    for (unsigned int i = id.size()-1; i>0; i--)
                    {
                        if (!isdigit(id[i])) break;
                        suffix_length++;
                    }
                    std::string id_base = id.substr(0,id.size()-suffix_length);
                    std::string suffix = id.substr(id.size()-suffix_length,suffix_length);
                    unsigned int first, last;
                    {
                        std::istringstream i(suffix);
                        i >> first;
                    }
                    {
                        std::istringstream i(ui);
                        i >> last;
                    }
                    if (first >= last)
                    {
                        context_ref.unsuccessful_compile=true;
                        std::ostringstream o;
                        o << "<Error> Invalid numbered hostname range - in \"" << id << "-" << ui << "\", the starting host number is must be smaller than the ending number." << std::endl;
                        context_ref.message += o.str();
                    }

                    for (unsigned int i=first; i<=last; i++)
                    {
                        std::ostringstream o;
                        o << id_base << i;
                        context_ref.hosts.push_back(o.str());
                    }
                }
                context_ref.has_hostname_range = true;
                delete $2; delete $4;
            }
        }
    | host_list IDENTIFIER
        {
            if (trace_hosts_parser) std::cout<<"hosts_list parser host_list IDENTIFIER=" << (*($2)) << std::endl;
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
		std::cout << "<Error> at " << loc << " invalid list of hostnames - " << s << std::endl;
		std::cout << "A valid list of hostnames looks like \"winter, host16-18, 192.168.0.0\" which means the same as \"winter, host16, host17, host18, 192.168.0.0\"." << std::endl;
		context_ref.unsuccessful_compile = true;
	}
}
