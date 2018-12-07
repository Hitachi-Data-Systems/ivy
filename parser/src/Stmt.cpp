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
#include <sys/stat.h>

#include "Ivy_pgm.h"
#include "Stmt.h"
#include "ivy_engine.h"
#include "SymbolTableEntry.h"

extern std::string inter_statement_divider;

extern bool routine_logging;

bool Stmt_hosts::execute()
{
    if (trace_evaluate) { trace_ostream << "[hosts] statement at " << bookmark << ":" << std::endl; }

    if (p_Ivy_pgm->have_hosts)
    {
        std::ostringstream o;

        o << "<Error> [Hosts] statement at " << bookmark << " - There may be only one [Hosts] statement." << std::endl;

        p_Ivy_pgm->error(o.str());
    }

    p_Ivy_pgm->have_hosts = true;

    std::string hosts_string;

    if (p_hosts_Xpr) p_hosts_Xpr->evaluate((void*)&hosts_string);

    if (hosts_string.size() == 0)
    {
        std::ostringstream o;

        o << "<Error> [Hosts] statement at " << bookmark << " - missing or null list of test hosts." << std::endl;
        p_Ivy_pgm->error(o.str());
    }


    std::string select_string;

    if (p_select_Xpr) p_select_Xpr->evaluate((void*)&select_string);

    if (select_string.size() == 0)
    {
        std::ostringstream o;

        o << "[Hosts] statement at " << bookmark << " - error - missing or null [Select] clause." << std::endl;
        p_Ivy_pgm->error(o.str());
    }

    std::ostringstream console_msg;

    extern std::string inter_statement_divider;

    console_msg
        << inter_statement_divider << std::endl
        << "[Hosts] \"" << hosts_string << "\" [Select] \"" << select_string << "\";" << std::endl << std::endl;

    std::cout << console_msg.str();

    std::pair<bool,std::string>
        rc = m_s.startup(p_Ivy_pgm->output_folder_root,p_Ivy_pgm->test_name, p_Ivy_pgm->ivyscript_filename, hosts_string, select_string);

    if (!rc.first)
    {
        p_Ivy_pgm->error(rc.second);
    }

    return false; // false means "no return statement was executed"
}

Stmt* make_Stmt_Xpr(Xpr *p_xpr, int perform_once)
{
	switch ((enum basic_types)p_xpr->genre()) {
	case type_int:
		if (perform_once) return (Stmt*) new Stmt_once(
			(Stmt*) new Stmt_xpr<int>(p_xpr));
		else return (Stmt*) new Stmt_xpr<int>(p_xpr);
	case type_double:
		if (perform_once) return (Stmt*) new Stmt_once((Stmt*) new Stmt_xpr<double>(p_xpr));
		else return (Stmt*)new Stmt_xpr<double>(p_xpr);
	case type_string:
		if (perform_once) return (Stmt*)new Stmt_once((Stmt*)new Stmt_xpr<std::string>(p_xpr));
		else return (Stmt*)new Stmt_xpr<std::string>(p_xpr);
	default:
		assert(0);
	}
	return nullptr; // keep compiler happy
}

bool Stmt_return::execute()
{
	if (trace_evaluate)
	{
        if (p_xpr == nullptr)
        {
            trace_ostream << "Stmt_return at " << bookmark << " has null pointer to its return expression." << std::endl;
        }
        else
        {
            trace_ostream << "Stmt_return at " << bookmark << " is executing its return expression:" << std::endl;
        }

//        trace_ostream
//            << "Stmt_return::execute() - p_Ivy_pgm = " << p_Ivy_pgm << ", p_Ivy_pgm->stack = " << ((void*)(p_Ivy_pgm->stack) )
//            << ", p_Ivy_pgm->executing_frame = " << (p_Ivy_pgm->executing_frame) << std::endl;}
    }

	// the stack entry is the pointer to the return value

	void* p_p_return = (void*) &(p_Ivy_pgm->stack[p_Ivy_pgm->executing_frame]);

//	if (trace_evaluate) trace_ostream << "Stmt_return::execute() - p_p_return = " << p_p_return << std::endl;

	void* p_return = * ( (void**) p_p_return );

//	if (trace_evaluate) trace_ostream << "Stmt_return::execute() - p_return = " << p_return << std::endl;

	p_xpr->evaluate(p_return);
	return true;
}

bool Stmt_while::execute()
{
    int test_result;

    if (trace_evaluate) { trace_ostream  << "Stmt_while executing top of loop test." << std::endl; }

    p_test_x->evaluate((void*)&test_result);
    while(test_result)
    {
        if (trace_evaluate) { trace_ostream  << "while-loop test result is true, executing loop body statement." << std::endl; }
        if (p_body_Stmt->execute())
        {
            if (trace_evaluate) { trace_ostream  << "while-loop body executed a \"return\" statement, exiting." << std::endl; }
            return true; // return Stmt
        }
        if (trace_evaluate) { display(0,trace_ostream); trace_ostream  << " executing top of loop test." << std::endl; }
        p_test_x->evaluate((void*)&test_result);
    }
    return false;
}

bool Stmt_do::execute()
{
    if (trace_evaluate) { trace_ostream << "Stmt_do: starting." << std::endl; }

    int test_result {1}; // not a bool because ivyscript doesn't have bool, uses int like old time C for logical expressions.
    while(test_result)
    {
        if (trace_evaluate) { trace_ostream << "Stmt_do: executing loop body statement." << std::endl; }
        if (p_body_Stmt->execute())
        {
            if (trace_evaluate) { trace_ostream  << "Stmt_do: loop body executed a \"return\" statement, exiting." << std::endl; }
            return true; // return Stmt
        }
        p_test_x->evaluate((void*)&test_result);
    }
    return false;
}

bool Stmt_set_iosequencer_template::execute()
{

    if (trace_evaluate) { trace_ostream << "Stmt_set_iosequencer_template: starting." << std::endl; }

    if ( !m_s.haveHosts )
    {
        std::ostringstream o;
        o << "At " << bookmark << " - error - no preceeding [hosts] statement to start up the ivy engine and discover the test configuration." << std::endl;
        p_Ivy_pgm->error(o.str());
    }


    std::string iogen, parameters;

    if (p_iosequencer_Xpr==nullptr) throw std::runtime_error("Aaah! Stmt.cpp: Stmt_set_iosequencer_template::execute() p_iosequencer_Xpr==nullptr!");
    if (p_parameters_Xpr==nullptr) throw std::runtime_error("Aaah! Stmt.cpp: Stmt_set_iosequencer_template::execute() p_parameters_Xpr==nullptr!");

    p_iosequencer_Xpr->evaluate((void*)&iogen);
    p_parameters_Xpr->evaluate((void*)&parameters);

    std::ostringstream console_msg;
    console_msg << inter_statement_divider << std::endl << "[SetIosequencerDefault] \"" << iogen << "\" [Parameters] \"" << parameters << "\";" << std::endl << std::endl;
    std::cout << console_msg.str();
    log(m_s.masterlogfile,console_msg.str());

    std::pair<bool,std::string> rc = m_s.set_iosequencer_template(iogen,parameters);
    if (!rc.first) p_Ivy_pgm->error(rc.second);

    m_s.iosequencer_template_was_used = true;
    m_s.print_iosequencer_template_deprecated_msg();

    return false;
}

Stmt_create_rollup::Stmt_create_rollup(const yy::location& l, Xpr* prn, bool ncsv, Xpr* pqx, Xpr* pmdx)
        :  p_rollup_spec_Xpr(prn), haveNocsv(ncsv), p_quantity_Xpr(pqx), p_max_droop_Xpr(pmdx)
{
    bookmark=l;

    if (p_rollup_spec_Xpr == nullptr) throw std::runtime_error("Stmt_create_rollup constructor called with null pointer to rollup spec expression.");

    if (p_quantity_Xpr == nullptr) haveQuantity=false;
    else                           haveQuantity=true;

    if (p_max_droop_Xpr == nullptr) haveMaxDroopMaxtoMinIOPS=false;
    else                            haveMaxDroopMaxtoMinIOPS=true;

}

bool Stmt_create_rollup::execute()
{
    if (trace_evaluate) { trace_ostream << "Stmt_create_rollup: starting." << std::endl; }

    // [CreateRollup] Serial_Number+LDEV
    // [CreateRollup] host               [nocsv] [quantity] 16 [maxDroopMaxtoMinIOPS] 25 %

    std::string attributeNameComboText;

    ivy_int int_quantity = 1;

    p_rollup_spec_Xpr->evaluate((void*)&attributeNameComboText);

    std::ostringstream console_msg;
    console_msg
        << inter_statement_divider << std::endl
        << "[CreateRollup] \"" << attributeNameComboText << "\"";

    if (haveQuantity)
    {
        p_quantity_Xpr->evaluate((void*)&int_quantity);
        console_msg << " [quantity] " << int_quantity;
        if (int_quantity <= 0)
        {
            std::ostringstream o;
            o << "[CreateRollup] at " << bookmark << " failed - [quantity] expression evaluated to " << int_quantity << " which must be greater than zero." << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }
    }

    ivy_float max_IOPS_droop;

    if (haveMaxDroopMaxtoMinIOPS)
    {
        if (p_max_droop_Xpr == nullptr) throw std::runtime_error("Stmt_create_rollup::execute() - my p_max_droop_Xpr == nullptr.");

        p_max_droop_Xpr->evaluate((void*)&max_IOPS_droop);
        console_msg << " [MaxDroop] " << max_IOPS_droop;
        if (max_IOPS_droop < 0. || max_IOPS_droop > 1.)
        {
            std::ostringstream o;
            o << "[CreateRollup] at " << bookmark << " failed - [MaxDroop] expression evaluated to " << max_IOPS_droop << " which must be in the range from 0.0 to 1.0 ( 0% to 100%)." << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }
    }

    console_msg << ";" << std::endl << std::endl;

    std::cout << console_msg.str();
    log (m_s.masterlogfile, console_msg.str());

    if (trace_evaluate)
    {
        trace_ostream << "Stmt_create_rollup::execute() - [CreateRollup] \"" << attributeNameComboText << "\" ";
        if (haveNocsv) trace_ostream << "[noCsv] ";
        if (haveQuantity) {trace_ostream << "[Quantity] " << int_quantity << " ";}
        if (haveMaxDroopMaxtoMinIOPS) {trace_ostream << "[MaxDroop] " << max_IOPS_droop;}
        trace_ostream << std::endl;

        trace_ostream << "About to call ivy engine create rollup." << std::endl;
    }

    std::pair<bool,std::string> rc = m_s.create_rollup
    (
          attributeNameComboText
        , haveNocsv
        , haveQuantity
        , haveMaxDroopMaxtoMinIOPS
        , int_quantity
        , max_IOPS_droop
    );

    if ( !rc.first )
    {
        std::ostringstream o;
        o << "<Error> ivy engine - create rollup failed - " << rc.second << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        p_Ivy_pgm->error(o.str());
    }

    return false;
}

Stmt_delete_rollup::Stmt_delete_rollup(const yy::location& l, Xpr* prn)
  : p_rollup_spec_Xpr(prn)
{
    bookmark=l;
}


bool Stmt_delete_rollup::execute()
{
//    std::cout << std::endl
//    << "     |" << std::endl
//    << "     |" << std::endl
//    << "     |" << std::endl
//    << "     |" << std::endl
//    << "     =========> Stmt_delete_rollup::execute()" << std::endl;

    if (trace_evaluate) { trace_ostream << "Stmt_delete_rollup: starting." << std::endl; }

    // [DeleteRollup] "Serial_Number+LDEV";

    std::string attributeNameComboText;

    if (p_rollup_spec_Xpr) p_rollup_spec_Xpr->evaluate((void*)&attributeNameComboText);

    {
        std::ostringstream o;
        o << inter_statement_divider << std::endl << "[DeleteRollup] \"" << attributeNameComboText << "\";" << std::endl << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
    }

    std::pair<bool,std::string> rc = m_s.delete_rollup(attributeNameComboText);

    if (!rc.first) p_Ivy_pgm->error(rc.second);

///--------------------


    return false;
}


bool Stmt_create_workload::execute()
{
    if (trace_evaluate) { trace_ostream << "Stmt_create_workload: starting." << std::endl; }

    if ( !m_s.haveHosts )
    {
        std::ostringstream o;
        o << "At " << bookmark << " - error - no preceeding [hosts] statement to start up the ivy engine and discover the test configuration." << std::endl;
        p_Ivy_pgm->error(o.str());
    }

    std::string select_string;
    if (p_select_Xpr) p_select_Xpr->evaluate((void*)&select_string);

    std::string workloadName, iosequencerName, parameters;

    if (p_workload_name_Xpr == nullptr) throw std::runtime_error("Ouch! Stmt_create_workload::execute() - p_workload_name_Xpr is nullptr.");
    if (p_iosequencer_Xpr == nullptr) throw std::runtime_error("Ouch! Stmt_create_workload::execute() - p_iosequencer_Xpr is nullptr.");

    p_workload_name_Xpr->evaluate((void*)&workloadName);

    std::ostringstream console_msg;
    console_msg << inter_statement_divider << std::endl << "[CreateWorkload] \"" << workloadName << "\"";


    p_iosequencer_Xpr->evaluate((void*)&iosequencerName);
    if (haveParameters)
    {
        p_parameters_Xpr->evaluate((void*)&parameters);
        console_msg << " [Parameters] \"" << parameters << "\"";
    }

    console_msg << ";" << std::endl << std::endl;

    std::cout << console_msg.str();
    log(m_s.masterlogfile, console_msg.str());

    if (trace_evaluate)
    {
        trace_ostream << "Stmt_create_workload::execute() - [CreateWorkload] \"" << workloadName << "\" ";
        trace_ostream << "[Select] " << select_string << " ";
        trace_ostream << "[Iosequencer] " << iosequencerName << " ";
        trace_ostream << "[Parameters] " << parameters << " ;" << std::endl;
        trace_ostream << "About to call m_s::createWorkload()." << std::endl;
    }

    std::pair<bool,std::string> rc = m_s.createWorkload(workloadName, select_string, iosequencerName, parameters);
    if (!rc.first)
    {
        std::ostringstream o;
        o << "[CreateWorkload] at " << bookmark << " failed - " << rc.second << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }

    //m_s.need_to_rebuild_rollups=true; - removed so scripting language builtins see up to date data structures
    m_s.rollups.rebuild();

    return false;
}


bool Stmt_delete_workload::execute()
{
    if (trace_evaluate) { trace_ostream << "Stmt_delete_workload: starting." << std::endl; }

    if ( !m_s.haveHosts )
    {
        std::ostringstream o;
        o << "At " << bookmark << " - error - no preceeding [hosts] statement to start up the ivy engine and discover the test configuration." << std::endl;
        p_Ivy_pgm->error(o.str());
    }

    std::string workloadName {};

    if (p_workload_name_Xpr != nullptr)
    {
        p_workload_name_Xpr->evaluate((void*)&workloadName);
    }

    std::ostringstream console_msg;
    console_msg << inter_statement_divider << std::endl << "[DeleteWorkload] \"" << workloadName << "\";" << std::endl << std::endl;
    std::cout << console_msg.str();
    log(m_s.masterlogfile,console_msg.str());


    std::string select_string;
    if (p_select_Xpr) p_select_Xpr->evaluate((void*)&select_string);

    if (trace_evaluate)
    {
        trace_ostream << "Stmt_delete_workload::execute() - [DeleteWorkload] \"" << workloadName << "\" [select] \"" << select_string <<"\"." << std::endl;

        trace_ostream << "About to call m_s::deleteWorkload()." << std::endl;
    }

    std::pair<bool,std::string> rc = m_s.deleteWorkload(workloadName, select_string);
    if (!rc.first)
    {
        std::ostringstream o;
        o << "[DeleteWorkload] at " << bookmark << " failed - " << rc.second << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }

    //m_s.need_to_rebuild_rollups=true; - removed so scripting language builtins see up to date data structures
    m_s.rollups.rebuild();

    return false;
}



bool Stmt_edit_rollup::execute()
{
    if (trace_evaluate) { trace_ostream << "Stmt_edit_rollup: starting." << std::endl; }

    if ( !m_s.haveHosts )
    {
        std::ostringstream o;
        o << "At " << bookmark << " - error - no preceeding [hosts] statement to start up the ivy engine and discover the test configuration." << std::endl;
        p_Ivy_pgm->error(o.str());
    }

    std::string rollupSpecText{}, parameters{};

    p_rollup_name_Xpr->evaluate((void*)&rollupSpecText);
    p_parameters_Xpr->evaluate((void*)&parameters);

    std::ostringstream console_msg;
    console_msg << inter_statement_divider << std::endl
        << "[EditRollup] \"" << rollupSpecText << "\" [Parameters] \"" << parameters << "\";" << std::endl << std::endl;

    std::cout << console_msg.str();
    if (routine_logging) log(m_s.masterlogfile, console_msg.str());

    std::pair<bool,std::string>
    rc = m_s.edit_rollup(rollupSpecText, parameters);
    if (!rc.first )
    {
        std::ostringstream o;
        o << "<Error> ivy engine API edit_rollup(\"" << rollupSpecText << "\", \"" << parameters << "\") failed - " << rc.second << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        p_Ivy_pgm->error(o.str());
    }

    return false;
}


bool Stmt_go::execute()
{
    if (trace_evaluate) { trace_ostream << "Stmt_go: starting." << std::endl; }

    std::string parameters {};

    if (p_go_parameters) { p_go_parameters->evaluate((void*)&parameters); }

    {
        std::ostringstream o;
        o << inter_statement_divider << std::endl
            << "[Go!] \"" << parameters << "\";" << std::endl << std::endl;
        std::cout << o.str();
        log(m_s.masterlogfile,o.str());
    }

    std::pair<bool,std::string> rc = m_s.go(parameters);

    if (!rc.first) p_Ivy_pgm->error(rc.second);

    return false;
}


Stmt_for_xprlist::Stmt_for_xprlist(const yy::location& l, std::pair<const std::string,SymbolTableEntry>* p_Sp, std::list<Xpr*>* p_xl, Stmt* pS)
        : p_STE_pair(p_Sp), p_xprlist(p_xl), pStmt(pS)
{
    bookmark = l;

    if (p_STE_pair == nullptr || (p_STE_pair->second.entry_form) != STE_form::variable)
        throw std::runtime_error("Stmt_for_int_xprlist - invalid int variable to assign to.");

    if (p_xprlist == nullptr) throw std::runtime_error("Stmt_for_int_xprlist - null pointer to xprlist.");

    if (pStmt == nullptr) throw std::runtime_error("Stmt_for_int_xprlist - null pointer to loop body statement.");

    for (Xpr* plistX : *p_xprlist)
    {
        Xpr* p_new;

        if (!assign_types_OK(bookmark, &p_new,&(p_STE_pair->second),plistX))
        {
            std::ostringstream o;
            o << "for " << p_STE_pair->second.STE_type.print() << " " <<  p_STE_pair->first <<  " = { <expression>, ... } ; at " << bookmark
                << " - error - cannot assign a value from an expression of type " << plistX->genre().print() <<  "." << std::endl;
            p_Ivy_pgm->error(o.str());
        }
        assign_xprlist.push_back(p_new);
    }
}


bool Stmt_for_xprlist::execute()
{
    ivy_int int_result;
    ivy_float float_result;
    std::string string_result;

    if (trace_evaluate) { display(0,trace_ostream); trace_ostream  << " executing." << std::endl; }

    for (Xpr* pXpr : assign_xprlist) // constructor ensures not null
    {
        if (trace_evaluate) { trace_ostream  << "for <variable> = { ...} - about to assign and then execute loop body." << std::endl; }

        switch (pXpr->genre().value_type)
        {
            case type_int:
                pXpr->evaluate((void*)&int_result); // perform variable assignment;
                break;
            case type_double:
                pXpr->evaluate((void*)&float_result); // perform variable assignment;
                break;
            case type_string:
                pXpr->evaluate((void*)&string_result); // perform variable assignment;
                break;
            default:
                {
                    std::ostringstream o;
                    o << "Stmt_for_xprlist::execute(): Assignment expression is not type_int, nor type_double, nor type_string, but instead " << pXpr->genre().value_type;
                    p_Ivy_pgm->error(o.str());
                }
        }

        if (pStmt->execute())
        {
            if (trace_evaluate) { trace_ostream  << "for-loop body statement executed a \"return\" statement." << std::endl; }
            return true;
        }
    }

    return false;
}


void Stmt_for_xprlist::display(const std::string& indent, std::ostream& os)
{
    os << indent << "Stmt_for_xprlist at " << bookmark
        << " assigning to " << p_STE_pair->first << " " << p_STE_pair->second.info()
        << " has " << p_xprlist->size() << " expressions to assign from:"<< std::endl;

    for (Xpr* p : *p_xprlist) p->display(indent+indent_increment,os);

    os << indent << "Stmt_for_xprlist has body statement:" << std::endl;
    pStmt->display(indent+indent_increment,os);
}









