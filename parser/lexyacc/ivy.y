
%language "C++"
%defines
%locations
%define parser_class_name "ivy"

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
#include <cassert>

#include "SelectClause.h"
#include "Select.h"
#include "Xpr.h"
#include "host_range.h"
#include "Ivy_pgm.h"
#include "Arg.h"
#include "Stmt.h"
#include "Stmt_nested_Block.h"
#include "Type.h"
#include "ivyhelpers.h"
#include "master_stuff.h"

void setup_output_folder(std::string*);

using namespace std;

%}

%parse-param { Ivy_pgm&  context_ref }
%lex-param { Ivy_pgm&  context_ref }

%union {
    bool heleboel;
    int not_used;
	enum basic_types typeval;
	unsigned int flags;
	double dblval;
	int intval;
	unsigned int uintval;
	const char* p_chars;
	std::string* p_str;
	Arg* p_arg;
	Xpr* p_xpr;
	arglist* p_arglist;
	SymbolTableEntry* p_STE;
	Stmt* p_stmt;
	std::list<Xpr*>* p_xprlist;
	std::list<std::string>* p_stringlist;
	std::pair<const std::string,SymbolTableEntry>* p_STE_pair;
	Frame* p_Frame;
	std::list<host_spec*>* p_host_spec_pointer_list;

	SelectClauseValue* p_SelectClauseValue;
	std::list<SelectClauseValue*>* p_SelectClauseValue_pointer_list;

	SelectClause* p_SelectClause;
	std::list<SelectClause*>* p_SelectClause_pointer_list;
	Select* p_Select;
};
/* declare tokens */

%token <intval> INT_CONSTANT
%token <dblval> DOUBLE_CONSTANT
%token <p_str> ID STRING_CONSTANT DOTTED_QUAD_IP_ADDR
%token <p_chars> LOGICAL_OR LOGICAL_AND BITWISE_OR BITWISE_XOR BITWISE_AND EQ NE LT GT LE GE PLUS MINUS MULT DIV REMAINDER ASSIGN
%token KW_INT KW_DOUBLE KW_STRING KW_IF KW_THEN KW_ELSE KW_FOR KW_WHILE KW_RETURN KW_STATIC KW_CONST KW_DO
%token KW_IS KW_TO KW_HOSTS KW_OUTPUT_FOLDER_ROOT KW_SET_IOGENERATOR_TEMPLATE KW_CREATE_WORKLOAD KW_DELETE_WORKLOAD KW_SELECT
%token KW_IOGENERATOR KW_PARAMETERS KW_CREATE_ROLLUP KW_NOCSV KW_QUANTITY KW_MAX_DROOP_MAX_TO_MIN_IOPS KW_DELETE_ROLLUP KW_EDIT_ROLLUP KW_GO

%nonassoc NOT_ELSE
%nonassoc ELSE
%nonassoc NOT_EXPRESSION
%right ASSIGN
%left LOGICAL_OR
%left LOGICAL_AND
%left BITWISE_OR
%left BITWISE_XOR
%left BITWISE_AND
%left EQ NE
%left LT GT LE GE
%left PLUS MINUS
%left MULT DIV REMAINDER
%right TYPECAST
%right UNARY POINTSTO
%left SUBSCRIPT

%type <not_used> statements decl_statement decl_body decl_item id_with_initializer function_declaration_or_definition
%type <typeval> basic_type
%type <p_STE> decl_prefix
%type <p_STE_pair> variable allocated_id
%type <p_Frame> function_declaration
%type <p_xpr> x logical_x string_x possibly_null_x int_x double_x optional_quantity optional_max_droop_max_to_min_iops optional_parameters parameters
%type <p_stmt> statement
%type <p_arg> func_arg_decl
%type <p_arglist> func_arg_decl_list func_arg_decl_nonempty_list
%type <p_xprlist> xprlist xprlist_nonempty
%type <p_host_spec_pointer_list> host_list
%type <p_SelectClauseValue> select_clause_value
%type <p_SelectClauseValue_pointer_list> select_clause_value_list
%type <p_SelectClause> select_clause
%type <p_SelectClause_pointer_list> select_clause_list
%type <p_Select> select optional_select
%type <heleboel> optional_nocsv


%{
extern int yylex(yy::ivy::semantic_type *yylval, yy::ivy::location_type* yylloc,  Ivy_pgm&  context_ref);
void myout(int val, int radix);
%}

%initial-action {
 // Filename for locations here
 @$.begin.filename = @$.end.filename = &context_ref.ivyscript_filename;
 /*debug*/if (trace_parser) {trace_ostream << "%initial-action filename " << context_ref.ivyscript_filename << "\n";}
}

%%

program:

	{
        if (trace_parser) trace_ostream << "parser: (null)" << std::endl;
	}

	statements
        {
            if (trace_parser) trace_ostream << "program: statements - now going to YYACCEPT." << std::endl;

            if (!context_ref.unsuccessful_compile) context_ref.successful_compile=true;
            YYACCEPT;
        }
;

statements:
	/* empty */
		{
            $$=0;/* not used */
            if (trace_parser) trace_ostream << "statements: (null), location " << @$ << std::endl;
		}

	| statements statement
		{
            $$=0;/* not used */
            if (trace_parser) {trace_ostream << "statements: statements statement" << std::endl; ($2)->display("",trace_ostream); trace_ostream << std::endl;}
            if ($2) context_ref.p_current_Block()->code.push_back($2);
        }

	| statements decl_statement
		{
            $$=0;/* not used */
            /*debug*/ if (trace_parser) trace_ostream << "statements: statements decl_statement" << std::endl;
		}
;


decl_statement:
	decl_prefix decl_body ';'
		{
            $$=0;/* not used */
            /*debug*/ if (trace_parser) trace_ostream << "decl_statement: decl_prefix decl_body \';\'" << std::endl;

            SymbolTableEntry* pSTE = context_ref.decl_prefix_stack.front();
            delete pSTE;  /* this is just a template used to construct possibly multiple STEs for the variables being declared */
            context_ref.decl_prefix_stack.pop_front();
            /*debug*/ if (trace_parser) context_ref.snapshot(trace_ostream);
        }

statement:
	x ';'
		{
            $$=make_Stmt_Xpr($1,0);
        }

	| '{'
            {
                context_ref.start_Block();
            }
	   statements
            {
            }
	  '}'
            {
                $$=new Stmt_nested_Block(context_ref.p_current_Block());
                context_ref.end_Block();
            }

	| KW_RETURN x ';'
		{ $$ = context_ref.make_Stmt_return(@$,$2); if (!$$) {YYERROR;} }

	| KW_IF '(' logical_x ')' statement %prec NOT_ELSE
		{ $$ = (Stmt*) new Stmt_if(@$,$3,$5); }

	| KW_IF '(' logical_x ')' statement KW_ELSE statement
		{ $$ = (Stmt*) new Stmt_if(@$,$3,$5,$7); }

	| KW_IF '(' logical_x ')' KW_THEN statement %prec NOT_ELSE
		{ $$ = (Stmt*) new Stmt_if(@$,$3,$6); }

	| KW_IF '(' logical_x ')' KW_THEN statement KW_ELSE statement
		{ $$ = (Stmt*) new Stmt_if(@$,$3,$6,$8); }

	| KW_FOR '(' possibly_null_x ';' logical_x ';' possibly_null_x ')' statement
		{
            /* NOTE: because we use possibly_null_x, we can't say for(int i=0; i<5; i=i+1) instead have to say int i; for (i=0; ...*/

            /*debug*/ if (trace_parser) {trace_ostream << "parser about to: new Stmt_for(" << std::endl;
                trace_ostream << "initializer expresssion: "; $3->display("",trace_ostream); trace_ostream << std::endl;
                trace_ostream << "logical expresssion: "; $5->display("",trace_ostream); trace_ostream << std::endl;
                trace_ostream << "post expresssion: "; $7->display("",trace_ostream); trace_ostream << std::endl;
                trace_ostream << "loop body statement: "; $9->display("",trace_ostream); trace_ostream << std::endl;
                trace_ostream << ") " << std::endl; }
            $$ = (Stmt*) new Stmt_for(@$,$3,$5,$7,$9);
        }

    | KW_FOR variable ASSIGN '{'  xprlist  '}' statement
        {
            if ($2 == nullptr || ($2->second).entry_form != STE_form::variable)
            {
                throw std::runtime_error("bison parser KW_FOR variable ASSIGN '{'  xprlist  '}' statement ; - pointer to symbol table entry is null or STE is not variable type.\n");
            }

            if ($2->second.is_const())
			{
				context_ref.error(std::string("Cannot assign to \"") + $2->first + std::string("\" - it is const."));
				YYERROR;
			}

            $$ = (Stmt*) new Stmt_for_xprlist(@$,$2,$5,$7);
        }

	| KW_WHILE '(' logical_x ')' statement
		{ $$ = (Stmt*) new Stmt_while(@$,$3,$5); }

	| KW_DO statement KW_WHILE '(' logical_x ')' ';'
		{ $$ = (Stmt*) new Stmt_do(@$,$2,$5); }

	| ';'
		{ $$=new Stmt_null(@$); }

    | KW_OUTPUT_FOLDER_ROOT STRING_CONSTANT ';'
        {
            if (m_s.have_parsed_OutputFolderRoot)
            {
                std::ostringstream o;
                o << "<Error> - two [OutputFolderRoot] statements in one program." << std::endl;
                p_Ivy_pgm->error(o.str());
            }

            m_s.have_parsed_OutputFolderRoot=true;

            m_s.outputFolderRoot = *$2;

            $$  = new Stmt_null(@$);
            if (trace_parser)
            {
                trace_ostream << "Created output folder structure at compile time, and created null statement for run time." << std::endl;
                $$->display("",trace_ostream);
            }
        }

    | KW_HOSTS host_list select ';'
        {
            if (trace_parser)
            {
                trace_ostream << "[Hosts] host_list:";
                for (auto& s: *$2) { s->display(indent_increment,trace_ostream); }
                trace_ostream << "[Hosts] select_clause_list:";
                { $3->display(indent_increment,trace_ostream); }
            }
            $$ = (Stmt*) new Stmt_hosts(@$,$2,$3);
        }

    | KW_HOSTS host_list ';'
        {
            if (trace_parser)
            {
                trace_ostream << "[Hosts] host_list:";
                for (auto& s: *$2) { s->display(indent_increment,trace_ostream); }
                trace_ostream << "[Hosts] statement - missing [Select] clause.";
            }
            $$ = (Stmt*) new Stmt_hosts(@$,$2,nullptr);
        }
    | KW_SET_IOGENERATOR_TEMPLATE string_x KW_PARAMETERS string_x ';'
        {
            $$ = new Stmt_set_iogenerator_template(@$,$2,$4);
            if (trace_parser)
            {
                $$->display("",trace_ostream);
            }
        }

    | KW_CREATE_WORKLOAD string_x optional_select KW_IOGENERATOR string_x optional_parameters ';'
        {
            $$ = new Stmt_create_workload(@$,$2,$3,$5,$6);
            if (trace_parser)
            {
                $$->display("",trace_ostream);
            }
        }

    | KW_DELETE_WORKLOAD ';'
        {
            $$ = new Stmt_delete_workload(@$,nullptr,nullptr);
            if (trace_parser)
            {
                $$->display("",trace_ostream);
            }
        }

    | KW_DELETE_WORKLOAD string_x optional_select ';'
        {
            $$ = new Stmt_delete_workload(@$,$2,$3);
            if (trace_parser)
            {
                $$->display("",trace_ostream);
            }
        }

    | KW_GO string_x ';'
        {
            $$ = new Stmt_go(@$,$2);
            if (trace_parser)
            {
                $$->display("",trace_ostream);
            }
        }

    | KW_GO ';'
        {
            $$ = new Stmt_go(@$,nullptr);
            if (trace_parser)
            {
                $$->display("",trace_ostream);
            }
        }

    | KW_CREATE_ROLLUP  string_x  optional_nocsv  optional_quantity  optional_max_droop_max_to_min_iops ';'
        {
            $$ = new Stmt_create_rollup(@$,$2,$3,$4,$5);
            if (trace_parser)
            {
                $$->display("",trace_ostream);
            }
        }

    | KW_EDIT_ROLLUP string_x KW_PARAMETERS string_x ';'
        {
            $$ = new Stmt_edit_rollup(@$,$2,$4);
            if (trace_parser)
            {
                $$->display("",trace_ostream);
            }
        }

    | KW_DELETE_ROLLUP  string_x ';'
        {
            $$ = new Stmt_delete_rollup(@$,$2);
            if (trace_parser)
            {
                $$->display("",trace_ostream);
            }
        }

    | KW_DELETE_ROLLUP ';'
        {
            $$ = new Stmt_delete_rollup(@$,nullptr);
            if (trace_parser)
            {
                $$->display("",trace_ostream);
            }
        }

	| error ';'
		{
            yyerrok;
            $$=nullptr;
        }
;


basic_type:
	KW_INT
        {
            $$=type_int;
            /*debug*/ if (trace_parser) trace_ostream << "basic_type: KW_INT" << std::endl;
        }
	| KW_DOUBLE
        {
            $$=type_double;
            /*debug*/ if (trace_parser) trace_ostream << "basic_type: KW_DOUBLE" << std::endl;
        }
	| KW_STRING
        {
            $$=type_string;
            /*debug*/ if (trace_parser) trace_ostream << "basic_type: KW_STRING" << std::endl;
        }
;

decl_prefix:
	KW_STATIC KW_CONST basic_type
		{
            $$=new SymbolTableEntry($3,context_ref.p_current_Block()->p_Frame,0,STE_flag_static+STE_flag_const);
			context_ref.decl_prefix_stack.push_front($$);
			/*debug*/ if (trace_parser) {trace_ostream << "After push_front($$):" << std::endl; int i=0; for (auto& p_STE : context_ref.decl_prefix_stack) trace_ostream << "decl_prefix_stack[" << i++ << "] = " << p_STE->info() << std::endl;}
        }

	| KW_STATIC basic_type
		{
            $$=new SymbolTableEntry($2,context_ref.p_current_Block()->p_Frame,0,STE_flag_static);
			context_ref.decl_prefix_stack.push_front($$);
			/*debug*/ if (trace_parser) {trace_ostream << "After push_front($$):" << std::endl; int i=0; for (auto& p_STE : context_ref.decl_prefix_stack) trace_ostream << "decl_prefix_stack[" << i++ << "] = " << p_STE->info() << std::endl;}
        }

	| KW_CONST basic_type
		{
            $$=new SymbolTableEntry($2,context_ref.p_current_Block()->p_Frame,0,STE_flag_const);
			context_ref.decl_prefix_stack.push_front($$);
			/*debug*/ if (trace_parser) {trace_ostream << "After push_front($$):" << std::endl; int i=0; for (auto& p_STE : context_ref.decl_prefix_stack) trace_ostream << "decl_prefix_stack[" << i++ << "] = " << p_STE->info() << std::endl;}
        }

	| basic_type
		{
            /*debug*/ if (trace_parser) trace_ostream << "decl_prefix:  basic_type = \"" << (Type($1)).print() << "\" \';\'" << std::endl;

            $$=new SymbolTableEntry($1,context_ref.p_current_Block()->p_Frame,0);

			context_ref.decl_prefix_stack.push_front($$);

			/*debug*/ if (trace_parser) {trace_ostream << "After push_front($$):" << std::endl; int i=0; for (auto& p_STE : context_ref.decl_prefix_stack) trace_ostream << "decl_prefix_stack[" << i++ << "] = " << p_STE->info() << std::endl;}
        }
;

decl_body:
	decl_item
        {
            $$=0;/* not used */
            /*debug*/ if (trace_parser) trace_ostream << "decl_body:  decl_item" << std::endl;
        }
	| decl_body ',' decl_item
        {
            $$=0;/* not used */
            /*debug*/ if (trace_parser) trace_ostream << "decl_body:  decl_body \',\' decl_item" << std::endl;
        }
;

decl_item:
	id_with_initializer
        {
            $$=0;/* not used */
            /*debug*/ if (trace_parser) trace_ostream << "decl_item:  id_with_initializer" << std::endl;
        }
	| function_declaration_or_definition
        {
            $$=0;/* not used */
            /*debug*/ if (trace_parser) trace_ostream << "decl_item:  function_declaration_or_definition" << std::endl;
        }
;

id_with_initializer:
	allocated_id
		{
            $$=0;/* not used */
            /*debug*/ if (trace_parser) trace_ostream << "id_with_initializer:  allocated_id" << std::endl;
            context_ref.chain_default_initializer(@$,$1);
        }

	| allocated_id ASSIGN x
		{
            $$=0;/* not used */
            /*debug*/ if (trace_parser) trace_ostream << "id_with_initializer:  allocated_id ASSIGN x" << std::endl;
            if (context_ref.chain_explicit_initializer(@$,$1, $3)) YYERROR;
        }
;

allocated_id:
	ID
		{
            /*debug*/ if (trace_parser) trace_ostream << "allocated_id: ID = \"" << *($1) << "\"" << std::endl;

            $$=context_ref.add_variable(*$1);
            if (!$$) YYERROR;

            /*debug*/ if (trace_parser) trace_ostream << "\"allocated_id: ID\" returned pointer to <\"" << $$->first << "\"," << $$->second.info() << ">" << std::endl;
        }
;

function_declaration_or_definition:
    function_declaration
	'{'
		{
            /*debug*/ if (trace_parser) trace_ostream << "function_declaration_or_definition: function_declaration \'{\'" << std::endl;

			Frame* p_Frame=$1;
			if (p_Frame->p_code_Block)
			{
				context_ref.error(std::string("duplicate definitions for ")
					+ printed_form(*(p_Frame->p_arglist)) );
				YYERROR;
			}
			p_Frame->start_Block();
		}
	statements
	'}'
		{
            $$=0;/* not used */
            /*debug*/ if (trace_parser) trace_ostream << "function_declaration_or_definition: function_declaration \'{\' statements \'}\'" << std::endl;

            context_ref.end_Block();
        }
	|

	function_declaration /* declaration only without definition */
        {
            $$=0;/* not used */
            /* WHAT??  $1 function_declaration returns a Frame pointer, but we throw it away!*/
            /*debug*/ if (trace_parser) trace_ostream << "function_declaration_or_definition: function_declaration returned pFrame = " << $1 << " " << (*$1) << std::endl;
            /* nothing more here */
        }
;

function_declaration:

	ID '(' func_arg_decl_list ')'
		{
		    /*debug*/ if (trace_parser) trace_ostream << "function_declaration: ID = \"" << (*$1)
            /*debug*/     << "\" \'(\' func_arg_decl_list = " << (*$3) << "\')\'" << std::endl;

            $$=context_ref.declare_function($1,$3);
            if (!$$) YYERROR;
        }
;

func_arg_decl_list:
	/* empty */
		{
		    /*debug*/ if (trace_parser) trace_ostream << "func_arg_decl_list: (null)" << std::endl;
			$$=new arglist;
		}
	| func_arg_decl_nonempty_list
		{
		    /*debug*/ if (trace_parser) trace_ostream << "func_arg_decl_list: func_arg_decl_nonempty_list" << std::endl;
			$$=$1;
		}
;

func_arg_decl_nonempty_list:
	func_arg_decl_nonempty_list ',' func_arg_decl
		{
		    /*debug*/ if (trace_parser) trace_ostream << "func_arg_decl_nonempty_list: func_arg_decl_nonempty_list \',\' func_arg_decl" << std::endl;
			$$=$1;
			(*($$)).push_back(*$3);
			delete $3;
		}
	| func_arg_decl
		{
		    /*debug*/ if (trace_parser) trace_ostream << "func_arg_decl_nonempty_list: func_arg_decl" << std::endl;
			$$=new arglist;
			(*($$)).push_back(*$1);
			delete $1;
		}
;

func_arg_decl:
	basic_type ID
        {
		    /*debug*/ if (trace_parser) trace_ostream << "func_arg_decl: basic_type ID" << std::endl;
            $$=new Arg($1,(*$2));
        }
	| basic_type
        {
		    /*debug*/ if (trace_parser) trace_ostream << "func_arg_decl: basic_type" << std::endl;
            $$=new Arg($1);
        }
;

possibly_null_x:
    /* null expression */
            {
                /*debug*/ if (trace_parser) trace_ostream << "possibly_null_x: null expression" << std::endl;
                $$ = (Xpr*) new Xpr_null(@$);
                if (!($$)) YYERROR;
            }
    | x
            {
                /*debug*/ if (trace_parser) trace_ostream << "possibly_null_x: not-null expression" << std::endl;
                $$ = $1;
            }

;

x:
      x PLUS x        { /*debug*/ if (trace_parser) trace_ostream << "x: x PLUS x"        << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x MINUS x       { /*debug*/ if (trace_parser) trace_ostream << "x: x MINUS x"       << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x MULT x        { /*debug*/ if (trace_parser) trace_ostream << "x: x MULT x"        << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x DIV x         { /*debug*/ if (trace_parser) trace_ostream << "x: x DIV x"         << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x REMAINDER x   { /*debug*/ if (trace_parser) trace_ostream << "x: x REMAINDER x"   << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x EQ x          { /*debug*/ if (trace_parser) trace_ostream << "x: x EQ x"          << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x NE x          { /*debug*/ if (trace_parser) trace_ostream << "x: x NE x"          << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x LT x          { /*debug*/ if (trace_parser) trace_ostream << "x: x LT x"          << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x LE x          { /*debug*/ if (trace_parser) trace_ostream << "x: x LE x"          << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x GT x          { /*debug*/ if (trace_parser) trace_ostream << "x: x GT x"          << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x GE x          { /*debug*/ if (trace_parser) trace_ostream << "x: x GE x"          << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x BITWISE_OR x  { /*debug*/ if (trace_parser) trace_ostream << "x: x BITWISE_OR x"  << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x BITWISE_AND x { /*debug*/ if (trace_parser) trace_ostream << "x: x BITWISE_AND x" << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}
	| x BITWISE_XOR x { /*debug*/ if (trace_parser) trace_ostream << "x: x PLUS x"        << std::endl; $$ = (Xpr*) new_binary_op_xpr(@$, $2,$1,$3); if (!($$)) YYERROR;}

	| MINUS x %prec UNARY{ /*debug*/ if (trace_parser) trace_ostream << "x: MINUS x [percent]prec UNARY" << std::endl; $$=(Xpr*) new_prefix_op_xpr(@$, $1,$2); if (!($$)) YYERROR;}

	| x LOGICAL_OR x
		{
			/*debug*/ if (trace_parser) trace_ostream << "x: x LOGICAL_OR x" << std::endl;
			Xpr* p_xpr1 = xpr_to_logical(@$, $1);
			Xpr* p_xpr2 = xpr_to_logical(@$, $3);
			if (p_xpr1 && p_xpr2)
			{
				$$=(Xpr*) new Xpr_logical_or(@$, p_xpr1,p_xpr2);
			} else {
				if (p_xpr1) delete p_xpr1;
				if (p_xpr2) delete p_xpr2;
				$$ = nullptr;
				YYERROR;
			}
		}
	| x LOGICAL_AND x
		{
			/*debug*/ if (trace_parser) trace_ostream << "x: x LOGICAL_AND x at " << @$ << std::endl;
			Xpr* p_xpr1 = xpr_to_logical(@1, $1);
			Xpr* p_xpr2 = xpr_to_logical(@3, $3);
			if (p_xpr1 && p_xpr2)
			{
				$$=(Xpr*) new Xpr_logical_and(@$, p_xpr1,p_xpr2);
			} else {
				if (p_xpr1) delete p_xpr1;
				if (p_xpr2) delete p_xpr2;
				$$ = nullptr;
				YYERROR;
			}
		}
	| variable ASSIGN x
		{
			/*debug*/ if (trace_parser) trace_ostream << "x: variable ASSIGN x" << std::endl;

            Xpr* px{nullptr};

			if ($1->second.is_const())
			{
				context_ref.error(std::string("Cannot assign to \"")
				+ $1->first
				+ std::string("\" - it is const."));
				YYERROR;
			}
			if (!assign_types_OK(@$, &px,&($1->second),$3))
			{
				context_ref.error(std::string("assignment type mismatch \"")
				+ ($1->first) + "\" ("
				+ ($1->second.genre()).print() + ") = ("
				+ ($3->genre()).print() + ")");
				YYERROR;
			}
			$$=px;
		}

	| '(' x ')'
		{
            /*debug*/ if (trace_parser) trace_ostream << "x: \'(\' x \')\'" << std::endl;
            $$ = $2;
        }

	| ID '(' xprlist ')'
		{
            /*debug*/ if (trace_parser) trace_ostream << "x: ID \'(\' xprlist \')\'" << std::endl;
            $$ = context_ref.build_function_call(@$,$1,$3);
            if (!($$)) YYERROR;
        }

	| basic_type '(' x ')'
		{
            /*debug*/ if (trace_parser) trace_ostream << "x: basic_type \'(\' x \')\'" << std::endl;
            /* int(...), double(...), string(...) */
			$$ = make_xpr_type_gender_bender(@$, Type($1),$3);
        }

	| DOUBLE_CONSTANT
		{
            /*debug*/ if (trace_parser) trace_ostream << "x: DOUBLE_CONSTANT" << std::endl;
            $$ = (Xpr*) new Xpr_constant<double>(@$,$1,type_double);
        }

	| INT_CONSTANT
		{
            /*debug*/ if (trace_parser) trace_ostream << "x: INT_CONSTANT" << std::endl;
            $$ = (Xpr*) new Xpr_constant<int>(@$,$1,type_int);
        }

	| STRING_CONSTANT
		{
            /*debug*/ if (trace_parser) trace_ostream << "x: STRING_CONSTANT" << std::endl;
            $$ = (Xpr*) new Xpr_constant<string>(@$,*$1,type_string);
        }

	| variable
		{
            /*debug*/ if (trace_parser) trace_ostream << "x: variable" << std::endl;
            $$ = (Xpr*) new_variable_reference(@$, &($1->second));
        }
;


variable:
	ID
		{
			/*debug*/ if (trace_parser) trace_ostream << "variable: ID" << std::endl;
			auto ST_iter=context_ref.SymbolTable_search(*($1));
			if (!ST_iter) {
                std::ostringstream o;
                o << "Unknown identifier \"" << (*($1)) << "\" at " << @$ << std::endl;
				context_ref.error(o.str());
				YYERROR;
			}
			if (!ST_iter->second.is_a_variable())
			{
                std::ostringstream o;
                o << "Identifier \"" << (*($1)) << "\" at " << @$ << " is in the symbol table, but not a variable." << std::endl;
                o << "The symbol table entry for \"" << (*($1)) << "\": " << ST_iter->second.info() << std::endl;
				context_ref.error(o.str());
				YYERROR;
			}
			$$=ST_iter;
		}
;

xprlist:
	/* empty */
		{
            /*debug*/ if (trace_parser) trace_ostream << "xprlist: (null)" << std::endl;
            $$ = new std::list<Xpr*>;
        }
	| xprlist_nonempty
		{
            /*debug*/ if (trace_parser) trace_ostream << "xprlist: xprlist_nonempty" << std::endl;
            $$ = $1;
        }
;

xprlist_nonempty:
	x
		{
			/*debug*/ if (trace_parser) trace_ostream << "xprlist_nonempty: x" << std::endl;
			$$ = new std::list<Xpr*>;
			$$->push_back($1);
		}

	| x ',' xprlist_nonempty
		{
			/*debug*/ if (trace_parser) trace_ostream << "xprlist_nonempty: x \',\' xprlist_nonempty" << std::endl;
			$$ = $3;
			$$->push_front($1);
		}
;

logical_x:
	x
		{
			/*debug*/ if (trace_parser) trace_ostream << "logical_x: x" << std::endl;
			$$ = xpr_to_logical(@$, $1); if (!($$)) YYERROR;
        }
;

string_x:
	x
		{
			/*debug*/ if (trace_parser) trace_ostream << "string_x: x" << std::endl;
			$$ = xpr_to_string(@$, $1); if (!($$)) YYERROR;
        }
;

int_x:
	x
		{
			/*debug*/ if (trace_parser) trace_ostream << "int_x: x" << std::endl;
			$$ = xpr_to_int(@$, $1); if (!($$)) YYERROR;
        }
;

double_x:
	x
		{
			/*debug*/ if (trace_parser) trace_ostream << "double_x: x" << std::endl;
			$$ = xpr_to_double(@$, $1); if (!($$)) YYERROR;
        }
;



host_list:
    /* empty */
        {
            $$ = new std::list<host_spec*>();
            /*debug*/ if (trace_parser) display_host_spec_pointer_list($$,trace_ostream);
        }

    | host_list ','
        {
            /* use of comma list separators is optional */
            if (trace_parser) { trace_ostream << "host_list at " << @$ << " ignoring commas." << std::endl; }
            $$=$1;
        }

    | host_list string_x KW_TO string_x
        {
            $1->push_back(new hostname_range(@$,$2,$4));
            $$ = $1;
            if (trace_parser)
            {
                trace_ostream << "host_list range - from and to expressions follow:" << std::endl;
                $2->display(indent_increment,trace_ostream);
                $4->display(indent_increment,trace_ostream);
            }
        }
    | host_list string_x
        {
            $1->push_back(new individual_hostname(@$,$2));
            $$ = $1;
            if (trace_parser)
            {
                trace_ostream << "host_list at " << @$ << " adding string expression: " << std::endl;
                $2->display(indent_increment,trace_ostream);
            }

        }
    | host_list DOTTED_QUAD_IP_ADDR
        {
            $1->push_back(new individual_dotted_quad(@$,*($2)));
            $$ = $1;
            if (trace_parser)
            {
                trace_ostream << "host_list at " << @$ << " adding dotted quad IP address: " << (*$2) << std::endl;
            }
        }
;

optional_select: /* type Select*, which is nullptr if there was no [Select] keyword */
        {
            $$ = nullptr;
        }

    | select
        {
            $$ = $1;
        }
;

select:
    KW_SELECT select_clause_list
        {
            $$ = new Select(@$,$2,m_s.masterlogfile);
            if (trace_parser) {$$->display("",trace_ostream);}
        }
;

select_clause_list:
    /* empty */
        {
            $$ = new std::list<SelectClause*>();
            if (trace_parser) { trace_ostream << "select_clause_list at " << @$ << " - first pass, creating new empty list." << std::endl; }
        }

    | select_clause_list ','
        {
            $$=$1;
            if (trace_parser) { trace_ostream << "select_clause_list at " << @$ << " ignoring commas." << std::endl; }
        }

    | select_clause_list select_clause
        {
            $1->push_back($2);
            $$ = $1;
            if (trace_parser) { trace_ostream << "select_clause_list at " << @$ << " adding list item:" << std::endl; $2->display("",trace_ostream); }
        }
;

select_clause:
    string_x KW_IS select_clause_value
        {
            $$ = new SelectClause(@$,$1,new std::list<SelectClauseValue*>(),m_s.masterlogfile);
            $$->p_SelectClauseValue_pointer_list->push_back($3);
            if (trace_parser)
            {
                trace_ostream << "select_clause with a single value at " << @$ << " on:" << std::endl;
                $3->display("",trace_ostream);
            }
        }
    | string_x KW_IS '{' select_clause_value_list '}'
        {
            $$ = new SelectClause(@$,$1,$4,m_s.masterlogfile);
            if (trace_parser) { trace_ostream << "select_clause with a list of values at " << @$ << std::endl; }
        }
;


select_clause_value:
    string_x
        {
            $$ = new SelectClauseValue(@$,$1);
            if (trace_parser) { trace_ostream << "select_clause_value at " << @$ <<" on:" << std::endl; $1->display("",trace_ostream); }
        }
;

select_clause_value_list:
    // create list on first (empty) match and then add entries
        {
            $$=new std::list<SelectClauseValue*>();
            if (trace_parser) {trace_ostream << "Recognizing select_clause_value_list at " << @$ << " - first pass, creating new empty list." << std::endl;}
        }
    | select_clause_value_list select_clause_value
        {
            $1->push_back($2);
            $$ = $1;
            if (trace_parser) { trace_ostream << "select_clause_value_list at " << @$ << " adding list item:" << std::endl;$2->display("",trace_ostream); }
        }
    | select_clause_value_list ','
        {
            $$ = $1;
            if (trace_parser) { trace_ostream << "select_clause_value_list at " << @$ << " ignoring commas." << std::endl; }
        }
;

optional_parameters:
        {
            $$ = nullptr;
        }
    | parameters
        {
            $$ = $1;
        }
;

parameters:
    KW_PARAMETERS string_x
        {
            $$ = $2;
        }
;

optional_nocsv:
        { $$ = false;}
    | KW_NOCSV
        { $$ = true; }
;

optional_quantity:
        { $$ = nullptr; }
    | KW_QUANTITY int_x
        { $$ = $2; }
;

optional_max_droop_max_to_min_iops:
        { $$ = nullptr; }
    | KW_MAX_DROOP_MAX_TO_MIN_IOPS double_x
        { $$ = $2; }
;
%%

// C++ code section of parser

namespace yy
{
	void ivy::error(location const &loc, const std::string& s)
	{
		std::cerr << "error at " << loc << ": " << s << std::endl;
		context_ref.unsuccessful_compile = true;
	}
}
