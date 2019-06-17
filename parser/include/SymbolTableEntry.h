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

#include <string>
#include <unordered_map>
#include <iostream>

#include "Type.h"
#include "Builtin.h"
#include "OverloadSet.h"

class Frame;
class Ivy_pgm;

enum class STE_form
{
	variable,         // key "building32", with frame offset
    function,     // key "sin_double"
    OverloadSet // key "sin" for overload list { "double sin(double)", ... }
};


class SymbolTableEntry {
public:
	Type STE_type;

	STE_form entry_form;

	unsigned int STE_flags;
#define          STE_flag_const 1u
#define          STE_flag_static 2u

	Frame* STE_p_Frame;

	union {
		size_t STE_frame_offset;
		OverloadSet* STE_p_OverloadSet;
	} u;

    std::string id {"<id not set>"};  // This is only used for SymbolTableEntry::info() for tracing.
                                      // This was expedient rather than fixing all the places where I had a
                                      // pointer to a SymbolTableEntry to use a pointer to a std::pair<std::string, SymbolTableEntry>
                                      // which is the thing that is stored in a SymbolTable, and which has the ID.
                                      // The only STEs that don't have an ID are the ones in the decl_prefix_stack,
                                      // which are templates to build variables or functions, and which don't have and ID.

public:
    SymbolTableEntry();
    ~SymbolTableEntry();
    SymbolTableEntry(const Type&t, Frame* p_f=(Frame*)0, unsigned int offset=0u, unsigned int flags=0u);
    SymbolTableEntry(const enum basic_types bt, Frame* p_f=(Frame*)0, unsigned int offset=0u, unsigned int flags=0u);
    SymbolTableEntry(const SymbolTableEntry& s);

    SymbolTableEntry& operator=(const SymbolTableEntry& other);

    bool is_const()           const { return STE_flags & STE_flag_const; };
    int is_static()          const { return STE_flags & STE_flag_static; };
    int is_a_variable()      const { return entry_form == STE_form::variable;};
    int is_a_function()      const { return entry_form == STE_form::function;};
    int is_an_OverloadSet()  const { return entry_form == STE_form::OverloadSet;};

    void* generate_addr();

    const Type& genre() const      { return STE_type; };
    Frame* p_Frame()               { return STE_p_Frame; };
    OverloadSet* p_OverloadSet()   { return u.STE_p_OverloadSet; };
    void set_offset(unsigned int o);
    void set_static(unsigned int onoff);
    void set_const(unsigned int onoff);
    void set_variable_form();
    void set_function_form();
    void set_p_Frame(Frame* p_F)                { STE_p_Frame=p_F; return;};
    void set_p_OverloadSet(OverloadSet* p_o)  { u.STE_p_OverloadSet=p_o; return;};
    void set_OverloadSet_form();
    void set_type(enum basic_types bt);
    void set_type(const Type& t);
    std::string info(const std::string& indent = std::string("")) const;
};

typedef std::unordered_map<std::string, SymbolTableEntry> SymbolTable;

std::ostream& operator<<(std::ostream&, const SymbolTable&);

//    As the source program is parsed, blockStack keeps track of the onion skin enclosing Block layers
//    to be able to look up identifiers in the current Block's symbol table, and in successive
//    outermore Block layers up to the Frame the blocks are running in.
//
//    blockStack does't "own" Blocks, it's just a way of getting to outermore Block layered symbol tables.
//
//    Blocks live in ivy program data structure, where you can have a statement to run a nested Block
//    and where the statement has the owning pointer to the Block.
//
//    As a variable is declared, its SymbolTableEntry is built and inserted in the Block's symbol table.
//
//    The STE for a variable will have a "frame offset" location relative to the point on the
//    stack where the Frame starts.  A call to a function is implemented by starting a new
//    Frame at the top of the stack.  The current frame context is saved and then a return value
//    variable for the function is allocated on the stack at the beginning of the frame, followed by
//    variables representing the parameter values being passed by the caller.  The caller puts
//    the parameter values on the stack, and then the function body's Block's symbol table has
//    entries for the parameter names mapping to those locations on the stack.
//
//    After the parameters in a frame is where variables are allocated.  The code to
//    construct / destruct variables is chained in a block's prologue and epilogue,
//    with destructors called in order - last to construct is first to destruct.
//
//    At run time when expressions are being evaluated, a reference to a variable
//    or a parameter is recorded as a pointer to a SymbolTableEntry.
//
//    That STE has an offset on the stack from the beginning of the frame.
//
//    And there is a variable that holds the location where the currently running frame starts.
//    (This bookmark is saved when a method is called and restored when the method returns.)
//
//    Any variables defined as static are allocated at the bottom of the stack before where
//    the first frame will be placed.

//    In other words, the SymbolTableEntry objects are used at runtime, but it's pointers to
//    STEs that are used as variable references in expressions.

//    But lookups in symbol tables are only done while the program is being parsed in order
//    to generate those pointers to STEs that are stored in expressions.
