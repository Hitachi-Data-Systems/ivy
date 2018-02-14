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

#include <algorithm>  // for find_if
#include <list>
#include <string>
#include <regex>
#include <stdint.h>

#include "ivydefines.h"

class Ivy_pgm;

extern Ivy_pgm* p_Ivy_pgm;
extern std::regex digits_regex;
extern std::regex unsigned_int_regex;
extern std::regex int_regex;
extern std::regex leading_zero_regex;
extern std::regex float_number_regex;
extern std::regex float_number_optional_trailing_percent_regex;
extern std::regex identifier_regex;
extern std::regex identifier_trailing_digits_regex;
extern std::regex dotted_quad_regex;
extern std::regex MMSS_regex;
extern std::regex HHMMSS_regex;
extern std::regex has_trailing_fractional_zeros_regex;

extern std::string indent_increment;

void format_for_log_trailing_slashr_slashn(std::string);

std::string GetStdoutFromCommand(std::string cmd); // DOES NOT VALIDATE SAFETY OF SHELL COMMAND TO BE EXECUTED

void rewritePG(std::string& pg);

void fileappend(std::string filename, std::string s);

void log(std::string filename, std::string msg);

std::string format_utterance(std::string speaker, std::string utterance);

// For these functions I was having trouble with c++11 regexs, // at the time - later code uses regexes.
// so some of these do things the old fashioned way.
std::pair<bool,std::string> isValidTestNameIdentifier(std::string s);
bool endsWithPoundSign(std::string s);
bool startsWith(std::string s, std::string starter);
bool startsWith(std::string starter, std::string line, std::string& remainder);  // Sorry didn't notice overloaded function name at the time - originally evolved in different source files
bool endsWith(std::string s, std::string ending);
bool isalldigits(std::string s);
bool isDigitsWithOptionalDecimalPoint(std::string s);
bool endsIn(std::string s, std::string ending);

std::string rewrite_HHMMSS_to_seconds(std::string);

std::string safe_string(const unsigned char* p, unsigned int l); // stops at nulls, unprintables changed to '.'
std::string remove_non_alphameric(std::string s);
std::string convert_non_alphameric_to_underscore(std::string s);
std::string convert_non_alphameric_or_hyphen_or_period_to_underscore(std::string s);
std::string convert_non_alphameric_or_hyphen_or_period_or_equals_to_underscore(std::string s);
std::string convert_non_alphameric_or_hyphen_or_equals_to_underscore(std::string s);
std::string edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(std::string);
bool stringCaseInsensitiveEquality(std::string s1, std::string s2);

bool looksLikeHostname(std::string s);
bool looksLikeIPv4Addr(std::string s);
bool looksLikeNumberedHostRange(std::string s, std::list<std::string>&hnl);

std::string myhostname();

std::string load_hosts(std::string&s, std::list<std::string>&hosts);

std::string removeComments(std::string s);  // removes double-slash C++ style comments like this one and also script comments # like this.

std::string removeLeadingZeros(std::string s);

static inline std::string &ltrim(std::string &s){s.erase(s.begin(),std::find_if(s.begin(),s.end(),std::not1(std::ptr_fun<int,int>(std::isspace))));return s;}
static inline std::string &rtrim(std::string &s){s.erase(std::find_if(s.rbegin(),s.rend(),std::not1(std::ptr_fun<int,int>(std::isspace))).base(),s.end());return s;}
static inline std::string &trim (std::string &s){return ltrim(rtrim(s));}

std::string toUpper(std::string s);
std::string toLower(std::string s);
std::string justTheCommas(std::string);
std::string remove_underscores(std::string);
std::string LDEV_to_string(uint16_t);
std::string LDEV_to_string(unsigned int); // calls the uint16_t version

// CSV file operations - quoted strings in csv column values.
// A quoted string starts with either a single or double quote - the "starting quote" and continues to the closing quote mark of the same type as the starting quote.
// Single quote marks ' or double quote marks " when immediately preceded by a backslash \ said to be "escapted" and are not recognized as opening or closing quotes.
int countCSVlineUnquotedCommas(const std::string& csvline);
std::string retrieveRawCSVcolumn(std::string& csvline, int n_from_zero);
std::string UnwrapCSVcolumn(std::string s);
// An unwrapped CSV column value first has leading/trailing whitespace removed and then if what remains is a single quoted string,
// the quotes are removed and any internal "escaped" quotes have their escaping backslashes removed, i.e. \" -> ".

inline bool isIvyWhitespaceCharacter(char c) {return isspace(c) || ( ',' == c );}
void ivyTrim(std::string&); // remove leading & trailing isIvyWhitespaceCharacter() characters


bool isPlusSignCombo(std::string& callers_error_message, int& callersNumberOfFields, const std::string& token );
// Lets you do at least very basic checking on a token
// - May not be the empty string
// - May not begin or end with '+'
// - May not have two consecutive '+'
// Number of fields returned is the same as 1 + number of '+'

ivy_float number_optional_trailing_percent(const std::string& s /* e.g. "1.2%" */, std::string name_associated_with_value_for_error_message = std::string("") );  // throws std::invalid_argument
unsigned int unsigned_int(const std::string&, std::string name_associated_with_value_for_error_message = std::string("") );

std::string render_string_harmless(const std::string s);
std::string quote_wrap(const std::string s);
std::string quote_wrap_except_number(const std::string s);
std::string quote_wrap_csvline_except_numbers(const std::string csvline); // This is specifically so that when you double-click on an ivy csv file to launch Excel, it won't think LDEV names are times, etc.

std::string column_header_to_identifier(const std::string&s);
std::string munge_to_identifier(const std::string&s);

void xorshift64star(uint64_t&);

uint64_t number_optional_trailing_KiB_MiB_GiB_TiB(const std::string& s_parameter);

std::string put_on_KiB_etc_suffix(uint64_t n);

std::string normalize_identifier(const std::string& s); // translate to lower case and remove underscores

bool normalized_identifier_equality(const std::string& s1, const std::string& s2);

std::string remove_trailing_fractional_zeros(std::string s);

std::string rewrite_HHMMSS_to_seconds(std::string s);

std::string byte_as_hex(unsigned char);

std::string left(const std::string&,unsigned int);  // leftmost "n" characters
std::string right(const std::string&,unsigned int);
