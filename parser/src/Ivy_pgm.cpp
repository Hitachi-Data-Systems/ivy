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
#include <iostream>
#include <sstream>

#include "Type.h"
#include "Ivy_pgm.h"
#include "Stmt.h"
#include "ivy.parser.hh"
#include "ivy_engine.h"
#include "ivyhelpers.h"


Ivy_pgm::Ivy_pgm(const std::string& f, const std::string& tn) : ivyscript_filename(f), test_name(tn)
{
    blockStack.push_front(new Block()); // this is the outermost or "global Block".
    init_builtin_table();
    //*debug*/ if (trace_parser) std::cout << "builtin_table:" << std::endl << builtin_table << std::endl;
    /*debug*/ if (trace_parser) std::cout << "&pile = " << &pile << ", stack=" << ( (void*) stack) << ", Ivy_pgm at " << this << std::endl;
}

Ivy_pgm::~Ivy_pgm()
{
    for (auto& pBlock : blockStack) delete pBlock;
}

void Ivy_pgm::snapshot(std::ostream& o)
{
    o << std::endl << "------------------------------ start of snapshot ---------------------------------" << std::endl;
    o << "Ivy_pgm::snapshot: executing_frame = " << executing_frame << ", next_avail_static = " << next_avail_static << ", blockStack.size() = "
        << blockStack.size() << ", decl_prefix_stack.size() = " << decl_prefix_stack.size() << std::endl;
    o << "p_global_Block() = " << p_global_Block() << ", p_current_Block() = " << p_current_Block()
        << ", successful_compile = " << ( successful_compile ? "true" : "false" ) << ", unsuccessful_compile = " << ( unsuccessful_compile ? "true" : "false" ) << std::endl;
    for (auto& pBlock : blockStack)
    {
        o << std::endl << "Block at " << pBlock << " ";
        pBlock->display("",o);
    }
    for (auto& pSTE : decl_prefix_stack)
    {
        o << std::endl << "decl_prefix_stack pointer to SymbolTableEntry " << pSTE << " " << pSTE->info() << std::endl;
    }
    o << "------------------------------- end of snapshot --------------------------------" << std::endl;
}

std::pair<const std::string,SymbolTableEntry>* Ivy_pgm::SymbolTable_search(const std::string& id)
{
	for (auto& pBlock : blockStack)
	{
		auto ST_iter = pBlock->symbol_table.find(id);
		if (ST_iter != pBlock->symbol_table.end()) return &(*ST_iter);
	};
	return (nullptr);
}

std::pair<const std::string,SymbolTableEntry>* Ivy_pgm::add_variable(const std::string& id)
{
	// - insert ID into this block's symbol table,
	// and barfing if ID already exists in this block
	// - allocate space on the stack (static or dynamic)
	// - bump up max used offset within block

	SymbolTable& st = blockStack.front()->symbol_table;

	auto it = st.find(id);
    if (it != st.end())
    {
		error(std::string("duplicate declaration of \"") + id + std::string("\""));
		return nullptr;
	}

    st[id] = ( *(decl_prefix_stack.front()) ); // copy the template in decl_prefix_stack.front(), which may be reused when declaring multiple variables like "int i,j;"

    auto& ste = st[id];

    ste.id = id;

    if (ste.is_static())
    {
        ste.set_offset(next_avail_static);
        next_avail_static += (ste.genre()).maatvan();
        if (trace_parser) std ::cout << "Ivy_pgm::add_variable(\"" << id << "\") - added static variable at offset " << ste.u.STE_frame_offset << ".  New next_avail_static = " << next_avail_static << std::endl;
    } else {
        ste.set_offset(p_current_Block()->next_avail);
        blockStack.front()->next_avail+=(ste.genre()).maatvan();
        if (trace_parser) std ::cout << "Ivy_pgm::add_variable(\"" << id << "\") - added variable at offset " << ste.u.STE_frame_offset
            << " in block at address " << p_current_Block() << ". New next_avail in block = " << p_current_Block()->next_avail<< std::endl;
    }

	// if variable type needs a destructor, chain
	// destructor to beginning of block exit statement
	// list (destruct in reverse order of construction).
	if ((ste.genre()).needs_destructor()) {
		// destructors for static variables are chained
		// on the destructors list for the global (first) block
		( ste.is_static() ? p_global_Block() : p_current_Block() ) -> destructors.push_front( new Stmt_destructor(&ste) );
	}
	it = st.find(id);
	if (it == st.end())
	{
        throw std::runtime_error(std::string("Ivy_pgm::add_variable() - this isn\'t supposed to be able to happen."));
	}

	if (trace_parser) snapshot(std::cout);

	return &(*it);
}

void Ivy_pgm::chain_default_initializer(const yy::ivy::location_type& l, std::pair<const std::string,SymbolTableEntry>* p_STE_pair)
{
	// chain default initializer head of block's
	// executable code list;

	if (trace_parser)
	{
        std::cout << "Ivy_pgm::chain_default_initializer(" << p_STE_pair << "-> <\"" << p_STE_pair->first << "\", " << p_STE_pair->second.info() << ">"
            << " at " << l << std::endl;
	}

	p_current_Block()->constructors.push_back( (Stmt*) new Stmt_constructor(l, &(p_STE_pair->second)) );

	return;
}

int Ivy_pgm::chain_explicit_initializer(const yy::ivy::location_type& l, std::pair<const std::string,SymbolTableEntry>* p_STE_pair, Xpr* p_xpr)
{
	// If type needs explicit initialization,
	// queue one on the code block.
	// Then queue the assignment statement.

	auto& ste = p_STE_pair->second;

	if (ste.genre().needs_constructor()) {
		p_current_Block()->constructors.push_back( (Stmt*) new Stmt_constructor(l, &ste) );
		// constructors for static variables are "perform once"
	}

	Xpr* p_init_xpr;

	if (assign_types_OK(l, &p_init_xpr,&ste,p_xpr))
	{
		blockStack.front()->code.push_back( (Stmt*) make_Stmt_Xpr(p_init_xpr, (ste.is_static())) );
	// here, for static variables, the assignment
	// statement is fitted with a "perform once" adapter
	} else {
		error(std::string("assignment type mismatch \"")
		+ p_STE_pair->first + "\" ("
		+ (ste.genre()).print() + ") = ("
		+ (p_xpr->genre()).print() + ")");
		return 1;
	}
	return 0;
}

Frame* Ivy_pgm::declare_function(std::string* id, std::list<Arg>* p_arglist)
{
    /*debug*/ if (trace_parser) {std::cout << "Ivy_pgm::declare_function() entry snapshot: "; snapshot(std::cout);}

    if (p_arglist == nullptr)
    {
        error("Ivy_pgm::declare_function() called with null pointer to argument list.") ;
    }

	SymbolTableEntry* p_prefix_STE = decl_prefix_stack.front();

	if (p_prefix_STE->is_static())
		{error("\"static\" not supported for function results");}

	if (p_prefix_STE->is_const())
		{error("\"const\" not supported for function results");}

	/* now add function name and type to beginning of arglist */
	p_arglist->push_front(Arg(p_prefix_STE->genre(),*id)); // copy constructor

/*debug*/ if (trace_parser) std::cout << "Ivy_pgm::declare_function() - after push_front( Arg<function_name,return type> ) to the arglist: " << (*p_arglist) << std::endl;

	std::string mangled = mangle(*p_arglist);
	std::string with_return_type = printed_form(*p_arglist);

/*debug*/ if (trace_parser) std::cout << "mangled=\"" << mangled << "\"." << std::endl;
/*debug*/ if (trace_parser) std::cout << "with_return_type=\"" << with_return_type << "\"." << std::endl;


	SymbolTable& st = p_current_Block()->symbol_table;

	/* now get a pointer to this Block's overload list
	   for this function name (check to make sure the
	   ID is not already in use as a variable) */

	OverloadSet* p_OverloadSet;

	SymbolTable::iterator ST_overload_it = st.find(*id);
	if (ST_overload_it == st.end())
	{
        /*debug*/ if (trace_parser) std::cout << "identifier \"" << (*id) << "\" not yet in this SymbolTable." << std::endl;

		/* ID not yet in this SymbolTable */
		/* Make new OverloadSet for this ID */

        auto& overload_STE = st[*id];

        overload_STE.id = *id;

		overload_STE.set_OverloadSet_form();

		p_OverloadSet = overload_STE.u.STE_p_OverloadSet = new OverloadSet;

		ST_overload_it = st.find(*id);
		if (ST_overload_it == st.end())
		{
            error("Ivy_pgm::declare_function() - internal programming error - could not find function overload set SymbolTableEntry after creating it.");
		}

	} else {

        /*debug*/ if (trace_parser) std::cout << "identifier \"" << (*id) << "\" Found in this SymbolTable." << std::endl;

		/* lookup() of ID in SymbolTable returned something */
		auto& overload_STE = ST_overload_it->second;

		if (!overload_STE.is_an_OverloadSet())
		{
			delete p_arglist;
			error(std::string("Name \"") + *id + std::string("\" is already in use, cannot use as a function name."));
			return nullptr;
		}
		p_OverloadSet = overload_STE.p_OverloadSet();

	}

    (*p_OverloadSet)[mangled] = with_return_type;

	Frame* p_Frame;

	auto ST_func_it = st.find(mangled);
	if (ST_func_it != st.end())
	{
		/* debug */ if (trace_parser) std::cout << "mangled name already in SymbolTable." << std::endl;

		auto& func_STE = ST_func_it->second;

		if (!func_STE.is_a_function())
		{
			delete p_arglist;
			error(std::string("Weird programmer error - mangled function name \"")
			 + (mangled)
				+ std::string("\" is already in SymbolTable, but not as function"));
			return nullptr;
		}
		if ( ((enum basic_types) return_type(*p_arglist))
		  != ((enum basic_types) func_STE.genre()))
		{
			error(std::string("Declarations for \"")
				+ printed_form(*(p_Frame->p_arglist)) + "\" and \""
				+ with_return_type + "\" differ only by return type");
			delete p_arglist;
			return nullptr;
		}
		p_Frame = func_STE.p_Frame();

		//  If there was only a previous declaration, but not
		//  a definition of the function, replace the previously
		//  saved parameter names with the ones we are seeing
		//  now.  If it turns out later that we are parsing a
		//  function definition, not just a declaration, then
		//  we will have captured the definitive parameter names.

		if (!(p_Frame->p_code_Block))  // if not previously defined
		{
			delete p_Frame->p_arglist;
				// throw away bogus parameter names from
				// previous declaration (not definition)
			p_Frame->p_arglist = p_arglist;
		} else {
			delete p_arglist;
			// throw away the parameter names
			// we are seeing now.  If this turns out also
			// to be a definition later, this is an error
			// anyway, because there are duplicate definitions.
		}
	} else {
		/* mangled name not yet in SymbolTable */

		/* debug*/ if (trace_parser) std::cout << "mangled name not yet in SymbolTable." << std::endl;

		p_Frame = new Frame(p_arglist);

        /*debug*/ if (trace_parser) std::cout << "built new Frame - " << (*p_Frame) << std::endl;


        auto& func_STE = st[mangled];

        func_STE.id = mangled;

        func_STE = *(decl_prefix_stack.front());
		func_STE.set_function_form();
		func_STE.set_p_Frame(p_Frame);
		func_STE.set_type(return_type(*p_arglist));

		/* first time we've seen this mangled name,
		   put the name stripped of parameter names
		   into the overload list */
	}

	/*debug*/ if (trace_parser)
	{
        std::cout << "At Ivy_pgm::declare_function() exit, identifier \"" << (*id) << "\"\'s OverloadSet = " << (*p_OverloadSet) << std::endl;
        std::cout << "At Ivy_pgm::declare_function() exit, p_current_Block()->symbol_table contains: " << std::endl << p_current_Block()->symbol_table << std::endl;
    }

	return p_Frame;

}

Stmt* Ivy_pgm::make_Stmt_return(const yy::location& l, Xpr* p_xpr)
{
	if (!blockStack.front()->p_Frame)
	{
		error("\"return\" statement in global code outside of any function");
		return nullptr;
	}
	const Type& return_type=blockStack.front()->p_Frame->genre();
	if (return_type.both_numeric_or_both_string(p_xpr->genre()))
	{
		return new Stmt_return(l, make_xpr_type_gender_bender(l,return_type,p_xpr));
	}
	error(std::string("\"return\" expression type \"")
		+ (p_xpr->genre()).print()
		+ std::string("\" is incompatible with function return type \"")
		+ return_type.print() + std::string("\"")
	);
	return nullptr;
}

Xpr* Ivy_pgm::build_function_call(const yy::location& l, const std::string* p_ID, std::list<Xpr*>* p_xprlist)
{
	std::string mangled = mangle(p_ID,p_xprlist);

	auto p_STE_pair = SymbolTable_search(mangled);
	if (p_STE_pair != nullptr)
	{
		assert( p_STE_pair->second.is_a_function() );
		return (Xpr*) new Xpr_function_call(l, p_STE_pair->second.p_Frame(),p_xprlist);
	}

	std::list<Xpr*>::iterator p_it;
	auto BT_iter = builtin_table.find(mangled);
	if (BT_iter != builtin_table.end())
	{
		Builtin& b = BT_iter->second;

		Xpr *arg1, *arg2, *arg3;
		switch (p_xprlist->size())
		{
		case 0:
			return new_nullary_builtin_xpr(l,b);
		case 1:
			arg1 = p_xprlist->front();
			return new_unary_builtin_xpr(l,b,arg1);
		case 2:
			arg1 = p_xprlist->front();
            arg2 = p_xprlist->back();
			return new_binary_builtin_xpr(l,b,arg1,arg2);
		case 3:
            p_it = p_xprlist->begin();
			arg1 = *p_it;
			p_it++;
			arg2 = *p_it;
			p_it++;
			arg3 = *p_it;
			return new_trinary_builtin_xpr(l,b,arg1,arg2,arg3);
		default:
			assert(0);
		}
		return nullptr; // keep compiler happy
	}
	// mangled name not found - generate error message
	// with names of potential overloads to help user
	// pick the right one;

	// Make an empty OverloadSet, then go back through
	// all the Blocks, starting with any built-in overloads
	// and then in reverse order from the global block through
	// the currently running Block.

	// Each time we find an overload in a particular Block
	// we add it into the OverloadSet, overlaying any
	// earlier ones so that the ones in the set at the end
	// are the overloads reachable from this point in the program.

	OverloadSet visible_overloads;

	// first add in any matches on builtin functions

	auto BIOT_it = builtin_overloads.find(*p_ID);
	if (BIOT_it != builtin_overloads.end())
	{
        const OverloadSet& builtin_overload_set = BIOT_it->second;
		for (auto& s : builtin_overload_set) visible_overloads.insert(s);
	}

    // now add in overloads from the outermost block to the innermost, so that
    // it will correctly show the "visible" overload with its return value.

    // My original code from 1998 showed the block nesting level for each visible overload.

    for (std::list<Block*>::reverse_iterator rit = blockStack.rbegin(); rit!=blockStack.rend(); ++rit)
    {
        Block& rit_Block = *(*rit);
        auto rit_ST_it = rit_Block.symbol_table.find(*p_ID);
        if (rit_ST_it != rit_Block.symbol_table.end())
        {
            const SymbolTableEntry& ste = (*rit_ST_it).second;
            if (ste.is_an_OverloadSet())
            {
                const OverloadSet& os = (*ste.u.STE_p_OverloadSet);
                for (auto& s : os) visible_overloads.insert(s);
            }
        }
    }

	if (visible_overloads.size() == 0)
	{
		error(std::string("function \"")+(*p_ID)+std::string("(...)\" not found"));
	} else {
        std::ostringstream o;
        o << "no match found for \"" << printed_form(p_ID,p_xprlist) << "\".  Possible overload candidates are:";
        o << visible_overloads << std::endl;
        error(o.str());
	}
	return (Xpr*)0;
}

void Ivy_pgm::error(const std::string& s)
{
    std::cout << s << std::endl;
    log(m_s.masterlogfile, s);
    m_s.kill_subthreads_and_exit(); // no return from kill_subthreads_and_exit() - it exits.
}

void Ivy_pgm::warning(const std::string& s)
{
	std::ostringstream o;

	warning_count++;

	o << "*** Warning: " <<  s << " ***" << std::endl
        << "*** Line " << ivyscript_srclineno << " of " + ivyscript_filename << " ***" << std::endl;
	std::cout << o.str();
	compile_msg += o.str();
#ifdef _DEBUG
	trace << "\n*** Warning: " << s << " ***\n";
	trace << "\n*** Line " << ivyscript_srclineno << " of " << input_file_name << " ***" <<endl;
#endif
	return;
}

void Ivy_pgm::start_Block() {

    /*debug*/ if (trace_parser)  std::cout << "From level "<< blockStack.size() << " at offset "<<blockStack.front()->next_avail <<" within "<<blockStack.front()->frame_Name()<<" Frame"<<std::endl;

	// start new nested level
	Block* p_Block=new Block(
		p_current_Block()->next_avail,
		p_current_Block()->p_Frame
	);
	blockStack.push_front(p_Block);

    /*debug*/ if (trace_parser)  std::cout << "To new nested level "<<blockStack.size() <<" at offset "<<blockStack.front()->next_avail <<" within "<<blockStack.front()->frame_Name()<<" Frame"<<std::endl;
	return;
}

void Ivy_pgm::end_Block() {
    /*debug*/ if (trace_parser)  std::cout << "Leaving level "<<blockStack.size() <<" at offset "<<blockStack.front()->next_avail <<" within "<<blockStack.front()->frame_Name()<<" Frame"<<std::endl;

	blockStack.pop_front();

    // note that blockStack does not "own" Blocks - they are owned by statements.  So we don't delete a Block when we pop it off blockStack.

    /*debug*/ if (trace_parser)  std::cout << "Back to level "<<blockStack.size() <<" at offset "<<blockStack.front()->next_avail <<" within "<<blockStack.front()->frame_Name()<<" Frame"<<std::endl;

}

void Ivy_pgm::show_syntax_error_location()
{
    std::cout << "Syntax error is from line " << syntax_error_first_line << " column " << syntax_error_first_column
        << " to line " << syntax_error_last_line << " column " << syntax_error_last_column << "." << std::endl << std::endl;

    std::cout << "========================================================================" << std::endl << std::endl;

    std::cout << "Note: To have the syntax error location more accurately pointed out," << std::endl
        << "      please put your inklude statements by themselves on a line." << std::endl << std::endl;

    std::cout << "========================================================================" << std::endl << std::endl;

    load_source_file_for_error_location(ivyscript_filename);

    for (unsigned int line_number = 1; line_number <= source_lines_for_error.size(); line_number++)
    {
        const std::string& line = source_lines_for_error[line_number - 1];

        std::cout << line << std::endl;

        if (line_number == syntax_error_first_line)
        {
            for (unsigned int i = 1; i < syntax_error_first_column; i++)
            {
                std::cout << " ";
            }

            std::cout << "|";

            if (line_number == syntax_error_last_line)
            {
                // all on one line
                if (syntax_error_last_column > syntax_error_first_column)
                {
                    for (unsigned int i = syntax_error_first_column + 1; i < syntax_error_last_column; i++)
                    {
                        std::cout << "=";
                    }
                    std::cout << "|";
                }

            }
            std::cout << std::endl;

            for (unsigned int i = 1; i < syntax_error_first_column; i++)
            {
                std::cout << " ";
            }
            std::cout << "Syntax error" << std::endl;

            for (unsigned int i = 1; i < syntax_error_first_column; i++)
            {
                std::cout << " ";
            }
            std::cout << "============" << std::endl;
        }
        else if (line_number > syntax_error_first_line && line_number < syntax_error_last_line)
        {
            for(unsigned int i = 0; i < (line.size() - 1); i++)
            {
                std::cout << "=";
            }
            std::cout << std::endl<< std::endl;
        }
        else if (line_number != syntax_error_first_line && line_number == syntax_error_last_line)
        {
            for (unsigned int i = 1; i < syntax_error_last_column; i++)
            {
                std::cout << "=";
            }
            std::cout << "|" << std::endl << std::endl;
        }
    }

    std::cout << "========================================================================" << std::endl;
}

std::regex inklude_regex( R"([ \t]*[iI][nN][kK][lL][uU][dD][eE][ \t]*([^ \t].*[^ \t])[ \t]*)" ); // whitespace inklude whitespace (something starting & ending with a non space/tab) whitespace

void Ivy_pgm::load_source_file_for_error_location(const std::string& f)
{
    auto it = inklude_file_names.find(f);

    if (inklude_file_names.end() != it)
    {
        std::cout << "<Error> Ivy_pgm::load_source_file_for_error_location() - inklude file loop, second time loading \"" << ivyscript_filename << "\"." << std::endl;
        exit(-1);
    }

    inklude_file_names.insert(f);

    std::ifstream ifs(f);

    if (!ifs)
    {
        std::cout << "<Error> Ivy_pgm::load_source_file_for_error_location() - failed opening \"" << f << "\"." << std::endl;
        exit(-1);
    }

    std::smatch entire_match;
    std::ssub_match sub;

    for (std::string line; std::getline(ifs,line); )
    {
        source_lines_for_error.push_back(line);

        if (std::regex_match(line, entire_match, inklude_regex))
        {
            sub = entire_match[1];
            load_source_file_for_error_location(sub.str());
        }
    }

    ifs.close();

    return;
}



















