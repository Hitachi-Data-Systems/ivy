//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <iostream>
#include <cassert>

#include "Ivy_pgm.h"
#include "Xpr.h"
#include "location.hh"
#include "host_range.h"
#include "SelectClause.h"
#include "Select.h"

class Stmt {
public:
    yy::location bookmark;
	virtual bool execute()=0;
		// return value nonzero means "return" statement
		// has been executed in current function,
		// execute destructors (e.g. variable destructor) code
		// and get out.
	virtual ~Stmt() {}
	virtual void display(const std::string&, std::ostream& os)=0;
};

class Stmt_null : public Stmt{
public:
	virtual bool execute() override
	{
        if (trace_evaluate) { trace_ostream << "Stmt_null at " << bookmark << "executing." << std::endl; }
        return false;
    }
	virtual ~Stmt_null() override {}
	Stmt_null(const yy::location& l) {bookmark=l;}
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        os << indent << "Stmt_null at " << bookmark << std::endl;
    }
};

class Stmt_once : public Stmt{
	// This is an adapter you can put over
	// a Stmt so that it is executed only once.
	// Originally designed for use with constructors
	// for static objects.  This way there is no
	// restriction on the expressions used to
	// initialize static variables - you can even
	// use the values of "existing" automatic
	// variables in the assignment expression
	// for the static variable by running the
	// constructor in the regular flow of code the
	// first time we run through the code where
	// where the declaration/initialization
	// of the static variable was found.
private:
	bool executed {false};
	Stmt* p_Stmt;
public:
	Stmt_once(Stmt* p_s): p_Stmt(p_s) {bookmark=p_s->bookmark;};
	virtual bool execute() override
		{
			if (executed)
			{
                if (trace_evaluate) trace_ostream << "Stmt_once - not doing again." << std::endl;
			} else {
                if (trace_evaluate) trace_ostream << "Stmt_once - will now execute for the only time"<< std::endl;
				p_Stmt->execute();
				executed=true;
			}
			return false;
		};
	virtual ~Stmt_once() { delete p_Stmt; };
	virtual void display (const std::string& indent, std::ostream& os) override
	{
        os << indent << "Stmt_once: at " << bookmark << " on:"<< std::endl;
        p_Stmt->display(indent+indent_increment,os);
    }
};

template <class C>
class Stmt_xpr: public Stmt {
private:
	Xpr* p_xpr;
	C result;
public:
	virtual bool execute() override
	{
		if (trace_evaluate) { trace_ostream  << "Stmt_xpr at " << bookmark << " executing its expression:" << std::endl;}
		p_xpr->evaluate((void*)&result);
		return false;
	};
	Stmt_xpr(Xpr* p_x): p_xpr(p_x) {bookmark = p_x->bookmark;};
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        os << indent << "Stmt_xpr: at " << bookmark << " on:" << std::endl;
        p_xpr->display(indent+indent_increment,os);
    }
};

Stmt* make_Stmt_Xpr(Xpr *p_xpr, int perform_once);

class Stmt_return : public Stmt {
private:
	Xpr* p_xpr;
public:
	virtual bool execute() override ;
	Stmt_return(const yy::location& l, Xpr* p_x): p_xpr(p_x) { bookmark = l;};
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        os << indent << " "; os << "Stmt_return at " << bookmark << std::endl;
    }
};

class Stmt_constructor : public Stmt {
private:
	SymbolTableEntry* p_STE;
	bool constructed {false};
public:
	Stmt_constructor(const yy::location& l, SymbolTableEntry* p_s) : p_STE(p_s) { bookmark = l; if (trace_parser) std::cout << "Stmt_constructor(" << p_s->info() << std::endl;};
	virtual bool execute() override
	{

		if (p_STE->is_static() && constructed)
        {
            if (trace_evaluate) { display(0,trace_ostream); trace_ostream  << " not executing - already constructed." << std::endl; }
            return false;
        }
        if (trace_evaluate) { display(0,trace_ostream); trace_ostream  << " executing." << std::endl; }

		constructed = true;
		switch ((enum basic_types)(p_STE->genre())) {
		case type_string:
			new (p_STE->generate_addr()) std::string();
			break;
		case type_int:
			*((int*)p_STE->generate_addr())=0;
			break;
		case type_double:
			*((double*)p_STE->generate_addr())=0.0;
			break;
		default:
			assert(0);
		}
		return false;
	};
	virtual void display(const std::string& indent, std::ostream& os) override
	{ os << indent << "Stmt_constructor at " << bookmark << ": " << p_STE->info() << std::endl; }

};

class Stmt_destructor : public Stmt {
private:
	SymbolTableEntry* p_STE;
public:
	Stmt_destructor(SymbolTableEntry* p_S): p_STE(p_S) {};
	virtual bool execute() override {
        if (trace_evaluate) { display(0,trace_ostream); trace_ostream  << " executing." << std::endl; }
		switch ((enum basic_types)(p_STE->genre())) {
		case type_string:
            typedef std::string std_string;
			((std::string*)p_STE->generate_addr()) -> ~std_string();
			break;
		default:
			assert(1);
		}
		return false;
	};
	virtual void display(const std::string& indent, std::ostream& os) override
	{ os << indent << "Stmt_destructor: " << p_STE->info() << std::endl; }
};

class Stmt_if : public Stmt{
private:
	Xpr* p_test_x;
	Stmt *p_then_Stmt, *p_else_Stmt;
public:
	virtual bool execute() override
	{
        if (trace_evaluate) { display(0,trace_ostream); trace_ostream  << " executing." << std::endl; }

		int test_result;
		p_test_x->evaluate((void*)&test_result);
		if (test_result)
		{
            if (trace_evaluate) { trace_ostream  << "If condition was true, executing \"then\" statement." << std::endl; }
            return p_then_Stmt->execute();
        }
		else
		{
            if (trace_evaluate) { trace_ostream  << "If condition was true, executing \"then\" statement." << std::endl; }
			if (p_else_Stmt)
			{
                if (trace_evaluate) { trace_ostream  << "If condition was false, executing \"else\" statement." << std::endl; }
                return p_else_Stmt->execute();
            }
			else
			{
                if (trace_evaluate) { trace_ostream  << "If condition was false, no \"else\" statement to execute." << std::endl; }
                return false;
            }
        }
	}

	virtual ~Stmt_if() {delete p_test_x;delete p_then_Stmt;if(p_else_Stmt) delete p_else_Stmt;}
	Stmt_if(const yy::location& l, Xpr* p_test, Stmt* p_then, Stmt* p_else=(Stmt*)0):
		p_test_x(p_test), p_then_Stmt(p_then), p_else_Stmt(p_else) { bookmark=l;};
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        os << indent << "Stmt_if () at " << bookmark << std::endl;
        p_then_Stmt->display(indent+indent_increment,os);
    }
};

class Stmt_for : public Stmt{
private:
	Xpr* p_init_x;
	Xpr* p_test_x;
	Xpr* p_post_x;
	Stmt* p_body_Stmt;

public:
	virtual bool execute() override
	{
        union {
			int test_result;
			char any_type[biggest_type];
		} u;

		if (trace_evaluate) { display(0,trace_ostream); trace_ostream  << " executing." << std::endl; }

		if (p_init_x == nullptr)
		{
            if (trace_evaluate) { trace_ostream  << "nullptr, no for-loop initializing expression." << std::endl; }
		}
		else
		{
            if (trace_evaluate) { trace_ostream  << "executing for-loop initializing expression." << std::endl; }
            p_init_x->evaluate((void*)&u.any_type);
        }
		while(1)
		{
            if (p_test_x == nullptr)
            {
                if (trace_evaluate) { trace_ostream  << "nullptr, no test expression - breaking without executing loop body." << std::endl; }
                break; // Same as if test fails.
            }
            else
            {
                if (trace_evaluate) { trace_ostream  << "for-loop - executing loop iteration test expression" << std::endl; }
                p_test_x->evaluate((void*)&u.test_result);

                if (!u.test_result)
                {
                    if (trace_evaluate) { trace_ostream  << "for-loop - test condition evaluated false, exiting for-loop." << std::endl; }
                    break;
                }
                if (trace_evaluate) { trace_ostream  << "for-loop - test condition evaluated true, executing loop body statement." << std::endl; }
                if (p_body_Stmt->execute())
                {
                    if (trace_evaluate) { trace_ostream  << "for-loop body statement executed a \"return\" statement." << std::endl; }
                    return true;
                }

                if (p_post_x == nullptr)
                {
                    if (trace_evaluate) { trace_ostream  << "nullptr - no for-loop iteration epilogue expression." << std::endl; }
                }
                else
                {
                    if (trace_evaluate) { trace_ostream  << "executing for-loop iteration epilogue expression." << std::endl; }
                    p_post_x->evaluate((void*)&u.any_type);
                }
            }
        }
        return false;
	}
	virtual ~Stmt_for()
		{delete p_init_x;delete p_test_x;delete p_post_x;
		delete p_body_Stmt;}
	Stmt_for(const yy::location& l, Xpr* p_init, Xpr* p_test, Xpr* p_post, Stmt* p_body):
		p_init_x(p_init), p_test_x(p_test), p_post_x(p_post), p_body_Stmt(p_body) {bookmark=l;};
	virtual void display(const std::string& indent, std::ostream& os) override
	{ os << indent << "Stmt_for (;;) at " << bookmark << std::endl;
	p_body_Stmt->display(indent+indent_increment,os); }
};

class Stmt_for_xprlist : public Stmt
{
public:
    std::pair<const std::string,SymbolTableEntry>* p_STE_pair;
    std::list<Xpr*>* p_xprlist;
    Stmt* pStmt;

    std::list<Xpr*> assign_xprlist;

//methods
    Stmt_for_xprlist(const yy::location&, std::pair<const std::string,SymbolTableEntry>*, std::list<Xpr*>*, Stmt* pS);
    virtual ~Stmt_for_xprlist() { delete p_xprlist; delete pStmt; }
    virtual bool execute() override;
    virtual void display(const std::string& indent, std::ostream& os) override;
};

class Stmt_while : public Stmt{
private:
	Xpr* p_test_x;
	Stmt* p_body_Stmt;
public:
	virtual bool execute() override;
	virtual ~Stmt_while() { delete p_test_x; delete p_body_Stmt; }
	Stmt_while(const yy::location& l, Xpr* p_test, Stmt* p_body):
		p_test_x(p_test), p_body_Stmt(p_body) { bookmark=l; };
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        os << indent << "Stmt_while () at " << bookmark << std::endl;
        p_body_Stmt->display(indent+indent_increment,os);
    }
};

class Stmt_do : public Stmt {
private:
	Stmt* p_body_Stmt;
	Xpr* p_test_x;
public:
	virtual bool execute() override;
	virtual ~Stmt_do() { delete p_body_Stmt; delete p_test_x; }
	Stmt_do(const yy::location& l, Stmt* p_body, Xpr* p_test) : p_body_Stmt(p_body), p_test_x(p_test) { bookmark=l; };
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        os << indent << "Stmt_do at " << bookmark << " {" << std::endl;
            p_body_Stmt->display(indent+indent_increment,os);
        os << indent << "} while (" << std::endl;

            p_test_x->display(indent+indent_increment, os);  // expressions don't have display indent levels. yet.

        os << indent << ")"<< std::endl;
    }
};

class Stmt_set_iogenerator_template : public Stmt {
public:
    Xpr* p_iogenerator_Xpr {nullptr};
    Xpr* p_parameters_Xpr {nullptr};

// methods
	Stmt_set_iogenerator_template(const yy::location& l, Xpr* pix, Xpr* ppx) :  p_iogenerator_Xpr(pix), p_parameters_Xpr(ppx) { bookmark=l;}
	virtual ~Stmt_set_iogenerator_template() override
	{
        if (p_iogenerator_Xpr != nullptr) delete p_iogenerator_Xpr;
        if (p_parameters_Xpr != nullptr) delete p_parameters_Xpr;
    }

	virtual bool execute() override;
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        os << indent << "Stmt_set_iogenerator_template at " << bookmark <<" on iogenerator name expression :" << std::endl;
        p_iogenerator_Xpr->display(indent+indent_increment,os);
        os << indent << "Stmt_set_iogenerator_template at " << bookmark <<" (continued) on parameters expression :" << std::endl;
        p_parameters_Xpr->display(indent+indent_increment,os);
    }
};

class Stmt_hosts : public Stmt {
public:
    std::list<host_spec*>* phspl {nullptr};
    Select* p_Select;

// methods
	Stmt_hosts(const yy::location& l, std::list<host_spec*>* p, Select* ps) : phspl(p), p_Select(ps)
        {
            bookmark=l;
            if (phspl==nullptr) throw std::runtime_error("Stmt_hosts constructor called with null pointer to host_spec pointer list.");
            if (p_Select==nullptr) throw std::runtime_error("Stmt_hosts constructor called with null pointer to Select.");
        }
	virtual ~Stmt_hosts() override { if (phspl != nullptr) for (auto& p :(*phspl)) delete p; }

	virtual bool execute() override;
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        if (phspl == nullptr) { os << indent << "Stmt_hosts at " << bookmark <<" has null pointer to host_spec pointer list :" << std::endl; }
        else                  { os << indent << "Stmt_hosts at " << bookmark <<" on host spec pointer list :" << std::endl;
                                for (auto& p:(*phspl)) {p->display(indent+indent_increment,os);} }

        if (p_Select == nullptr) { os << indent << "Stmt_hosts at " << bookmark <<" (continued) has null pointer to Select." << std::endl;}
        else                     { os << indent << "Stmt_hosts at " << bookmark <<" (continued) Select clause :" << std::endl;
                                   p_Select->display(indent+indent_increment,os); }
    }
};

class Stmt_delete_rollup : public Stmt {
public:
    Xpr* p_rollup_spec_Xpr {nullptr};

// methods
	Stmt_delete_rollup(const yy::location& l, Xpr* prn);

	virtual ~Stmt_delete_rollup() override {}

	virtual bool execute() override;
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        os << indent << "Stmt_delete_rollup at " << bookmark <<" on rollup spec expression :" << std::endl;
        p_rollup_spec_Xpr->display(indent+indent_increment,os);
	}
};

class Stmt_create_workload : public Stmt
{
public:
    Xpr* p_workload_name_Xpr {nullptr};
    Select* p_Select {nullptr};
    Xpr* p_iogenerator_Xpr {nullptr};
    Xpr* p_parameters_Xpr {nullptr};

    bool haveParameters {false};

// methods
	Stmt_create_workload(const yy::location& l, Xpr* pwn, Select* ps, Xpr* pix, Xpr* ppx)
        :  p_workload_name_Xpr(pwn), p_Select(ps), p_iogenerator_Xpr(pix), p_parameters_Xpr(ppx)
        {
            bookmark=l;
            if (p_parameters_Xpr != nullptr) haveParameters = true;
            if (p_workload_name_Xpr==nullptr) throw std::runtime_error("Stmt_create_workload constructor called with p_workload_name_Xpr==nullptr.");
            if (p_iogenerator_Xpr==nullptr) throw std::runtime_error("Stmt_create_workload constructor called with p_iogenerator_Xpr==nullptr.");
        }

	virtual ~Stmt_create_workload() override
	{
        if (p_workload_name_Xpr != nullptr) delete p_workload_name_Xpr;
        if (p_Select            != nullptr) delete p_Select;
        if (p_iogenerator_Xpr   != nullptr) delete p_iogenerator_Xpr;
        if (p_parameters_Xpr    != nullptr) delete p_parameters_Xpr;
    }

	virtual bool execute() override;
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        os << indent << "Stmt_create_workload at " << bookmark << " on workload name expression:";

        if (p_workload_name_Xpr == nullptr)
        {
            os << " nullptr to workload name expression." << std::endl;
        }
        else
        {
            os << std::endl;
            p_workload_name_Xpr->display(indent+indent_increment,os);
        }

        os << indent << "Stmt_create_workload at " << bookmark << " continuing with iogenerator name expression." << std::endl;
        if (p_iogenerator_Xpr == nullptr)
        {
            os << indent+indent_increment << "Stmt_create_workload - nullptr to iogenerator name expression." << std::endl;
        }
        else
        {
            p_iogenerator_Xpr->display(indent+indent_increment,os);
        }
        if (p_Select == nullptr) { os << indent << "Stmt_create_workload at " << bookmark <<" (continued) - null pointer to Select." << std::endl; }
        else
        {
            os << indent << "Stmt_create_workload at " << bookmark <<" (continued) - Select:" << std::endl;
            p_Select->display(indent+indent_increment,os);
        }
        if (p_parameters_Xpr == nullptr) { os << indent << "Stmt_create_workload at " << bookmark <<" (continued) - null pointer to parameters expression." << std::endl;  }
        else
        {
            os << indent << "Stmt_create_workload at " << bookmark <<" (continued) - parameters expression:" << std::endl;
            p_parameters_Xpr->display(indent+indent_increment,os);
        }
	}
};

class Stmt_delete_workload : public Stmt
{
public:
    Xpr* p_workload_name_Xpr {nullptr};
    Select* p_Select {nullptr};

// methods
	Stmt_delete_workload(const yy::location& l, Xpr* pwn, Select* ps)
        :  p_workload_name_Xpr(pwn), p_Select(ps)
        {
            bookmark=l;
        }

	virtual ~Stmt_delete_workload() override
	{
        if (p_workload_name_Xpr != nullptr) delete p_workload_name_Xpr;
        if (p_Select            != nullptr) delete p_Select;
    }

	virtual bool execute() override;

	virtual void display(const std::string& indent, std::ostream& os) override
	{
        os << indent << "Stmt_delete_workload at " << bookmark
        <<" on workload name expression :" << std::endl;
        if (p_workload_name_Xpr == nullptr)
        {
            os << indent+indent_increment << "Stmt_delete_workload - nullptr to workload name expression." << std::endl;
        }
        else
        {
            p_workload_name_Xpr->display(indent+indent_increment,os);
        }
        if (p_Select == nullptr) { os << indent << "Stmt_delete_workload at " << bookmark <<" (continued) - null pointer to Select." << std::endl; }
        else
        {
            os << indent << "Stmt_delete_workload at " << bookmark <<" (continued) - Select:" << std::endl;
            p_Select->display(indent+indent_increment,os);
        }
	}
};

class Stmt_create_rollup : public Stmt {
public:
    Xpr* p_rollup_spec_Xpr {nullptr};

    bool haveNocsv {false};

    Xpr* p_quantity_Xpr {nullptr};
    bool haveQuantity;

    Xpr* p_max_droop_Xpr {nullptr};
    bool haveMaxDroopMaxtoMinIOPS;

// methods
	Stmt_create_rollup(const yy::location& l, Xpr* prn, bool ncsv, Xpr* pqx, Xpr* pmdx);

	virtual ~Stmt_create_rollup() override
	{
        if (p_rollup_spec_Xpr != nullptr) delete p_rollup_spec_Xpr;
        if (p_quantity_Xpr    != nullptr) delete p_quantity_Xpr;
        if (p_max_droop_Xpr   != nullptr) delete p_max_droop_Xpr;
    }

	virtual bool execute() override;
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        os << indent << "Stmt_create_rollup at " << bookmark <<" on rollup spec expression :" << std::endl;
        p_rollup_spec_Xpr->display(indent+indent_increment,os);
        if (haveNocsv) { os << indent << "Stmt_create_rollup at " << bookmark <<" (continued) - [noCsv] option selected." << std::endl; }
        else           { os << indent << "Stmt_create_rollup at " << bookmark <<" (continued) - producing csv files." << std::endl; }
        if (p_quantity_Xpr == nullptr)
        {
            os << indent << "Stmt_create_rollup at " << bookmark <<" (continued) - null pointer to quantity expression." << std::endl;
        }
        else
        {
            os << indent << "Stmt_create_rollup at " << bookmark <<" (continued) - quantity expression:" << std::endl;
            p_quantity_Xpr->display(indent+indent_increment,os);
        }
        if (p_max_droop_Xpr == nullptr)
        {
            os << indent << "Stmt_create_rollup at " << bookmark <<" (continued) - null pointer to max droop expression." << std::endl;
        }
        else
        {
            os << indent << "Stmt_create_rollup at " << bookmark <<" (continued) - max droop max to min expression:" << std::endl;
            p_max_droop_Xpr->display(indent+indent_increment,os);
        }
	}
};

class Stmt_edit_rollup : public Stmt {
public:
    Xpr* p_rollup_name_Xpr {nullptr};
    Xpr* p_parameters_Xpr {nullptr};

// methods
	Stmt_edit_rollup(const yy::location& l, Xpr* prn, Xpr* ppx)
        :  p_rollup_name_Xpr(prn), p_parameters_Xpr(ppx) { bookmark=l;}
	virtual ~Stmt_edit_rollup() override
	{
        if (p_rollup_name_Xpr != nullptr) delete p_rollup_name_Xpr;
        if (p_parameters_Xpr  != nullptr) delete p_parameters_Xpr;
    }

	virtual bool execute() override;
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        if (p_rollup_name_Xpr == nullptr)
        {
            os << indent << "Stmt_go at " << bookmark <<" - null pointer to rollup spec expression." << std::endl;
        }
        else
        {
            os << indent << "Stmt_go at " << bookmark <<" - rollup spec expression:" << std::endl;
            p_rollup_name_Xpr->display(indent+indent_increment,os);
        }
        if (p_parameters_Xpr == nullptr)
        {
            os << indent << "Stmt_go at " << bookmark <<" - null pointer to parameters expression." << std::endl;
        }
        else
        {
            os << indent << "Stmt_go at " << bookmark <<" - parameters expression:" << std::endl;
            p_parameters_Xpr->display(indent+indent_increment,os);
        }
	}
};

class Stmt_go : public Stmt {
public:
    Xpr* p_go_parameters {nullptr};

// methods
	Stmt_go(const yy::location& l, Xpr* prp)
        :  p_go_parameters(prp) { bookmark=l;}
	virtual ~Stmt_go() override
	{
        if (p_go_parameters != nullptr) delete p_go_parameters;
    }

	virtual bool execute() override;
	virtual void display(const std::string& indent, std::ostream& os) override
	{
        if (p_go_parameters == nullptr)
        {
            os << indent << "Stmt_go at " << bookmark <<" - null pointer to parameters expression." << std::endl;
        }
        else
        {
            os << indent << "Stmt_go at " << bookmark <<" - parameters expression:" << std::endl;
            p_go_parameters->display(indent+indent_increment,os);
        }
	}
};
