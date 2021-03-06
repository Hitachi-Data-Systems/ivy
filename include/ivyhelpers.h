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
//Authors: Allart Ian Vogelesang <ian.vogelesang@hitachivantara.com>
//
//Support:  "ivy" is not officially supported by Hitachi Vantara.
//          Contact one of the authors by email and as time permits, we'll help on a best efforts basis.
#pragma once

#include "ivytypes.h"
#include "ivytime.h"
#include "ivydefines.h"
#include "RunningStat.h"

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
extern std::regex frag_regex;
extern std::regex last_regex;
extern std::regex LDEV_regex;
extern std::regex PG_regex;
extern std::regex set_command_regex;

extern std::string indent_increment;

void format_for_log_trailing_slashr_slashn(std::string);

std::string GetStdoutFromCommand(std::string cmd); // DOES NOT VALIDATE SAFETY OF SHELL COMMAND TO BE EXECUTED

void rewritePG(std::string& pg);

void fileappend(std::string filename, std::string s);

//extern std::map<std::string /*csv filename*/, std::pair<ivytime /*time of last log */,RunningStat<long double, uint32_t> /*time to perform a log*/ >> log_stats_by_filename;

//std::string print_logfile_stats();

std::string format_utterance(std::string speaker, std::string utterance, ivytime delta = ivytime_zero);

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
std::string remove_non_alphameric(const std::string& s);

std::string convert_invalid_characters_to_underscore(const std::string& s, const std::string& valid_characters);
std::string convert_non_alphameric_to_underscore(const std::string& s);
std::string convert_non_alphameric_or_hyphen_or_period_to_underscore(const std::string s);
std::string convert_non_alphameric_or_hyphen_or_period_or_equals_to_underscore(const std::string& s);
std::string edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(const std::string&);
std::string edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_or_plus_to_underscore(const std::string&);
std::string convert_commas_to_semicolons(const std::string&);
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
std::string retrieveRawCSVcolumn(const std::string& csvline, int n_from_zero);
std::string UnwrapCSVcolumn(const std::string& s);
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

ivy_float number_optional_trailing_percent(const std::string& s /* e.g. "1.2%" */, const std::string& name_associated_with_value_for_error_message = std::string("") );  // throws std::invalid_argument
unsigned int unsigned_int(const std::string&, std::string name_associated_with_value_for_error_message = std::string("") );
unsigned long unsigned_long(const std::string&, std::string name_associated_with_value_for_error_message = std::string("") );

std::string render_string_harmless(const std::string s);
std::string quote_wrap(const std::string s);
std::string quote_wrap_LDEV_PG_leading_zero_number(const std::string s, bool formula_wrapping);
std::string quote_wrap_csvline_LDEV_PG_leading_zero_number(const std::string csvline, bool formula_wrapping); // This is specifically so that when you double-click on an ivy csv file to launch Excel, it won't think LDEV names are times, etc.

std::string column_header_to_identifier(const std::string&s);
std::string munge_to_identifier(const std::string&s);

void xorshift64star(uint64_t&);

uint64_t number_optional_trailing_KiB_MiB_GiB_TiB(const std::string& s_parameter);

std::string put_on_KiB_etc_suffix(uint64_t n);

std::string normalize_identifier(const std::string& s); // translate to lower case and remove underscores

bool normalized_identifier_equality(const std::string& s1, const std::string& s2);

std::string remove_trailing_fractional_zeros(std::string s);

std::string byte_as_hex(unsigned char);

std::string left(const std::string&,unsigned int);  // leftmost "n" characters
std::string right(const std::string&,unsigned int);

std::map<unsigned int /* core */, std::vector<unsigned int /* processor (hyperthread) */>> get_processors_by_core();

std::pair<bool /*false means attempt to read past the end*/, std::string /*field*/>
    get_next_field(const std::string& s, unsigned int& cursor, char delimiter);
    // for "1-1/1-2" with delimeter '/' starting with cursor 0 returns "1-1" and then "1-2"
    // for "/" returns "" then ""

std::string seconds_to_hhmmss(unsigned int input_value);

bool parse_boolean(const std::string&); // throws std::invalid_argument

unsigned int round_up_to_4096_multiple(unsigned int);

std::string size_GB_GiB_TB_TiB(ivy_float);

std::ostream& interpret_struct_timespec_as_localtime(std::ostream&, const struct timespec&);

template<class T> std::ostream& print_in_dec_and_hex(std::ostream& o, T thing)
{
    o << std::dec
          << thing
      << " (0x" << std::hex << std::setw(2 * sizeof(T)) << std::uppercase << std::setfill('0')
          << thing << ")";

    return o;
}

struct counter { unsigned long c {0}; };

