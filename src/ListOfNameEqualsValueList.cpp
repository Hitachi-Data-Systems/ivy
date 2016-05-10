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
//NameEqualsValueList.cpp

#include <cctype> // for isalpha(), etc.
#include <iostream>
#include <sstream>

#include "ivyhelpers.h"

#include "NameEqualsValueList.h"
#include "ListOfNameEqualsValueList.h"

//#define IVY_PARSE_TRACE_ON

std::string ListOfNameEqualsValueList::toString()
{
	std::ostringstream o;

	o << "ListOfNameEqualsValueList::toString() - text=\"" << text << "\"" << std::endl;

#ifdef IVY_PARSE_TRACE_ON
	o << "compiledOK = " << (compiledOK ? "true" : "false") << std::endl;
	o << "parse_error_message follows:" << std::endl << parse_error_message << std::endl;
	o << "atmostwhitespace = " << (atmostwhitespace ? "true" : "false") << std::endl;
#else//IVY_PARSE_TRACE_ON
	if (!compiledOK)
	{
		o << "Did not parse OK.  Parse error message follows:" << std::endl << parse_error_message << std::endl;
		return o.str();
	}

	o << "Parsed OK";
	if (atmostwhitespace) o << " and is at most whitespace";
	else                  o << " and has " << clauses.size() << " clauses.";
	o <<  std::endl;

#endif//IVY_PARSE_TRACE_ON


// ListOfNameEqualsValueList "LDEV = 00:00-00:FF, Port = (1A,3A,5A,7A}  PG={ 1:3-*, 15-15 }"
//       NameEqualsValueList "LDEV = 00:00-00:FF"
//           name token      "LDEV"
//           value token list
//                value token "00:00-00:FF"
//       NameEqualsValueList "Port = (1A,3A,5A,7A}"
//           name token      "Port"
//           value token list
//                value token "1A"
//                value token "3A"
//                value token "5A"
//                value token "7A"
//       NameEqualsValueList "PG={ 1:3-*, 15-15 }"
//           name token      "PG"
//           value token list
//                value token "1:3-*"
//                value token "15-15"

	o << "ListOfNameEqualsValueList \"" << text << "\"" << std::endl;
	for (auto& p_NameEqualsValueList : clauses)
	{
		o << "      NameEqualsValueList \"" << p_NameEqualsValueList->text << "\"" << std::endl;
		o << "          name token      \"" << p_NameEqualsValueList->name_token << "\"" << std::endl;
		o << "          value token list" << std::endl;
		for (auto& valueToken : p_NameEqualsValueList->value_tokens)
		{
			o << "              value token \"" << valueToken << "\"" << std::endl;
		}
	}
	return o.str();
}

void ListOfNameEqualsValueList::clear()
{
#ifdef IVY_PARSE_TRACE_ON
	std::cout << "ListOfNameEqualsValueList::clear()" << std::endl;
#endif//IVY_PARSE_TRACE_ON
	text.clear();
	parse_error_message.clear();
	compiledOK = false;
	atmostwhitespace = true;
	for (auto& pNameEqualsValueList : clauses)
	{
#ifdef IVY_PARSE_TRACE_ON
		std::cout << "ListOfNameEqualsValueList::clear() - deleting NameEqualsValueList clause:" << std::endl;
		std::cout << pNameEqualsValueList->toString();
#endif//IVY_PARSE_TRACE_ON
		delete pNameEqualsValueList;
	}
	clauses.clear();
#ifdef IVY_PARSE_TRACE_ON
	std::cout << "ListOfNameEqualsValueList::clear() - after clear, toString() shows:" << std::endl;
	std::cout << toString() << std::endl;
#endif//IVY_PARSE_TRACE_ON

}

std::string ListOfNameEqualsValueList::text_with_vertical_bar_cursor_line()
{
	std::ostringstream o;

	o 	<< "text = \"" << text << "\"" << std::endl
		<< "       ";
	for (unsigned int i=0; i<=cursor; i++) o << ' ';
	o << '|' << std::endl;
	return o.str();
}


void ListOfNameEqualsValueList::logParseFailure(std::string msg)
{
	std::ostringstream o;
	o << std::endl << text_with_vertical_bar_cursor_line();
	o << "Parse failure - " << msg;

	parse_error_message += o.str();

#ifdef IVY_PARSE_TRACE_ON
	std::cout << o.str();
#endif//IVY_PARSE_TRACE_ON

}


bool ListOfNameEqualsValueList::isUnquotedOKinNameTokens(char c)
{
	if (isalnum(c)) return true;

	switch (c)
	{
		case '_':
		case '+':
			return true;
		default:
			return false;
	}
}
std::string ListOfNameEqualsValueList::unquotedOKinNameTokens()
{
	return std::string("[a-zA-Z0-9_+]");
}


bool ListOfNameEqualsValueList::isUnquotedOKinValueTokens(char c)
{
	if (isalnum(c)) return true;

	switch (c)
	{
		case '_':
		case '-':
		case ':':
		case '*':
		case '+':
		case '.':
		case '#':
		case '@':
		case '$':
			return true;
		default:
			return false;
	}
}
std::string ListOfNameEqualsValueList::unquotedOKinValueTokens()
{
	return std::string("[a-zA-Z0-9_-:*+.#@$]");
}


bool ListOfNameEqualsValueList::consume_value_token(std::string& s)
{
	s.clear();

#ifdef IVY_PARSE_TRACE_ON
	std::cout << std::endl << text_with_vertical_bar_cursor_line();
	std::cout << "consume_value_token() - Entry." << std::endl;
#endif//IVY_PARSE_TRACE_ON

	if (pastLastCursor())
	{
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << "consume_value_token() - At end of text upon entry." << std::endl;
#endif//IVY_PARSE_TRACE_ON
		return false;
	}

	if ('\'' == c || '\"' == c )
	{
		consume_quoted_string(s);  // always true since we already see the startingQuote
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::ostringstream o;
		o << "ListOfNameEqualsValueList::consume_value_token(): Consumed quoted string.  Returning true with token \"" << s << "\"." << std::endl;
		std::cout << o.str();
#endif//IVY_PARSE_TRACE_ON
		return true;
	}

	if (isUnquotedOKinValueTokens(c))
	{
		while ( (!pastLastCursor() ) && (isUnquotedOKinValueTokens(c)) )
		{
			s.push_back(c);
			consume();
		}
		stepOverWhitespace();
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::ostringstream o;
		o << "ListOfNameEqualsValueList::consume_value_token(): Consumed token \"" << s << "\" and returning true." << std::endl;
		std::cout << o.str();
#endif//IVY_PARSE_TRACE_ON

		return true;
	}

	logParseFailure("consume_value_token() - Failed to parse a value token and returning false.");

	s.clear();
	return false;
}


bool ListOfNameEqualsValueList::consume_closing_brace()
{
	if (pastLastCursor() || '}' != c)
	{
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << "consume_closing_brace() - failed - did not find a next candidate character with value \'}\'." << std::endl;
#endif//IVY_PARSE_TRACE_ON
		return false;
	}
	consume();
	stepOverWhitespace();
#ifdef IVY_PARSE_TRACE_ON
	std::cout << std::endl << text_with_vertical_bar_cursor_line();
	std::cout << "consume_closing_brace() - Consumed closing brace and stepped over any following whitespace." << std::endl;
#endif//IVY_PARSE_TRACE_ON
	return true;
}


void ListOfNameEqualsValueList::set(std::string t)
{
	clear();

	text=t;

	trim(text);

	if (0==text.length())
	{
		compiledOK=true;
		atmostwhitespace=true;
		return;
	}

	cursor=0;
	if (0<text.length()) c=text[0];

	NameEqualsValueList* p_NameEqualsValueList;


#ifdef IVY_PARSE_TRACE_ON
	std::cout << std::endl << text_with_vertical_bar_cursor_line();
	std::cout << std::string("ListOfNameEqualsValueList()::set - about to start parsing.\n");
#endif//IVY_PARSE_TRACE_ON

	while (true)
	{
		// "cursor" is from zero and is the index into "text", the [select] expression text.
		// pastLastCursor() means "cursor >= text.length()"
		// "c" is the next character to be consumed.
		// always use "consume()" to keep "c" updated when the cursor is bumped.

#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << std::string("ListOfNameEqualsValueList()::set - beginning of name = valuelist clause.\n");
#endif//IVY_PARSE_TRACE_ON


		stepOverWhitespace();
		if (pastLastCursor()) break;

		// We have a non-whitespace character starting a new "attribute_name_token = valuelist"

		// It's called attribute_name_token because it could either be quoted string
		// or an identifier completely composed of underscores and alphamerics that doesn't start with a digit.

		savepoint=cursor;
		start_of_clause_point=cursor;

		p_NameEqualsValueList = new NameEqualsValueList();

		atmostwhitespace = false;


		if ( !( consume_quoted_string(p_NameEqualsValueList->name_token) || consume_name_token(p_NameEqualsValueList->name_token)  ) )
		{
			logParseFailure(
				std::string("Looking for \"attribute_name_token = valuelist\".  Did not get attribute name token either as a quoted \n"
				"string or as a sequence of alphabetic a-zA-Z or digit 0-9 characters or the isUnquotedOKinNameTokens() characters ")
				  + unquotedOKinNameTokens()
			);
			delete p_NameEqualsValueList;
			compiledOK = false;
			return;
		}
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << "ListOfNameEqualsValueList()::set - consumed name token \"" << p_NameEqualsValueList->name_token << "\"." << std::endl;
		std::cout << "p_NameEqualsValueList->toString(): " << p_NameEqualsValueList->toString() << std::endl;
#endif//IVY_PARSE_TRACE_ON

		savepoint=cursor;
		if (!consume_equals_sign() )
		{
			logParseFailure("Looking for attribute_name_token = valuelist.  Did not get equals sign \'=\' after attribute name.");
			delete p_NameEqualsValueList;
			compiledOK = false;
			return;
		}
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << "ListOfNameEqualsValueList()::set - consumed equals sign." << std::endl;
#endif//IVY_PARSE_TRACE_ON

		p_NameEqualsValueList->value_tokens.clear();

		savepoint=cursor;
		std::string parsed_value_token;

		if (consume_opening_brace())
		{
			while(true){
				if (pastLastCursor())
				{
					logParseFailure("Attribute value list did not have closing brace (curly bracket \'}\').\n");
					compiledOK = false;
					return;
				}

				if (consume_closing_brace()) break;

				if ( consume_quoted_string(parsed_value_token) || ( consume_value_token(parsed_value_token) ) )
				{
					p_NameEqualsValueList->value_tokens.push_back(parsed_value_token);
#ifdef IVY_PARSE_TRACE_ON
					std::cout << std::endl << text_with_vertical_bar_cursor_line();
					std::cout << "ListOfNameEqualsValueList()::set - pushing back value token \"" << parsed_value_token << "\", resulting in:" << std::endl;
					std::cout << "p_NameEqualsValueList->toString(): " << p_NameEqualsValueList->toString() << std::endl;
#endif//IVY_PARSE_TRACE_ON
				}
				else
				{
					logParseFailure("Invalid value token.\n");
					compiledOK = false;
					return;

				}
			}
		}
		else
		{
			// not a list.  Hopefully a single value
			if ( consume_quoted_string(parsed_value_token) || ( consume_value_token(parsed_value_token) ) )
			{
				p_NameEqualsValueList->value_tokens.push_back(parsed_value_token);
#ifdef IVY_PARSE_TRACE_ON
				std::cout << std::endl << text_with_vertical_bar_cursor_line();
				std::cout << "ListOfNameEqualsValueList()::set - pushing back value token \"" << parsed_value_token << "\", resulting in:" << std::endl;
				std::cout << "p_NameEqualsValueList->toString(): " << p_NameEqualsValueList->toString() << std::endl;
#endif//IVY_PARSE_TRACE_ON
			}
			else
			{
				logParseFailure("Expecting attribute value after equals sign.");
				delete p_NameEqualsValueList;
				compiledOK = false;
				return;
			}
		}

		p_NameEqualsValueList->text = text.substr(start_of_clause_point,cursor-start_of_clause_point);

		ivyTrim(p_NameEqualsValueList->text);  // removes trailing spaces and comma

#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << "ListOfNameEqualsValueList()::set - pushing back NameEqualsValueList:" << std::endl;
		std::cout << "p_NameEqualsValueList->toString(): " << p_NameEqualsValueList->toString() << std::endl;
#endif//IVY_PARSE_TRACE_ON

		clauses.push_back(p_NameEqualsValueList);
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << "ListOfNameEqualsValueList()::set - after pushing back NameEqualsValueList pointer:" << std::endl;
		std::cout << "ListOfNameEqualsValueList->toString(): " << toString() << std::endl;
#endif//IVY_PARSE_TRACE_ON

	}

	compiledOK=true;
	return;
}


void ListOfNameEqualsValueList::stepOverWhitespace()
{
	// Commas , are considered whitespace in ivy.  You can use them to delimit lists or not.

#ifdef IVY_PARSE_TRACE_ON
	std::cout << std::endl << text_with_vertical_bar_cursor_line();
	std::cout << "ListOfNameEqualsValueList::stepOverWhitespace() entered:" << std::endl;
#endif//IVY_PARSE_TRACE_ON

	while (true)
	{
		if (pastLastCursor())
		{
#ifdef IVY_PARSE_TRACE_ON
			std::cout << std::endl << text_with_vertical_bar_cursor_line();
			std::cout << "ListOfNameEqualsValueList::stepOverWhitespace() exit - reached end of text string.\n";
#endif//IVY_PARSE_TRACE_ON
			return;
		}
		c = text[cursor];
		if (' ' !=c && '\t' !=c && ',' != c)
		{
#ifdef IVY_PARSE_TRACE_ON
			std::cout << std::endl << text_with_vertical_bar_cursor_line();
			std::cout << "ListOfNameEqualsValueList::stepOverWhitespace() normal exit.:\n";
#endif//IVY_PARSE_TRACE_ON
			return;
		}
		cursor++;
	}
}


bool ListOfNameEqualsValueList::consume_quoted_string(std::string& s)
{
	s.clear();

#ifdef IVY_PARSE_TRACE_ON
	std::cout << std::endl << text_with_vertical_bar_cursor_line();
	std::cout << "consume_quoted_string() entered." << std::endl;
#endif//IVY_PARSE_TRACE_ON

	// consumes if the next character is ' or ", set into "startingQuote".

	// Subsequent characters up to the ending quote (same character value as the startingQuote)
	// are put in "s" and consumed.

	// The only special processing of any characters between beginning and ending quotes
	// is the case of a backlash-startingQuote pair, where the backslash does not make it into the string s.

	if (pastLastCursor())
	{
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << "ListOfNameEqualsValueList::consume_quoted_string(): at end of text string upon entry, returning false.\n";
#endif//IVY_PARSE_TRACE_ON
		return false;
	}

	if ('\'' != c && '\"' != c)
	{
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << "ListOfNameEqualsValueList::consume_quoted_string(): First character is not a single or double quote, returning false.\n";
#endif//IVY_PARSE_TRACE_ON
		return false;
	}

	startingQuote=c;
	consume();  // consume the starting quote

	while (true)
	{
		if (pastLastCursor())
		{
#ifdef IVY_PARSE_TRACE_ON
			std::cout << std::endl << text_with_vertical_bar_cursor_line();
			std::cout << "ListOfNameEqualsValueList::consume_quoted_string(): hit end of text string - considered to be a close-quote.  Returning true." << std::endl;
#endif//IVY_PARSE_TRACE_ON
			return true;  // in other words, running off the end of the text is considered to be a close-quote :-)
		}

		if (c == startingQuote)
		{
			consume(); // consume ending quote and exit.
#ifdef IVY_PARSE_TRACE_ON
			std::cout << std::endl << text_with_vertical_bar_cursor_line();
			std::ostringstream o;
			o << "ListOfNameEqualsValueList::consume_quoted_string(): Consumed ending quote, returning true with string \"" << s << "\"." << std::endl;
			std::cout << o.str();
#endif//IVY_PARSE_TRACE_ON
			return true;
		}

		if ( ( c == '\\' ) && ( cursor < ( -1 + text.length() ) ) && ( startingQuote == text[cursor+1] ) )
			consume(); // consume the backslash but don't put it in "s".
		s.push_back(c);
		consume(); // eat the character pushed back
	}
}


bool ListOfNameEqualsValueList::consume_name_token(std::string& s)
{
	s.clear();

#ifdef IVY_PARSE_TRACE_ON
	std::cout << std::endl << text_with_vertical_bar_cursor_line();
	std::cout << "consume_name_token() - Entry." << std::endl;
#endif//IVY_PARSE_TRACE_ON

	if (pastLastCursor())
	{
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << "consume_name_token() - Ran off the end." << std::endl;
#endif//IVY_PARSE_TRACE_ON
		return false;
	}

	if ('\'' == c || '\"' == c )
	{
		consume_quoted_string(s);  // always true since we already see the startingQuote
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::ostringstream o;
		o << "ListOfNameEqualsValueList::consume_name_token(): Consumed quoted token \"" << s << "\" and returning true." << std::endl;
		std::cout << o.str();
#endif//IVY_PARSE_TRACE_ON
		return true;
	}

	if (isUnquotedOKinNameTokens(c))
	{
		while ( (!pastLastCursor() ) && (isUnquotedOKinNameTokens(c)) )
		{
			s.push_back(c);
			consume();
		}
		stepOverWhitespace();
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::ostringstream o;
		o << "ListOfNameEqualsValueList::consume_name_token(): Consumed token \"" << s << "\" and returning true." << std::endl;
		std::cout << o.str();
#endif//IVY_PARSE_TRACE_ON

		return true;
	}

#ifdef IVY_PARSE_TRACE_ON
	std::cout << std::endl << text_with_vertical_bar_cursor_line();
	std::cout << "consume_name_token() - Failed to parse a name token and returning false." << std::endl;
#endif//IVY_PARSE_TRACE_ON

	s.clear();
	return false;
}


bool ListOfNameEqualsValueList::consume_equals_sign()
{
	if (pastLastCursor() || '=' != c)
	{
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << "consume_equals_sign() - failed - did not find a next candidate character with value \'=\'." << std::endl;
#endif//IVY_PARSE_TRACE_ON
		return false;
	}
	consume();
	stepOverWhitespace();
#ifdef IVY_PARSE_TRACE_ON
	std::cout << std::endl << text_with_vertical_bar_cursor_line();
	std::cout << "consume_equals_sign() - Consumed equals sign and stepped over any following whitespace." << std::endl;
#endif//IVY_PARSE_TRACE_ON
	return true;
}


bool ListOfNameEqualsValueList::consume_opening_brace()
{
	if (pastLastCursor() || '{' != c)
	{
#ifdef IVY_PARSE_TRACE_ON
		std::cout << std::endl << text_with_vertical_bar_cursor_line();
		std::cout << "consume_opening_brace() - failed - did not find a next candidate character with value \'{\'." << std::endl;
#endif//IVY_PARSE_TRACE_ON
		return false;
	}
	consume();
	stepOverWhitespace();
#ifdef IVY_PARSE_TRACE_ON
	std::cout << std::endl << text_with_vertical_bar_cursor_line();
	std::cout << "consume_opening_brace() - Consumed and stepped over any following whitespace." << std::endl;
#endif//IVY_PARSE_TRACE_ON
	return true;
}




