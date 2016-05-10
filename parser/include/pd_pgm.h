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
#ifndef included_pd_pgm_h
#define included_pd_pgm_h

#include <math.h>
#include <iostream.h>
#include <fstream.h>
#include <iomanip.h>

#define INDENT 3
#define STACKSIZE 4096
#define PI 3.14159265358979

int CountCarriageReturns(const char* s)
{
	int lines=0;
	if (!s) return 0;
	const char*p=s;
	while (*p)
	{
		if ('\r'==(*p))
		{
			lines++;
			if ('\n'==(*(p+1))) p++;
		} else if ('\n'==(*p))
		{
			lines++;
		}
		p++;
	}
	return lines;
}

void init_KeywordTable();


void yyerror(const char*);
int yylex(void);
int yywrap();
int yyparse (void*);

// Ideally, the following lex things should be by document,
// but we _probably_ only open one document at a time, and
// making a custom version of lex is low priority compared
// to other features ;-)

extern int srclineno;
extern int string_start_line;
extern FILE* yyin;

#ifdef _DEBUG
extern int yydebug;
#endif


#endif
