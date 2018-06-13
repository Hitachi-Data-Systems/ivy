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

#include <list>
#include <string>
#include <iostream>

class LUN;

std::string squashed_attribute_name(const std::string&);  // removes spaces and underscores and translates to lower case
bool squashed_equivalence(const std::string&,const std::string&);

enum class JSON_select_value_type
{
    invalid,
    bare_JSON_number,
    quoted_string,
    true_keyword,
    false_keyword,
    null_keyword
};

class JSON_select_value
{
public:
    JSON_select_value_type t;
    std::string* p_s;

    JSON_select_value(JSON_select_value_type t2) : t(t2) {}
    JSON_select_value(JSON_select_value_type t2, std::string* p_s2) : t(t2), p_s(p_s2) {}

    std::string bare_value();
};

std::ostream& operator<< (std::ostream& o, const JSON_select_value&);

class JSON_select_clause
{
public:
	std::string* p_attribute_name;
	std::list<JSON_select_value*>* p_value_pointer_list;

// methods

	JSON_select_clause(std::string* p, std::list<JSON_select_value*>* p_l)
        : p_attribute_name(p), p_value_pointer_list(p_l) {};

    ~JSON_select_clause()
    {
        delete p_attribute_name;
        if (p_value_pointer_list != nullptr) for (auto& p : *p_value_pointer_list) delete p;
    }

	bool matches(LUN*);

	bool ldev_matches(LUN*);
	bool host_matches(LUN*);
	bool pg_matches(LUN*);
	bool default_matches(LUN*);

	void complain_pg(std::string);
};

std::ostream& operator<< (std::ostream& o, const JSON_select_clause& Jsc);

class JSON_select
{
public:
// variables
	std::list<JSON_select_clause*> JSON_select_clause_pointer_list {};
	bool successful_compile {false};
	bool unsuccessful_compile {false};
	std::string error_message {};

// methods
    JSON_select(){};
    ~JSON_select()
    {
        for (auto& p : JSON_select_clause_pointer_list) delete p;
    }
    void clear();
    bool compile_JSON_select(const std::string&);
    bool matches(LUN*);
    bool contains_attribute(const std::string&);
    void warning(const std::string& s) { error_message += s;}
    void error(const std::string&s) {error_message += s; successful_compile=false;}
    bool is_null() { return JSON_select_clause_pointer_list.size() == 0; }

};

std::ostream& operator<< (std::ostream& o, const JSON_select& Js);



// ivyscript [select] clause or ivy engine control API select clause

// Example
//         { "host" : "cb30-33", "LDEV" : "00:00-00:FF 01:00-01:ff 0f:00", "PG" : [ "1-1", "2-1:3", "*-4"] }

// JSON_select specifies a filter to be applied against a set of LUNs, and is used in several contexts:
// - A select is used in the [hosts] statement as described above.  It populates "available test LUNs" as a filter against "all discovered LUNs".
//                                                                  - At value for least one of "serial_number" and/or "vendor" must be specified.
// - A select is used in [CreateWorkload] statements.               It filters LUNs from "available test LUNs".
// - A select is used in [EditWorkload] and [DeleteWorkload].       It filters existing Workload objects based on their underlying LUNs.

// A null string is a select clause that selects everything.

// All attribute names must be in double quotes, as required by JSON.

// As required by JSON, all attribute values must be quoted, with the exception of some JSON literals, namely
//  - unsigned fixed point numbers without a leading zero, and where if there is a decimal point, there must be a digit following the decimal point.
//    E.g.   5     5.5   .01  but not 5. and not 05.5
//  - the keywords true, false, and null.

// When ivy matches against an attribute name, "SerialNumber", "serial_number", and "serial number" are equivalent.
//  - Before matching against an attribute name, blanks are removed, underscores are removed, and letters are translated to lower case.

// Matching against attribute values is a simple case-insensitive compare, with the exception of
// the built-in attribute names "LDEV", "PG, and "host", which have special code.
// 	- The  "LDEV":"00:00-00:FF"  builtin makes it easy to write an expression for an LDEV range.
//  - The PG matcher sees "01-01" as equivalent to "1-1".
//	-    "PG" : "2-1:3"]  matches same as  "PG":["2-1", "2-2", "2-3"]
//	-    "PG" : "*-4"  matches on any parity group ending in -4.
// 	- The  "host":"cb30-33"  builtin matches as if you had written "hosts":["CB30","CB31","CB32","CB33"].

// The concepts we have are

// 1) The LUN object.  Has a map from string attribute name keyword to string attribute value.
//                     There is one <keyword,value> pair for each column in the LUN attributes csv file.
//                     The keywords are the column headers, and the values are the values in that column for the row for a particular LUN.
//                     You can lookup the string value for each keyword.
//    The ivymaster main thread builds a set of LUN objects, one for each LUN on each test host.
//    LUNs are constant objects, in other words which LUNs exist on a test host and the attributes of the LUNs on a test host are assumed to be constant over an ivy run.

// 2) The LUNpointerList - a list of pointers to LUN objects.

//    Ivymaster has a couple of LUNpointerList objects, "allDiscoveredLUNs" (on all hosts) and "availableTestLUNs", which are
//    the subset of allDiscoveredLUNs that matched the "[selects]" clause on the [hosts] statement.

//    There is a mechanism to help prevent the user from accidentally selecting the wrong LUNs as test LUNs.

//    On the [hosts] statement, like "[hosts] cb30-33 sun159 [select] { "serial_number" : [201234, 304321]}",
//    the host statement select clause must specify a value for either "serial_number", or "vendor".

//    All SCSI LUNs must report at least "vendor", "product", and "product revision".

//    All Hitachi subsystems report "serial number".

//    Requiring the user to specify at least one of "serial_number", and "vendor" in the [hosts] statement [select] clause
//    was put in place to put some basic protection in to stop the user from accidentally selecting, for example, a boot device as a test LUN.

//    There may be multiple workloads layered on a particular LUN.  Workload objects include a pointer to the corresponding LUN objects.
//    In [Create/Edit/DeleteWorkload] statements, there is a [select] clause.
//        In [CreateWorkload] statements, the [select] clause filters LUNs from availableLUNs that we are going to build the new workload threads on.
//        In [EditWorkload] or [DeleteWorkload] statements, the [select] clause filters existing Workload objects, by their LUN's attributes

// 3) The set of Workloads which currently exist, or a subset of those, tracked with a WorkloadTrackerPointerList.
//         Each WorkloadTracker tracks an I/O driver subthread on a test host.
//         Each WorkloadTracker has a pointer to a LUN representing the LUN that Workload drives I/O to.
//         Therefore when we apply a JSON_select to a WorkloadTrackerPointerList when creating a rollup, we iterate over the WorkloadTrackers,
//         applying the JSON_select to the underlying LUN*.

// 4) A JSON_select_clause is something like    "port" : ["1A", "2A"]
//    and a JSON_select is something like     { "port" : ["1A", "2A"], "PoolID" : 0, "LDEV" : "00:00" }

//    There is the JSON_select_clause itself that contains the attribute name string, and a list of string tokens to match against.



