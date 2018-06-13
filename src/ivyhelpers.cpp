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
// ivyhelpers.cpp


#include <cctype>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <list>
#include <algorithm>    // std::find_if
#include <regex>

#include "ivytime.h"
#include "ivyhelpers.h"


using namespace std;

Ivy_pgm* p_Ivy_pgm {nullptr};

std::ostream& trace_ostream = std::cout;

bool trace_lexer {false};
bool trace_evaluate {false};
bool trace_parser {false};

std::string indent_increment {"   "};

std::regex identifier_regex(                "[[:alpha:]][[:alnum:]_]*");
std::regex identifier_trailing_digits_regex("[[:alpha:]][[:alnum:]_]*[[:digit:]]");
std::regex digits_regex(                  "[[:digit:]]+");

std::regex float_number_regex                          (  R"([+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?)"    );
std::regex float_number_optional_trailing_percent_regex(  R"([+-]?(\d+(\.\d*)?|\.\d+)([eE][+-]?\d+)?%?)"  );
std::regex unsigned_int_regex                          (  R"(\d+)"                                        );
std::regex int_regex                                   (  R"((-)?\d+)"                                        );
std::regex leading_zero_regex                          (  R"(0[0-9].*)"                                   );

std::regex dotted_quad_regex  (  R"ivy((([0-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-5]))\.(([0-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-5]))\.(([0-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-5]))\.(([0-9])|([1-9][0-9])|(1[0-9][0-9])|(2[0-4][0-9])|(25[0-5])))ivy");

std::regex has_trailing_fractional_zeros_regex( R"((([[:digit:]]+)\.([[:digit:]]*[1-9])?)0+)" );

std::regex MMSS_regex( std::string( R"ivy(([[:digit:]]+):((([012345])?[[:digit:]])(\.([[:digit:]])*)?))ivy" ) ); // any number of minutes followed by ':' followed by 00-59 or 0-59.
std::regex HHMMSS_regex( std::string( R"ivy(([[:digit:]]+):(([012345])?[[:digit:]]):((([012345])?[[:digit:]])(\.([[:digit:]])*)?))ivy" ) ); // any number of hours followed by ':' followed by 00-59 or 0-59 followed by ':' followed by 00-59 or 0-59.

void format_for_log_trailing_slashr_slashn(std::string s)
{
	for (int i = -1 + s.length(); i >= 0 && ('\r' == s[i] || '\n' == s[i]); i--)
	{
		if ('\r' == s[i])
			s.replace(i,1,std::string("\\r"));
		else
			s.replace(i,1,std::string("\\n"));
	}
	return;
}

std::string GetStdoutFromCommand(std::string cmd) {  // DOES NOT CHECK IF COMMAND TO BE EXECUTED IS SAFE
	// this function was scraped and pasted from an internet forum

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
    return data;
}

void rewritePG(std::string& pg)  // changes "01-01" to "1-1".  Does nothing if it doesn't look well-formed.
{
	std::string text = pg;

	trim(text);

	if ( 0 == text.length() )
	{
		return;
	}

	std::string cooked;
	int cursor = 0;
	int last_cursor = -1 + text.length();

	while (cursor <= last_cursor && '0' == text[cursor]) cursor++;

	if (cursor > last_cursor) return;

	while (cursor <= last_cursor && '-' != text[cursor])
	{
		cooked.push_back(text[cursor]);
		cursor++;
	}

	if (cursor > last_cursor) return;

	cooked.push_back('-');

	while (cursor <= last_cursor && '0' == text[cursor]) cursor++;

	if (cursor > last_cursor) return;

	while (cursor <= last_cursor)
	{
		cooked.push_back(text[cursor]);
		cursor++;
	}

	pg = cooked;

	return;
}



void fileappend(std::string filename, std::string s) {
        std::ofstream o;
        o.open(filename, ios::out | ios::app );
        o << s << std::flush;
        o.close();
}


void log(std::string filename, std::string s) {
	ivytime now;

	now.setToNow();

    std::ofstream o;

    o.open(filename, ios::out | ios::app );

    o << now.format_as_datetime_with_ns() << " "<< s;

	if (s.length() > 0 && '\n' != s[s.length()-1]) o << std::endl;

	o.flush();

    o.close();
}

void ivy_log(std::string filename, std::string s) {log(filename,s);} // crutch for Builtin.cpp

std::string format_utterance(std::string speaker, std::string utterance, ivytime delta)
{
	std::ostringstream o;

	o << delta.format_as_duration_HMMSSns() << ' ';

	if (delta == ivytime(0))
	{
	    std::string same_size_blanks {};
	    for (unsigned int i = 0; i < o.str().size(); i++)
	    {
	        same_size_blanks.push_back(' ');
	    }
	    o.str() = same_size_blanks;
	}

	o << speaker << ": \"";
	if (utterance.length()>0) {
		for (unsigned int i=0; i<utterance.length(); i++) {
			switch (utterance[i]) {
				case '\n': o << "\\n"; break;
				case '\r': o << "\\r"; break;
				case '\t': o << "\\t"; break;
				case '\v': o << "\\v"; break;
				case '\f': o << "\\f"; break;
				default:
					if (isprint(utterance[i])) {
						o<< utterance[i];
					} else {
						o<< "[x\'" << std::hex << ((int)utterance[i]) << "\']";
					}
			}
		}
	}
	o << "\" (" << std::dec << utterance.length() << ")" << std::endl;
	return o.str();
}

std::string load_hosts(std::string&s, std::list<std::string>&hosts) {
	if (!hosts.empty()) return "load_hosts() called with non-empty host list";
	if (s.length()==0) return "empty list";
	unsigned int i=0;
	while ((i<s.length()) && (isspace(s[i]) || s[i]==',')) i++;
	unsigned int l;
	while (i<s.length()){
		l=0;
		while ( ( (!isspace(s[i+l])) && (!(s[i]==','))) && ((i+l)<s.length())) l++;
		std::string token = s.substr(i,l);

		if (!looksLikeNumberedHostRange(token,hosts)) {
			if (looksLikeHostname(token) || looksLikeIPv4Addr(token)) {
				hosts.push_back(token);
			} else {
				return token; // a non-empty return value is a bad token
			}
		}

		i+=l;
		while ((i<s.length()) && isspace(s[i])) i++;

	}
	if (hosts.size()>0) {
		return "";  // empty string means no errors
	} else {
		return "empty list";
	}
}


std::string myhostname()
{
	char buf[256];
	buf[255]=0;
	if (0 != gethostname(buf,255))
		return std::string("");
	return std::string((char*)buf);
}

std::pair<bool,std::string> isValidTestNameIdentifier(std::string s) {

    std::regex valid_chars (R"([-一-龯ぁ-んァ-ン[:alnum:]_\. ]+)");  // regex based on https://gist.github.com/terrancesnyder/1345094#file-regex-japanese-txt

    if (!std::regex_match(s,valid_chars))
    {
        std::ostringstream o;
        o << "Invalid ivy test name \"" << s << "\" - must consist of ASCII alphabetics & numerics, Japanese kanji / hiragana / katakana, spaces, underscores, hyphens, and periods." << std::endl;
        return std::make_pair(false,o.str());
    }
	return std::make_pair(true,"");
}
bool endsWithPoundSign(std::string s) {
	if (0 == s.length()) return false;
	int i=s.length()-1;
	while ((i>0) && (isspace(s[i]))){i--;};
	return ('#' == s[i]);
}

bool startsWith(std::string s, std::string starter) {
	if (0==starter.length()) return true;
	if (s.length()<starter.length()) return false;
	for (unsigned int i=0; i<starter.length(); i++) {
		if (toupper(s[i]) != toupper(starter[i]))
			return false;
	}
	return true;
}

bool endsWith(std::string s, std::string ending) {
	if (0==ending.length()) return true;
	if (s.length()<ending.length()) return false;
	return stringCaseInsensitiveEquality(s.substr(s.length()-ending.length(),ending.length()), ending);
}

bool isalldigits(std::string s) {
	if (s.length()==0) return false;
	for (unsigned int i=0; i<s.length(); i++) if (!isdigit(s[i])) return false;
	return true;
}


bool isDigitsWithOptionalDecimalPoint(string s)
{
	if (s.length()==0) return false;

	bool leadingDigits {false}, trailingDigits {false};

	int cursor=0;
	int last_cursor = -1 + s.length();

	while (cursor<=last_cursor && isdigit(s[cursor]))
	{
		leadingDigits = true;
		cursor++;
	}

	if (cursor>last_cursor) return leadingDigits;

	if ('.' != s[cursor]) return false;

	cursor++;

	while (cursor<=last_cursor && isdigit(s[cursor]))
	{
		trailingDigits = true;
		cursor++;
	}

	if (cursor <= last_cursor) return false;

	return (leadingDigits || trailingDigits);
}


bool endsIn(std::string s, std::string ending) {
	if (s.length()<ending.length()) return false;
	return s.substr(s.length()-ending.length()) == ending;
}


bool looksLikeHostname(std::string s) {
	// returns true if starts with alphabetic and remainder is all alphanumerics and underscores
	if (s.length()==0) return false;
	if (!isalpha(s[0])) return false;
	for (unsigned int i=0; i<s.length(); i++) {
		if (!(isalnum(s[i]) || s[i]=='_')) return false;
	}
	return true;
}

bool looksLikeIPv4Addr(std::string s) {
	// returns true if it sees nnn.nn.nnn.n type IP v4 address.
	if (s.length()<7) return false;
	if ( (!isdigit(s[0])) || (!isdigit(s[s.length()-1])) ) return false; // must start and end with digit
	int dots=0;
	for (unsigned int i=0; i<s.length(); i++) {
		if (!isdigit(s[i])) {
			if (s[i]!='.') return false;
			if (s[i+1]=='.') return false;
			dots++;
		}
	}
	if (dots==3) {
		return true;
	} else {
		return false;
	}
}

bool looksLikeNumberedHostRange(std::string s, std::list<std::string>&hnl) {
	// returns true if expresses a range of hostnames which end in a number, like cb31-38
	// and adds the individual hostnames in the range to the list.
	if (s.length()==0) return false;
	if (!isalpha(s[0])) return false;
	unsigned int n1start=0, n1length=0, n2start=0, n2length=0;
	bool lookingatn1=false;
	for (unsigned int i=0; i<s.length(); i++) {
		if (isdigit(s[i])) {
			if (!lookingatn1) {
				n1start=i;
				lookingatn1=true;
			}
			if ( ((i+2)<s.length()) && (s[i+1]=='-') && isdigit(s[i+2]) ) {
				n1length=1+i-n1start;
				n2start=i+2;
				break;
			}

		} else {
			lookingatn1=false;

			if (!(isalpha(s[i]) || s[i]=='_')) return false;
		}
	}
	for (unsigned int i=n2start; i<s.length(); i++) {
		if (!isdigit(s[i])) return false;
		if ((i+1)>=s.length()) {
			n2length=1+i-n2start;
			break;
		}
	}
	std::string baseidentifier=s.substr(0,n1start);
	std::string n1string=s.substr(n1start,n1length);
	std::string n2string=s.substr(n2start,n2length);
	istringstream n1is(n1string);
	istringstream n2is(n2string);
	int n1=0,n2=0;
	n1is >> n1;
	n2is >> n2;
	if (n1>n2) return false;
	for (int i=n1; i<=n2; i++) {
		{
			ostringstream os;
			os << i;
			hnl.push_back(baseidentifier+os.str());
		}
	}
	return true;
}

// removes double-slash C++ style comments like this one, or comments starting with '#'
std::string removeComments(std::string s)
{
	if ((s.length() >= 1) && ('#' == s[0])) return std::string("");

	if (s.length()<2) return s;

	for (unsigned int i=1; i<s.length(); i++)
	{
		if ('#' == s[i])
			return s.substr(0,i);
		if ('/' == s[i-1] && '/' == s[i])
		{
			return s.substr(0,i-1);
		}
	}

	return s;
}

bool startsWith(std::string starter, std::string line, std::string& remainder){

	if (starter.length()==0){
		remainder=line;
		return true;
	}

	if (starter.length()>line.length()) {
		remainder="";
		return false;
	}

	for (unsigned int i=0; i<starter.length(); i++) {
		if (toupper(starter[i]) != toupper(line[i])) {
			remainder="";
			return false;
		}
	}

	if (starter.length()==line.length()) {
		remainder="";
	} else {
		remainder=line.substr(starter.length());
		ltrim(remainder);
	}

	return true;
}

std::string remove_non_alphameric(std::string s)
{
	std::string result;
	if (s.length()>0)  // yes, not necessary
	{
		for (unsigned int i=0; i<s.length(); i++)
		{
			if (isalnum(s[i]))
			{
				result.push_back(s[i]);
			}
		}
	}
	return result;
}

std::string convert_non_alphameric_to_underscore(std::string s){
	std::string result=s;
	if (s.length()>0) {
		for (unsigned int i=0; i<s.length(); i++) {
			if (!isalnum(s[i])) {
				result[i]='_';
			}
		}
	}
	return result;
}

std::string convert_non_alphameric_or_hyphen_or_period_to_underscore(std::string s){
	std::string result=s;
	if (s.length()>0) {
		for (unsigned int i=0; i<s.length(); i++) {
			if ((!isalnum(s[i])) && ('-' != s[i]) && ('.' != s[i])) {
				result[i]='_';
			}
		}
	}
	return result;
}

std::string convert_invalid_characters_to_underscore(std::string s, std::string valid_characters)
{
    std::regex find_a_char_to_convert_to_underscore
    (
        std::string( R"ivy(([)ivy" )
        + valid_characters
        + std::string( R"ivy(]+)([^)ivy" )
        + valid_characters
        + std::string( R"ivy(])(.*))ivy" )
    );

    std::smatch entire_match;
    std::ssub_match part1, part3;

    while (std::regex_match(s, entire_match, find_a_char_to_convert_to_underscore))
    {
        part1 = entire_match[1];
        part3 = entire_match[3];
        s = part1.str() + std::string("_") + part3.str();
    }

	return s;
}

std::string convert_non_alphameric_or_hyphen_or_period_or_equals_to_underscore(std::string s)
{
    return convert_invalid_characters_to_underscore(s, std::string( R"(一-龯ぁ-んァ-ン[:alnum:]_\.=)"));
}

std::string edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(std::string s)
{
    return convert_invalid_characters_to_underscore(s, std::string( R"(-一-龯ぁ-んァ-ン[:alnum:]_=)"));
}

bool stringCaseInsensitiveEquality(std::string s1, std::string s2) {
        if (s1.length()!=s2.length()) return false;
        if (s1.length()==0) return true;
        for (unsigned int i=0; i<s1.length(); i++) if (toupper(s1[i]) != toupper(s2[i])) return false;
        return true;
}

std::string removeLeadingZeros(std::string s) {
	trim(s);
	unsigned int i=0;
	unsigned int l=s.length();
	if (0==l) return s;
	while (i<s.length() && '0'==s[i]) {
		i++;
		l--;
	}
	if (0==l) {
		return std::string("");
	} else {
		return s.substr(i,l);
	}
}

std::string toUpper(std::string s) {
	std::string u=s;
        for (unsigned int i=0; i<u.length(); i++) u[i]=toupper(u[i]);
        return u;
}

std::string toLower(std::string s) {
	std::string l=s;
        for (unsigned int i=0; i<l.length(); i++) l[i]=tolower(l[i]);
        return l;
}

std::string justTheCommas(std::string s)  // Didn't have "advanceToNextUnquotedComma()" when this was written
{
	std::string commas;
	for (unsigned int i=0; i < s.length(); i++) if (',' == s[i]) commas.push_back(',');
	return commas;
}


std::string remove_underscores(std::string s)
{	std::string r;
	for (unsigned int i=0; i < s.length(); i++)
	{
        auto c = s[i];

        if ('_' != c) r.push_back(c);
    }
	return r;
}

std::string normalize_identifier(const std::string& s) // translate to lower case and remove underscores
{
    if (!regex_match(s,identifier_regex))
    {
        std::ostringstream o;
        o << "normalize_identifier(\"" << s << "\") - is not an identifier, meaning a string starting with an alphabetic and continuing with alphanumerics and underscores.  "
            << "  Occurred at " << __FILE__ << " line " << __LINE__;
        throw std::invalid_argument(o.str());
    }
    std::string r;
    for (auto c : s)
    {
        if ('_' != c) r.push_back(tolower(c));
    }
    return r;
}

bool normalized_identifier_equality(const std::string& s1, const std::string& s2)
{
    return 0 == normalize_identifier(s1).compare(normalize_identifier(s2));
}


bool advanceToNextUnquotedComma(const string& csvline, unsigned int& cursor_from_zero)
{
	// The cursor must already be positioned at a valid existing character in the string
	// with at least one additional character past the cursor or else we fail out.

	if ( cursor_from_zero >= (csvline.length()-1) )
	{
		cursor_from_zero=-1;
		return false;
	}

	// Now we keep moving the cursor further into the csvline string until either
	// 1) we hit the end of the string, in which case we have failed, where we do the "cursor_from_zero=-1; return false;" bit, or
	// 2) we hit an un-quoted comma, at which point we stop and return success leaving the cursor at the un-quoted comma we just found.

	// A comma character in "csvline" becomes a "quoted comma" if it comes after
	// a "starting quote" but before the corresponding ending quote.

	// Once a "starting quote" character has been seen, all characters are stepped over
	// without being recognized until the corresponding ending quote consisting of the
	// first subsequent character of the same value as the starting character which is
	// not immediately preceeded by a backslash '\'.

// NOTE - took out recognition of single quotes, as they can occur as apostrophes, as in "Little's law"


	bool mid_quoted_string;
	char starting_quote {' '};
	char next_char;

	if ( /*('\'' == csvline[cursor_from_zero]) || */('\"' == csvline[cursor_from_zero]))
	{
		starting_quote=csvline[cursor_from_zero];
		mid_quoted_string=true;
	}
	else
	{
		mid_quoted_string=false;
	}

	int remaining_characters = (csvline.length() - cursor_from_zero) -1;
	while (remaining_characters > 0)
	{
		remaining_characters--; cursor_from_zero++; next_char=csvline[cursor_from_zero];

		if (mid_quoted_string)
		{
			if ( ( next_char == starting_quote ) && (csvline[cursor_from_zero-1] != '\\') )
			{
				// found ending quote
				mid_quoted_string=false;
			}
		}
		else	// not mid_quoted_string
		{
			if (',' == next_char) return true;

			if ( /*(next_char == '\'') || */ (next_char == '\"') )
			{
				// found starting quote
				starting_quote = next_char;
				mid_quoted_string=true;
			}
		}
	}

	// ran off the end looking for an unquoted comma

	cursor_from_zero = -1;
	return false;
}

// CSV file operations - quoted strings in csv column values.
// A quoted string starts with either a single or double quote - the "starting quote" and continues to the closing quote mark of the same type as the starting quote.
// Single quote marks ' or double quote marks " when immediately preceded by a backslash \ said to be "escapted" and are not recognized as opening or closing quotes.
int countCSVlineUnquotedCommas(const std::string& csvline)
{
	if (0 == csvline.length()) return 0;

	unsigned int cursor=0;
	unsigned int count;

	if (',' == csvline[0]) count=1;
	else                   count=0;
	while (advanceToNextUnquotedComma(csvline,cursor)) count++;

	return count;
}

std::string retrieveRawCSVcolumn(std::string& csvline, int n_from_zero)
{
	if ( 0 > n_from_zero || 0 == csvline.length() ) return std::string("");

	unsigned int cursor=0;
	unsigned int number_of_columns_to_skip = n_from_zero;
	bool skipped_columns=false;

	if (',' == csvline[0])
	{
		if (number_of_columns_to_skip >0)
		{
			number_of_columns_to_skip--;
			skipped_columns=true;
			// the reason for this is that "advanceToNextUnquotedComma()" doesn't look at the character the cursor is already pointing too.
		}
	}

	while (number_of_columns_to_skip >0)
	{
		number_of_columns_to_skip--;
		skipped_columns=true;

		if (false == advanceToNextUnquotedComma( csvline, cursor ))
		{
			// hit the end, no such column
			return std::string("");
		}

		if (cursor==(csvline.length()-1))
		{
			// The comma we found is the last character in the line.
			return std::string("");
		}
	}
	if (skipped_columns)
	{
		// skip the comma itself at the beginning of the field;
		cursor++;
	}

	if (',' == csvline[cursor])
	{
		// if field is empty
		return std::string("");
	}

	int starting_cursor = cursor;
	int longeur;

	if ( advanceToNextUnquotedComma(csvline, cursor) )
	{
		// Not the last column this line.
		// Pick out substring up to by not including next comma.
		longeur = cursor - starting_cursor;
	}
	else
	{
		// last column on this line
		longeur = csvline.length()-starting_cursor;
	}

	return csvline.substr(starting_cursor,longeur);
}

std::string UnwrapCSVcolumn(std::string s)
{
    // first if it starts with =" and ends with ", we just clip those off and return the rest.

    // Otherwise, an unwrapped CSV column value first has leading/trailing whitespace removed and then if what remains is a single quoted string,
	// the quotes are removed and any internal "escaped" quotes have their escaping backslashes removed, i.e. \" -> ".
	trim(s);

	if (s.length()>=3 && s[0] == '=' && s[1] == '\"' && s[s.length()-1] == '\"')
	{
        return s.substr(2,s.length()-3);
	}

	if (s.length()>=2)
	{
		if ( ('\'' == s[0])  || ('\"' == s[0]) )
		{
			char starting_quote=s[0];

			std::string unwrapped;
			int cursor=0;
			int last_char_cursor = s.length()-1;
			while ((++cursor) <= last_char_cursor)
			{
				// are we looking at the ending quote as the last character?
				if ( (cursor == last_char_cursor) && (starting_quote == s[cursor]) && ('\\' != s[cursor-1]) )
				{
					return unwrapped;
				}

				// are we looking at an escaped quote character?
				if ( ('\\' == s[cursor]) && (cursor < last_char_cursor) && (starting_quote == s[cursor+1]) )
				{
					unwrapped.push_back(starting_quote);
					cursor++;
					continue;
				}

				// are we looking at a quote that is not escaped?
				if (starting_quote == s[cursor])
				{
					return s;
				}

				unwrapped.push_back(s[cursor]);
			}
		}
	}
	return s;
}

std::string render_string_harmless(const std::string s)
{
    std::ostringstream o;
    for (unsigned int i=0; i<s.size(); i++)
    {
        auto c = s[i];
        switch (c)
        {
            case '\'': o << "\\\'"; break;
            case '\"': o << "\\\""; break;
            case '\t': o << "\\t";  break;
            case '\r': o << "\\r";  break;
            case '\n': o << "\\n";  break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b";  break;
            case '\f': o << "\f";   break;
            case '\v': o << "\\v";  break;
            case '\0': o << "\\0";  break;
            default:
            {
                if (isprint(c)) o << c;
                else            o << byte_as_hex(c);
            }
        }
    }
    return o.str();
}

std::string quote_wrap(const std::string s)
{
    std::ostringstream o;
    o << '\"';
    render_string_harmless(s);
    o << '\"';
    return o.str();
}


std::string quote_wrap_except_number(const std::string s)
// This is so that Excel won't try to interpret strings, only numbers.  Lets PGs and LDEVs look normal when you double-click on an ivy csv file.
{
    if (stringCaseInsensitiveEquality("true",s)) return s;
    if (stringCaseInsensitiveEquality("false",s)) return s;
    if (s.size() == 0) return s;
    if ( (!std::regex_match(s,leading_zero_regex)) && std::regex_match(s,float_number_optional_trailing_percent_regex)) return s;
    if (s.size() >=2 && s[0] == '\"' && s[s.size()-1] == '\"') return std::string("=")+s; // already double-quoted, just put equals sign on.
    return std::string("=\"") + s + std::string("\"");
}

std::string quote_wrap_csvline_except_numbers(const std::string csvline) // This is specifically so that when you double-click on an ivy csv file to launch Excel, it won't think LDEV names are times, etc.
{
    std::ostringstream o;

    if (csvline.size() == 0) return "";

    unsigned int last_char = csvline.size()-1;

    unsigned int cursor {0};
    unsigned int last_comma {0};

    if (csvline[0] == ',')
    {
          o << ',';
          cursor = 0;
          last_comma = 0;
    }
    else // csv line does not start with a comma
    {
        if (last_char == 0) // if there is only the one character that is not a comma
        {
            return quote_wrap_except_number(csvline);
        }

        if (advanceToNextUnquotedComma(csvline, cursor))
        {
            o << quote_wrap_except_number(csvline.substr(0,cursor)) << ',';
            last_comma = cursor;
        }
        else
        {
            // no comma was found at all
            return quote_wrap_except_number(csvline);
        }
    }

    // now cursor and last_comma both point to the most recently found comma, which has already been printed

    while (true)  // start a new field.  cursor == last_comma, points at the comma ending the previous field

    {
        // cursor is sitting at a comma, which has already been processed

        if (cursor == last_char) return o.str();

        if (advanceToNextUnquotedComma(csvline,cursor))
        {
            if ( cursor > (last_comma + 1) ) // if between the commas there was text to process
            {
                o << quote_wrap_except_number(csvline.substr(last_comma+1,cursor-(last_comma+1)));
            }
            o << ',';
            last_comma=cursor;
        }
        else
        {
            // there were more characters, but no comma
            o << quote_wrap_except_number(csvline.substr(last_comma+1, csvline.size()-(last_comma+1)));
            return o.str();
        }
    }


}


void ivyTrim(string& s) // remove leading & trailing isIvyWhitespaceCharacter() characters
{
    trim(s);
}



bool isPlusSignCombo(std::string& callers_error_message, int& callersNumberOfFields, const std::string& token )
// Lets you do at least very basic checking on a token
// - May not be the empty string
// - May not begin or end with '+'
// - May not have two consecutive '+'
// Number of fields returned is the same as 1 + number of '+'
{
	callers_error_message.clear();
	callersNumberOfFields = -1;

	int plus_signs_seen {0};

	if (0 == token.length())
	{
		callers_error_message = "An attribute name combination or attribute value combination may not be the null string.";
		return false;
	}

	if ( '+' == token[0]  ||  '+' == token[-1+token.length()] )
	{
		callers_error_message = "The plus sign \'+\' may not be the first or last character of an attribute name combination or attribute value combination.";
		return false;
	}

	if (2 < token.length())
	{
		for (unsigned int i=1; i < (-1+token.length()); i++)
		{
			if ('+' == token[i])
			{
				plus_signs_seen++;
				if ('+' == token[i-1] || '+' == token[i+1])
				{
					callers_error_message = "Two consecutive plus signs do not occur in an attribute name combination or attribute value combination.";
					return false;
				}
			}
		}
	}

	callersNumberOfFields = 1 + plus_signs_seen;
	return true;
}



ivy_float number_optional_trailing_percent(const std::string& s /* e.g. "1.2%" */, std::string name_associated_with_value_for_error_message)  // throws std::invalid_argument
{
	if (!std::regex_match(s, float_number_optional_trailing_percent_regex))
	{
		std::ostringstream o;
		o << "number_optional_trailing_percent(\"" << s << "\") - for \"" << name_associated_with_value_for_error_message << "\" - character string must look like \"56\" or \"2.7\", or \"50%\" or \"0.2%\"." << std::endl ;
		throw std::invalid_argument(o.str());
	}

	ivy_float value;
	{
		std::istringstream is(s);
		is >> value;
	}

	if ('%' == s[s.size()-1])
	{
		value /= 100.;
	}

	return value;
}



unsigned int unsigned_int(const std::string& s /* e.g. "5" */, std::string name_associated_with_value_for_error_message)  // throws std::invalid_argument
{
	if (!std::regex_match(s, unsigned_int_regex))
	{
		std::ostringstream o;
		o << "unsigned_int(\"" << s << "\") - for \"" << name_associated_with_value_for_error_message << "\" - character string must look like \"0\" or \"27\"." << std::endl ;
		throw std::invalid_argument(o.str());
	}

	unsigned int value;
	{
		std::istringstream is(s);
		is >> value;
	}

	return value;
}


std::string column_header_to_identifier(const std::string&s)
{
    return toLower(convert_non_alphameric_or_hyphen_or_period_or_equals_to_underscore(UnwrapCSVcolumn(s)));
}


void xorshift64star(uint64_t& x)  //https://en.wikipedia.org/wiki/Xorshift
{
	x ^= x >> 12; // a
	x ^= x << 25; // b
	x ^= x >> 27; // c
	x *= UINT64_C(2685821657736338717);
	return;
}

uint64_t number_optional_trailing_KiB_MiB_GiB_TiB(const std::string& s_parameter)
{
    std::string s = s_parameter;

    uint64_t multiplier {1};

    if (endsWith(s,"KiB"))
    {
        multiplier = ((uint64_t) 1024);
        s.erase(s.size()-3,3);
    }
    else if (endsWith(s,"MiB"))
    {
        multiplier = ((uint64_t) 1024) * ((uint64_t) 1024);
        s.erase(s.size()-3,3);
    }
    else if (endsWith(s,"GiB"))
    {
        multiplier = ((uint64_t) 1024) * ((uint64_t) 1024) * ((uint64_t) 1024);
        s.erase(s.size()-3,3);
    }
    else if (endsWith(s,"TiB"))
    {
        multiplier = ((uint64_t) 1024) * ((uint64_t) 1024) * ((uint64_t) 1024) * ((uint64_t) 1024);
        s.erase(s.size()-3,3);
    }

    trim(s);

    if (!std::regex_match(s, digits_regex))
	{
		std::ostringstream o;
		o << "<Error> number_optional_trailing_KiB_MiB_GiB_TiB(\"" << s_parameter << "\") - not one or more digits optionally followed by KiB, MiB, GiB, or TiB." << std::endl ;
		throw std::invalid_argument(o.str());
	}

	uint64_t value;
	{
		std::istringstream is(s);
		is >> value;
	}

    return value * multiplier;
}


std::string put_on_KiB_etc_suffix(uint64_t n)
{
    static const uint64_t tera {((uint64_t) 1024) * ((uint64_t) 1024) * ((uint64_t) 1024) * ((uint64_t) 1024) };
    static const uint64_t giga {((uint64_t) 1024) * ((uint64_t) 1024) * ((uint64_t) 1024) };
    static const uint64_t mega {((uint64_t) 1024) * ((uint64_t) 1024) };
    static const uint64_t kilo {((uint64_t) 1024) };

    std::string suffix {};

    if (n>0)
    {
             if (0 == n % tera) {suffix = " TiB"; n /= tera;}
        else if (0 == n % giga) {suffix = " GiB"; n /= giga;}
        else if (0 == n % mega) {suffix = " MiB"; n /= mega;}
        else if (0 == n % kilo) {suffix = " KiB"; n /= kilo;}
    }

    std::ostringstream o;
    o << n << suffix;

    return o.str();
}

std::string remove_trailing_fractional_zeros(std::string s)
{
    std::smatch entire_match;
    std::ssub_match before, after, before_point_after;

    if (std::regex_match(s, entire_match, has_trailing_fractional_zeros_regex))
    {
        before_point_after = entire_match[1];
        before = entire_match[2];
        after = entire_match[3];

        if (0 == after.str().size())
        {
            return before.str();  // We remove the decimal point if there are only zeros after it.
                                  // This does change it from a floating point literal to an unsigned int literal.
        }
        else
        {
            return before_point_after.str();
        }
    }
    else
    {
        return s;
    }
}

std::string rewrite_HHMMSS_to_seconds( std::string s )
{
        std::smatch entire_match;
        std::ssub_match hh, mm, ss, fractional_part;

        if (std::regex_match(s, entire_match, MMSS_regex))
        {
            mm = entire_match[1];
            ss = entire_match[2];
            std::istringstream imm(mm.str());
            std::istringstream iss(ss.str());
            double m, s;
            imm >> m;
            iss >> s;
            std::ostringstream o;
            o << std::fixed << (s+(60*m));
            return remove_trailing_fractional_zeros(o.str());
        }

        if (std::regex_match(s, entire_match, HHMMSS_regex))
        {
            hh = entire_match[1];
            mm = entire_match[2];
            ss = entire_match[4];
            std::istringstream ihh(hh.str());
            std::istringstream imm(mm.str());
            std::istringstream iss(ss.str());
            double h, m, s;
            ihh >> h;
            iss >> s;
            imm >> m;
            std::ostringstream o;
            o << std::fixed << (s+(60*(m+(60*h))));
            return remove_trailing_fractional_zeros(o.str());
        }

        return s;
}


std::string LDEV_to_string(unsigned int ldev)
{
    if ( ldev > 0xffff )
    {
        std::ostringstream o;
        o << "LDEV_to_string( ldev = 0x" << std::uppercase << std::hex << ldev << " ) - max ldev value is 0xFFFF";
        throw std::invalid_argument(o.str());
    }
    return LDEV_to_string((uint16_t) ldev);
}

std::string LDEV_to_string(uint16_t ldev)
{
    std::ostringstream helldev;

    helldev << std::hex << std::setfill('0') << std::setw(4) << std::uppercase << ldev;

    return helldev.str().substr(0,2) + std::string(":") + helldev.str().substr(2,2);
}


std::string byte_as_hex(unsigned char c)
{
    std::ostringstream o;
    o << "0x" << std::hex << std::setfill('0') << std::setw(2) << (unsigned int) c;
    return o.str();
}

std::string safe_string(const unsigned char* p, unsigned int l) // stops at nulls, unprintables changed to '.'
{
    std::string s {};
    for (unsigned int i = 0; i < l; i++)
    {
        unsigned char c = *p++;
        if (c == 0) break;
        if (isprint(c)) { s.push_back(c)   ;}
        else            { s.push_back('.') ;}
    }
    return s;
}

std::string munge_to_identifier(const std::string&s)
{
    std::ostringstream o;
    for (auto c : s)
    {
        if (isalnum(c)) o << c;
        else            o << '_';
    }
    return o.str();
}

std::string left(const std::string& s,unsigned int n)  // leftmost "n" characters
{
    auto sn = s.size();

    if ( n >= sn ) return s;

    return (s.substr(0,n));
}

std::string right(const std::string& s,unsigned int n )
{
    auto sn = s.size();

    if ( n >= sn ) return s;

    return (s.substr(sn-n,n));
}














