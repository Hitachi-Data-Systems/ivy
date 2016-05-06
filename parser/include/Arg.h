//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <stdexcept>
#include <list>

#include "Type.h"

class Arg { // function argument
	friend  std::ostream& operator<<(std::ostream&, const Arg& );
	friend  std::ostream& operator<<(std::ostream&, const std::list<Arg>& );
public:
	Type arg_type;
	std::string arg_ID;

	Arg(Type t=Type(type_void)): arg_type(t), arg_ID() {};
	Arg(Type t, const std::string& c): arg_type(t), arg_ID(c) {};
	Arg(const Arg& f): arg_type(f.arg_type), arg_ID(f.arg_ID) {};
};

// arglist first entry is function return type and name.

// Ivy_pgm::declare_function() takes the bare list of just the arguments
// and inserts at the beginning the (return type, function name) node.

// Only in such function declarations the bare argument names (arg_ID) are possibly "".

const Type& return_type(const std::list<Arg>& fl);

typedef std::list<Arg> arglist;

std::string mangle(const std::list<Arg>& fl);
std::string printed_form(const std::list<Arg>& fl);
