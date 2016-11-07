%option prefix="xx_"
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

    // A JSON number is recognized by and returned to the parser as such by the lexer.

    // A JSON number starts with an integer portion consisting of an optional minus sign and
    // one or more digits with no leading zero optionally followed by a fraction portion with
    // a decimal point and one or more digits optionally followed by an exponent portion
    // consisting of the letter e and one or more digits.


# include <iostream>
# include "JSON_select.h"
# include "select.parser.hh"

#define YY_DECL int yylex(xx_::select::semantic_type *yylval, xx_::select::location_type *yylloc,  JSON_select&  context_ref)

bool trace_select_lexer {false};

// make location include the current token

# define YY_USER_ACTION yylloc->columns (yyleng);

typedef xx_::select::token token;

int select_srclineno=0;
int select_string_start_line;

%}

ws         [ \t]+

bare_JSON_number    ((-)?[1-9][0-9]*)|((-)?[1-9][0-9]*\.[0-9]+)|((-)?[1-9][0-9]*e[0-9]+)|((-)?[1-9][0-9]*\.[0-9]+e[0-9]+)

true    [tT][rR][uU][eE]
false   [fF][aA][lL][sS][eE]
null    [nN][uU][lL][lL]

identifier [a-zA-Z]([a-zA-Z0-9]|_)*

hostrange [a-zA-Z]([a-zA-Z0-9]|_)*[0-9]-[0-9]+

float      (([0-9]+\.[0-9]*)|([0-9]*\.[0-9]+)([eE][+-]?[0-9]{1,3})?)|([0-9]+[eE][+-]?[0-9]{1,3})

zero255    ([0-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-5])
dottedquad_ip_addr {zero255}\.{zero255}\.{zero255}\.{zero255}

one256    ([0]?[1-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-6])
pg        {one256}-{one256}

ldev      [0-9a-fA-F][0-9a-fA-F](:)?[0-9a-fA-F][0-9a-fA-F]
ldev_range {ldev}-{ldev}

%x	STRING STRING_ESCAPE

%%

 // rules for C++-compatible scanner
%{
	// start where previous token ended
	yylloc->step();
%}


<STRING,STRING_ESCAPE><<EOF>>	{
		context_ref.error("unexpected end-of-file looking for end of quoted string.");
		return 0;
	}

<STRING>"\r\n"|"\r"|"\n"	{
			select_srclineno++;
			yylloc->lines(1);
			(*yylval->p_str)+=yytext;
		}

<STRING>"\\"	BEGIN STRING_ESCAPE;

<STRING>"\""	{
		BEGIN INITIAL;
		if (trace_select_lexer) std::cout << "select lexer returning QUOTED_STRING =>" << *(yylval->p_str) << "<=" << std::endl;
		return token::QUOTED_STRING;
	}

<STRING>.	(*yylval->p_str)+= yytext;

<STRING_ESCAPE>n	{ (*yylval->p_str)+='\n'; BEGIN STRING; }

<STRING_ESCAPE>r	{ (*yylval->p_str)+='\r'; BEGIN STRING; }

<STRING_ESCAPE>t	{ (*yylval->p_str)+='\t'; BEGIN STRING; }

<STRING_ESCAPE>[0-7]{1,3}	{
		int c=0;
		for (char* p=yytext; (*p); p++)
		{
			c<<=3;
			c+=(*p)-'0';
		}
		(*yylval->p_str)+=(char)c;
		BEGIN STRING;
	}


<STRING_ESCAPE>x[0-9a-fA-F]{1,2}	{
		int c=0;
		int nybble;
		const char* hex_digits="0123456789ABCDEFabcdef";
		for (const char* p=(yytext+1); (*p); p++)
		{
			c<<=4;
			nybble=index(hex_digits,*p)-hex_digits;
			if (nybble>15) nybble-=6;
			c+=nybble;
		}
		(*yylval->p_str)+=(char)c;
		BEGIN STRING;
	}

<STRING_ESCAPE>.	{ (*yylval->p_str)+= yytext[0]; BEGIN STRING; }

{ws}	yylloc->step(); /* skip over white space */

\r\n	|
\r	|
\n	|
\v  { select_srclineno++; yylloc->lines(1); }

\"	{
		yylval->p_str = new std::string;
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		BEGIN STRING;
	}

{true} {
		if (trace_select_lexer) std::cout << "select lexer returning KW_TRUE" << std::endl;
        return token::KW_TRUE;
    }

{false} {
		if (trace_select_lexer) std::cout << "select lexer returning KW_FALSE" << std::endl;
        return token::KW_FALSE;
    }

{null} {
		if (trace_select_lexer) std::cout << "select lexer returning KW_NULL" << std::endl;
        return token::KW_NULL;
    }

{bare_JSON_number} {
		yylval->p_str = new std::string(yytext);
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		if (trace_select_lexer) std::cout << "select lexer returning BARE_JSON_NUMBER = " << *(yylval->p_str) << std::endl;
		return token::BARE_JSON_NUMBER;
    }

{identifier} {
		yylval->p_str = new std::string(yytext);
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		if (trace_select_lexer) std::cout << "select lexer returning QUOTED_STRING for bare identifier = " << *(yylval->p_str) << std::endl;
		return token::QUOTED_STRING;
    }

{hostrange} {
		yylval->p_str = new std::string(yytext);
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		if (trace_select_lexer) std::cout << "select lexer returning QUOTED_STRING for bare hostrange = " << *(yylval->p_str) << std::endl;
		return token::QUOTED_STRING;
    }

{float} {
		yylval->p_str = new std::string(yytext);
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		if (trace_select_lexer) std::cout << "select lexer returning QUOTED_STRING for bare float value = " << *(yylval->p_str) << std::endl;
		return token::QUOTED_STRING;
    }

{dottedquad_ip_addr} {
		yylval->p_str = new std::string(yytext);
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		if (trace_select_lexer) std::cout << "select lexer returning QUOTED_STRING for IPV4 dotted quad = " << *(yylval->p_str) << std::endl;
		return token::QUOTED_STRING;
    }

{pg} {
		yylval->p_str = new std::string(yytext);
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		if (trace_select_lexer) std::cout << "select lexer returning QUOTED_STRING for parity group = " << *(yylval->p_str) << std::endl;
		return token::QUOTED_STRING;
    }


{ldev} {
		yylval->p_str = new std::string(yytext);
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		if (trace_select_lexer) std::cout << "select lexer returning QUOTED_STRING for LDEV = " << *(yylval->p_str) << std::endl;
		return token::QUOTED_STRING;
    }

{ldev_range} {
		yylval->p_str = new std::string(yytext);
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		if (trace_select_lexer) std::cout << "select lexer returning QUOTED_STRING for LDEV range = " << *(yylval->p_str) << std::endl;
		return token::QUOTED_STRING;
    }

.	{
        char c = yytext[0];
		if (trace_select_lexer) std::cout << "select lexer returning bare character =>" << c << "<=" << std::endl;
        return c;
    }

%%

