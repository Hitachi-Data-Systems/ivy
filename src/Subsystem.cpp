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
#include <sys/stat.h>

#include "Subsystem.h"
#include "ivy_engine.h"

Subsystem::Subsystem(std::string sn) : serial_number(sn) {} // Note constructor leaves LUNpointers empty

Subsystem::~Subsystem() {}

std::string Subsystem::type() { return "default"; }

void Subsystem::gather() {}  // see pipe_driver_subthread for gather processing

void Subsystem::print_subinterval_csv_line_set( std::string subfolder_leaf_name /* like "401034.perf" */)
// Used to print the contents of the performance GatherData object vector, where in the designated
// subfolder, further subfolders for each element type such as LDEV or Port are created.

// Then in these subfolders, a separate file is used for each instance of the element type.

// This file has a header line, and one data line for each GatherData object in the vector,
// that is, one data line for each subinterval.

{
    if (gathers.size() == 0) return;

    std::string& root_folder = m_s.stepFolder;

	struct stat struct_stat;

	// check that the test step's root_folder already exists and is a directory

	if( stat(root_folder.c_str(),&struct_stat))
	{
		std::ostringstream o;
		o << "<Error> GatherData::print_subinterval_csv_line_set(std::string root_folder = \"" << root_folder << "\" - must already exist and be a directory, std::string subfolder_leaf_name=\""
			<< subfolder_leaf_name << "\" like \"401034.config\" or \"410034.t=0\")." << std::endl
			<< "root_folder \"" << root_folder << "\" does not exist." << std::endl;
		throw std::invalid_argument(o.str());
	}

	if(!S_ISDIR(struct_stat.st_mode)){
		std::ostringstream o;
		o << "<Error> GatherData::print_subinterval_csv_line_set(std::string root_folder = \"" << root_folder << "\" - must already exist and be a directory, std::string subfolder_leaf_name=\""
			<< subfolder_leaf_name << "\" like \"401034.config\" or \"410034.t=0\")." << std::endl
			<< "root_folder \"" << root_folder << "\" is not a directory." << std::endl;
		throw std::invalid_argument(o.str());
	}


	// check that output subfolder (leaf) name looks OK

	std::regex leaf_name_regex(OK_LEAF_NAME_REGEX);

	if (!std::regex_match(subfolder_leaf_name,leaf_name_regex))
	{
		std::ostringstream o;
		o << "<Error> GatherData::print_subinterval_csv_line_set(std::string root_folder = \"" << root_folder << "\" - must already exist and be a directory, std::string subfolder_leaf_name=\""
			<< subfolder_leaf_name << "\" like \"401034.config\" or \"410034.t=0\")." << std::endl
			<< "invalid subfolder_leaf_name \"" << subfolder_leaf_name << "\"." << std::endl;
		throw std::invalid_argument(o.str());
	}


	// Create the output leaf subfolder if it doesn't already exist

	std::string leaf_folder	= root_folder + path_separator + subfolder_leaf_name;

	if(0==stat(leaf_folder.c_str(),&struct_stat))
	{
		 // already exists

		if(!S_ISDIR(struct_stat.st_mode))
		{
			std::ostringstream o;
			o << "<Error> GatherData::print_subinterval_csv_line_set(std::string root_folder = \"" << root_folder << "\" - must already exist and be a directory, std::string subfolder_leaf_name=\""
				<< subfolder_leaf_name << "\" like \"401034.config\" or \"410034.t=0\")." << std::endl
				<< "subfolder leaf \"" << leaf_folder << "\" exists but is not a directory." << std::endl;
			throw std::invalid_argument(o.str());
		}
	}
	else
	{
		if (mkdir(leaf_folder.c_str(),S_IRWXU | S_IRWXG | S_IRWXO)) {
			std::ostringstream o;
			o << "<Error> GatherData::print_subinterval_csv_line_set(std::string root_folder = \"" << root_folder << "\" - must already exist and be a directory, std::string subfolder_leaf_name=\""
				<< subfolder_leaf_name << "\" like \"401034.config\" or \"410034.t=0\")." << std::endl
				<< "Failed to create leaf subfolder \"" << leaf_folder
				<< "\" - errno = " << errno << " - " << strerror(errno) << std::endl;
			throw std::invalid_argument(o.str());
		}
	}

    // Element types or metrics that are derived from the difference in cumulative counter values in successive subintervals don't appear
    // until the 2nd gather for subinterval 0.

    // (The first gather is done at t=0, so subinterval 0 corresponds to the 2nd gather.)

	for (auto& element_pair : gathers.back().data)
	// For each element type like LDEV, or PG
	// We get the element types from the last gather because some element types that are derived from the delta between successive gathers don't appear until the 2nd gather for subinterval 0.
	{
		const std::string& element_type = element_pair.first;
//		auto& element_instances = element_pair.second;

		// Prepare header line for this element type

		// The header line has a column for every metric that appears for any instance in any gather (subinterval).

		// This means that all the instance csv files (LDEV=00:00) for a given element type (LDEV) have the same header line / same csv columns,
		// even though not all instances may have the same metrics.

		std::set<std::string> column_headers;

        for (unsigned int i = 0; i < gathers.size(); i++)
        {
            GatherData& gd = gathers[i];

            auto element_it = gd.data.find(element_type);

            if (element_it == gd.data.end()) continue; // not all elements appear in gathers[0];

            for (auto& instance_pair : element_it->second)
            {
                for (auto& metric_pair : instance_pair.second)
                {
                    const std::string& metric = metric_pair.first;

                    if (!endsWith(metric,"_count")) column_headers.insert(metric);
                }
            }
        }

		std::ostringstream o;

		o << element_type << ",subinterval number";
		for (auto& header : column_headers) o << ',' << header;
		o << std::endl;

		std::string header_line = o.str();

        std::string normalized_element_type = LUN::normalize_attribute_name(element_type);

		// Create the sub-subfolder for the element, if it doesn't already exist.

		if (!std::regex_match(normalized_element_type,leaf_name_regex))
		{
			std::ostringstream o;
			o << "<Error> Subsystem::print_subinterval_csv_line_set(root_folder = \"" << root_folder << "\", subfolder_leaf_name=\"" << subfolder_leaf_name << "\"):" << std::endl
				<< "Invalid element type \"" << element_type << "\" - unsuitable to use as subfolder name." << std::endl;
			std::cout << o.str();
			log(m_s.masterlogfile, o.str());
			m_s.kill_subthreads_and_exit();
		}

		std::string element_folder = leaf_folder + path_separator + normalized_element_type;

		if( stat(element_folder.c_str(),&struct_stat))
		{
			// element folder doesn't exist yet - need to make it

			if (mkdir(element_folder.c_str(),S_IRWXU | S_IRWXG | S_IRWXO)) {
				std::ostringstream o;
				o << "<Error> Subsystem::print_subinterval_csv_line_set(root_folder = \"" << root_folder << "\",subfolder_leaf_name=\"" << subfolder_leaf_name << "\"):" << std::endl
					<< "Failed to create element subfolder \"" << element_folder
					<< "\" - errno = " << errno << " - " << strerror(errno) << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile, o.str());
                m_s.kill_subthreads_and_exit();
			}
		}
		else
		{
			if(!S_ISDIR(struct_stat.st_mode))
			{
				std::ostringstream o;
				o << "<Error> Subsystem::print_subinterval_csv_line_set(root_folder = \"" << root_folder << "\",subfolder_leaf_name=\"" << subfolder_leaf_name << "\"):" << std::endl
					<< "element folder \"" << element_folder << "\" already exists but is not a directory." << std::endl;
                std::cout << o.str();
                log(m_s.masterlogfile, o.str());
                m_s.kill_subthreads_and_exit();
			}
		}

		// print a csv file for each instance

		for (auto& instance_pair : element_pair.second)
		{
			const std::string& instance = instance_pair.first;

			// b1_demo1_fixed.step0000.step_eh.all=all.step_detail_by_subinterval.csv

			std::string instance_filename = element_folder + path_separator
                //+ edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.testName)
                + m_s.testName
                + "."
                + m_s.stepNNNN
                + "."
                + edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(m_s.stepName)
                + ".rmlib."
                + edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(element_type)
                + std::string("=")
                + edit_out_colons_and_convert_non_alphameric_or_hyphen_or_equals_to_underscore(instance)
                + std::string(".csv");

			std::ofstream instance_file(instance_filename, std::fstream::out);

            instance_file << header_line;

            for (unsigned int t = 0; t < gathers.size(); t++)
            {
                auto x = gathers[t].data.find(element_type);
                if (x == gathers[t].data.end()) continue;  // not all element types appear at t=0

                instance_file << quote_wrap_LDEV_PG_leading_zero_number(instance_pair.first,m_s.formula_wrapping) << ","; // instance name

                if (t==0) instance_file << "t=0";
                else      instance_file << (t-1); // subinterval number

                for (const std::string& header : column_headers)
                {
                    instance_file << ',';

                    auto& metric_map = (*x).second[instance];

                    std::map<std::string, metric_value>::iterator
                        att_it = metric_map.find(header);

                    if (att_it != metric_map.end())
                    {
                        instance_file << quote_wrap_LDEV_PG_leading_zero_number(att_it->second.string_value(),m_s.formula_wrapping);
                    }
                }
                instance_file << std::endl;
            }
		}
	} // for each element type like subsystem or LDEV or port.

	return;
}


Hitachi_RAID_subsystem::Hitachi_RAID_subsystem(std::string serial_number, LUN* pL) : Subsystem(serial_number), pCmdDevLUN(pL) {}

Hitachi_RAID_subsystem::~Hitachi_RAID_subsystem() {}

std::string Hitachi_RAID_subsystem::type() { return "Hitachi RAID"; }

void Hitachi_RAID_subsystem::gather()
{
	// Looks like gather function done elsewhere - by pipe_driver_subthread???  I forget.
}

ivy_float Hitachi_RAID_subsystem::get_wp(const std::string& CLPR, int subinterval)
{
	// use subinterval = -1 to get the WP value from the gather at t=0

	if (gathers.size() == 0)
	{
		std::ostringstream o;
		o << "<Error> Hitachi_RAID_subsystem::get_wp( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
			<< " - get_wp() called when gathers.size()=0"<< std::endl;
		throw std::invalid_argument(o.str());
	}

	int max_subinterval = gathers.size() - 2;  // if there is one gather, the t=0 gather was performed, and we can retrieve that, but there are no subinterval end gathers yet.

	if( (subinterval < -1) || (subinterval > max_subinterval))
	{
		std::ostringstream o;
		o << "<Error> Hitachi_RAID_subsystem::get_wp( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
			<< " - invalid subinterval.  There is a gather at t=0 (use subinterval=-1), and there are gathers for " << (gathers.size()-1) << " subintervals.  Valid subinterval numbers for get_wp are -1 through " << (gathers.size()-2) << std::endl;
		throw std::invalid_argument(o.str());
	}

	int gather_index = subinterval+1;
//	GatherData& gd = gathers[gather_index];

	std::string string_value;

//	ivy_float WP_MiB, size_MiB;
//
//	try
//	{
//		metric_value& mv = gathers[gather_index].get(std::string("CLPR"), CLPR, std::string("WP MiB"));
//		string_value = mv.string_value();
//		WP_MiB = number_optional_trailing_percent(string_value);
//	}
//	catch (std::out_of_range& oor)  // this is what gathers.get() throws
//	{
//		std::ostringstream o;
//		o << "<Error> Hitachi_RAID_subsystem::get_wp( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
//			<< " - " << oor.what() << std::endl;
//		throw std::invalid_argument(o.str());
//	}
//	catch(std::invalid_argument& iaex)  // this is what number_optional_trailing_percent() throws
//	{
//		std::ostringstream o;
//		o << "<Error> Hitachi_RAID_subsystem::get_wp( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
//			<< " - retrieved \"WP MiB\" metric \"" << string_value << "\" did not parse as a number with optional trailing percent." << std::endl << iaex.what() << std::endl;
//		throw std::invalid_argument(o.str());
//	}
//
//
//	try
//	{
//		metric_value& mv = gathers[gather_index].get(std::string("CLPR"), CLPR, std::string("size MiB"));
//		string_value = mv.string_value();
//		size_MiB = number_optional_trailing_percent(string_value);
//	}
//	catch (std::out_of_range& oor)  // this is what gathers.get() throws
//	{
//
//		std::ostringstream o;
//		o << "<Error> Hitachi_RAID_subsystem::get_wp( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
//			<< " - " << oor.what() << std::endl;
//		throw std::invalid_argument(o.str());
//	}
//	catch(std::invalid_argument& iaex)  // this is what number_optional_trailing_percent() throws
//	{
//		std::ostringstream o;
//		o << "<Error> Hitachi_RAID_subsystem::get_wp( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
//			<< " - retrieved \"size MiB\" metric \"" << string_value << "\" did not parse as a number with optional trailing percent." << std::endl << iaex.what() << std::endl;
//		throw std::invalid_argument(o.str());
//	}
//
//	if (size_MiB <= 0.0)
//	{
//		std::ostringstream o;
//		o << "<Error> Hitachi_RAID_subsystem::get_wp( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
//			<< " - retrieved size_MB metric \"" << string_value << "\" which was zero or negative." << std::endl;
//		throw std::invalid_argument(o.str());
//	}
//
//	return WP_MiB / size_MiB;


    ivy_float WP_percent;

	try
	{
		metric_value& mv = gathers[gather_index].get(std::string("CLPR"), CLPR, std::string("WP %"));
		string_value = mv.string_value();
		WP_percent = number_optional_trailing_percent(string_value);
	}
	catch (std::out_of_range& oor)  // this is what gathers.get() throws
	{

		std::ostringstream o;
		o << "<Error> Hitachi_RAID_subsystem::get_wp( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
			<< " - " << oor.what() << std::endl;
		throw std::invalid_argument(o.str());
	}
	catch(std::invalid_argument& iaex)  // this is what number_optional_trailing_percent() throws
	{
		std::ostringstream o;
		o << "<Error> Hitachi_RAID_subsystem::get_wp( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
			<< " - retrieved \"WP %\" metric \"" << string_value << "\" did not parse as a number with optional trailing percent." << std::endl << iaex.what() << std::endl;
		throw std::invalid_argument(o.str());
	}

	return WP_percent;
}


ivy_float Hitachi_RAID_subsystem::get_sidefile(const std::string& CLPR, int subinterval)
{
	// use subinterval = -1 to get the WP value from the gather at t=0

	if (gathers.size() == 0)
	{
		std::ostringstream o;
		o << "<Error> Hitachi_RAID_subsystem::get_sidefile( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
			<< " - get_sidefile() called when gathers.size()=0"<< std::endl;
		throw std::invalid_argument(o.str());
	}

	int max_subinterval = gathers.size() - 2;  // if there is one gather, the t=0 gather was performed, and we can retrieve that, but there are no subinterval end gathers yet.

	if( (subinterval < -1) || (subinterval > max_subinterval))
	{
		std::ostringstream o;
		o << "<Error> Hitachi_RAID_subsystem::get_sidefile( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
			<< " - invalid subinterval.  There is a gather at t=0 (use subinterval=-1), and there are gathers for " << (gathers.size()-1) << " subintervals.  Valid subinterval numbers for get_wp are -1 through " << (gathers.size()-2) << std::endl;
		throw std::invalid_argument(o.str());
	}

	int gather_index = subinterval+1;
//	GatherData& gd = gathers[gather_index];

	std::string string_value;

    ivy_float sidefile_percent;

	try
	{
		metric_value& mv = gathers[gather_index].get(std::string("CLPR"), CLPR, std::string("Sidefile %"));
		string_value = mv.string_value();
		sidefile_percent = number_optional_trailing_percent(string_value);
	}
	catch (std::out_of_range& oor)  // this is what gathers.get() throws
	{

		std::ostringstream o;
		o << "<Error> Hitachi_RAID_subsystem::get_sidefile( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
			<< " - " << oor.what() << std::endl;
		throw std::invalid_argument(o.str());
	}
	catch(std::invalid_argument& iaex)  // this is what number_optional_trailing_percent() throws
	{
		std::ostringstream o;
		o << "<Error> Hitachi_RAID_subsystem::get_sidefile( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
			<< " - retrieved \"Sidefile %\" metric \"" << string_value << "\" did not parse as a number with optional trailing percent." << std::endl << iaex.what() << std::endl;
		throw std::invalid_argument(o.str());
	}

	return sidefile_percent;
}


ivy_float Hitachi_RAID_subsystem::get_wp_change_from_last_subinterval(const std::string& CLPR, unsigned int subinterval)
{
	// throws std::invalid_argument

	// slew is defined for any valid subinterval index from 0, because before subinterval 0 we did a gather at t=0.

	if ( gathers.size() < (subinterval+2) )
	{
		std::ostringstream o;
		o << "<Error> Hitachi_RAID_subsystem::get_wp_change_from_last_subinterval( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
			<< " - no such subinterval \"" << subinterval << "\"."<< std::endl;
		throw std::invalid_argument(o.str());
	}

	ivy_float WP_then, WP_the_time_before, WP_delta;

	WP_then = get_wp(CLPR,subinterval);
	WP_the_time_before = get_wp(CLPR,subinterval-1);
	WP_delta = WP_then - WP_the_time_before;

//*debug*/ std::cout << std::endl << " Hitachi_RAID_subsystem::get_wp_change_from_last_subinterval(CLPR=\"" << CLPR << "\",subinterval=" << subinterval <<") WP that time " << std::fixed << std::setprecision(3) << (100.*WP_then) << "%. the time before " << std::fixed << std::setprecision(3) << (100.*WP_the_time_before) << "%, returning delta=" << std::fixed << std::setprecision(3) << (100.*WP_delta) << "%" << std::endl << std::endl;

	return WP_delta;
}


ivy_float Hitachi_RAID_subsystem::get_sidefile_change_from_last_subinterval(const std::string& CLPR, unsigned int subinterval)
{
	// throws std::invalid_argument

	// slew is defined for any valid subinterval index from 0, because before subinterval 0 we did a gather at t=0.

	if ( gathers.size() < (subinterval+2) )
	{
		std::ostringstream o;
		o << "<Error> Hitachi_RAID_subsystem::get_sidefile_change_from_last_subinterval( CLPR = \"" << CLPR << "\", subinterval = " << subinterval << " )) for subsystem " << serial_number
			<< " - no such subinterval \"" << subinterval << "\"."<< std::endl;
		throw std::invalid_argument(o.str());
	}

	ivy_float sidefile_then, sidefile_the_time_before, sidefile_delta;

	sidefile_then            = get_sidefile(CLPR,subinterval  );
	sidefile_the_time_before = get_sidefile(CLPR,subinterval-1);
	sidefile_delta = sidefile_then - sidefile_the_time_before;

//*debug*/ std::cout << std::endl << " Hitachi_RAID_subsystem::get_sidefile_change_from_last_subinterval(CLPR=\"" << CLPR << "\",subinterval=" << subinterval <<") WP that time " << std::fixed << std::setprecision(3) << (100.*sidefile_then) << "%. the time before " << std::fixed << std::setprecision(3) << (100.*sidefile_the_time_before) << "%, returning delta=" << std::fixed << std::setprecision(3) << (100.*sidefile_delta) << "%" << std::endl << std::endl;

	return sidefile_delta;
}


std::string Hitachi_RAID_subsystem::pool_vols_by_pool_ID()
{
    std::ostringstream o;

    o << "Hitachi_RAID_subsystem::pool_vols_by_pool_ID()" << std::endl;

    for (auto& pear : pool_vols)
    {
        const std::string& id = pear.first;
        o << "For Pool_ID = \"" << id << "\" -";
        for (auto& p : pear.second)
        {
            const std::string& pv = p->first;
            o << " " << pv;
        }

    }
    o << std::endl;
    return o.str();
}
