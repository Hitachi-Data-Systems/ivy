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
#include <cassert>
#include <regex>
#include <sstream>

#include "Xpr.h"

std::regex xpr_double_regex (  R"([+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?)"    );
std::regex xpr_int_regex        (  R"([+-]?\d+)"                                   );

void Xpr_string_to_numeric::evaluate(void* p_result)
{
    if (type_string != ((enum basic_types)p_from->genre()))
    {
        std::ostringstream o;
        o << "Xpr_string_to_numeric::evaluate() at " << bookmark
            << " - \"from\" expression is type " << p_from->genre().print()
            << ".  From expression must be type string.  Probable parser programming error." << std::endl;
        p_Ivy_pgm->error(o.str());
    }
    p_from->evaluate((void*)&from_string);

    bool trailing_percent{false};
    double d;

    switch ((enum basic_types) to_type) {
        case type_int:
            if (std::regex_match(from_string, xpr_int_regex))
            {
                std::istringstream is(from_string);
                is >> (*((int*)p_result));
            }
            else if (std::regex_match(from_string, xpr_double_regex))
            {
                double d;
                std::istringstream is(from_string);
                is >> d;
                (*((int*)p_result)) = (int) d;
                if (trace_evaluate) trace_ostream << "Xpr_string_to_numeric::evaluate() at " << bookmark
                    << " string value \"" << from_string << " converted to int value " << (*((int*)p_result)) << std::endl;
            }
            else
            {
                std::ostringstream o;
                o << "Xpr_string_to_numeric::evaluate() at " << bookmark << " - \"" << from_string << "\") - not recognized as an integer or floating point value." << std::endl;
                p_Ivy_pgm->error(o.str());
            }
            break;
        case type_double:

            if (from_string.size()>1 && '%' == from_string[from_string.size()-1])
            {
                trailing_percent = true;
                from_string.erase(from_string.size()-1,1);
            }

            if (!std::regex_match(from_string, xpr_double_regex))
            {
                std::ostringstream o;
                o << "Xpr_string_to_numeric::evaluate() - \"" << from_string << "\") - not recognized as a floating point value." << std::endl;
                p_Ivy_pgm-> error(o.str());
            }
            {
                std::istringstream is(from_string);
                is >> d;
                if (trailing_percent) d = d / 100.;
                (*((double*)p_result))  =d;
                if (trace_evaluate) trace_ostream << "Xpr_string_to_numeric::evaluate() at " << bookmark
                    << " string value \"" << from_string << " converted to double value " << (*((double*)p_result)) << std::endl;
            }
            break;
        default:
            assert(0);
        return;
    }
};


Xpr* make_xpr_type_gender_bender(const yy::location& l, const Type& to_type, Xpr* source_xpr)
{
	switch ((enum basic_types) to_type) {
	case type_int:
		switch ((enum basic_types)source_xpr->genre()) {
		case type_int:
			return source_xpr;
		case type_double:
			return new Xpr_typecast_gender_bender<int,double>(l,to_type,source_xpr);
		case type_string:
			return new Xpr_string_to_numeric(l,to_type,source_xpr);
		default:
			assert(0);
		}
		break;
	case type_double:
		switch ((enum basic_types)source_xpr->genre()) {
		case type_int:
			return new Xpr_typecast_gender_bender<double,int>(l,to_type,source_xpr);
		case type_double:
			return source_xpr;
		case type_string:
			return new Xpr_string_to_numeric(l,to_type,source_xpr);
		default:
			assert(0);
		}
		break;
	case type_string:
		switch ((enum basic_types)source_xpr->genre()) {
		case type_int:
			return new Xpr_typecast_gender_bender_to_string<int>(l,source_xpr);
		case type_double:
			return new Xpr_typecast_gender_bender_to_string<double>(l,source_xpr);
		case type_string:
			return source_xpr;
		default:
			assert(0);
		}
		break;
	default:
		assert(0);
	}
	return (Xpr*)0; // to keep the compile from grumbling
}

Xpr* new_variable_reference(const yy::location& l, SymbolTableEntry* p_STE) {
    if (trace_parser) trace_ostream << "new_variable_reference() at " << l << " " << p_STE->info() << " ";
	switch ((enum basic_types) p_STE->genre())
	{
	case type_int:
        if (trace_parser) trace_ostream << " new Xpr_variable_reference<int>";
		return (Xpr*) new Xpr_variable_reference<int>(l,p_STE);
	case type_double:
        if (trace_parser) trace_ostream << " new Xpr_variable_reference<double>";
		return (Xpr*) new Xpr_variable_reference<double>(l,p_STE);
	case type_string:
        if (trace_parser) trace_ostream << " new Xpr_variable_reference<string>";
		return (Xpr*) new Xpr_variable_reference<std::string>(l,p_STE);
	default:
		assert(0);
	}
	return nullptr; // shut up compiler
}

Xpr* xpr_to_logical(const yy::location& l, Xpr* p_xpr)
{
	switch ((enum basic_types) p_xpr->genre())
	{
		case type_int:
            if (trace_parser) trace_ostream << "xpr_to_logical() - passing through existing int expression as logical expression.";
			return p_xpr;
			break;
		case type_double:
            if (trace_parser) trace_ostream << "xpr_to_logical() - returning new Xpr_double_as_logical.";
			return new Xpr_double_as_logical(l, p_xpr);
			break;
		case type_string:
            {
                std::ostringstream o; o << "xpr_to_logical() at " << l << " string expression may not be used as logical expression";
                p_Ivy_pgm->error(o.str());
			}
			break;
		default:
			assert(0);
	}
	return (Xpr*)0;
}

Xpr* xpr_to_int(const yy::location& l, Xpr* p_xpr)
{
	switch ((enum basic_types) p_xpr->genre())
	{
		case type_int:
            if (trace_parser) trace_ostream << "xpr_to_int() - passing through existing int expression.";
			return p_xpr;
			break;
		case type_double:
            if (trace_parser) trace_ostream << "xpr_to_logical() - returning new Xpr_double_as_int.";
			return new Xpr_double_as_int(l, p_xpr);
			break;
		case type_string:
            return new Xpr_string_to_numeric(l,type_int,p_xpr);
            break;
		default:
			assert(0);
	}
	return (Xpr*)0;
}


Xpr* xpr_to_double(const yy::location& l, Xpr* p_xpr)
{
	switch ((enum basic_types) p_xpr->genre())
	{
		case type_double:
            if (trace_parser) trace_ostream << "xpr_to_double() - passing through existing int expression.";
			return p_xpr;
		case type_int:
            if (trace_parser) trace_ostream << "xpr_to_logical() - returning new Xpr_int_as_double.";
			return new Xpr_int_as_double(l, p_xpr);
		case type_string:
            return new Xpr_string_to_numeric(l,type_double,p_xpr);
		default:
			throw std::runtime_error("xpr_to_double() - expression is of unknown type I can\'t handle.");
	}
	return (Xpr*)0; // unreachable
}


Xpr* xpr_to_string(const yy::location& l, Xpr* p_xpr)
{
	switch ((enum basic_types) p_xpr->genre())
	{
		case type_int:
            if (trace_parser) trace_ostream << "xpr_to_string() at " << l << " returning new Xpr_int_as_string()";
			return new Xpr_int_as_string(l,p_xpr);
			break;
		case type_double:
            if (trace_parser) trace_ostream << "xpr_to_string() at " << l << " returning new Xpr_double_as_string()";
			return new Xpr_double_as_string(l,p_xpr);
			break;
		case type_string:
            if (trace_parser) trace_ostream << "xpr_to_string() - passing through existing string expression.";
			return p_xpr;
			break;
		default:
			assert(0);
	}
	return (Xpr*)0;
}



int assign_types_OK(const yy::location& l, Xpr** pp_new_xpr /* returns a new expression via this pointer */,
                    SymbolTableEntry* p_lval_STE,
					Xpr* p_rval_xpr)
{
	Type lval_type=p_lval_STE->genre();
	Type rval_type=p_rval_xpr->genre();

	switch ((enum basic_types) lval_type) {
	case type_int:
		switch ((enum basic_types) rval_type) {
		case type_int:
            if (trace_parser) trace_ostream << "assign_types_OK() at " << l << " returning new Xpr_same_type_assign<int>";
			(*pp_new_xpr) = new Xpr_same_type_assign<int>(l,p_lval_STE,p_rval_xpr);
			return 1;
		case type_double:
            if (trace_parser) trace_ostream << "assign_types_OK() at " << l << " returning new Xpr_typecast_assign<int,double> (i.e. double to int)";
			(*pp_new_xpr) = new Xpr_typecast_assign<int,double>(l,p_lval_STE,p_rval_xpr);
			return 1;
		case type_string:
			{
                std::ostringstream o;
                o << "Error at " << l << " - assigning string value to int variable - use \"int( string )\" to explictly convert a string that looks like an int.";
                p_Ivy_pgm->warning(o.str());
			}
			return 0;
		default:
			assert(0);
		}
		break;
	case type_double:
		switch ((enum basic_types) rval_type) {
		case type_int:
            if (trace_parser) trace_ostream << "assign_types_OK() at " << l << " returning new Xpr_typecast_assign<double,int> (i.e. int to double)";
			(*pp_new_xpr) = new Xpr_typecast_assign<double,int>(l,p_lval_STE,p_rval_xpr);
			return 1;
		case type_double:
            if (trace_parser) trace_ostream << "assign_types_OK() at " << l << " returning new Xpr_same_type_assign<double>";
			(*pp_new_xpr) = new Xpr_same_type_assign<double>(l,p_lval_STE,p_rval_xpr);
			return 1;
		case type_string:
			{
                std::ostringstream o;
                o << "Error at " << l << " - assigning string value to double variable - use \"double( string )\" to explictly convert a string that looks like a double.";
                p_Ivy_pgm->warning(o.str());
			}
			return 0;
		default:
			assert(0);
		}
		break;
	case type_string:
		switch ((enum basic_types) rval_type) {
		case type_int:
            if (trace_parser) trace_ostream << "assign_types_OK() at " << l << " returning new Xpr_typecast_assign_to_string<int>";
			(*pp_new_xpr) = new Xpr_typecast_assign_to_string<int>(l,p_lval_STE,p_rval_xpr);
			return 1;
		case type_double:
            if (trace_parser) trace_ostream << "assign_types_OK() at " << l << " returning new Xpr_typecast_assign_to_string<double>";
			(*pp_new_xpr) = new Xpr_typecast_assign_to_string<double>(l,p_lval_STE,p_rval_xpr);
			return 1;
		case type_string:
            if (trace_parser) trace_ostream << "assign_types_OK() at " << l << " returning new Xpr_same_type_assign<string>";
			(*pp_new_xpr) = new Xpr_same_type_assign<std::string>(l,p_lval_STE,p_rval_xpr);
			return 1;
		default:
			assert(0);
		}
		break;
	default:
		assert(0);
	}
	return 0;
}

Xpr* new_prefix_op_xpr(const yy::location& l, const std::string& op, Xpr* p_Xpr1)
{
	assert(p_Xpr1);

	std::string op_name { std::string("prefix_") + op };

	std::string mangled_name { mangle_name1arg(op_name,(*p_Xpr1).genre()) };

	auto it = builtin_table.find(mangled_name);
	if (it != builtin_table.end())
	{
        if (trace_parser) trace_ostream << "new_prefix_op_xpr() at " << l << " found builtin function " << mangled_name << " returning new_unary_builtin_xpr()";
        return new_unary_builtin_xpr(l,it->second,p_Xpr1);
    }
    else
	{
		// TODO here is where we might try "promoting" ints to double, etc.
		std::ostringstream errMsg;

		auto ti = builtin_overloads.find(op_name);
		if (ti != builtin_overloads.end())
		{
			errMsg << "No match for <prefix>\'" << op << "\'("  << p_Xpr1->genre().print() << ")" << std::endl;
			errMsg << "builtin_overloads contains:" << std::endl << builtin_overloads << std::endl;
			errMsg << "However, the following built in prefix " << op << " operators are available:" << std::endl;
			errMsg << ti->second << std::endl;
		} else {
			errMsg << "No such operator <prefix>\'" << op << "\'("  << p_Xpr1->genre().print() << ")" << std::endl;
		}
		p_Ivy_pgm->error(errMsg.str());
		return nullptr;
	}
}

Xpr* new_binary_op_xpr(const yy::location& l, const std::string& op_name, Xpr* p_Xpr1, Xpr* p_Xpr2)
{
	assert( p_Xpr1 && p_Xpr2 );

	std::string mangled_name { mangle_name2args(op_name,(*p_Xpr1).genre(),(*p_Xpr2).genre()) };
	auto BT_iter = builtin_table.find(mangled_name);
	if (BT_iter != builtin_table.end())
	{
        if (trace_parser) trace_ostream << "new_binary_op_xpr() at " << l << " found builtin function " << mangled_name << " returning new_binary_builtin_xpr()";
        return new_binary_builtin_xpr(l,BT_iter->second,p_Xpr1,p_Xpr2);
    }
    else
	{
		// TODO here is where we might try "promoting" ints to double, etc.
		std::ostringstream errMsg;

        errMsg << "At location " << l << std::endl;
		errMsg << "No match for " << op_name << "("  << (*p_Xpr1).genre().print() << "," << (*p_Xpr2).genre().print() << ")" << std::endl;
        errMsg << "builtin_overloads contains:" << std::endl << builtin_overloads << std::endl;

		auto ti = builtin_overloads.find(op_name);
		if (ti != builtin_overloads.end())
		{
			errMsg << "However, the built in " << op_name << " functions are:" << std::endl;
			errMsg << ti->second << std::endl;
        }

        p_Ivy_pgm->error(errMsg.str());
		return nullptr;
	}
}

typedef std::string string;

Xpr* new_nullary_builtin_xpr(const yy::location& l, const Builtin& builtin)
{
	Xpr* p_new_xpr;
	switch ((int)(builtin.return_type))
	{
#define NULLARYOPCASE(RT) case (type_ ## RT): \
        p_new_xpr = new Xpr_builtin0<RT>(l,  &builtin, (RT (*)()) (builtin.entry_pt), builtin.return_type, builtin.volatile_function ); \
		break;
	NULLARYOPCASE(int)
	NULLARYOPCASE(double)
	NULLARYOPCASE(string)
#undef NULLARYOPCASE
	default:
		throw std::runtime_error("new_nullary_builtin_xpr() - dreaded programmer error - impossible nullary builtin return type");
	}
	if (trace_parser) trace_ostream << "new_nullary_builtin_xpr() at " << l << " " << builtin.display_name;
	return p_new_xpr;
}

Xpr* new_unary_builtin_xpr(const yy::location& l, const Builtin& builtin, Xpr* p_Xpr1)
{
	assert(p_Xpr1);
	Xpr* p_new_xpr;
	switch (
		((int)(builtin.arg1_type)
		)*type_base + ((int)(builtin.return_type))
	)
	{
#define UNARYOPCASE(RT,A1T) case (((type_void * type_base)+type_ ## A1T)*type_base + type_ ## RT): \
	p_new_xpr = new Xpr_builtin1<RT,A1T>( l, &builtin, (RT (*)(A1T)) (builtin.entry_pt), builtin.return_type, \
		p_Xpr1, builtin.volatile_function ); \
    break;

	UNARYOPCASE(int,int)
	UNARYOPCASE(int,double)
	UNARYOPCASE(int,string)
	UNARYOPCASE(double,int)
	UNARYOPCASE(double,double)
	UNARYOPCASE(double,string)
	UNARYOPCASE(string,int)
	UNARYOPCASE(string,double)
	UNARYOPCASE(string,string)

#undef UNARYOPCASE
	default:
		throw std::runtime_error("new_unary_builtin_xpr() - dreaded programmer error - impossible unary operator arg & return type signature");
	}
	if (trace_parser)
	{
        trace_ostream << "new_unary_builtin_xpr() at " << l << " " << builtin.display_name << " operating on: " << std::endl;
        p_Xpr1->display("",trace_ostream);
    }

	return p_new_xpr;
}

Xpr* new_binary_builtin_xpr(const yy::location& l, const Builtin& builtin, Xpr* p_Xpr1, Xpr* p_Xpr2)
{
	Xpr* p_new_xpr;
	assert( (p_Xpr1) && (p_Xpr2));
	switch (
		((((int)(builtin.arg2_type))*type_base)+((int)(builtin.arg1_type))
		)*type_base + ((int)(builtin.return_type))
	)
	{

#define BINOPCASE(RT,A1T,A2T) case \
	(((type_ ## A2T *type_base)+type_ ## A1T)*type_base + type_ ## RT): \
	p_new_xpr = new Xpr_builtin2<RT,A1T,A2T>( \
		l, &builtin, (RT (*)(A1T,A2T)) (builtin.entry_pt), builtin.return_type, \
		p_Xpr1, p_Xpr2, builtin.volatile_function ); \
			  break;

	BINOPCASE(int,int,int)
	BINOPCASE(int,int,double)
	BINOPCASE(int,int,string)
	BINOPCASE(int,double,int)
	BINOPCASE(int,double,double)
	BINOPCASE(int,double,string)
	BINOPCASE(int,string,int)
	BINOPCASE(int,string,double)
	BINOPCASE(int,string,string)
	BINOPCASE(double,int,int)
	BINOPCASE(double,int,double)
	BINOPCASE(double,int,string)
	BINOPCASE(double,double,int)
	BINOPCASE(double,double,double)
	BINOPCASE(double,double,string)
	BINOPCASE(double,string,int)
	BINOPCASE(double,string,double)
	BINOPCASE(double,string,string)
	BINOPCASE(string,int,int)
	BINOPCASE(string,int,double)
	BINOPCASE(string,int,string)
	BINOPCASE(string,double,int)
	BINOPCASE(string,double,double)
	BINOPCASE(string,double,string)
	BINOPCASE(string,string,int)
	BINOPCASE(string,string,double)
	BINOPCASE(string,string,string)
#undef BINOPCASE

	default:
		throw std::runtime_error("new_binary_builtin_xpr() - dreaded programmer error - impossible binary builtin arg & return type signature");
	}
	if (trace_parser)
	{
        trace_ostream << "new_binary_builtin_xpr() at " << l << " " << builtin.display_name << " operating on:" << std::endl;
        p_Xpr1->display("",trace_ostream);
        trace_ostream << "and" << std::endl;
        p_Xpr2->display("",trace_ostream);
    }
	return p_new_xpr;
}


Xpr* new_trinary_builtin_xpr(const yy::location& l, const Builtin& builtin, Xpr* p_Xpr1, Xpr* p_Xpr2, Xpr* p_Xpr3)
{
	Xpr* p_new_xpr;
	assert( (p_Xpr1) && (p_Xpr2) && (p_Xpr3));
	switch (
		(
             (   ((int)(builtin.arg3_type)*type_base + ((int)(builtin.arg2_type))  ) * type_base)   +    ((int)(builtin.arg1_type))
		)
		*type_base + ((int)(builtin.return_type))
	)
	{

#define TRIOPCASE(RT,A1T,A2T,A3T) case \
	((((type_ ## A3T * type_base + type_ ## A2T) *type_base)+type_ ## A1T)*type_base + type_ ## RT): \
	p_new_xpr = new Xpr_builtin3<RT,A1T,A2T,A3T>( \
		l, &builtin, (RT (*)(A1T,A2T,A3T)) (builtin.entry_pt), builtin.return_type, \
		p_Xpr1, p_Xpr2, p_Xpr3, builtin.volatile_function ); \
			  break;

	TRIOPCASE(int,int,int,int)
	TRIOPCASE(int,int,int,double)
	TRIOPCASE(int,int,int,string)
	TRIOPCASE(int,int,double,int)
	TRIOPCASE(int,int,double,double)
	TRIOPCASE(int,int,double,string)
	TRIOPCASE(int,int,string,int)
	TRIOPCASE(int,int,string,double)
	TRIOPCASE(int,int,string,string)
	TRIOPCASE(int,double,int,int)
	TRIOPCASE(int,double,int,double)
	TRIOPCASE(int,double,int,string)
	TRIOPCASE(int,double,double,int)
	TRIOPCASE(int,double,double,double)
	TRIOPCASE(int,double,double,string)
	TRIOPCASE(int,double,string,int)
	TRIOPCASE(int,double,string,double)
	TRIOPCASE(int,double,string,string)
	TRIOPCASE(int,string,int,int)
	TRIOPCASE(int,string,int,double)
	TRIOPCASE(int,string,int,string)
	TRIOPCASE(int,string,double,int)
	TRIOPCASE(int,string,double,double)
	TRIOPCASE(int,string,double,string)
	TRIOPCASE(int,string,string,int)
	TRIOPCASE(int,string,string,double)
	TRIOPCASE(int,string,string,string)
	TRIOPCASE(double,int,int,int)
	TRIOPCASE(double,int,int,double)
	TRIOPCASE(double,int,int,string)
	TRIOPCASE(double,int,double,int)
	TRIOPCASE(double,int,double,double)
	TRIOPCASE(double,int,double,string)
	TRIOPCASE(double,int,string,int)
	TRIOPCASE(double,int,string,double)
	TRIOPCASE(double,int,string,string)
	TRIOPCASE(double,double,int,int)
	TRIOPCASE(double,double,int,double)
	TRIOPCASE(double,double,int,string)
	TRIOPCASE(double,double,double,int)
	TRIOPCASE(double,double,double,double)
	TRIOPCASE(double,double,double,string)
	TRIOPCASE(double,double,string,int)
	TRIOPCASE(double,double,string,double)
	TRIOPCASE(double,double,string,string)
	TRIOPCASE(double,string,int,int)
	TRIOPCASE(double,string,int,double)
	TRIOPCASE(double,string,int,string)
	TRIOPCASE(double,string,double,int)
	TRIOPCASE(double,string,double,double)
	TRIOPCASE(double,string,double,string)
	TRIOPCASE(double,string,string,int)
	TRIOPCASE(double,string,string,double)
	TRIOPCASE(double,string,string,string)
	TRIOPCASE(string,int,int,int)
	TRIOPCASE(string,int,int,double)
	TRIOPCASE(string,int,int,string)
	TRIOPCASE(string,int,double,int)
	TRIOPCASE(string,int,double,double)
	TRIOPCASE(string,int,double,string)
	TRIOPCASE(string,int,string,int)
	TRIOPCASE(string,int,string,double)
	TRIOPCASE(string,int,string,string)
	TRIOPCASE(string,double,int,int)
	TRIOPCASE(string,double,int,double)
	TRIOPCASE(string,double,int,string)
	TRIOPCASE(string,double,double,int)
	TRIOPCASE(string,double,double,double)
	TRIOPCASE(string,double,double,string)
	TRIOPCASE(string,double,string,int)
	TRIOPCASE(string,double,string,double)
	TRIOPCASE(string,double,string,string)
	TRIOPCASE(string,string,int,int)
	TRIOPCASE(string,string,int,double)
	TRIOPCASE(string,string,int,string)
	TRIOPCASE(string,string,double,int)
	TRIOPCASE(string,string,double,double)
	TRIOPCASE(string,string,double,string)
	TRIOPCASE(string,string,string,int)
	TRIOPCASE(string,string,string,double)
	TRIOPCASE(string,string,string,string)
#undef TRIOPCASE

	default:
		throw std::runtime_error("new_trinary_builtin_xpr() - dreaded programmer error - impossible trinary builtin arg & return type signature");
	}
	if (trace_parser)
	{
        trace_ostream << "new_trinary_builtin_xpr() at " << l << " " << builtin.display_name << " operating on:" << std::endl;
        p_Xpr1->display("",trace_ostream);
        trace_ostream << "and" << std::endl;
        p_Xpr2->display("",trace_ostream);
        trace_ostream << "and" << std::endl;
        p_Xpr3->display("",trace_ostream);
    }
	return p_new_xpr;
}


void Xpr_function_call::evaluate(void* p_result)
{
    if (trace_evaluate) {trace_ostream << "Xpr_function_call::evaluate() for " << printed_form(*(p_Frame->p_arglist))
        << " p_result points to offset " << ( ((char*) p_result) - p_Ivy_pgm->stack ) << " from stack."
        << std::endl << "Xpr_function_call::p_Frame->" << std::endl << (*p_Frame) << std::endl
        << "Xpr_function_call - evaluating arguments:"; }

	char *original_stacktop=p_Ivy_pgm->stacktop;
	unsigned int new_frame_offset=(p_Ivy_pgm->stacktop)-(p_Ivy_pgm->stack);

	// push p_result onto stack
	*((void**)p_Ivy_pgm->stacktop)=p_result;

	p_Ivy_pgm->stacktop+=sizeof(void*);

	for (auto& p_xpr : (*p_xprlist))
	{
		// for each argument Xpr:
		//    - construct an object of Xpr type at cursor location.
		//    - call Xpr.evaluate((void*) cursor)
		void* p_xpr_result=(void*)p_Ivy_pgm->stacktop;
		p_Ivy_pgm->stacktop+=(p_xpr->genre()).maatvan();
		(p_xpr->genre()).construct_at(p_xpr_result);
		p_xpr->evaluate(p_xpr_result);
	}
	p_Frame->invoke(new_frame_offset);
	// function.invoke(frame pointer)
		// function.invoke() will:
		//	push frame pointer on FrameStack
		//	run code, destructors
		//	pop FrameStack
		//	return

	p_Ivy_pgm->stacktop=original_stacktop+sizeof(void*);

	for (auto& p_xpr : (*p_xprlist))
	{
		// call destructors for parameter objects
		// DESTRUCTORS MAY NOT ALLOCATE ANYTHING ON THE STACK
		// We destruct in the "wrong" direction on the stack,
		// (messing with an object in the middle of the stack)
		// because the xprlist is a single-linked list.
		void* p_xpr_result=(void*)p_Ivy_pgm->stacktop;
		p_Ivy_pgm->stacktop+=(p_xpr->genre()).maatvan();
		(p_xpr->genre()).destroy_at(p_xpr_result);
	}
	p_Ivy_pgm->stacktop=original_stacktop;
	return;
}

bool Xpr_function_call::is_const()
{
	return false;
	/* revisit to see if we can make a better answer */
	/* - not simple at first glance */
}

const Type& Xpr_function_call::genre()
{
	return p_Frame->genre();
}


			// and then do it, e.g. with atoi((const char*)(...))

std::string mangle(const std::string* p_ID, const std::list<Xpr*>* p_xprlist)
{
	std::string mangled {*p_ID};

	mangled+='(';

	int i=0;
	for (Xpr* p_Xpr : (*p_xprlist))
	{
		if(i++) mangled+=',';
		mangled+=p_Xpr->genre().print();
	}
	mangled+=')';
	return mangled;
}

std::string printed_form(const std::string* p_ID, const std::list<Xpr*>* p_xprlist)
{
	std::string mangled {*p_ID};

	mangled+='(';

	int i=0;
	for (Xpr* p_Xpr : (*p_xprlist))
	{
		if(i++) mangled+=',';
		mangled+=p_Xpr->genre().print();
	}
	mangled+=')';
	return mangled;
}

