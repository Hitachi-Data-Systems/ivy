//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <set>
#include <map>
#include <iostream>

typedef std::map<std::string /* "sin(double)" */, std::string/* "double sin(double)" */> OverloadSet;

  // It's called a BlockStack to alert you that executing Blocks are nested, but when we look up
  // in symbol tables, we iterate over the nested blocks from the innermost block at the beginning of
  // the list.  You can't iterate over a std::stack, so I used a std::list.

  // On the block stack, front() is the innermost currently executing Block and back() is what Ivy_pgm::global_Block
  // points to.

  // Each nested Block has its own SymbolTable that will remain empty if no variables or functions are declared in that Block.

std::ostream& operator<< (std::ostream&os, const OverloadSet&);

typedef std::map<std::string,OverloadSet> OverloadTable;
//  e.g. "min" -> { {"min(int,int)","int min(int,int)}, {"min(double,double)","double min(double,double)"}, ... }, ...
std::ostream& operator<<(std::ostream&os, const OverloadTable&);

extern OverloadTable builtin_overloads;
