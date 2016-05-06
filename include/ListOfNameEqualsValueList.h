//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once


class ListOfNameEqualsValueList
{
private:
// variables
	std::string text;
	std::string parse_error_message;
	bool compiledOK {false};  // is turned to true if text parsed OK.  A zero-length or just whitespace text compiles OK with zero clauses.
	bool atmostwhitespace=true;  // text is has zero length or is just whitespace

	unsigned int cursor;
	char c;
	char startingQuote;
	int savepoint,start_of_clause_point;

// methods
	inline bool pastLastCursor() {return cursor>=text.length();}
	inline void consume() { cursor++; if (!pastLastCursor()) c = text[cursor];}

	std::string text_with_vertical_bar_cursor_line();
	void logParseFailure(std::string msg);

	bool isUnquotedOKinNameTokens(char c);
	std::string unquotedOKinNameTokens();   // e.g. but not necessarily now "[a-zA-Z0-9_+]"

	bool isUnquotedOKinValueTokens(char c);
	std::string unquotedOKinValueTokens();  // e.g. but not necessarily now "[a-zA-Z0-9_-:*+.#@$]"

	void stepOverWhitespace();
	bool consume_quoted_string(std::string& s);
	bool consume_name_token(std::string& s);
	bool consume_equals_sign();
	bool consume_opening_brace();
	bool consume_value_token(std::string& s);
	bool consume_closing_brace();

public:
// variables
	std::list<NameEqualsValueList*> clauses;

// methods
	void clear();
	void set(std::string text);

	ListOfNameEqualsValueList(std::string text){set(text);}
	ListOfNameEqualsValueList(){clear();}

	inline bool parsedOK(){return compiledOK;}
	inline bool isNull(){return atmostwhitespace;}
	inline std::string getParseErrorMessage() {return parse_error_message;}
	std::string toString();

};

