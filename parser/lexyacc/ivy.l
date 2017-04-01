
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

# include <cstdlib>
# include <strings.h> // for index()
# include "Ivy_pgm.h"
# include "ivydefines.h"
# include "ivy.parser.hh"
#include <iostream>

#define YY_DECL int yylex(yy::ivy::semantic_type *yylval, yy::ivy::location_type *yylloc,  Ivy_pgm&  context_ref)

// make location include the current token

# define YY_USER_ACTION yylloc->columns (yyleng);

typedef yy::ivy::token token;


int ivyscript_srclineno=0;
int ivyscript_string_start_line;

#define MAX_TRAILING_PERCENT_TOKEN_SIZE 80

char trail_pct_buf[MAX_TRAILING_PERCENT_TOKEN_SIZE];

%}

ws         [ \t]+
identifier [a-zA-Z一-龯ぁ-んァ-ン]([a-zA-Z0-9一-龯ぁ-んァ-ン]|_)*
integer_trailing_percent    [0-9]+[ \t]*%
integer    [0-9]+
hex_integer 0x[0-9a-fA-F]+
float_trailing_percent      ((([0-9]+\.[0-9]*)|([0-9]*\.[0-9]+)([eE][+-]?[0-9]{1,3})?)|([0-9]+[eE][+-]?[0-9]{1,3}))[ \t]*%
float      (([0-9]+\.[0-9]*)|([0-9]*\.[0-9]+)([eE][+-]?[0-9]{1,3})?)|([0-9]+[eE][+-]?[0-9]{1,3})
zero255    ([0-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-5])
dottedquad_ip_addr {zero255}\.{zero255}\.{zero255}\.{zero255}

%x	STRING STRING_ESCAPE inklude

%%

 // rules for C++-compatible scanner
%{
	// start where previous token ended
	yylloc->step();
%}

include	BEGIN(inklude);

<inklude>[ \t]*	/*ignore whitespace before filename */
<inklude>[^ \t\n\r]+	{
		yyin = fopen(yytext, "r");
		if (!yyin) {
			context_ref.error( std::string("Failed trying to open include file \"") + yytext + std::string("\".\n") );
			return 0;
		}
		yypush_buffer_state(yy_create_buffer( yyin, YY_BUF_SIZE));
		BEGIN(INITIAL);
	}

<STRING,STRING_ESCAPE><<EOF>>	{
		context_ref.error("unexpected end-of-file looking for end of quoted string.");
		return 0;
	}

<STRING>"\r\n"|"\r"|"\n"	{
			ivyscript_srclineno++;
			yylloc->lines(1);
			(*yylval->p_str)+=yytext;
		}

<STRING>"\\"	BEGIN STRING_ESCAPE;

<STRING>"\""	{
		BEGIN INITIAL;

    /*debug*/if(trace_lexer) std::cout << std::endl << "lexer returning STRING_CONSTANT \"" << *yylval->p_str << "\"." << std::endl;

		return token::STRING_CONSTANT;
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
\v  { ivyscript_srclineno++; yylloc->lines(1); }

"/*"	{
		int c;
		for (;;)
		{
			if (EOF == (c=yyinput()))
			{
				context_ref.error("unexpected EOF in comment");
				return 0;
			}

			if ('\r' == c)
			{
				if (EOF == (c=yyinput()))
				{
					context_ref.error("unexpected EOF in comment");
					return 0;
				}
				if ('\n' != c)
					unput(c);
				ivyscript_srclineno++;
				yylloc->lines(1);
			}
			else if ('\n' == c)
			{
                yylloc->lines(1);
				ivyscript_srclineno++;
            }
			else if ('*' == c)
			{
				while ('*' == (c=yyinput())) ;
				if (EOF == c)
				{
					context_ref.error("unexpected EOF in comment");
					return 0;
				}
				if ('/' == c)
					break;
			}
		}
	}

"//"	{
		/* C++ style comments */
		int c;
		for (;;)
		{
			if (EOF == (c = yyinput()))
			{
				return 0;
			}
			if ('\r' == c)
			{
				ivyscript_srclineno++;
				yylloc->lines(1);
				if (EOF == (c=yyinput()))
					return 0;
				if ('\n' != c)
					unput(c);
				break;
			}
			else if ('\n' == c)
			{
				ivyscript_srclineno++;
				yylloc->lines(1);
				break;
			}
			else if ('\v' == c)
			{
				ivyscript_srclineno++;
				yylloc->lines(1);
				break;
			}
		}
	}

#	{
		/* comments starting with # */
		int c;
		for (;;)
		{
			if (EOF == (c = yyinput()))
			{
				return 0;
			}
			if ('\r' == c)
			{
				ivyscript_srclineno++;
				yylloc->lines(1);
				if (EOF == (c=yyinput()))
					return 0;
				if ('\n' != c)
					unput(c);
				break;
			}
			else if ('\n' == c)
			{
				ivyscript_srclineno++;
				yylloc->lines(1);
				break;
			}
			else if ('\v' == c)
			{
				ivyscript_srclineno++;
				yylloc->lines(1);
				break;
			}
		}
	}

\"	{
		yylval->p_str = new std::string;
		if (!yylval->p_str) {
			context_ref.error("out of memory in lexer");
			exit(1);
		}
		BEGIN STRING;
	}

\[\ *[tT][oO]\ *(\]) {
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_TO" << std::endl; return token::KW_TO; }

\[\ *[hH][oO][sS][tT][sS]\ *(\]) {
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_HOSTS" << std::endl; return token::KW_HOSTS; }

\[\ *[oO][uU][tT][pP][uU][tT]\ *[fF][oO][lL][dD][eE][rR]\ *[rR][oO][oO][tT]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_OUTPUT_FOLDER_ROOT" << std::endl; return token::KW_OUTPUT_FOLDER_ROOT; }

\[\ *[sS][eE][tT]\ *[gG][eE][nN][eE][rR][aA][tT][oO][rR]\ *[tT][eE][mM][pP][lL][aA][tT][eE]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_SET_IOSEQUENCER_TEMPLATE" << std::endl; return token::KW_SET_IOSEQUENCER_TEMPLATE; }

\[\ *[sS][eE][tT]\ *[iI][oO][gG][eE][nN][eE][rR][aA][tT][oO][rR]\ *[tT][eE][mM][pP][lL][aA][tT][eE]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_SET_IOSEQUENCER_TEMPLATE" << std::endl; return token::KW_SET_IOSEQUENCER_TEMPLATE; }

\[\ *[sS][eE][tT]\ *[iI][oO][gG][eE][nN][eE][rR][aA][tT][oO][rR]\ *[dD][eE][fF][aA][uU][lL][tT]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_SET_IOSEQUENCER_TEMPLATE" << std::endl; return token::KW_SET_IOSEQUENCER_TEMPLATE; }

\[\ *[cC][rR][eE][aA][tT][eE]\ *[wW][oO][rR][kK][lL][oO][aA][dD]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_CREATE_WORKLOAD" << std::endl; return token::KW_CREATE_WORKLOAD; }

\[\ *[dD][eE][lL][eE][tT][eE]\ *[wW][oO][rR][kK][lL][oO][aA][dD]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_DELETE_WORKLOAD" << std::endl; return token::KW_DELETE_WORKLOAD; }

\[\ *[sS][eE][lL][eE][cC][tT]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_SELECT" << std::endl; return token::KW_SELECT; }

\[\ *[iI][oO][sS][eE][qQ][uU][eE][nN][cC][eE][rR]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_IOSEQUENCER" << std::endl; return token::KW_IOSEQUENCER; }

\[\ *[iI][oO][gG][eE][nN][eE][rR][aA][tT][oO][rR]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_IOSEQUENCER" << std::endl; return token::KW_IOSEQUENCER; }

\[\ *[pP][aA][rR][aA][mM][eE][tT][eE][rR][sS]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_PARAMETERS" << std::endl; return token::KW_PARAMETERS; }

\[\ *[cC][rR][eE][aA][tT][eE]\ *[rR][oO][lL][lL][uU][pP]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_CREATE_ROLLUP" << std::endl; return token::KW_CREATE_ROLLUP; }

\[\ *[nN][oO][cC][sS][vV]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_NOCSV" << std::endl; return token::KW_NOCSV; }

\[\ *[qQ][uU][aA][nN][tT][iI][tT][yY]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_QUANTITY" << std::endl; return token::KW_QUANTITY; }

\[\ *[mM][aA][xX]\ *[dD][rR][oO][oO][pP]\ *[mM][aA][xX]\ *[tT][oO]\ *[mM][iI][nN]\ *[iI][oO][pP][sS]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_MAX_DROOP_MAX_TO_MIN_IOPS" << std::endl; return token::KW_MAX_DROOP_MAX_TO_MIN_IOPS; }

\[\ *[mM][aA][xX]\ *[iI][oO][pP][sS]\ *[dD][rR][oO][oO][pP]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_MAX_DROOP_MAX_TO_MIN_IOPS" << std::endl; return token::KW_MAX_DROOP_MAX_TO_MIN_IOPS; }

\[\ *[mM][aA][xX]\ *[dD][rR][oO][oO][pP]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_MAX_DROOP_MAX_TO_MIN_IOPS" << std::endl; return token::KW_MAX_DROOP_MAX_TO_MIN_IOPS; }

\[\ *[eE][dD][iI][tT]\ *[rR][oO][lL][lL][uU][pP]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_EDIT_ROLLUP" << std::endl; return token::KW_EDIT_ROLLUP; }

\[\ *[gG][oO](!)?\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_GO" << std::endl; return token::KW_GO; }

\[\ *[dD][eE][lL][eE][tT][eE]\ *[rR][oO][lL][lL][uU][pP]\ *\] 	{
    /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_DELETE_ROLLUP" << std::endl; return token::KW_DELETE_ROLLUP; }


"int"       { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_INT" << std::endl; return token::KW_INT; }
"double"    { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_DOUBLE" << std::endl; return token::KW_DOUBLE; }
"string"    { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_STRING" << std::endl; return token::KW_STRING; }
"if"        { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_IF" << std::endl; return token::KW_IF; }
"then"      { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_THEN" << std::endl; return token::KW_THEN; }
"else"      { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_ELSE" << std::endl; return token::KW_ELSE; }
"for"       { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_FOR" << std::endl; return token::KW_FOR; }
"while"     { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_WHILE" << std::endl; return token::KW_WHILE; }
"return"    { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_RETURN" << std::endl; return token::KW_RETURN; }
"static"    { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_STATIC" << std::endl; return token::KW_STATIC; }
"const"     { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_CONST" << std::endl; return token::KW_CONST; }
"do"        { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_DO" << std::endl; return token::KW_DO; }
"to"        { /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning KW_TO" << std::endl; return token::KW_TO; }

{identifier}	{
		yylval->p_str = new std::string(yytext);
		/*debug*/if (trace_lexer) {std::cout << std::endl << "lexer returning ID = \"" << (*(yylval->p_str)) << "\"." << std::endl;}
		return token::ID;
	}

{integer}	{
			yylval->intval=atoi(yytext);
		/*debug*/if (trace_lexer) {std::cout << std::endl << "lexer returning INT_CONSTANT = " << yylval->intval << "." << std::endl;}
			return token::INT_CONSTANT;
		}

{integer_trailing_percent}  {

        char* p = yytext;
        unsigned int count = (unsigned int) yyleng;

        if (count >= MAX_TRAILING_PERCENT_TOKEN_SIZE) context_ref.error("size of numeric constant with trailing percent exceeds maximum allowed length.");

        p += count;

        count--; /* count without trailing percent */
        p--;     /*point to % */
        p--;     /*point to last char before % */

        while ( ((*p) == ' ') || ((*p) == '\t') )
        {
            p--;
            count--;
        }

        p = yytext;
        char* q = trail_pct_buf;

        while (count)
        {
            *q++ = *p++;
            count--;
        }
        *q = 0;

        yylval->dblval=atof(trail_pct_buf);
        yylval->dblval /= 100.;
		/*debug*/if (trace_lexer) {std::cout << std::endl << "lexer returning DOUBLE_CONSTANT = " << yylval->dblval << "." << std::endl;}
		return token::DOUBLE_CONSTANT;
    }


{float_trailing_percent}  {

        char* p = yytext;
        unsigned int count = (unsigned int) yyleng;

        if (count >= MAX_TRAILING_PERCENT_TOKEN_SIZE) context_ref.error("size of numeric constant with trailing percent exceeds maximum allowed length.");

        p += count;

        count--; /* count without trailing percent */
        p--;     /*point to % */
        p--;     /*point to last char before % */

        while ( ((*p) == ' ') || ((*p) == '\t') )
        {
            p--;
            count--;
        }

        p = yytext;
        char* q = trail_pct_buf;

        while (count)
        {
            *q++ = *p++;
            count--;
        }
        *q = 0;

        yylval->dblval=atof(trail_pct_buf);
        yylval->dblval /= 100.;
		/*debug*/if (trace_lexer) {std::cout << std::endl << "lexer returning DOUBLE_CONSTANT = " << yylval->dblval << "." << std::endl;}
		return token::DOUBLE_CONSTANT;
    }


{hex_integer}   {

    /* e.g. 0x5E3 */

    /* Note that a decimal digit "integer" constant does not have a sign - zero or positive. */

    /* In ivyscript, a negative integer like -5 is parsed as an expression with */
    /* a unary - operator followed by an integer constant.*/

    /* We could come back later and figure out something better later, but for now */
    /* hex_integer constants can only represent the same, non-negative values. */

    /* In other words, we don't allow a hex literal to represent a negative integer. */

    /* Also - note that for now the parser uses yylval->intval is a C++ "int", not an ivy_int ... */

		unsigned int max_digits = 2 * sizeof(int);

		unsigned int hex_digits=0;

		for (const char* p=(yytext+2); (*p); p++)
		{
            hex_digits++;
		}

		if (hex_digits > max_digits)
		{
            std::ostringstream o;
            o << "Parse error for hex integer constant " << yytext << " - too many hex digits.  Max hex digits is " << max_digits << ".";
            context_ref.error(o.str());
            return 0;
		}

		int result = 0;

		const char* hex_digit_template="0123456789ABCDEFabcdef";

		int hex_digit = 0;

		for (const char* p=(yytext+2); (*p); p++)
		{
            hex_digit = index(hex_digit_template,*p)-hex_digit_template;
			if (hex_digit>15) hex_digit-=6;

			if ( (p == (yytext+2)) && (hex_digits==max_digits) && (hex_digit > 8) )
            {
                std::ostringstream o;
                o << "Parse error for hex integer constant " << yytext << " - may not represent a negative number.";
                context_ref.error(o.str());
                return 0;
            }

			result<<=4;

			result +=hex_digit;
		}

		yylval->intval=result;

		/*debug*/if (trace_lexer) {std::cout << std::endl << "lexer returning INT_CONSTANT = " << yylval->intval << " for hex_integer literal " << yytext << "." << std::endl;}

		return token::INT_CONSTANT;
    }

{float}	{
		yylval->dblval=atof(yytext);
		/*debug*/if (trace_lexer) {std::cout << std::endl << "lexer returning DOUBLE_CONSTANT = " << yylval->dblval << "." << std::endl;}
		return token::DOUBLE_CONSTANT;
	}

{dottedquad_ip_addr} {
		yylval->p_str = new std::string(yytext);
		/*debug*/if (trace_lexer) {std::cout << std::endl << "lexer returning DOTTED_QUAD_IP_ADDR = \"" << (*(yylval->p_str)) << "\"." << std::endl;}
		return token::DOTTED_QUAD_IP_ADDR;
	}

"\+"	{yylval->p_chars="+"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning PLUS (\'" << yylval->p_chars << "\')." << std::endl; return token::PLUS;}
"-" 	{yylval->p_chars="-"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning MINUS (\'" << yylval->p_chars << "\')." << std::endl; return token::MINUS;}
"\*"	{yylval->p_chars="*"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning MULT (\'" << yylval->p_chars << "\')." << std::endl; return token::MULT;}
"/" 	{yylval->p_chars="/"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning DIV (\'" << yylval->p_chars << "\')." << std::endl; return token::DIV;}
"%" 	{yylval->p_chars="%"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning REMAINDER (\'" << yylval->p_chars << "\')." << std::endl; return token::REMAINDER;}
"=="	{yylval->p_chars="=="; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning EQ (\'" << yylval->p_chars << "\')." << std::endl; return token::EQ;}
"!="	{yylval->p_chars="!="; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning NE (\'" << yylval->p_chars << "\')." << std::endl; return token::NE;}
"<" 	{yylval->p_chars="<"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning LT (\'" << yylval->p_chars << "\')." << std::endl; return token::LT;}
">" 	{yylval->p_chars=">"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning GT (\'" << yylval->p_chars << "\')." << std::endl; return token::GT;}
"<="	{yylval->p_chars="<="; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning LE (\'" << yylval->p_chars << "\')." << std::endl; return token::LE;}
">="	{yylval->p_chars=">="; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning GE (\'" << yylval->p_chars << "\')." << std::endl; return token::GE;}
"=" 	{yylval->p_chars="="; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning ASSIGN (\'" << yylval->p_chars << "\')." << std::endl; return token::ASSIGN;}
"->"	{yylval->p_chars="->"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning POINTSTO) (\'" << yylval->p_chars << "\')." << std::endl; return token::POINTSTO;}
"&&" 	{yylval->p_chars="&&"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning LOGICAL_AND (\'" << yylval->p_chars << "\')." << std::endl; return token::LOGICAL_AND;}
"||" 	{yylval->p_chars="||"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning LOGICAL_OR (\'" << yylval->p_chars << "\')." << std::endl; return token::LOGICAL_OR;}
"&" 	{yylval->p_chars="&"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning BITWISE_AND (\'" << yylval->p_chars << "\')." << std::endl; return token::BITWISE_AND;}
"|" 	{yylval->p_chars="|"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning BITWISE_OR (\'" << yylval->p_chars << "\')." << std::endl; return token::BITWISE_OR;}
"^" 	{yylval->p_chars="^"; /*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning BITWISE_XOR (\'" << yylval->p_chars << "\')." << std::endl; return token::BITWISE_XOR;}

.	/*debug*/ if (trace_lexer) std::cout << std::endl << "lexer returning \'" << yytext[0] << "\'." << std::endl; return yytext[0];

<<EOF>> {
            yypop_buffer_state();

            if ( !YY_CURRENT_BUFFER )
            {
                return token::END_OF_FILE;
            }

        }

%%

