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
	std::pair<bool,std::string> set(std::string text, LUN* p_SampleLUN);  // true if text was a valid attribute combo token
};


