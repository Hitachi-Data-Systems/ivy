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
#pragma once

#include "Ivy_pgm.h"

class Xpr {
public:
    yy::location bookmark;
	virtual void evaluate(void* p_result)=0;

	// The caller provides the place to store
	// the result of the evaluation.

	// The thing that p_result points to is
	// presumed to be a fully constructed object
	// of type indicated by Xpr::genre() ready
	// for assignment of the value resulting
	// from the evaluation of the expression.

	// Sorry, right now there are no pointers
	// and references - yes, there's excessive
	// copying of objects such as strings.

	virtual const Type& genre()=0;
	virtual bool is_const()=0;
	virtual ~Xpr(){}; // deletes whole expression tree
	virtual void display(const std::string& /*indent string*/, std::ostream&)=0;
};

Xpr* new_nullary_builtin_xpr(const yy::location&, const Builtin&);
Xpr* new_unary_builtin_xpr(const yy::location&, const Builtin&, Xpr* arg1);
Xpr* new_binary_builtin_xpr(const yy::location&, const Builtin&, Xpr* arg1, Xpr* arg2);
Xpr* new_trinary_builtin_xpr(const yy::location&, const Builtin&, Xpr* arg1, Xpr* arg2, Xpr* arg3);

class Xpr_null: public Xpr
{
public:
    Xpr_null(const yy::location& l) {bookmark = l;}
    Type t {type_void};
	virtual void evaluate(void* p_result) override
        {if (trace_evaluate) trace_ostream << "Xpr_null::evaluate() at " << bookmark << "." << std::endl; return;};
    virtual const Type& genre() override {return t;};
    virtual bool is_const() override {return true;};
	virtual ~Xpr_null() override {};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_null at " << bookmark << std::endl;
        return;
    };
};

template<class C>
class Xpr_constant: public Xpr {
private:
	C constant_value;
	Type constant_type;
public:
	Xpr_constant(const yy::location& l, const C& v, const Type& t): constant_value(v), constant_type(t) {bookmark = l;};
	virtual void evaluate(void* p_result)
        {
            *((C*)p_result)=constant_value;
            if (trace_evaluate) trace_ostream << "Xpr_constant::evaluate() at " << bookmark << " returning " << constant_value << std::endl;
            return;
        };
	virtual const Type& genre() {return constant_type;};
	virtual bool is_const() {return true;};
	virtual ~Xpr_constant(){};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_constant<" << constant_type.print() << "> at " << bookmark << std::endl;
        return;
    };
};

template<class C>
class Xpr_variable_reference: public Xpr {
private:
	class SymbolTableEntry* p_STE;
public:
	Xpr_variable_reference(const yy::location& l, SymbolTableEntry* p_S): p_STE(p_S) {bookmark = l;};
	virtual void evaluate(void* p_result)
		{
            if (trace_evaluate) { trace_ostream << "Xpr_variable_reference::evaluate(): p_result = " << p_result
                << ", p_STE->" << p_STE->info() << std::endl; }
            *((C*)p_result) = *( (C*) ((*p_STE).generate_addr()) );
            if (trace_evaluate) trace_ostream << "Xpr_variable_reference::evaluate() at " << bookmark
                << " returning " << (*((C*)p_result)) << std::endl;
            return;
        };
	virtual const Type& genre() {return (*p_STE).genre();};
	virtual bool is_const() {return (*p_STE).is_const(); };
	virtual ~Xpr_variable_reference() {};

	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_variable_reference<" << p_STE->info() << "> at" << bookmark << std::endl;
        return;
    };
};

template<class R>
class Xpr_builtin0: public Xpr {
private:
    const Builtin* pBuiltin;
	R (*entry_pt)();
	Type my_type;
	unsigned int is_volatile;
public:
	Xpr_builtin0(const yy::location& l, const Builtin* pB, R (*ep)(), const Type& t, unsigned int iv=0):
		pBuiltin(pB), entry_pt(ep), my_type(t), is_volatile(iv) {bookmark = l;};
	virtual void evaluate(void* p_result)
		{
            (*((R*)p_result))=(*entry_pt)();
            if (trace_evaluate) {trace_ostream << "Xpr_builtin0: " << pBuiltin->display_name << " evaluate() at " << bookmark << " "
                << " returning " << (*((R*)p_result)) << std::endl; }
            return;
        };
	virtual const Type& genre() {return my_type;};
	virtual bool is_const() {return !is_volatile;};
	virtual ~Xpr_builtin0() {};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_builtin0 " <<  pBuiltin->display_name << " at " << bookmark << std::endl;
        return;
    };
};

template<class R, class A1>
class Xpr_builtin1: public Xpr {
private:
    const Builtin* pBuiltin;
	R (*entry_pt)(A1);
	Type my_type;
	Xpr* p_arg1_xpr;
	unsigned int is_volatile;
	A1 arg1;
public:
	Xpr_builtin1(const yy::location& l, const Builtin* pB, R (*ep)(A1), const Type& t, Xpr* a1, unsigned int iv=0):
		pBuiltin(pB), entry_pt(ep), my_type(t), p_arg1_xpr(a1), is_volatile(iv) {bookmark=l;};
	virtual void evaluate(void* p_result)
		{
            p_arg1_xpr->evaluate((void*)&arg1);
            (*((R*)p_result))=(*entry_pt)(arg1);
            if (trace_evaluate) { trace_ostream <<"Xpr_builtin1 " <<  pBuiltin->display_name << " evaluate() at " << bookmark
            << " operating on \"" << arg1 << "\" returning \"" << (*((R*)p_result)) << "\"." << std::endl; }
            return;
		};
	virtual const Type& genre() {return my_type;};
	virtual bool is_const()
		{return (!is_volatile) && (p_arg1_xpr->is_const());};
	virtual ~Xpr_builtin1() { delete p_arg1_xpr;};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_builtin1 " <<  pBuiltin->display_name << " at " << bookmark << " on:"<< std::endl;
        p_arg1_xpr->display(indent+indent_increment,o);
        return;
    };
};

template<class R, class A1, class A2>
class Xpr_builtin2: public Xpr {
private:
    const Builtin* pBuiltin;
	R (*entry_pt)(A1,A2);
	Type my_type;
	Xpr *p_arg1_xpr, *p_arg2_xpr;
	unsigned int is_volatile;
	A1 arg1;
	A2 arg2;
public:
	Xpr_builtin2(const yy::location& l, const Builtin* pB, R (*ep)(A1,A2), const Type& t, Xpr* a1, Xpr* a2,
		unsigned int iv=0): pBuiltin(pB), entry_pt(ep), my_type(t), p_arg1_xpr(a1),
		p_arg2_xpr(a2), is_volatile(iv) {bookmark=l;};
	virtual void evaluate(void* p_result)
		{
            p_arg1_xpr->evaluate((void*)&arg1);
            p_arg2_xpr->evaluate((void*)&arg2);
            (*((R*)p_result))=(*entry_pt)(arg1,arg2);
            if (trace_evaluate) { trace_ostream <<"Xpr_builtin2 " <<  pBuiltin->display_name << " evaluate() at " << bookmark
                << " operating on (\"" << arg1 << "\", \"" << arg2 << "\") returning \"" << (*((R*)p_result)) << "\"" << std::endl; }
            return;
        };
	virtual const Type& genre() {return my_type;};
	virtual bool is_const()
		{return (!is_volatile) && (p_arg1_xpr->is_const())
			&& (p_arg2_xpr->is_const());};
	virtual ~Xpr_builtin2() { delete p_arg1_xpr; delete p_arg2_xpr;};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_builtin2 " << pBuiltin->display_name << " at " << bookmark << " on:" << std::endl;
        p_arg1_xpr->display(indent+indent_increment,o);
        o << indent << "and:" << std::endl;
        p_arg2_xpr->display(indent+indent_increment,o);
        return;
    };
};

template<class R, class A1, class A2, class A3>
class Xpr_builtin3: public Xpr {
private:
    const Builtin* pBuiltin;
	R (*entry_pt)(A1,A2,A3);
	Type my_type;
	Xpr *p_arg1_xpr, *p_arg2_xpr, *p_arg3_xpr;
	unsigned int is_volatile;
	A1 arg1;
	A2 arg2;
	A3 arg3;
public:
	Xpr_builtin3(const yy::location& l, const Builtin* pB, R (*ep)(A1,A2,A3), const Type& t, Xpr* a1, Xpr* a2, Xpr* a3,
		unsigned int iv=0): pBuiltin(pB), entry_pt(ep), my_type(t),
		p_arg1_xpr(a1), p_arg2_xpr(a2), p_arg3_xpr(a3), is_volatile(iv) {bookmark=l;};
	virtual void evaluate(void* p_result)
		{
            p_arg1_xpr->evaluate((void*)&arg1);
            p_arg2_xpr->evaluate((void*)&arg2);
            p_arg3_xpr->evaluate((void*)&arg3);
            (*((R*)p_result))=(*entry_pt)(arg1,arg2,arg3);
            if (trace_evaluate) { trace_ostream <<"Xpr_builtin3 " <<  pBuiltin->display_name << " evaluate() at " << bookmark
                << " operating on (\"" << arg1 << "\", \"" << arg2 << "\", \"" << arg3 << "\") returning \"" << (*((R*)p_result)) << "\"" << std::endl; }
            return;
        };
	virtual const Type& genre() {return my_type;};
	virtual bool is_const()
		{return (!is_volatile) && (p_arg1_xpr->is_const())
			&& (p_arg2_xpr->is_const()) && (p_arg3_xpr->is_const());};
	virtual ~Xpr_builtin3() { delete p_arg1_xpr; delete p_arg2_xpr; delete p_arg3_xpr;};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_builtin3 " << pBuiltin->display_name << " at " << bookmark << " on:" << std::endl;
        p_arg1_xpr->display(indent+indent_increment,o);
        o << indent << "and:" << std::endl;
        p_arg2_xpr->display(indent+indent_increment,o);
        o << indent << "and:" << std::endl;
        p_arg3_xpr->display(indent+indent_increment,o);
        return;
    };
};


template <class C>
class Xpr_same_type_assign: public Xpr {
private:
	SymbolTableEntry *p_STE;
	Xpr *p_arg;
public:
	Xpr_same_type_assign(const yy::location& l, SymbolTableEntry *p_S, Xpr *a):
		p_STE(p_S), p_arg(a) {bookmark = l;};
	virtual void evaluate(void* p_result)
		{
			void* p_var = (*p_STE).generate_addr();
			(*p_arg).evaluate(p_var);
//std::cout << "Xpr_same_type_assign evaluate() at " << bookmark << std::endl;
			*((C*)p_result)=*((C*)p_var);
            if (trace_evaluate) { trace_ostream <<"Xpr_same_type_assign evaluate() at " << bookmark
                << " storing to address " << p_var << " (offset " << (((char*)p_var)-(p_Ivy_pgm->stack)) << " from stack) " << p_STE->info() << " returning value " << (*((C*)p_var)) << std::endl; }
			return;
		};
	virtual const Type& genre() {return p_arg->genre();};
	virtual bool is_const() {return false;};
	virtual ~Xpr_same_type_assign() { delete p_arg; };
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_same_type_assign at " << bookmark << " on:" << std::endl;
        p_arg->display(indent+indent_increment,o);
        return;
    };
};

template <class LVAL, class RVAL>
class Xpr_typecast_assign: public Xpr {
private:
	SymbolTableEntry *p_STE;
	Xpr *p_arg;
	RVAL rval_arg;
public:
	Xpr_typecast_assign(const yy::location& l, SymbolTableEntry *p_S,Xpr *a): p_STE(p_S), p_arg(a) {bookmark=l;};
	virtual void evaluate(void* p_result)
		{
			p_arg->evaluate((void*)&rval_arg);
			void* p_var = (*p_STE).generate_addr();
			*((LVAL*)p_var)=(LVAL)rval_arg;
			*((LVAL*)p_result)=*((LVAL*)p_var);
			if (trace_evaluate) trace_ostream << "Xpr_typecast_assign from " << p_arg->genre().print() << " value " << rval_arg
                << " to " << p_STE->info() << " value " << *((LVAL*)p_result) << " at " << bookmark << std::endl;
			return;
		};
	virtual const Type& genre() {return p_STE->genre();};
	virtual bool is_const() {return false;};
	virtual ~Xpr_typecast_assign() { delete p_arg; };
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_typecast_assign at " << bookmark << " on:" << std::endl;
        p_arg->display(indent+indent_increment,o);
        return;
    };
};

template <class RVAL>
class Xpr_typecast_assign_to_string: public Xpr {
private:
	SymbolTableEntry *p_STE;
	Xpr *p_arg;
	RVAL rval_arg;

public:
	Xpr_typecast_assign_to_string(const yy::location& l, SymbolTableEntry *p_S,Xpr *a): p_STE(p_S), p_arg(a) {bookmark=l;};
	virtual ~Xpr_typecast_assign_to_string() { delete p_arg; };

	virtual void evaluate(void* p_result)
		{
			p_arg->evaluate((void*)&rval_arg);

			std::ostringstream os;
			os << rval_arg;

			void* p_var = (*p_STE).generate_addr();
			*((std::string*)p_var)=os.str();
			*((std::string*)p_result)=os.str();

			if (trace_evaluate) trace_ostream << "Xpr_typecast_assign_to_string from " << p_arg->genre().print() << " value " << rval_arg
                << " to " << p_STE->info() << " value \"" << *((std::string*)p_result) << "\" at " << bookmark << std::endl;

			return;
		};
	virtual const Type& genre() {return p_STE->genre();};
	virtual bool is_const() {return false;};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_typecast_assign_to_string at " << bookmark << " on:" << std::endl;
        p_arg->display(indent+indent_increment,o);
        return;
    };
};

template <class TO, class FROM>
class Xpr_typecast_gender_bender: public Xpr {
private:
	Type to_type;
	Xpr *p_from;
	FROM from_arg;
public:
	Xpr_typecast_gender_bender(const yy::location& l, const Type& t, Xpr *f):
		to_type(t), p_from(f) {bookmark=l;};
	virtual void evaluate(void* p_result)
		{
			p_from->evaluate((void*)&from_arg);
			*((TO*)p_result)=(TO)from_arg;
			if (trace_evaluate) trace_ostream << "Xpr_typecast_gender_bender::evaluate() at " << bookmark
                << " from " << p_from->genre().print() << " value " << from_arg
                << " to " << to_type.print() << " value " << *((TO*)p_result) << std::endl;
			return;
		};
	virtual const Type& genre() {return to_type;};
	virtual bool is_const() {return p_from->is_const();};
	virtual ~Xpr_typecast_gender_bender() { delete p_from; };
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_typecast_gender_bender at " << bookmark << " on:" << std::endl;
        p_from->display(indent+indent_increment,o);
        return;
    };
};

template <class FROM>
class Xpr_typecast_gender_bender_to_string: public Xpr {
private:
    Type to_type {type_string};
	Xpr *p_from;
	FROM from_arg;
public:
	Xpr_typecast_gender_bender_to_string(const yy::location& l, Xpr *f) : p_from(f) {bookmark=l;};
	virtual void evaluate(void* p_result)
		{
			p_from->evaluate((void*)&from_arg);

			std::ostringstream os;
			os /* << std::fixed  - took this out as it had the unintended consequence of having it print annoying trailing zeros. */ << from_arg;
			*((std::string*)p_result)=os.str();
			if (trace_evaluate) trace_ostream << "Xpr_typecast_gender_bender_to_string::evaluate() at " << bookmark
                << " from " << p_from->genre().print() << " value " << from_arg
                << " to " << to_type.print() << " value \"" << *((std::string*)p_result) << "\"." << std::endl;
			return;
		};
	virtual const Type& genre() {return to_type;};
	virtual bool is_const() {return p_from->is_const();};
	virtual ~Xpr_typecast_gender_bender_to_string() { delete p_from; };
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_typecast_gender_bender_to_string at " << bookmark << " on:" << std::endl;
        p_from->display(indent+indent_increment,o);
        return;
    };
};

class Xpr_double_as_logical: public Xpr {
private:
	double double_value;
	Type type_value;
	Xpr* p_xpr;
public:
	virtual void evaluate(void* p_result)
		{
            p_xpr->evaluate((void*)&double_value);
            if (double_value==0.0) *((int*)p_result)=0;
            else                   *((int*)p_result)=1;
            if (trace_evaluate) trace_ostream << "Xpr_double_as_logical::evaluate() at " << bookmark
                << " from double value " << double_value << " to " << *((int*)p_result) << std::endl;
            return;
        };
	virtual const Type& genre() {return type_value;};
	virtual bool is_const() {return p_xpr->is_const();};
	Xpr_double_as_logical(const yy::location& l, Xpr* p_x):
		type_value(type_int), p_xpr(p_x) {bookmark=l;};
	virtual ~Xpr_double_as_logical(){delete p_xpr;};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_double_as_logical at " << bookmark << " on:" << std::endl;
        p_xpr->display(indent+indent_increment,o);
        return;
    };
};

class Xpr_double_as_int: public Xpr {
private:
	double double_value;
	Xpr* p_xpr;
	Type type {type_int};
public:
	virtual void evaluate(void* p_result)
		{
            p_xpr->evaluate((void*)&double_value);
            *((int*)p_result) = (int) double_value;
            if (trace_evaluate) trace_ostream << "Xpr_double_as_logical::evaluate() at " << bookmark
                << " from double value " << double_value << " to " << *((int*)p_result) << std::endl;
            return;
        };
	virtual const Type& genre() {return type;};
	virtual bool is_const() {return p_xpr->is_const();};
	Xpr_double_as_int(const yy::location& l, Xpr* p_x) : p_xpr(p_x) { bookmark=l; };
	virtual ~Xpr_double_as_int(){delete p_xpr;};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_double_as_int at " << bookmark << " on:" << std::endl;
        p_xpr->display(indent+indent_increment,o);
        return;
    };
};


class Xpr_double_as_string: public Xpr {
private:
	double double_value;
	Type type_value;
	Xpr* p_xpr;
public:
	virtual void evaluate(void* p_result)
		{
            p_xpr->evaluate((void*)&double_value);
            std::ostringstream o;
            o << double_value;
            *((std::string*)p_result) = o.str();
            if (trace_evaluate) trace_ostream << "Xpr_double_as_string::evaluate() at " << bookmark
                << " from double " << double_value << " to string \"" << *((std::string*)p_result) << std::endl;
            return;
        };
	virtual const Type& genre() {return type_value;};
	virtual bool is_const() {return p_xpr->is_const();};
	Xpr_double_as_string(const yy::location& l, Xpr* p_x):
		type_value(type_string), p_xpr(p_x) {bookmark=l;};
	virtual ~Xpr_double_as_string(){delete p_xpr;};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_double_as_string(";  o << ") at " << bookmark << " on:" << std::endl;
        p_xpr->display(indent+indent_increment,o);
        return;
    };
};


class Xpr_int_as_string: public Xpr {
private:
	int int_value;
	Type type_value;
	Xpr* p_xpr;
public:
	virtual void evaluate(void* p_result)
		{
            p_xpr->evaluate((void*)&int_value);
            std::ostringstream o;
            o << int_value;
            *((std::string*)p_result) = o.str();
            if (trace_evaluate) trace_ostream << "Xpr_int_as_string::evaluate() at " << bookmark
                << " from int value " << int_value << " to string \"" << *((std::string*)p_result) << "\"." << std::endl;
            return;
        };
	virtual const Type& genre() {return type_value;};
	virtual bool is_const() {return p_xpr->is_const();};
	Xpr_int_as_string(const yy::location& l, Xpr* p_x):
		type_value(type_string), p_xpr(p_x) {bookmark=l;};
	virtual ~Xpr_int_as_string(){delete p_xpr;};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_int_as_string at " << bookmark << " on:" << std::endl;
        p_xpr->display(indent+indent_increment, o);
        return;
    };
};

class Xpr_int_as_double: public Xpr {
private:
	int int_value;
	Type type_value;
	Xpr* p_xpr;
public:
	virtual void evaluate(void* p_result)
	{
		p_xpr->evaluate((void*)&int_value);
		*((double*)p_result)=(double)int_value;
		if (trace_evaluate) trace_ostream << "Xpr_int_as_double::evaluate() at " << bookmark
            << " from int " << int_value << " to double value " << *((double*)p_result) << std::endl;
		return;
	};
	virtual const Type& genre() {return type_value;};
	virtual bool is_const() {return p_xpr->is_const();};
	Xpr_int_as_double(const yy::location& l, Xpr* p_x):
		type_value(type_double), p_xpr(p_x) {bookmark=l;};
	virtual ~Xpr_int_as_double(){delete p_xpr;};
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_int_as_double at " << bookmark << " on:" << std::endl;
        p_xpr->display(indent+indent_increment,o);
        return;};
};

class Xpr_logical_or: public Xpr {
private:
	int int_value1, int_value2;
	Type type_value;
	Xpr* p_xpr1;
	Xpr* p_xpr2;
public:
	virtual void evaluate(void* p_result)
	{
		p_xpr1->evaluate((void*)&int_value1);
		if (int_value1)
		{
			*((int*)p_result)=1;
            if (trace_evaluate) trace_ostream << "Xpr_logical_or::evaluate() at " << bookmark
                << " expression1 " << p_xpr1->genre().print() << " value " << int_value1 << " returning " << *((int*)p_result)
                << " expression2 " << p_xpr1->genre().print() << " value not computed." << std::endl;
		} else {
			p_xpr2->evaluate((void*)&int_value2);
			if (int_value2)
			{
				*((int*)p_result)=1;
			}
			else
			{
				*((int*)p_result)=0;
			}
            if (trace_evaluate) trace_ostream << "Xpr_logical_or::evaluate() at " << bookmark
                << " expression1 " << p_xpr1->genre().print() << " value " << int_value1
                << ", expression2 " << p_xpr1->genre().print() << " value " << int_value2
                << " returning " << *((int*)p_result) << std::endl;
		}
		return;
	};
	virtual const Type& genre() {return type_value;};
	virtual bool is_const() {return p_xpr1->is_const() && p_xpr2->is_const();};
	Xpr_logical_or(const yy::location& l, Xpr* p_x1, Xpr* p_x2):
		type_value(type_int), p_xpr1(p_x1), p_xpr2(p_x2) {bookmark=l;};
	virtual ~Xpr_logical_or(){ delete p_xpr1; delete p_xpr2; };
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_logical_or at " << bookmark << " on:" << std::endl;

        if (p_xpr1 == nullptr) { o << indent << indent_increment << "< Xpr_logical_or\'s pointer to first operand expression is nullptr >"; }
        else { p_xpr1->display(indent+indent_increment,o); }

        o << indent << "and:" << std::endl;

        if (p_xpr2 == nullptr) { o << indent << indent_increment << "< Xpr_logical_or\'s pointer to second operand expression is nullptr >"; }
        else { p_xpr2->display(indent+indent_increment,o); }

        return;
    };
};

class Xpr_logical_and: public Xpr {
private:
	int int_value1, int_value2;
	Type type_value;
	Xpr* p_xpr1;
	Xpr* p_xpr2;
public:
	virtual void evaluate(void* p_result)
	{
		p_xpr1->evaluate((void*)&int_value1);
		if (int_value1==0)
		{
			*((int*)p_result)=0;
            if (trace_evaluate) trace_ostream << "Xpr_logical_and::evaluate() at " << bookmark
                << " expression1 " << p_xpr1->genre().print() << " value " << int_value1 << " returning " << *((int*)p_result)
                << " expression2 " << p_xpr1->genre().print() << " value not computed." << std::endl;
		} else {
			p_xpr2->evaluate((void*)&int_value2);
			if (int_value2)
			{
				*((int*)p_result)=1;
			} else {
				*((int*)p_result)=0;
			}
            if (trace_evaluate) trace_ostream << "Xpr_logical_and::evaluate() at " << bookmark
                << " expression1 " << p_xpr1->genre().print() << " value " << int_value1
                << ", expression2 " << p_xpr1->genre().print() << " value " << int_value2
                << " returning " << *((int*)p_result) << std::endl;
		}
		return;
	};
	virtual const Type& genre() {return type_value;};
	virtual bool is_const() {return p_xpr1->is_const() && p_xpr2->is_const();};
	Xpr_logical_and(const yy::location& l, Xpr* p_x1, Xpr* p_x2):
		type_value(type_int), p_xpr1(p_x1), p_xpr2(p_x2) {bookmark=l;};
	virtual ~Xpr_logical_and(){ delete p_xpr1; delete p_xpr2; };
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << indent << "Xpr_logical_and at " << bookmark << " on:" << std::endl;
        p_xpr1->display(indent+indent_increment,o);
        o << indent << "and:" << std::endl;
        p_xpr2->display(indent+indent_increment,o);
        return;
    };
};

class Xpr_string_to_numeric: public Xpr {
private:
	Type to_type;
	Xpr *p_from;
	std::string from_string;
public:
	Xpr_string_to_numeric(const yy::location& l, const Type& t, Xpr *f) : to_type(t), p_from(f) {bookmark=l;};
	virtual void evaluate(void* p_result);
	virtual const Type& genre() {return to_type;};
	virtual bool is_const() {return p_from->is_const();};
	virtual ~Xpr_string_to_numeric() { delete p_from; };
	virtual void display(const std::string& indent, std::ostream& o) override
	{
        o << "Xpr_string_to_numeric at " << bookmark << " on:" << std::endl;
        p_from->display(indent+indent_increment,o);
        return;
    };
};


class Xpr_function_call: public Xpr {
private:
	Frame* p_Frame;
	std::list<Xpr*>* p_xprlist;
public:
	virtual void evaluate(void* p_result);
	virtual const Type& genre();
	virtual bool is_const();
	virtual ~Xpr_function_call(){}; // deletes whole expression tree
	Xpr_function_call(const yy::location& l, Frame* p_F, std::list<Xpr*>* p_xl) : p_Frame(p_F), p_xprlist(p_xl) {bookmark=l;};
	virtual void display(const std::string& indent, std::ostream& o) override
        {
            o << indent << "Xpr_function_call at " << bookmark << " consisting of:" << std::endl;
            for (auto& p : (*p_xprlist))
            {
                p->display(indent+indent_increment,o);
            };
            return;
        };
};


Xpr* new_prefix_op_xpr(const yy::location& l, const std::string&, Xpr* arg1);

Xpr* new_binary_op_xpr(const yy::location& l, const std::string& op, Xpr* arg1, Xpr* arg2);

Xpr* new_variable_reference(const yy::location& l, SymbolTableEntry* p_STE);
Xpr* make_xpr_type_gender_bender(const yy::location& l, const Type& to_type, Xpr* source_xpr);

Xpr* xpr_to_logical(const yy::location& l, Xpr*);
Xpr* xpr_to_string(const yy::location& l, Xpr*);
Xpr* xpr_to_int(const yy::location& l, Xpr*);
Xpr* xpr_to_double(const yy::location& l, Xpr*);

int assign_types_OK(const yy::location& l, Xpr** pp_new_xpr,
					SymbolTableEntry* /* lvalue */,
					Xpr* p_rval_xpr);


std::string mangle(const std::string* p_ID, const std::list<Xpr*>* p_xprlist);
std::string printed_form(const std::string* p_ID, const std::list<Xpr*>* p_xprlist);

