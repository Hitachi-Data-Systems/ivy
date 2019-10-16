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

#include "AttributeNameCombo.h"


std::pair<bool,std::string> isValidAttributeName(const std::string& token, const LUN* pSampleLUN)
{
	if (stringCaseInsensitiveEquality(token, std::string("all"))) return std::make_pair(true,"");

	if (pSampleLUN->contains_attribute_name(token)) return std::make_pair(true,"");

    std::ostringstream o;

    o << "Invalid attribute name \"" << token << "\".  Valid LUN attribute names are " << pSampleLUN-> valid_attribute_names();

    return std::make_pair(false,o.str());
}


void AttributeNameCombo::clone( AttributeNameCombo& other )
{
	attributeNameComboID = other.attributeNameComboID;
	attributeNameComboID_preserving_case = other.attributeNameComboID_preserving_case;
	attributeNames.clear(); for (auto& name : other.attributeNames) attributeNames.push_back(name);
	isValid=other.isValid;
}


void AttributeNameCombo::clear()
{
	attributeNameComboID.clear();
	attributeNameComboID_preserving_case.clear();
	attributeNames.clear();
	isValid=false;
}


std::pair<bool,std::string> AttributeNameCombo::set(const std::string& t, LUN* pSampleLUN)  //  serial_number+Port
{
	// A valid attribute name combo token consists of one or more valid LUN-lister column header attribute names,
	// or ivy knicknames yielding possibly processed versions of LUN-lister attribute values,
	// joined into a single token using plus signs as separators.

	// The upper/lower case of the text that the user supplies to create a rollup
	// persists and shows in output csv files, but all comparisons for matches on
	// attribute names are case-insensitive.

	// LUN-lister column header tokens are pre-processed to a "flattened" form
	// for the purposes of generating keys and for matching identity.
	//  - Characters that would not be allowed in an identifier name are converted to underscores _.
	//  - Text is translated to lower case.

	// That means that at any time in ivy, saying "LUN Name" is the same as saying lun_name.
	// In other words, you can use the actual orginal LUN-lister csv headers if you put them in quotes.


	// The job of the constructor is to parse the text
	// - split the text into chunks separated by + signs.
	// - Validate that each chunk either matches a built-in attribute like host or workload,
	//   or that it matches one of the column headers in the sample LUN provided to the constructor.

	clear();

    std::string token {}, token_preserving_case {};

    for (unsigned int i = 0; i < t.size(); i++)
    {
        auto c = t[i];

        if (c == '+' || i == (t.size()-1))
        {
            if (i == (t.size()-1))
            {
                if (c == '+') { return std::make_pair(false, std::string("AttributeNameCombo::set(\""s + t + "\") - missing attribute name.")); }
                token.push_back(c);
            }

            trim(token);

            if (token.size() > 2 && token[0] == '\"' && token[token.size()-1] == '\"')
            {
                // remove double-quotes surrounding token.
                token.erase(token.size()-1,1);
                token.erase(0,1);
                trim(token);
            }

            if (0 == token.length())
            {
                return std::make_pair(false,std::string("AttributeNameCombo::set(\""s + t + "\") - missing attribute name."));
            }

            token = LUN::normalize_attribute_name(token);

            std::pair<bool,std::string> retval = isValidAttributeName(token, pSampleLUN);

            if (retval.first)  // either a LUN-lister column heading, or an ivy nickname
            {
                attributeNames.push_back(token);

                token_preserving_case = pSampleLUN->original_name(token);

                if (attributeNameComboID.size() > 0)
                {
                    attributeNameComboID                 += "+"s;
                    attributeNameComboID_preserving_case += "+"s;
                }

                attributeNameComboID                 += token;
                attributeNameComboID_preserving_case += token_preserving_case;

                token.clear();
            }
            else
            {
                std::ostringstream o;
                o << "AttributeNameCombo::set() - " << retval.second;
                return std::make_pair(false,o.str());
            }
        }
        else
        {
            token.push_back(c);
        }
    }

	isValid=true;

	return std::make_pair(true,"");
}















