//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
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


