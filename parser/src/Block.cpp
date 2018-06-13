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
#include "Stmt.h"
#include "Block.h"
#include "Ivy_pgm.h"
#include "Frame.h"


Block::Block(){}


Block::~Block()
{

	// Don't delete p_Frame here, as multiple Blocks
	// have pointers to the same Frame.  Actually, we
	// just deleted p_Frame, because when we destroyed
	// the SymbolTable before deleting it, we deleted
	// any function entries in it, with their Frames.

	// (Actually since we're trying to wipe out the entire
	// program in one fell swoop, we could eliminate memory
	// leaks be allocating all xpr and statement stuff out
	// of a pool, and just deleting the pool.  Hmmmm
	// Also, we never delete any Blocks, Frames, stmts,
	// and xprs until we are closing the document, when
	// we want to throw them all away at once.  String
	// variables are a special case - their contents are
	// subject to "random" creation and destruction during
	// program invocation.)

	for (auto& p : constructors) delete p;
	for (auto& p : code) delete p;
	for (auto& p : destructors) delete p;
}

bool Block::invoke() {

    for (auto& BMW : constructors) BMW->execute();

	bool return_signalled = false;

	for (auto& p_Stmt : code)
	{
        return_signalled = p_Stmt->execute();
		if ( return_signalled ) break;
    }

	for (auto& BMW : destructors) BMW->execute();

	return return_signalled;
}

std::string Block::frame_Name()
{
	if (p_Frame) return p_Frame->name();
	else         return "global";
}

void Block::display(const std::string& indent, std::ostream& os)
{
	os << indent << "Block: frameName() = " << frame_Name() << ", next_avail=" << next_avail << std::endl;

    if (symbol_table.size() > 0)
	{
		os << indent << "Block: Symbol table:" << std::endl;
		for (auto& pear : symbol_table)
		{
            os << indent << indent_increment << "\"" << pear.first << "\" -> " << pear.second.info("") << std::endl;
            if (pear.second.is_a_function())
            {
                Block *p_B = pear.second.p_Frame()->p_code_Block;
                if (p_B)
                    {p_B->display(indent+indent_increment+indent_increment,os);}
                else
                    {os << indent << indent_increment << "(function not defined)" << std::endl;}
            }
		}
	} else {
		os << indent << indent_increment << "Empty symbol table." << std::endl;
	}

	os << indent << "Block: constructors:" << std::endl;
	for (auto& p_Stmt : constructors)
	{
		os << indent << indent_increment << "p_Stmt = " << p_Stmt << std::endl;
		p_Stmt->display(indent+indent_increment+indent_increment,os);
	}

	os << indent << "Block: code:" << std::endl;
	for (auto& p_Stmt : code)
	{
		os << indent << indent_increment << "p_Stmt = " << p_Stmt << ", ";
		p_Stmt->display(indent+indent_increment+indent_increment,os);
	}

	os << indent << "Block: destructors:" << std::endl;
	for (auto& p_Stmt : destructors)
	{
		os << indent << indent_increment << "p_Stmt = " << p_Stmt << ", ";
		p_Stmt->display(indent+indent_increment+indent_increment,os);
	}

}

