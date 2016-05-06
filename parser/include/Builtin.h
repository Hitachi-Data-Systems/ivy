//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <unordered_map>
#include <set>

#include "Type.h"
#include "csvfile.h"

class Builtin {
	friend  std::ostream& operator<<(std::ostream&os, const Builtin& b);
public:
    std::string display_name;
	void (*entry_pt)();
	Type return_type, arg1_type, arg2_type, arg3_type;
	unsigned int volatile_function;

	Builtin() : entry_pt(nullptr), return_type(type_void), arg1_type(type_void), arg2_type(type_void), arg3_type(type_void), volatile_function(0) {};

	Builtin(const std::string& dn, void (*ep)(), const Type& rt, const Type& a1t, const Type& a2t, const Type& a3t, const unsigned int vf=0):
		display_name(dn), entry_pt(ep), return_type(rt), arg1_type(a1t), arg2_type(a2t), arg3_type(a3t), volatile_function(vf) {};

	Builtin(const Builtin& b2):
        display_name(b2.display_name),
		entry_pt(b2.entry_pt),
		return_type(b2.return_type),
		arg1_type(b2.arg1_type),
		arg2_type(b2.arg2_type),
		arg3_type(b2.arg3_type),
		volatile_function(b2.volatile_function) {};

	Builtin& operator=(const Builtin& b2) {
		display_name = b2.display_name,
		entry_pt=b2.entry_pt;
		return_type=b2.return_type;
		arg1_type=b2.arg1_type;
		arg2_type=b2.arg2_type;
		arg3_type=b2.arg3_type;
		volatile_function=b2.volatile_function;
		return *this;
	};
};

std::ostream& operator<<(std::ostream&os, const Builtin&);

typedef std::unordered_map<std::string,Builtin> BuiltinTable;
// e.g. "sin(double)" -> Builtin object with pointer to corresponding wrapper function calling underlying C/C++ library function.

std::ostream& operator<<(std::ostream&os, const BuiltinTable&);

extern BuiltinTable builtin_table;

extern std::unordered_map<std::string, csvfile> csvfiles;
extern std::unordered_map<std::string, csvfile>::iterator current_csvfile_it;
