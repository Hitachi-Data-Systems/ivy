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
#include <sstream>

#include "Frame.h"
#include "Ivy_pgm.h"

SymbolTableEntry::SymbolTableEntry():
	STE_type(type_void),
	entry_form(STE_form::variable),
	STE_flags(0u),
	STE_p_Frame((Frame*)0)
	{ u.STE_frame_offset=0u; }

SymbolTableEntry::SymbolTableEntry(const Type&t, Frame* p_f, unsigned int offset, unsigned int flags):
	STE_type(t),
	entry_form(STE_form::variable),
	STE_flags(flags),
	STE_p_Frame(p_f)
	{ u.STE_frame_offset=offset; }

SymbolTableEntry::SymbolTableEntry(const enum basic_types bt, Frame* p_f, unsigned int offset, unsigned int flags):
    STE_type(bt),
    entry_form(STE_form::variable),
    STE_flags(flags),
    STE_p_Frame(p_f)
    { u.STE_frame_offset=offset; }

SymbolTableEntry::SymbolTableEntry(const SymbolTableEntry& s):
    STE_type(s.STE_type),
	entry_form(s.entry_form),
	STE_flags(s.STE_flags),
    STE_p_Frame(s.STE_p_Frame)
	{
		switch(entry_form) {
			case STE_form::variable:
                u.STE_frame_offset=s.u.STE_frame_offset;
                break;
			case STE_form::OverloadSet:
                u.STE_p_OverloadSet=s.u.STE_p_OverloadSet;
                break;
			default: ; // e.g. STE_form::function
		}
	}

std::ostream& operator<<(std::ostream& o, const SymbolTable& st)
{
    if (st.size() == 0)
    {
        o << "<empty SymbolTable>" << std::endl;
    }
    else
    {
        o << "<SymbolTable>" << std::endl;
        for (auto& pear : st)
        {
            o << "\"" << pear.first << "\" -> " << pear.second.info() << std::endl;
        }
        o << "</SymbolTable>" << std::endl;
    }

    return o;
}


 void SymbolTableEntry::set_offset(unsigned int o)
	{ u.STE_frame_offset=o; return; }

 void SymbolTableEntry::set_static(unsigned int onoff)
	{ if (onoff) STE_flags|= STE_flag_static;
	  else       STE_flags^= STE_flag_static; return;}

 void SymbolTableEntry::set_const(unsigned int onoff)
	{ if (onoff) STE_flags|= STE_flag_const;
	  else       STE_flags^= STE_flag_const; return;}

 void SymbolTableEntry::set_variable_form()
	{ entry_form=STE_form::variable; }

 void SymbolTableEntry::set_function_form()
	{ entry_form=STE_form::function; }

 void SymbolTableEntry::set_OverloadSet_form()
	{ entry_form=STE_form::OverloadSet; }

 void SymbolTableEntry::set_type(enum basic_types bt)
	{ STE_type=bt; return; }

 void SymbolTableEntry::set_type(const Type& t)
	{ STE_type=t; return; }

SymbolTableEntry::~SymbolTableEntry()
{
	switch (entry_form)
	{
	case STE_form::function:
		delete STE_p_Frame;
		break;
	case STE_form::OverloadSet:
		delete u.STE_p_OverloadSet;
		break;
	default:
		;
	}
}

SymbolTableEntry& SymbolTableEntry::operator=(const SymbolTableEntry& other)
{
    STE_type = other.STE_type;
    entry_form = other.entry_form;
    STE_flags = other.STE_flags;
    STE_p_Frame = other.STE_p_Frame;
    u = other.u;
//    if (other.is_an_OverloadSet()) u.STE_p_OverloadSet = other.u.STE_p_OverloadSet;
//    else u.STE_frame_offset = other.u.STE_frame_offset;
    return *this;
}

void* SymbolTableEntry::generate_addr()
{
    if (trace_evaluate) trace_ostream << "SymbolTableEntry::generate_addr() for " << info() << std::endl;

	if ( entry_form != STE_form::variable) throw std::runtime_error("SymbolTableEntry::generate_addr() called for a symbol table entry of a type other than the type representing a variable.");

	size_t offset=u.STE_frame_offset;
	if (!is_static())
	{
		if (STE_p_Frame)
		{
            if (trace_evaluate) trace_ostream << "generating offset " << u.STE_frame_offset << " from " << STE_p_Frame->name() << " frame starting at " << STE_p_Frame->stack_offset << std::endl;

			offset+=STE_p_Frame->stack_offset;
		} else {
            if (trace_evaluate) trace_ostream <<"generating offset " << u.STE_frame_offset << " from global frame starting at " << p_Ivy_pgm->next_avail_static << std::endl;

			offset+=p_Ivy_pgm->next_avail_static;
		}
	}
    else
    {
        if (trace_evaluate) trace_ostream << "generating static address " << u.STE_frame_offset << std::endl;
    }

    if (trace_evaluate) trace_ostream << "generated stack offset = " << offset << std::endl;

	if (offset>(p_Ivy_pgm->max_stack_offset)) throw std::runtime_error("ivy interpreter - stack overflow.");

    if (trace_evaluate) switch((enum basic_types)STE_type)
    {
        case type_int:
            trace_ostream << "generated address-> int = " << *((int*)(p_Ivy_pgm->stack+offset)) << std::endl;break;
        case type_double:
            trace_ostream << "generated address-> double = " << *((double*)(p_Ivy_pgm->stack+offset)) << std::endl;break;
        default:;
	}

	return (void*)((p_Ivy_pgm->stack)+offset);
}

std::string SymbolTableEntry::info(const std::string& indent) const
{
	std::ostringstream rv;

	rv << indent << "id=\"" << id << "\" ";

	if (is_static()) rv << "static ";
	if (is_const())  rv << "const ";

	switch (entry_form)
	{
	case STE_form::variable:
		rv << STE_type.print() << " variable at offset " << u.STE_frame_offset << " within ";
		if (is_static()) rv << "static";
		else {
			if (STE_p_Frame) rv << STE_p_Frame->name();
			else             rv << "global";
		}
		rv << " frame";
		break;
	case STE_form::function:
		rv << "function " << printed_form(*(STE_p_Frame->p_arglist));
		break;
	case STE_form::OverloadSet:
		rv << "set of function overloads: " << *u.STE_p_OverloadSet;
		break;
	default:
		throw std::runtime_error("SymbolTableEntry::info() - called with unknown entry form.  Expecting a variable, function, or overload set.");
	}
	return rv.str();
}

