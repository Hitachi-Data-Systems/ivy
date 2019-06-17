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

#include <iostream>
#include <unordered_map>
#include <list>

#include "SymbolTableEntry.h"

class Frame;
class Ivy_pgm;
class Stmt;

class Block {
public:
	SymbolTable symbol_table {};
	Frame* p_Frame {nullptr};
	size_t next_avail {0u};
		// Since all code is parsed before execution begins,
		// we can accumulate all static variables at the
		// beginning of the stack.  Then when execution
		// starts, the "frame pointer" for the global
		// block is "next_avail_static".
	std::list<Stmt*> constructors {};
	std::list<Stmt*> code {};
	std::list<Stmt*> destructors {};

    Block();
	Block(size_t n_a, Frame* p_F) : p_Frame(p_F), next_avail(n_a) {};
	~Block();

	bool invoke(); // return value true from invoke() means a "return" statement has been executed;

	std::string frame_Name();
	void display(const std::string& indent, std::ostream& os);
};


