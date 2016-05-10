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
