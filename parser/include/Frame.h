//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <list>

#include "Arg.h"

#include "Block.h"

class Frame {

public:
	std::list<Arg> *p_arglist;
	size_t offset_after_args {0u};
	Block* p_code_Block {nullptr};
	unsigned int stack_offset {0u}; // offset from "stack"


	Frame(std::list<Arg>* p_args): p_arglist(p_args){};

	~Frame() { if (p_arglist) delete p_arglist; if (p_code_Block) delete p_code_Block; }

	void invoke(unsigned int /*frame_offset*/);
	void start_Block();
	const Type& genre() const { return p_arglist->front().arg_type; }
	const std::string& name() const { return p_arglist->front().arg_ID; }
};

std::ostream& operator<<(std::ostream&, const Frame&);
