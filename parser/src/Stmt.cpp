//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#include <sys/stat.h>

#include "Ivy_pgm.h"
#include "Stmt.h"
#include "master_stuff.h"
#include "SymbolTableEntry.h"

extern std::string inter_statement_divider;

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

bool Stmt_set_iogenerator_template::execute()
{

    if (trace_evaluate) { trace_ostream << "Stmt_set_iogenerator_template: starting." << std::endl; }

    if ( !m_s.haveHosts )
    {
        std::ostringstream o;
        o << "At " << bookmark << " - error - no preceeding [hosts] statement to start up the ivy engine and discover the test configuration." << std::endl;
        p_Ivy_pgm->error(o.str());
    }


    std::string iogen, parameters;

    if (p_iogenerator_Xpr==nullptr) throw std::runtime_error("Aaah! Stmt.cpp: Stmt_set_iogenerator_template::execute() p_iogenerator_Xpr==nullptr!");
    if (p_parameters_Xpr==nullptr) throw std::runtime_error("Aaah! Stmt.cpp: Stmt_set_iogenerator_template::execute() p_parameters_Xpr==nullptr!");

    p_iogenerator_Xpr->evaluate((void*)&iogen);
    p_parameters_Xpr->evaluate((void*)&parameters);

    std::ostringstream console_msg;
    console_msg << inter_statement_divider << std::endl << "[SetIogeneratorDefault] \"" << iogen << "\" [Parameters] \"" << parameters << "\";" << std::endl;
    std::cout << console_msg.str();
    log(m_s.masterlogfile,console_msg.str());

    std::unordered_map<std::string,IogeneratorInput*>::iterator iogen_it;
    iogen_it=m_s.iogenerator_templates.find(toLower(iogen));
    if (m_s.iogenerator_templates.end()==iogen_it)
    {
        std::ostringstream o;
        o << "<Error> [SetIogeneratorDefault] - invalid iogenerator name \"" << iogen << "\". "  << std::endl;
        o << "Valid iogenerator names are:";
        for (auto& pear : m_s.iogenerator_templates) o << " \"" << pear.first << "\" ";
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }
    IogeneratorInput* p=(*iogen_it).second;

    std::string error_message;
    if (!p->setMultipleParameters(error_message, parameters))
    {
        std::ostringstream o;
        o << "[SetIogeneratorDefault] at " << bookmark << " - For iogenerator named \"" << iogen << "\" - error setting parameter values \"" << parameters << "\" - error message was - " << error_message << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }

//    {
//        std::ostringstream o;
//        o << "[SetIogeneratorDefault] at " << bookmark << " - iogenerator template for \"" << iogen << "\" has been set to " << p->toString() << std::endl;
//        std::cout << o.str();
//        log(m_s.masterlogfile,o.str());
//    }

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

    if ( !m_s.haveHosts )
    {
        std::ostringstream o;
        o << "At " << bookmark << " - error - no preceeding [hosts] statement to start up the ivy engine and discover the test configuration." << std::endl;
        p_Ivy_pgm->error(o.str());
    }

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

    console_msg << ";" << std::endl;

    std::cout << console_msg.str();
    log (m_s.masterlogfile, console_msg.str());

    if (trace_evaluate)
    {
        trace_ostream << "Stmt_create_rollup::execute() - [CreateRollup] \"" << attributeNameComboText << "\" ";
        if (haveNocsv) trace_ostream << "[noCsv] ";
        if (haveQuantity) {trace_ostream << "[Quantity] " << int_quantity << " ";}
        if (haveMaxDroopMaxtoMinIOPS) {trace_ostream << "[MaxDroop] " << max_IOPS_droop;}
        trace_ostream << std::endl;

        trace_ostream << "About to call m_s::addRollupType()." << std::endl;
    }

    std::string my_error_message;
    if
    ( ! m_s.rollups.addRollupType
        (
            my_error_message
            , attributeNameComboText
            , haveNocsv
            , haveQuantity
            , haveMaxDroopMaxtoMinIOPS
            , "" /*nocsvText*/
            , int_quantity
            , max_IOPS_droop
        )
    )
    {
        std::ostringstream o;
        o << "[CreateRollup] at " << bookmark << " failed - " << my_error_message << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }

    auto it = m_s.rollups.rollups.find(toLower(attributeNameComboText));
    if (m_s.rollups.rollups.end() == it)
    {
        std::ostringstream o;
        o << "[CreateRollup] at " << bookmark << " failed - internal programming error.  Unable to find the new RollupType we supposedly just made."<< std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }

    //m_s.need_to_rebuild_rollups=true; - removed so scripting language builtins see up to date data structures
    m_s.rollups.rebuild();

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

    if ( !m_s.haveHosts )
    {
        std::ostringstream o;
        o << "At " << bookmark << " - error - no preceeding [hosts] statement to start up the ivy engine and discover the test configuration." << std::endl;
        p_Ivy_pgm->error(o.str());
    }

    // [DeleteRollup] "Serial_Number+LDEV";

    std::string attributeNameComboText;

    bool delete_all_not_all {false};

    if (p_rollup_spec_Xpr == nullptr)
    {
        delete_all_not_all = true;
        if (trace_evaluate) { trace_ostream << "Stmt_delete_rollup::execute() - null pointer to rollup spec, deleting all rollups except \"all\"." << std::endl; }

        std::cout << "[DeleteRollup] without rollup name - deleting all rollups except \"all\"." << std::endl;
    }
    else
    {
        p_rollup_spec_Xpr->evaluate((void*)&attributeNameComboText);
        if (trace_evaluate) { trace_ostream << "Stmt_delete_rollup::execute() - rollup spec expression yielded \"" << attributeNameComboText << "\"." << std::endl; }
        std::cout << inter_statement_divider << std::endl << "[DeleteRollup] \"" << attributeNameComboText << "\";" << std::endl;
        if (attributeNameComboText.size()==0 || attributeNameComboText == "*") { delete_all_not_all = true;}
    }

    if (delete_all_not_all)
    {
        // delete all rollups except "all"
        for (auto& pear : m_s.rollups.rollups)
        {
            if (stringCaseInsensitiveEquality("all", pear.first)) continue;

            std::string my_error_message;
            if ( ! m_s.rollups.deleteRollup(my_error_message, pear.first))
            {
                std::ostringstream o;
                o << "[DeleteRollup] at " << bookmark << " failed - " << my_error_message << std::endl;
                log(m_s.masterlogfile,o.str());
                std::cout << o.str();
                m_s.kill_subthreads_and_exit();
            }
        }
    }
    else
    {
        if (stringCaseInsensitiveEquality(std::string("all"),attributeNameComboText))
        {
            std::string msg = "\nI\'m so sorry, Dave, I cannot delete the \"all\" rollup.\n\n";
            log(m_s.masterlogfile,msg);
            std::cout << msg;
            m_s.kill_subthreads_and_exit();
        }

        if (trace_evaluate)
        {
            trace_ostream << "Stmt_delete_rollup::execute() - [DeleteRollup] \"" << attributeNameComboText << "\" ";
            trace_ostream << std::endl;
            trace_ostream << "About to call m_s.rollups.deleteRollup(\"" << attributeNameComboText << "\")." << std::endl;
        }

        std::string my_error_message;
        if ( ! m_s.rollups.deleteRollup(my_error_message, attributeNameComboText))
        {
            std::ostringstream o;
            o << "[DeleteRollup] at " << bookmark << " failed - " << my_error_message << std::endl;
            log(m_s.masterlogfile,o.str());
            std::cout << o.str();
            m_s.kill_subthreads_and_exit();
        }
    }

    //m_s.need_to_rebuild_rollups=true; - removed so scripting language builtins see up to date data structures
    m_s.rollups.rebuild();

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


    std::string workloadName, iogeneratorName, parameters;

    if (p_workload_name_Xpr == nullptr) throw std::runtime_error("Ouch! Stmt_create_workload::execute() - p_workload_name_Xpr is nullptr.");
    if (p_iogenerator_Xpr == nullptr) throw std::runtime_error("Ouch! Stmt_create_workload::execute() - p_iogenerator_Xpr is nullptr.");

    p_workload_name_Xpr->evaluate((void*)&workloadName);

    std::ostringstream console_msg;
    console_msg << inter_statement_divider << std::endl << "[CreateWorkload] \"" << workloadName << "\"";


    p_iogenerator_Xpr->evaluate((void*)&iogeneratorName);
    if (haveParameters)
    {
        p_parameters_Xpr->evaluate((void*)&parameters);
        console_msg << " [Parameters] \"" << parameters << "\"";
    }

    console_msg << std::endl;

    std::cout << console_msg.str();
    log(m_s.masterlogfile, console_msg.str());

    if (trace_evaluate)
    {
        trace_ostream << "Stmt_create_workload::execute() - [CreateWorkload] \"" << workloadName << "\" ";

        if (p_Select == nullptr) { trace_ostream << "<p_Select is nullptr> "; }
        else                     { trace_ostream << "<Select clause describes itself starting on the next line.> "; }

        trace_ostream << "[Iogenerator] " << iogeneratorName << " ";
        trace_ostream << "[Parameters] " << parameters << " ;" << std::endl;

        if (p_Select!=nullptr) {p_Select->display("",trace_ostream); }

        trace_ostream << "About to call m_s::createWorkload()." << std::endl;
    }

    std::string my_error_message;
    if (!m_s.createWorkload(my_error_message, workloadName, p_Select, iogeneratorName, parameters))
    {
        std::ostringstream o;
        o << "[CreateWorkload] at " << bookmark << " failed - " << my_error_message << std::endl;
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
    console_msg << inter_statement_divider << std::endl << "[DeleteWorkload] \"" << workloadName << "\";" << std::endl;
    std::cout << console_msg.str();
    log(m_s.masterlogfile,console_msg.str());

    if (trace_evaluate)
    {
        trace_ostream << "Stmt_delete_workload::execute() - [DeleteWorkload] \"" << workloadName << "\" ";

        if (p_Select == nullptr)
            {
                trace_ostream << "<p_Select is nullptr> ";
            }
        else
        {
            trace_ostream << "<Select clause describes itself starting on the next line.> ";
            p_Select->display("",trace_ostream);
        }

        trace_ostream << "About to call m_s::deleteWorkload()." << std::endl;
    }

    std::string my_error_message;
    if (!m_s.deleteWorkload(my_error_message, workloadName, p_Select))
    {
        std::ostringstream o;
        o << "[DeleteWorkload] at " << bookmark << " failed - " << my_error_message << std::endl;
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

    std::string my_error_message{}, rollupSpecText{}, parameters{};

    p_rollup_name_Xpr->evaluate((void*)&rollupSpecText);
    p_parameters_Xpr->evaluate((void*)&parameters);

    std::ostringstream console_msg;
    console_msg << inter_statement_divider << std::endl;  // m_s.editRollup() prints a console line message
    std::cout << console_msg.str();
    log(m_s.masterlogfile, console_msg.str());

    if (trace_evaluate) { trace_ostream << "Stmt_edit_rollup: about to m_s.editRollup(, rollupSpecText = \"" << rollupSpecText << "\", parameters = \"" << parameters <<"\"):" << std::endl; }

    if (!( m_s.editRollup(my_error_message, rollupSpecText, parameters) ))
    {
        std::ostringstream o;
        o << m_s.ivyscript_line_number << ": \"" << m_s.ivyscript_line << "\"" << std::endl
          << "- [EditRollup] failed - " << my_error_message << std::endl;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        m_s.kill_subthreads_and_exit();
    }

    return false;
}


bool Stmt_go::execute()
{
    if (trace_evaluate) { trace_ostream << "Stmt_go: starting." << std::endl; }

    if ( !m_s.haveHosts )
    {
        std::ostringstream o;
        o << "At " << bookmark << " - error - no preceeding [hosts] statement to start up the ivy engine and discover the test configuration." << std::endl;
        p_Ivy_pgm->error(o.str());
    }

    if (p_go_parameters == nullptr) { m_s.goParameters.clear(); }
    else                            { p_go_parameters->evaluate((void*)&m_s.goParameters); }

    {
        std::ostringstream o;
        o << inter_statement_divider << std::endl << "[Go!] \"" << m_s.goParameters << "\" ;" << std::endl;
        log (m_s.masterlogfile,o.str());
        std::cout << o.str();
    }

    void go_statement(yy::location);
    go_statement(bookmark);

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









