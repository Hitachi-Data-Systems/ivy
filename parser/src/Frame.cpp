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

#include "ivytypes.h"
#include "Ivy_pgm.h"

// A Frame is a template to manage how the stack is used
// when functions are invoked to store the argument parameter list and return value
// to communicate between function caller and callee.

// The function's variables are placed on the stack later as its code runs.

// When a function is called, we lay out space for a pointer to
// where the function should place its return value when it's done,
// and then we lay out space on the stack for each parameter value.

// Depending on the type of variable being place on the stack,
// if it needs a constructor we use a "placement new" which invokes
// the constructor operating on the location we alloccated on the stack.

// Then we chain a new nested Block to run the function, and
// as the function code runs the space for its variables is
// allocated on the stack.

// Before we invoke the function's outermost Block, we populate
// its symbol table with the parameter names specified in the
// function definition.  The SymbolTableEntry for a parameter
// points to a location on the stack where the "variable" (parameter)
// is found.

std::ostream& operator<<(std::ostream& o, const Frame& f)
{
    o << "Frame: arglist = " << (*(f.p_arglist))
        << ", offset_after_args = " << f.offset_after_args
        << ", p_code_Block = " << f.p_code_Block
        << ", stack_offset = " << f.stack_offset << std::endl;
    return o;
}

void Frame::invoke(unsigned int invoked_stack_offset)
{
    if (trace_evaluate) trace_ostream << "Frame::invoke() for " << name() << " being invoked at stack offset " << stack_offset << std::endl;

	unsigned int original_executing_frame = p_Ivy_pgm->executing_frame;
	p_Ivy_pgm->executing_frame = invoked_stack_offset;

	unsigned int original_stack_offset = stack_offset;
	stack_offset = invoked_stack_offset;

    if (trace_evaluate) { trace_ostream << "Frame::invoke() for " << name()
        << " original_executing_frame = " << original_executing_frame << ", original_stack_offset = " << original_stack_offset
        << " New Ivy_pgm::executing_frame = " << p_Ivy_pgm->executing_frame << ", Frame being invoked at stack offset " << stack_offset
        << std::endl; }

	if (!p_code_Block)
	{
		p_Ivy_pgm->error("function body not defined.");
	} else if ( ! ((*p_code_Block).invoke()) )
	{
		p_Ivy_pgm->error("ran off end of function without \"return\" statement");
	}

	stack_offset=original_stack_offset;

	p_Ivy_pgm->executing_frame=original_executing_frame;

	return;
}


void Frame::start_Block() {
    if (trace_parser) trace_ostream << "Frame::start_Block() - from level " << p_Ivy_pgm->blockStack.size()
        << " at offset " << p_Ivy_pgm->blockStack.front()->next_avail
        << " within " << p_Ivy_pgm->blockStack.front()->frame_Name() << " Frame" << std::endl;

	p_code_Block = new Block();

	p_code_Block->p_Frame = this;

	p_Ivy_pgm->blockStack.push_front(p_code_Block);

	assert(offset_after_args == 0);

	offset_after_args=sizeof(void*); // step over return value pointer

    SymbolTable& st = p_code_Block->symbol_table;

	std::list<Arg>::iterator arg_it = p_arglist->begin();
	// we skip the function name
	for (arg_it++; arg_it != p_arglist->end(); arg_it++)
	{
        const Type& t = arg_it->arg_type;
        const std::string& parameter_name = arg_it->arg_ID;

        if (st.find(parameter_name) != st.end())
		{
			p_Ivy_pgm->error( std::string("duplicate parameter name ") + parameter_name);
// Hey - what to do on this error?
		}
        SymbolTableEntry& ste = st[parameter_name];
        ste.id = parameter_name;
        ste.set_type(arg_it->arg_type);
        ste.set_p_Frame(this);
        ste.set_offset(offset_after_args);
        if (trace_parser) trace_ostream << "arg STE: \"" << parameter_name << "\" " << ste.info() << std::endl;
		offset_after_args+=t.maatvan();
	}

	p_code_Block->next_avail = offset_after_args;

    if (trace_parser) trace_ostream << "Frame::start_Block() - to level " << p_Ivy_pgm->blockStack.size()
        << " at offset " << p_Ivy_pgm->blockStack.front()->next_avail
        << " within " << p_Ivy_pgm->blockStack.front()->frame_Name() << " Frame" << std::endl;
	return;
}
