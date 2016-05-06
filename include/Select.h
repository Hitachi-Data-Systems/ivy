//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <list>
#include <string>
#include <iostream>

#include "location.hh"

class SelectClause;
class LUN;

class Select
{
public:
// variables
    yy::location bookmark; // The beginning/ending location in the source file - provided by the Bison parser
	std::list<SelectClause*>* p_SelectClause_pointer_list;
	std::string logfile{};

// methods
    Select(const yy::location& l, std::list<SelectClause*>* p, const std::string& logf)
     : bookmark(l), p_SelectClause_pointer_list(p), logfile(logf) {};
    bool matches(LUN*);
    bool contains_attribute(const std::string&);
    void display(const std::string& /*indent*/, std::ostream&);
};

// [select] , rule2, ...
// [select] rule1 rule2 ...
// There rule is is attribute_name = valuelist, or a rule could be builtin_function_name = shorthand_expresssion_for_set_of_values

// An example of a rule of the attribute = valuelist type could be "[select] Parity_Group = {"01-01", "01-02" "01-03" "01-04"}"

// There are "built in" keywords, "LDEV", "PG, and "host", which have code to process an expression describing a range of values.

// Example "[select] host="cb30-33, sun159", LDEV="00:00-00:FF 01:00-01:ff 0f:00"   PG = {1-1, 2-1:3, *-4}

// 	The "LDEV = expression" builtin makes it easy to write an expression for an LDEV range.

// 	The host = "cb30-33, sun159" matches hosts {CB30, CB31, CB32, CB33, SUN159}.

//	The PG = {1-1, 2-1:3, *-4} matches on "Parity Group" 01-01, 02-01, 02-02, 02-03, and matches on any parity group ending in -4 or -04.

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

//    On the [hosts] statement, like "[hosts] cb30-33 sun159 [select] serial_number = {201234, 304321}",
//    The select clause in the [hosts] must override a default serial_number="Serial_Number not provided by the user" clause.
//    This ensures that you can only select LUNs from a particular set of subsystem serial numbers, thus preventing you from
//    accidentally writing to local boot volumes, etc.

//    This default serial_number="Serial_Number not provided by the user" clause only applies to the [hosts] statement.
//    When you select LUNs for a test an empty [select] phrase selects all availableLUNs.

//    There may be multiple workloads layered on a particular LUN.  Workload objects include a pointer to the corresponding LUN objects.
//    In [Create/Edit/DeleteWorkload] statements, there is a [select] clause.
//        In [CreateWorkload] statements, the [select] clause filters LUNs from availableLUNs that we are going to build the new workload threads on.
//        In [EditWorkload] or [DeleteWorkload] statements, the [select] clause filters existing Workload objects, by their LUN's attributes

// 3) The set of Workloads which currently exist, or a subset of those, tracked with a WorkloadTrackerPointerList.
//         Each WorkloadTracker tracks an I/O driver subthread on a test host.
//         Each WorkloadTracker has a pointer to a LUN representing the LUN that Workload drives I/O to.
//         Therefore when we apply a Select to a WorkloadTrackerPointerList when creating a rollup, we iterate over the WorkloadTrackers,
//         applying the Select to the underlying LUN*.

// 4) A Select consists of a sequence of SelectClause objects, each of which corresponds to a Select ATTRIBUTE_NAME = VALUE_SET_EXPRESSION clause.

//    The SelectClause object is constructed with
//                   - an attribute name like "Parity_Group" formed from a column header in the LUN list csv file.
//                   - a list of one or more attribute values like "01-01" or "{01-01, 01-02 01-03 01-04}".

//    There is the SelectClause itself that contains the attribute name string, and a list of string tokens to match against.

//    Then there is the Matcher object, which you attach on to the SelectClause according to attribute name.

//    There is a pure virtual Matcher base class, and then there are the "builtin shorthand" Matchers.

//    The ColumnHeaderMatcher is the default Matcher used for all LUN-lister csv file header attributes.
//       - In this case a LUN matches if the LUN's attribute value matches on a caseInSensitiveComparison with
//         one of the tokens in the VALUE_SET_EXPRESSION token list.

//    The builtin attribute LDEV uses an LDEV_Matcher.  The LDEV_Matcher constructor compiles each of the value list text tokens
//    into an LDEVset object.  When the SelectClause says LDEV = "00:00, 01:00-01:FF" the LDEVset constructor interprets that and builds an LDEVset.

//    Even though you can express any combinations of LDEVs in a single token like that, it maybe be easier for you to type
//    LDEV = { 00:00, 01:00-01:FF } instead, in which case two LDEVset objects are compiled.

//    When testing a LUN, it matches if the LDEV is contained in any of the LDEVsets in the compiled list.
//    (The LDEVset class was what got me up and running quickly building ivy, but the implementation is a brute force std::set<int LDEVasInt>,
//     This seems to run OK, but somebody has some spare time we could make a much more efficient implementation.)


//    The builtin attribute host matches against the ivyscript_hostname attribute, which is the name that the user
//    used to identify the host in the ivyscript [hosts] statement.  So it's better to match against "host".
//    If you match against "hostname" this will match on the value returned by the hostname() system call on the test host.

//    Then the host builtin attribute lets you specify a range of hostnames that have numeric suffixes.
//    For example, for host = "sun159, cb28-31" or host = {sun159, cb28-31}, it builds the list of matching tokens
//    { "sun159", "cb28", "cb28", "cb30", "cb31" }.  The special behaviour of the "host" attribute is at compile time.
//    When you are matching the LUN against the list, it matches on caseInsensitiveCompare of any of the predefined host name tokens in the compiled list.

//    The builtin attribute PG matches against the "Parity Group" LUN-lister attribute.

//    Firstly we always want to recognize "1-1" as representing the same value as "01-01".

//    A well-formed PG match token has a first_number_part immediately followed by a hyphen '-' immediately followed by the second_number_part.

//    A match token number part takes one of three forms:
// 	- Matching against a single value, written as the text representation of a single positive integer, e.g. "1".
// 	- Matching against a range written as a range starting value, a colon ':', and the range ending value being greater or equal to the starting value, e.g. "2:3"
//	- Wild card matching anything, written as an asterix '*'.

//    For example, if you say in a select expression "PG = { 1-1, 2:3-* }" this matches on Parity Group 01-01
//    and every PG starting with "02-" or "03-".



// SORRY ABOUT THE PRIMITIVE 1970s-STYLE PARSER.

// Regular expressions are broken in g++ and I didn't want to go through the hassle of figuring out if it's OK to use Boost.

// Later when we have time, or it's open source and others have time, build a proper lex/yacc-style parser or whatever animal is in use today
// for an ivyscript language with the usual programming language conveniences and constructs.

//    The Select contructor parses a text string and remembers if the parse was successfull and the Select object is ready to use.

//    The Select parser recognizes as tokens
//	- Character strings starting/ending with single quotes ' or double quotes ".
//	- The quote character starting/ending a character string can be included within a quoted string by prefixing it (escaping it) with a backslash as in 'don\'t'.
//	- Sequences of characters comprised of alphamerics, and ivyhelper function "bool doesntNeedQuotes(char c)" characters,
//	  currently '_', '-', ':', '*', '+', '.', '#', '@', '$'.
//      - Individual characters that are not whitespace and not alphanumerics/doesntNeedQuotes() characters are recognized as single character tokens.
//    	- To be recognized as a token, any character strings containing whitespace, commas, or characters other than alphanumerics/doesntNeedQuotes()
//        characters must be enclosed in quotes.

//    Commas are treated as whitespace by the select phrase parser.

//    Examples of tokens:
//		123.567
//		Name_with_underscores
//		01-01
//		00:00
//              00:00-00:FF
//		"00:00-00:FF 01:00-01:ff"
//		host30-33
//		"host30-33, host40-43"

// Examples of valid select phrase text:

//              Serial_number = 210123, LDEV=00:00-00:ff    host = cb30-34, parity_group=01-01

//              Serial_number = {210123 310321}, LDEV="00:00-00:ff 01:00-01:ff"    host = "cb30-34 sun159", PARITY_group={01-01, 01-01 03-04}

// Select phrases are used in several contexts
// - A select phrase is used in the [hosts] statement as described above.  It populates "availableLUNs" by filtering against "allLUNs".
// - A select phrase is used in [CreateWorkload] statements.  It filters LUNs from "availableLUNs".
// - A select phrase is used in [EditWorkload] and [DeleteWorkload].  It filters existing Workload objects based on their underlying LUNs.

