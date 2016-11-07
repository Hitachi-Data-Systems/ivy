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
#include <string>
#include <list>
#include<iostream>

class hosts_list
{
public:
// variables
	std::string source_string     {};
	std::list<std::string> hosts  {};
	bool successful_compile       {false};
	bool unsuccessful_compile     {false};
	std::string message           {};
	bool has_hostname_range       {false};

// methods
	void error(const std::string& s) {unsuccessful_compile=true;message+=s;}
	bool compile(const std::string&);
	void clear()
	{
        source_string.clear();
        hosts.clear();
        successful_compile=false;
        unsuccessful_compile=false;
        message.clear();
        has_hostname_range=false;
    }
};

std::ostream& operator<< (std::ostream&, const hosts_list&);
