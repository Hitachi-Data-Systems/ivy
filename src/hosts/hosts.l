%option prefix="xy_"
%option noyywrap

%{
/*
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
*/

# include <iostream>
# include "hosts_list.h"
# include "hosts.parser.hh"

#define YY_DECL int yylex(xy_::hosts::semantic_type *yylval, xy_::hosts::location_type *yylloc,  hosts_list&  context_ref)

bool trace_hosts_lexer {false};

// make location include the current token

# define YY_USER_ACTION yylloc->columns (yyleng);

typedef xy_::hosts::token token;

int hosts_srclineno=0;
int hosts_string_start_line;

%}

ws         [ \t]+

hostname [a-zA-Z]([-_a-zA-Z0-9])*

unsigned_int [0-9]+

zero255    ([0-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-5])
dottedquad_ip_addr {zero255}\.{zero255}\.{zero255}\.{zero255}

%%

 // rules for C++-compatible scanner
%{
	// start where previous token ended
	yylloc->step();
%}


{ws}	yylloc->step(); /* skip over white space */

\r\n	|
\r	|
\n	|
\v  { hosts_srclineno++; yylloc->lines(1); }


{dottedquad_ip_addr} {
		yylval->p_str = new std::string(yytext);
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		if (trace_hosts_lexer) std::cout << "hosts lexer returning IPV4_DOTTED_QUAD = " << *(yylval->p_str) << std::endl;
		return token::IPV4_DOTTED_QUAD;
    }

{hostname} {
		yylval->p_str = new std::string(yytext);
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		if (trace_hosts_lexer) std::cout << "hosts lexer returning HOSTNAME = " << *(yylval->p_str) << std::endl;
		return token::HOSTNAME;
    }

{unsigned_int} {
		yylval->p_str = new std::string(yytext);
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		if (trace_hosts_lexer) std::cout << "hosts lexer returning UNSIGNED_INTEGER = " << *(yylval->p_str) << std::endl;
		return token::UNSIGNED_INTEGER;
    }


 .	{
		if (trace_hosts_lexer) { std::cout << "hosts lexer returning character \'" << yytext[0] << "\'" << std::endl; }

        return yytext[0];
    }

<<EOF>> return token::END_OF_FILE;

%%

