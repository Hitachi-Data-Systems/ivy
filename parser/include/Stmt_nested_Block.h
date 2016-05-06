//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include "Stmt.h"
#include "Block.h"

class Stmt_nested_Block: public Stmt {
private:
	Block* p_Block {nullptr};
public:
	Stmt_nested_Block(Block* p_B): p_Block(p_B) {}
	virtual ~Stmt_nested_Block() {if (p_Block) delete p_Block;};

	virtual bool execute() { return p_Block->invoke(); };
	virtual void display(const std::string& indent, std::ostream&) override;
};

