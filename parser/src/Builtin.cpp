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
#include <iostream>
#include <iomanip>
#include <cmath>

#include "Builtin.h"
#include "OverloadSet.h"
#include "Ivy_pgm.h"


// Note "shell_command" alias "system" is a built-in function.

// ivy runs as root, which means that the shell commands that you run are executed as root.

// For now that is OK, as ivy is designed to be a lab tool, but maybe come back and run shell commands as some other userid.

extern std::string masterlogfile();

// NOTE - I had an odd problem when I included ivyhelpers.h, where defining the "log" function in ivyhelpers
// was interfering with macro generated code for the "log()" builtin.

// Therefore the following are a few excerpts from ivyhelpers.h.
// I made a stub ivy_log() function to call from here, expressly where we don't "see" ivyhelpers' log().

void fileappend(std::string /*filename*/, std::string);
bool looksLikeIPv4Addr(std::string);
bool stringCaseInsensitiveEquality(std::string s1, std::string s2);
std::string toUpper(std::string s);
std::string toLower(std::string s);
void ivyTrim(std::string&); // remove leading & trailing isIvyWhitespaceCharacter() characters
extern std::regex digits_regex;
extern std::regex float_number_regex;
extern std::regex float_number_optional_trailing_percent_regex;
extern std::regex identifier_regex;

// In the next bit, we define some functions with "mangled" names specifying
// the return type and the types of each function operand, to make it easy to
// insert a symbol table entry, which is by mangled name.

std::string string_to_string_with_decimal_places_of_double_int(double d, int i)
{
    std::ostringstream o;

    if (i < 0)
    {
        std::ostringstream o;
        o << "string_to_string_with_decimal_places_of_double_int(double d = " << d << ", int i = " << i << ") - invalid argument, must be called with i >= 0.";
        throw std::invalid_argument("o.str()");
    }

    if (i == 0)  o << ((int) d);
    else         o << std::fixed << std::setprecision(i) << d;

    return o.str();

}

std::string string_substring_of_string_int_int(const std::string s, int begin_index_from_zero, int number_of_chars)
{ // sorry about making a copy of the source string, but couldn't remember if the mechanism would work with a const std::string&
  // ... come back and try later.  Then fix all these.  Or just be happy, computers are fast nowadays.
    if (begin_index_from_zero < 0)
    {
        std::ostringstream o;
        o << "<Error> substring(\"" << s << "\" length = " << s.size() << ", beginning index from zero = " << begin_index_from_zero << ", number of characters = " << number_of_chars << ") - requested beginning index is less than zero.";
        return o.str();
    };

    unsigned int i = (unsigned int) begin_index_from_zero;

    if (number_of_chars < 0)
    {
        std::ostringstream o;
        o << "<Error> substring(\"" << s << "\" length = " << s.size() << ", beginning index from zero = " << begin_index_from_zero << ", number of characters = " << number_of_chars << ") - requested number of characters is less than zero.";
        return o.str();
    };

    unsigned int n = (unsigned int) number_of_chars;

    if ((i+n) > s.size()) n = s.size() - i;

    return s.substr(i,n);
}

int            int_print_of_int(int i)                  { std::ostringstream o; o << i;               std::cout << o.str(); fileappend(masterlogfile(), o.str()); return i;}
double      double_print_of_double(double d)            { std::ostringstream o; o << std::fixed << d; std::cout << o.str(); fileappend(masterlogfile(), o.str()); return d;}
std::string string_print_of_string(const std::string s) {                                             std::cout << s;       fileappend(masterlogfile(), s);       return s;}
void ivy_log(std::string filename, std::string msg);

int int_log_of_string_string(std::string filename, std::string s)
{
    ivy_log(filename,s);
    return 1;
}

int int_regex_match_of_string_string(std::string s,std::string specified_regex)
{
    try
    {
        std::regex rx (specified_regex);
        if (std::regex_match(s,rx)) return 1;
        else                        return 0;
    }
    catch (const std::regex_error& e) {
        std::ostringstream o;
        o << "regex_error caught: " << e.what() << std::endl;
        std::cout << o.str();
        fileappend(masterlogfile(),o.str());
        return 0;
    }
}

int int_regex_sub_match_count_of_string_string(std::string s, std::string specified_regex)
{
    try
    {
        std::regex rx (specified_regex);
        std::smatch smash;
        if (std::regex_match(s,smash,rx))
        {
            return smash.size();
        }
        else return 0;
    }
    catch (const std::regex_error& e) {
        std::ostringstream o;
        o << "regex_error caught: " << e.what() << std::endl;
        std::cout << o.str();
        fileappend(masterlogfile(),o.str());
        return -1;
    }
}

std::string string_regex_sub_match_of_string_string_int(std::string s, std::string specified_regex, int n)
{

    if (n < 0)
    {
        std::ostringstream o;
        o << "<Error> regex_sub_match(\"" << s << "\" length = " << s.size()
            << ", regex \"" << specified_regex << "\""
            << ", sub match index from zero = " << n << ") - requested match subscript index is less than zero.";
        return o.str();
    };

    unsigned int un = (unsigned int)n;

    try
    {
        std::regex rx (specified_regex);
        std::smatch smash;
        if (std::regex_match(s,smash,rx))
        {
            if ( un < smash.size() )
            {
                return smash[un];
            }
            else
            {
                std::ostringstream o;
                o << "<Error> regex_sub_match(\"" << s << "\" length = " << s.size()
                    << ", regex \"" << specified_regex << "\""
                    << ", sub match index from zero = " << n << ") - number of submatches available is "<< smash.size()
                    << " - requested match subscript index is too big.";
                return o.str();
            }
        }
        else return "";
    }
    catch (const std::regex_error& e) {
        std::ostringstream o;
        o << "regex_error caught: " << e.what() << std::endl;
        std::cout << o.str();
        fileappend(masterlogfile(),o.str());
        return 0;
    }
}

std::string string_left_of_string_int(std::string s, int n)
{
    if (n < 0)
    {
        std::ostringstream o;
        o << "<Error> left(\"" << s << "\" length = " << s.size() << ", number of characters = " << n << ") - requested number of characters is less than zero.";
        return o.str();
    };

    if (n==0) return "";

    unsigned int un = (unsigned int) n;

    if ( un < s.size()) return s.substr(0,un);
    else                return s;
}

std::string string_right_of_string_int(std::string s, int n)
{
    if (n < 0)
    {
        std::ostringstream o;
        o << "<Error> right(\"" << s << "\" length = " << s.size() << ", number of characters = " << n << ") - requested number of characters is less than zero.";
        return o.str();
    };

    if (n==0) return "";

    unsigned int un = (unsigned int) n;

    if ( un < s.size()) return s.substr(s.size()-un,un);
    else                return s;
}

int int_stringCaseInsensitiveEquality_of_string_string(std::string s1,std::string s2)
{
    if (stringCaseInsensitiveEquality(s1,s2)) return 1;
    else                                      return 0;
}

std::string string_trim_of_string(std::string s)
{
    std::string local_copy = s;
    ivyTrim(local_copy);
    return local_copy;
}

std::string string_to_lower_of_string(std::string s)
{
    return toLower(s);
}

std::string string_to_upper_of_string(std::string s)
{
    return toUpper(s);
}

std::string string_int_to_ldev_of_int(int i)
{
    std::ostringstream o;
    if (i < 0)
    {
        o << "<Error> int_to_ldev(" << i << ") - may not be called with a negative value.";
        return o.str();
    }
    if (i > 65535)
    {
        std::ostringstream o;
        o << "<Error> int_to_ldev(" << i << ") - may not be called with a value greater than 65535 (greater than 0xFFFF).";
        return o.str();
    }
    o << std::hex << std::setw(4) << std::setfill('0') << i;
    return toUpper(o.str().substr(0,2)) + ":" + toUpper(o.str().substr(2,2));
}

int int_matches_digits_of_string(std::string s)
{
    if (std::regex_match(s,digits_regex))
        return 1;
    else
        return 0;
}

int int_matches_float_number_of_string(std::string s)
{
    if (std::regex_match(s,float_number_regex))
        return 1;
    else
        return 0;
}

int int_matches_float_number_optional_trailing_percent_of_string(std::string s)
{
    if (std::regex_match(s,float_number_optional_trailing_percent_regex))
        return 1;
    else
        return 0;
}

int int_matches_identifier_of_string(std::string s)
{
    if (std::regex_match(s,identifier_regex))
        return 1;
    else
        return 0;
}

int int_matches_IPv4_dotted_quad_of_string(std::string s)
{
    if (looksLikeIPv4Addr(s)) return 1;
    else                      return 0;
}

int int_fileappend_of_string_string(std::string filename, std::string s)
{
    fileappend(filename,s);
    return 0;
}


std::string string_shell_command_of_string(std::string cmd) {  // DOES NOT CHECK IF COMMAND TO BE EXECUTED IS SAFE
	// this function was scraped and pasted from an internet forum

    {
        std::ostringstream o;
        o << "About to issue system shell command \"" << cmd << "\"." << std::endl;
        std::cout << o.str();
        ivy_log(masterlogfile(),o.str());
	}

    std::string data;
    FILE * stream;
    const int max_buffer = 4096;
    char buffer[max_buffer];
    //cmd.append(" 2>&1");

    stream = popen(cmd.c_str(), "r");
    if (stream) {
    	while (!feof(stream))
    	if (fgets(buffer, max_buffer, stream) != NULL)
		data.append(buffer);
    	pclose(stream);
    }

    {
        std::ostringstream o;
        o << "System shell command \"" << cmd << "\" was executed returning \"" << data << "\"." << std::endl;
        std::cout << o.str();
        ivy_log(masterlogfile(),o.str());
	}

    return data;
}
std::string string_system_of_string(std::string s) {return string_shell_command_of_string(s);}

int int_set_csvfile_of_string(std::string filename)
{
    csvfile& cf = csvfiles[filename];
    auto rv = cf.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); }

    current_csvfile_it = csvfiles.find(filename);

    return rv.first;
}

int int_drop_csvfile_of_string(std::string filename)
{
    auto f_it = csvfiles.find(filename);

    if (f_it == csvfiles.end()) return 0;

    if (f_it == current_csvfile_it) current_csvfile_it = csvfiles.end();

    csvfiles.erase(f_it);

    return 1;
}

int csvfile_rows()
{
    if (current_csvfile_it == csvfiles.end()) return -1;

    return current_csvfile_it->second.rows();
}

int int_csvfile_columns_in_row_of_int(int row)
{
    if (current_csvfile_it == csvfiles.end()) return -1;

    return current_csvfile_it->second.columns_in_row(row);
}

int csvfile_header_columns()
{
    if (current_csvfile_it == csvfiles.end()) return -1;

    return current_csvfile_it->second.header_columns();
}

std::string string_csvfile_column_header_of_int(int col)
{
    if (current_csvfile_it == csvfiles.end()) return "";

    return current_csvfile_it->second.raw_cell_value(-1,col);
}

std::string string_csvfile_cell_value_of_int_int(int row /* from -1 */, int col /* from 0 */)
{
    if (current_csvfile_it == csvfiles.end()) return "";

    return current_csvfile_it->second.cell_value(row, col);

}

std::string string_csvfile_raw_cell_value_of_int_int(int row /* from -1 */, int col /* from 0 */)
{
    if (current_csvfile_it == csvfiles.end()) return "";

    return current_csvfile_it->second.raw_cell_value(row, col);

}

std::string string_csvfile_cell_value_of_int_string(int row /* from -1 */, std::string col_header)
{
    if (current_csvfile_it == csvfiles.end()) return "";

    return current_csvfile_it->second.cell_value(row, col_header);

}

std::string string_csvfile_raw_cell_value_of_int_string(int row /* from -1 */, std::string col_header)
{
    if (current_csvfile_it == csvfiles.end()) return "";

    return current_csvfile_it->second.raw_cell_value(row, col_header);

}

std::string string_csvfile_row_of_int(int row)
{
    if (current_csvfile_it == csvfiles.end()) return "";

    return current_csvfile_it->second.row(row);
}

std::string string_csvfile_column_of_int(int col)
{
    if (current_csvfile_it == csvfiles.end()) return "";

    return current_csvfile_it->second.column(col);
}

std::string string_csvfile_column_of_string(std::string column_header)
{
    if (current_csvfile_it == csvfiles.end()) return "";

    return current_csvfile_it->second.column(column_header);
}



int int_csv_lookup_column_of_string_string(std::string filename, std::string header)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return -1; }

    return c.lookup_column(header);
}

std::string string_csv_column_header_of_string_int(std::string filename, int col)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return ""; }

    return c.column_header(col);
}

int int_csv_rows_of_string(std::string filename)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return -1; }

    return c.rows();
}

int int_csv_header_columns_of_string(std::string filename)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return -1; }

    return c.header_columns();
}

int int_csv_columns_in_row_of_string_int(std::string filename, int row)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return -1; }

    return c.columns_in_row(row);
}

std::string string_csv_raw_cell_value_of_string_int_int(std::string filename, int row, int col)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return ""; }

    return c.raw_cell_value(row,col);
}

std::string string_csv_raw_cell_value_of_string_int_string(std::string filename, int row, std::string col)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return ""; }

    return c.raw_cell_value(row,col);
}


std::string string_csv_cell_value_of_string_int_int(std::string filename, int row, int col)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return ""; }

    return c.cell_value(row,col);
}

std::string string_csv_cell_value_of_string_int_string(std::string filename, int row, std::string col)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return ""; }

    return c.cell_value(row,col);
}

std::string string_csv_row_of_string_int(std::string filename, int row)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return ""; }

    return c.row(row);
}

std::string string_csv_column_of_string_int(std::string filename, int column)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return ""; }

    return c.column(column);
}

std::string string_csv_column_of_string_string(std::string filename, std::string column)
{
    csvfile c;

    auto rv = c.load(filename);

    if (!rv.first) { std::cout << rv.second; ivy_log(masterlogfile(),rv.second); return ""; }

    return c.column(column);
}


BuiltinTable builtin_table;

std::unordered_map<std::string, csvfile> csvfiles;
std::unordered_map<std::string, csvfile>::iterator current_csvfile_it {csvfiles.end()};

std::ostream& operator<<(std::ostream&os, const Builtin& b)
{
	if (b.volatile_function) os << "volatile ";

	// os << b.display_name;

	os << b.return_type.print() << " (*)(";
	if (type_void!=(enum basic_types)b.arg1_type)
	{
		os << b.arg1_type.print();
		if (type_void!=(enum basic_types)b.arg2_type)
			os << ',' << b.arg2_type.print();
	}
	os << ')';

	return os;
}

std::ostream& operator<<(std::ostream&os, const BuiltinTable& bt)
{
	os << "BuiltinTable size is " << bt.size() << std::endl;
	for (auto& pear : bt)
	{
        os << "\"" << pear.first << "\" -> " << pear.second << std::endl;
	}
	return os;
}


// First we use macros to generate a library of "ivy builtin" wrapper functions.

    // When reading these macro definitions, a reminder that macro definitions accept two special operators (# and ##) in the replacement sequence:

    // The operator #, followed by a parameter name, is replaced by a string literal that contains the
    // argument passed (as if enclosed between double quotes):

    //#define str(x) #x
    //cout << str(test);

    //This would be translated into:

    //cout << "test";

    // The operator ## concatenates two arguments leaving no blank spaces between them.

    // Oh, and the compiler concatenates consecutive double-quoted string literals into one big literal.

// Each wrapper function has an encoded name that we will refer to in the next step when
// we populate the builtin_table.



typedef std::string string;

#define prefixop(name,op,ret_type,arg1_type) \
 ret_type ret_type ## _prefix_ ## name ## _of_ ## arg1_type \
 ( const arg1_type arg1 ) \
 { return op ( arg1 )  ; }

prefixop(minus,-,int,int)
prefixop(minus,-,double,double)

#undef prefixop

#define binopfunc(name,op,ret_type,arg1_type,arg2_type) \
 ret_type ret_type ## _ ## name ## _of_ ## arg1_type ## _ ## arg2_type \
 ( const arg1_type arg1 , const arg2_type arg2 ) \
 { return ( arg1 ) op ( arg2 ) ; }

#define math_op(name,op) \
 binopfunc(name,op,int,int,int) \
 binopfunc(name,op,double,double,double) \
 binopfunc(name,op,double,int,double) \
 binopfunc(name,op,double,double,int)

#define comparison_op(name,op) \
 binopfunc(name,op,int,int,int) \
 binopfunc(name,op,int,double,int) \
 binopfunc(name,op,int,int,double) \
 binopfunc(name,op,int,double,double) \
 binopfunc(name,op,int,string,string)

math_op(plus,+)

math_op(minus,-)

math_op(multiply,*)

math_op(divide,/) // ***** hey no check for divide by zero!

binopfunc(remainder,%,int,int,int)

comparison_op(eq,==)

comparison_op(ne,!=)

comparison_op(lt,<)

comparison_op(le,<=)

comparison_op(gt,>)

comparison_op(ge,>=)

binopfunc(plus,+,string,string,string)

binopfunc(bitwise_or,|,int,int,int)

binopfunc(bitwise_and,&,int,int,int)

binopfunc(bitwise_xor,^,int,int,int)

binopfunc(logical_or,||,int,int,int)

binopfunc(logical_and,&&,int,int,int)

double pi() { return acos(-1.0); }
double e()  { return exp ( 1.0); }


int snapshot()
{
    if (p_Ivy_pgm == nullptr) throw std::runtime_error("Builtin snapshot() function - pointer to Ivy_pgm is nullptr.");
    p_Ivy_pgm -> snapshot(trace_ostream);
    return 0;
}

int int_trace_evaluate_of_int(int i)
{
    trace_evaluate = (i ? true : false);
    return i;
}

extern std::string outputFolderRoot();
extern std::string masterlogfile();
extern std::string testName();
extern std::string testFolder();
extern std::string stepNNNN();
extern std::string stepName();
extern std::string stepFolder();
extern std::string last_result();
extern std::string show_rollup_structure();

extern std::string ivy_engine_get(std::string thing);

extern int         ivy_engine_set(std::string thing, std::string value);

extern int exit_statement();  // doesn't actually return, but had to declare a return type of some kind.

void init_builtin_table()
{

    // hand-coded case for exit() due to name collision with C++ exit().
        builtin_table["exit()"] = Builtin( "exit()", (void (*)()) exit_statement, type_int, type_void, type_void, type_void );
        builtin_overloads[ "exit" ]["exit()"] = "int exit()" ;

    // Here is where we

    // 1) put pointers to the wrapper functions defined immediately above
    //    into the builtin_table, under a key like "sin(d)".  The builtin_table
    //    contains Builtin objects, and these actually contain the pointer to
    //    the wrapper function.

    // 2) In the builtin overloads table, we put <"sin(double)","double sin(double)"> in
    //    the OverloadSet filed under "sin" in the OverloadTable, and likewise for all the builtin functions.

    #define nullarybuiltin(name,ret_type) \
        builtin_table[ #name "()" ] = Builtin( #name "()", (void (*)()) name, type_ ## ret_type, type_void, type_void, type_void ); \
        builtin_overloads[ #name ][#name "()"] = #ret_type " " #name "()" ;

    #define unarybuiltin(name,ret_type,arg1_type) \
        builtin_table[ mangle_name1arg( #name, type_ ## arg1_type)] = Builtin( mangle_name1arg( #name, type_ ## arg1_type),(void (*)()) name, type_ ## ret_type, type_ ## arg1_type, type_void, type_void ); \
        builtin_overloads[ #name ][#name "(" #arg1_type ", "  #arg1_type ")" ] = #ret_type " " #name "(" #arg1_type ", "  #arg1_type ")" ;

    #define binarybuiltin(name,ret_type,arg1_type,arg2_type) \
        builtin_table[ mangle_name2args( #name, type_ ## arg1_type, type_ ## arg2_type) ] = Builtin( mangle_name2args( #name, type_ ## arg1_type, type_ ## arg2_type), (void (*)()) name, type_ ## ret_type, type_ ## arg1_type, type_ ## arg2_type, type_void ); \
        builtin_overloads[ #name ][#name "(" #arg1_type ", "  #arg1_type ", "  #arg2_type ")"] = #ret_type " " #name "(" #arg1_type ", "  #arg1_type ", "  #arg2_type ")" ;

    nullarybuiltin(outputFolderRoot,string)
    nullarybuiltin(masterlogfile,string)
    nullarybuiltin(testName,string)
    nullarybuiltin(testFolder,string)
    nullarybuiltin(stepNNNN,string)
    nullarybuiltin(stepName,string)
    nullarybuiltin(stepFolder,string)
    nullarybuiltin(last_result,string)
    nullarybuiltin(show_rollup_structure,string)


    nullarybuiltin(csvfile_rows,int)
    nullarybuiltin(csvfile_header_columns,int)

    unarybuiltin(ivy_engine_get,string,string)
    binarybuiltin(ivy_engine_set,int,string,string)

    nullarybuiltin(snapshot,int);

    nullarybuiltin(pi,double)

    nullarybuiltin(e,double)

    unarybuiltin(sin,double,double)

    unarybuiltin(cos,double,double)

    unarybuiltin(tan,double,double)

    unarybuiltin(sinh,double,double)

    unarybuiltin(cosh,double,double)

    unarybuiltin(tanh,double,double)

    unarybuiltin(asin,double,double)

    unarybuiltin(acos,double,double)

    unarybuiltin(atan,double,double)

    binarybuiltin(atan2,double,double,double)

    unarybuiltin(log,double,double)

    unarybuiltin(exp,double,double)

    unarybuiltin(log10,double,double)

    unarybuiltin(abs,int,int)

    unarybuiltin(exp,double,double)

    binarybuiltin(pow,double,double,double)

    unarybuiltin(sqrt,double,double)

    #undef nullarybuiltin
    #undef unarybuiltin
    #undef binarybuiltin

    #define unaryfunc(name,op,ret_type,arg1_type) \
        builtin_table[mangle_name1arg(#op,type_ ## arg1_type)] = \
            Builtin( \
                mangle_name1arg(#op,type_ ## arg1_type), \
                (void (*)()) \
                (&ret_type ## _ ## name ## _of_ ## arg1_type ), \
                type_ ## ret_type, type_ ## arg1_type, type_void, type_void \
            ); \
        builtin_overloads[ #op ][ #op "(" #arg1_type ")" ] =  #ret_type " " #op "(" #arg1_type ")" ;

    unaryfunc(trace_evaluate,trace_evaluate,int,int)

    unaryfunc(shell_command,shell_command,string,string)
    unaryfunc(system,system,string,string)

    unaryfunc(matches_IPv4_dotted_quad,matches_IPv4_dotted_quad,int,string)
    unaryfunc(matches_identifier,matches_identifier,int,string)
    unaryfunc(matches_digits,matches_digits,int,string)
    unaryfunc(matches_float_number,matches_float_number,int,string)
    unaryfunc(matches_float_number_optional_trailing_percent,matches_float_number_optional_trailing_percent,int,string)

    unaryfunc(print,print,int,int)
    unaryfunc(print,print,double,double)
    unaryfunc(print,print,string,string)

    unaryfunc(set_csvfile,             set_csvfile,           int,string)
    unaryfunc(drop_csvfile,            drop_csvfile,          int,string)
    unaryfunc(csvfile_columns_in_row,  csvfile_columns_in_row,int,int)
    unaryfunc(csvfile_column_header,   csvfile_column_header, string,int)
    unaryfunc(csvfile_row,             csvfile_row,           string,int)
    unaryfunc(csvfile_column,          csvfile_column,        string,int)
    unaryfunc(csvfile_column,          csvfile_column,        string,string)

    unaryfunc(trim,trim,string,string)
    unaryfunc(to_lower,to_lower,string,string)
    unaryfunc(to_upper,to_upper,string,string)

    unaryfunc(int_to_ldev,int_to_ldev,string,int)

    unaryfunc(csv_rows,csv_rows,int,string)
    unaryfunc(csv_header_columns,csv_header_columns,int,string)

    #undef unaryfunc

    #undef prefixopfunc
    #define prefixopfunc(name,op,ret_type,arg1_type) \
        builtin_table[ mangle_name1arg("prefix_" #op,type_ ## arg1_type) ] = \
            Builtin( \
                mangle_name1arg("prefix_" #op,type_ ## arg1_type), \
                (void (*)()) \
                (&ret_type ## _prefix_ ## name ## _of_ ## arg1_type ), \
                type_ ## ret_type, type_ ## arg1_type, type_void, type_void \
            ); \
        builtin_overloads[ #op ]["<prefix>\'" #op "\'(" #arg1_type ")" ] =  #ret_type " <prefix>\'" #op "\'(" #arg1_type ")" ;

    prefixopfunc(minus,-,int,int)
    prefixopfunc(minus,-,double,double)

    #undef prefixopfunc

    #undef binopfunc
    #define binopfunc(name,op,ret_type,arg1_type,arg2_type) \
        builtin_table[ mangle_name2args(#op,type_ ## arg1_type,type_ ## arg2_type) ] = \
            Builtin( \
                mangle_name2args(#op,type_ ## arg1_type,type_ ## arg2_type), \
                (void (*)()) \
                (&ret_type ## _ ## name ## _of_ ## arg1_type ## _ ## arg2_type), \
                type_ ## ret_type, type_ ## arg1_type, type_ ## arg2_type, type_void \
            ); \
        builtin_overloads[ #op ][ "\'" #op "\' (" #arg1_type "," #arg2_type ")" ] = #ret_type " \'" #op "\' (" #arg1_type "," #arg2_type ")" ;
    math_op(plus,+)
    math_op(minus,-)
    math_op(multiply,*)
    math_op(divide,/)
    binopfunc(remainder,%,int,int,int)
    comparison_op(eq,==)
    comparison_op(ne,!=)
    comparison_op(lt,<)
    comparison_op(le,<=)
    comparison_op(gt,>)
    comparison_op(ge,>=)

    binopfunc(plus,+,string,string,string)

    binopfunc(bitwise_or,|,int,int,int)
    binopfunc(bitwise_and,&,int,int,int)
    binopfunc(bitwise_xor,^,int,int,int)
    binopfunc(logical_or,||,int,int,int)
    binopfunc(logical_and,&&,int,int,int)
    binopfunc(csvfile_cell_value,csvfile_cell_value,string,int,int)
    binopfunc(csvfile_raw_cell_value,csvfile_raw_cell_value,string,int,int)
    binopfunc(csvfile_cell_value,csvfile_cell_value,string,int,string)
    binopfunc(csvfile_raw_cell_value,csvfile_raw_cell_value,string,int,string)

    binopfunc(fileappend,fileappend,int,string,string)
    binopfunc(log,log,int,string,string)
    binopfunc(left,left,string,string,int)
    binopfunc(right,right,string,string,int)
    binopfunc(stringCaseInsensitiveEquality,stringCaseInsensitiveEquality,int,string,string)
    binopfunc(regex_match,regex_match,int,string,string)
    binopfunc(regex_sub_match_count,regex_sub_match_count,int,string,string)

    binopfunc(to_string_with_decimal_places,to_string_with_decimal_places,string,double,int)

    binopfunc(csv_lookup_column,csv_lookup_column,int,string,string)
    binopfunc(csv_column_header,csv_column_header,string,string,int)
    binopfunc(csv_columns_in_row,csv_columns_in_row,int,string,int)
    binopfunc(csv_row,csv_row,string,string,int)
    binopfunc(csv_column,csv_row,string,string,int)
    binopfunc(csv_column,csv_row,string,string,string)


    #if defined(_DEBUG)
        // trace << builtin_table; trace.flush();
    #endif

    #undef binopfunc
    #undef comparison_op

    #define triopfunc(name,op,ret_type,arg1_type,arg2_type,arg3_type) \
        builtin_table[ mangle_name3args(#op,type_ ## arg1_type,type_ ## arg2_type,type_ ## arg3_type) ] = \
            Builtin( \
                mangle_name3args(#op,type_ ## arg1_type,type_ ## arg2_type,type_ ## arg2_type), \
                (void (*)()) \
                (&ret_type ## _ ## name ## _of_ ## arg1_type ## _ ## arg2_type ## _ ## arg3_type), \
                type_ ## ret_type, type_ ## arg1_type, type_ ## arg2_type, type_ ## arg3_type \
            ); \
        builtin_overloads[ #op ][ "\'" #op "\' (" #arg1_type "," #arg2_type "," #arg3_type ")" ] = #ret_type " \'" #op "\' (" #arg1_type "," #arg2_type "," #arg3_type ")" ;

    triopfunc(substring,substring,string,string,int,int)
    triopfunc(regex_sub_match,regex_sub_match,string,string,string,int)
    triopfunc(csv_raw_cell_value,csv_raw_cell_value,string,string,int,int)
    triopfunc(csv_raw_cell_value,csv_raw_cell_value,string,string,int,string)
    triopfunc(csv_cell_value,csv_cell_value,string,string,int,int)
    triopfunc(csv_cell_value,csv_cell_value,string,string,int,string)
}

