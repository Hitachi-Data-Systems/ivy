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
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sys/stat.h>
#include <string.h>

#include "ivytime.h"
#include "ivydefines.h"
#include "ivyhelpers.h"
#include "ivy_engine.h"

#include "GatherData.h"

std::string metric_value::toString()
{
    std::ostringstream o;
    o   << "{ start = \"" << start.format_as_datetime_with_ns()
        << "\", complete = " << complete.format_as_datetime_with_ns()
        << "\", value = \"" << value << "\" }";
	return o.str();
}

uint64_t metric_value::uint64_t_value(std::string name_associated_with_value_for_error_message)
{
	std::string s = value;
	trim(s);
	if (!(isalldigits(s))) throw std::invalid_argument
        (
            std::string("metric_value::uint64_t_value() for name = \"")
            + name_associated_with_value_for_error_message
            + std::string("\" - not an unsigned integer \"")
            + s
            + std::string("\".")
        );
	std::istringstream si(s);
	uint64_t n;
	si >> n;
	return n;
}

ivy_float metric_value::numeric_value(std::string name_associated_with_value_for_error_message)
{
	std::string s = value;
	trim(s);
    return number_optional_trailing_percent(s, name_associated_with_value_for_error_message);
}

std::ostream& operator<<(std::ostream& o, const GatherData& gd)
{
//*debug*/std::cout << "std::ostream& operator<<(std::ostream& o, const GatherData& gd) entry" << std::endl;

	o << "{ " << std::endl;

	for (auto& pear : gd.data)
	{
		const std::string& element = pear.first;

//*debug*/std::cout << "std::ostream& operator<<(std::ostream& o, const GatherData& gd) element=\"" << element << "\"" << std::endl;

		for (auto& peach : pear.second)
		{
			const std::string& instance = peach.first;

//*debug*/std::cout << "std::ostream& operator<<(std::ostream& o, const GatherData& gd) instance=\"" << instance << "\"" << std::endl;

			for (auto& banana : peach.second)
			{
				const std::string& metric = banana.first;
				const metric_value& mv = banana.second;

//*debug*/std::cout << "std::ostream& operator<<(std::ostream& o, const GatherData& gd) metric=\"" << metric << "\", string_value=\"" << mv.string_value()
//*debug*/<< "\"  , start=" << mv.gather_start().format_as_datetime_with_ns() << ", complete=" << mv.gather_complete().format_as_datetime_with_ns() << std::endl;

				o << "{ element=\"" << element << "\", instance=\"" << instance << "\", metric=\"" << metric << "\", value=\"" ;
				o << mv.string_value();
				o << "\", start=\"" << mv.gather_start().format_as_datetime_with_ns()
					<< "\", complete=\"" << mv.gather_complete().format_as_datetime_with_ns()
					<< " }" << std::endl;
			}
		}
	}

	o << "}" << std::endl;

//*debug*/std::cout << "std::ostream& operator<<(std::ostream& o, const GatherData& gd) exit" << std::endl;

    return o;

}

metric_value& GatherData::get(const std::string& element, const std::string& instance, const std::string& metric)
{
	auto element_it = data.find(element);
	if (data.end() == element_it)
	{
		std::ostringstream o;
		o << "No such element \"" << element << "\".";
		o << "  GatherData = " << (*this);
		throw std::out_of_range(o.str());
	}

	auto instance_it = (*element_it).second.find(instance);
	if ((*element_it).second.end() == instance_it)
	{
		std::ostringstream o;
		o << "No such instance \"" << instance << "\" for element \"" << element << "\".";
		o << "  GatherData = " << (*this);
		throw std::out_of_range(o.str());
	}

	auto metric_it = (*instance_it).second.find(metric);
	if ((*instance_it).second.end() == metric_it)
	{
		std::ostringstream o;
		o << "No such metric \"" << metric << "\" for instance \"" << instance << "\" for element \"" << element << "\".";
		o << "  GatherData = " << (*this);
		throw std::out_of_range(o.str());
	}

	return (*metric_it).second;
}


void GatherData::print_csv_file_set(std::string root_folder /* must already exist and be a directory */, std::string subfolder_leaf_name /* like "401034.config" */)

// Used to print a one-time standalone gather (e.g. config gather) in a designated subfolder to be created,
// where there will be a file for each element type, like LDEV or Port, and lines for each instance of that element type.

{

	struct stat struct_stat;

	if( stat(root_folder.c_str(),&struct_stat))
	{
		std::ostringstream o;
		o << "<Error> GatherData::print_csv_file_set(std::string root_folder = \"" << root_folder << "\" - must already exist and be a directory, std::string subfolder_leaf_name=\""
			<< subfolder_leaf_name << "\" like \"401034.config\" or \"410034.t=0\")." << std::endl
			<< "root_folder \"" << root_folder << "\" does not exist." << std::endl;
		throw std::invalid_argument(o.str());
	}

	if(!S_ISDIR(struct_stat.st_mode)){
		std::ostringstream o;
		o << "<Error> GatherData::print_csv_file_set(std::string root_folder = \"" << root_folder << "\" - must already exist and be a directory, std::string subfolder_leaf_name=\""
			<< subfolder_leaf_name << "\" like \"401034.config\" or \"410034.t=0\")." << std::endl
			<< "root_folder \"" << root_folder << "\" is not a directory." << std::endl;
		throw std::invalid_argument(o.str());
	}

	std::regex leaf_name_regex(OK_LEAF_NAME_REGEX);

	if (!std::regex_match(subfolder_leaf_name,leaf_name_regex))
	{
		std::ostringstream o;
		o << "<Error> GatherData::print_csv_file_set(std::string root_folder = \"" << root_folder << "\" - must already exist and be a directory, std::string subfolder_leaf_name=\""
			<< subfolder_leaf_name << "\" like \"401034.config\" or \"410034.t=0\")." << std::endl
			<< "invalid subfolder_leaf_name \"" << subfolder_leaf_name << "\"." << std::endl;
		throw std::invalid_argument(o.str());
	}

	std::string leaf_folder	= root_folder + path_separator + subfolder_leaf_name;

	if(0==stat(leaf_folder.c_str(),&struct_stat))
	{
		 // already exists

		if(!S_ISDIR(struct_stat.st_mode))
		{
			std::ostringstream o;
			o << "<Error> GatherData::print_csv_file_set(std::string root_folder = \"" << root_folder << "\" - must already exist and be a directory, std::string subfolder_leaf_name=\""
				<< subfolder_leaf_name << "\" like \"401034.config\" or \"410034.t=0\")." << std::endl
				<< "subfolder leaf \"" << leaf_folder << "\" exists but is not a directory." << std::endl;
			throw std::invalid_argument(o.str());
		}
	}
	else
	{
		if (mkdir(leaf_folder.c_str(),
		      S_IRWXU  // r,w,x user
		    | S_IRWXG  // r,w,x group
		    | S_IRWXO  // r,w,x other
        ))
        {
			std::ostringstream o;
			o << "<Error> GatherData::print_csv_file_set(std::string root_folder = \"" << root_folder << "\" - must already exist and be a directory, std::string subfolder_leaf_name=\""
				<< subfolder_leaf_name << "\" like \"401034.config\" or \"410034.t=0\")." << std::endl
				<< "Failed to create leaf subfolder \"" << leaf_folder
				<< "\" - errno = " << errno << " - " << strerror(errno) << std::endl;
			throw std::invalid_argument(o.str());
		}
	}



	for (auto& element_pair : data)
	{
		const std::string& element_type = element_pair.first;  // like ldev or port
		auto& element_instances = element_pair.second;

		// we make two passes though the element instances.

		// On the first pass, we accumulate the set of all attribute names that appear in at least one of the instances.

		// Then we print the title header line iterating over the unique attribute names.

		// Then for each element instance we print the instance type, instance ID, for each column title, we try to retrieve the attribute from the instance,
		// printing an empty column if the instance doesn't have the particular attribute.

		std::set<std::string> column_headers;

		for (auto& instance_pair : element_instances)
		{
			//const std::string& instance_name = instance_pair.first;
			for (auto& attribute_pair : instance_pair.second)
			{
				const std::string& attribute_name = attribute_pair.first;

				column_headers.insert(attribute_name);
			}
		}

		std::string filename  = leaf_folder + path_separator + subfolder_leaf_name + std::string(".") + element_type + std::string(".csv");

		std::ofstream ofs(filename);

		// print header line

		ofs << element_type;

		for (auto& header : column_headers)
		{
			ofs << ',' << header;
		}
		ofs << std::endl;

		// print a data line for each instance

		for (auto& instance_pair : element_instances)
		{
			ofs << quote_wrap_except_number(instance_pair.first,m_s.formula_wrapping);

			for (const std::string& header : column_headers)
			{
				ofs << ',';

				std::map<std::string, metric_value>::iterator
					att_it = instance_pair.second.find(header);

				if (instance_pair.second.end() != att_it)
				{
					ofs << quote_wrap_except_number((*att_it).second.string_value(),m_s.formula_wrapping);
				}
			}
			ofs << std::endl;
		}
		ofs.close();

	} // for each element type like subsystem or LDEV or port.

	return;
}












