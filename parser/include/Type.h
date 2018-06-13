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
#pragma once

#include <stddef.h>
#include <iostream>
#include <algorithm>


constexpr size_t biggest_type = sizeof(int) > (((sizeof(double) > sizeof(std::string)) ? sizeof(double) : sizeof(std::string)))
? sizeof(int) : (((sizeof(double) > sizeof(std::string)) ? sizeof(double) : sizeof(std::string)));

enum basic_types {
	type_void=0,
	type_int=1,
	type_double=2,
	type_string=3,
	type_base=4
	// type_base must always be 1 bigger than the
	// largest type, so that we can use it as the
	// base of the number system to mangle a set
	// of types into one integer.  If you add more
	// types, just make sure that you set type_base
	// one bigger than the last one.
};

class Type {
public:
	enum basic_types value_type;

	std::string print() const; // "void", "int", "double", "string" or throw std::runtime_error()
    Type(const Type& t)      : value_type(t.value_type) {};
    Type(enum basic_types t) : value_type(t)            {};

    Type& operator=(const Type& t2)            { value_type=t2.value_type; return *this; };
    Type& operator=(const enum basic_types bt) { value_type=bt; return *this; };
    int operator==(const Type& t2)             { return value_type==t2.value_type; };
    int operator!=(const Type& t2)             { return value_type!=t2.value_type; };
    operator enum basic_types() const          { return value_type; };
    operator int() const                       { return (int) value_type; };
    size_t maatvan() const; // size of
    bool needs_constructor() const;
    bool needs_destructor() const;
    void construct_at(void* p) const;
    void destroy_at(void* p) const;
    int both_numeric_or_both_string(const Type& t) const;
};

std::ostream& operator << (std::ostream& os, const Type& t);

std::string mangle_name1arg(const std::string&, const Type&);
std::string mangle_name2args(const std::string&, const Type&, const Type&);
std::string mangle_name3args(const std::string&, const Type&, const Type&, const Type&);
