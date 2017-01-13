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
#include <stdexcept>
#include <sstream>
#include <string.h>

#include "hosts_list.h"
#include "hosts/hosts.parser.hh"


typedef struct yy_buffer_state *YY_BUFFER_STATE;

extern YY_BUFFER_STATE xy__scan_string ( const char *yy_str  );
void xy__delete_buffer ( YY_BUFFER_STATE b  );


std::ostream& operator<< (std::ostream& o, const hosts_list& hl)
{
    o << "{ ";

    o << "\"source_string\" : \"" << hl.source_string << "\"";

    o << ", \"message\" : \"" << hl.message << "\"";

    o << ", \"count\" : " << hl.hosts.size();

    o << ", \"hosts\" : [ ";

        bool need_comma {false};
        for (auto& h: hl.hosts)
        {
            if (need_comma) o << ", ";
            need_comma = true;

            o << "\"" << h << "\"";
        }

    o << " ]";

    o <<  ", \"successful_compile\" : "   << (hl.successful_compile   ? "true" : "false" );
    o <<  ", \"unsuccessful_compile\" : " << (hl.unsuccessful_compile ? "true" : "false" );
    o <<  ", \"has_hostname_range\" : "   << (hl.has_hostname_range   ? "true" : "false" );


    o << " }";

    return o;
}

bool hosts_list::compile(const std::string& s)
{
    clear();

    source_string = s;

    if (s.size() == 0)
    {
        successful_compile = true;
        message = "test host list expression evaluated to null string";
        return true;
    }

	xy_::hosts parser(*this);

    YY_BUFFER_STATE xy_buffer = xy__scan_string(s.c_str());  // it's two underscores, the first is part of the xy_

	parser.parse();

    xy__delete_buffer(xy_buffer);

	if (successful_compile)
	{
        std::ostringstream o;
        o << "Successful compile.";
        message += o.str();  // concatenating to the end of what the parser put in message
        return true;
	}

    {
        std::ostringstream o;

        o << "Unsuccessful compile of list of test hosts \"" << s << "\"." << std::endl
        << "Example of a list of test hosts - \"aardvark, scooter12-15, 192.168.0.0\"." << std::endl
        << "ivy accepts only hostnames that look like an \"identifier\", that is, a word that starts with a letter" << std::endl
        << "followed by zero or more letters, digits, or underscores _. Ranges of hostnames with numbered suffixes" << std::endl
        << "as in scooter12-15 are expanded as in scooter12, scooter13, scooter14, scooter15.";

        message += o.str();
    }

    return false;
}
