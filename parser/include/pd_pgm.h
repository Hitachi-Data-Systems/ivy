//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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
