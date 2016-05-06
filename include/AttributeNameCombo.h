//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

class AttributeNameCombo  // serial_number+Port
{
private:
	char next_char;
	int cursor, last_cursor;

	void consume() { cursor++; if ( cursor <= last_cursor ) next_char = attributeNameComboID[cursor]; }

public:
//variables
	std::string attributeNameComboID;  // for example, "serial_number+port"
	std::list<std::string> attributeNames;
	bool isValid {false};

//methods
	AttributeNameCombo(){}
	void clear();
	void clone(AttributeNameCombo& other);
	bool set(std::string& callers_error_message, std::string text, LUN* p_SampleLUN);  // true if text was a valid attribute combo token
};


