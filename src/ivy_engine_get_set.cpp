//Copyright (c) 2016, 2017, 2018, 2019 Hitachi Vantara Corporation
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

#include "ivy_engine.h"


std::string ivy_engine_get(std::string thing)   //
{
    auto r = m_s.get(thing);
    if (!r.first)
    {
        std::ostringstream o;
        o << "For ivyscript builtin function ivy_engine_get(thing=\"" << thing << "\") "
            << "the corresponding ivy engine API call failed saying:" << std::endl
            << r.second;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
    }
    return r.second;
}

int ivy_engine_set(std::string thing, std::string value)
{
    auto r = m_s.set(thing,value);
    if (!r.first)
    {
        std::ostringstream o;
        o << "For ivyscript builtin function ivy_engine_set(thing=\"" << thing << "\", value=\"" << value << "\") "
            << "the corresponding ivy engine API call failed saying:" << std::endl
            << r.second;
        log(m_s.masterlogfile,o.str());
        std::cout << o.str();
        return 0;
    }
    return 1;
}


std::string valid_get_parameters()
{
    std::ostringstream o;
    o << "Valid ivy engine get parameters:"
            << " output_folder_root"
            << ", test_name"
            << ", last_result"
            << ", step_NNNN"
            << ", step_name"
            << ", step_folder"
            << ", achieved_IOPS_tolerance"
            << ", track_long_running_IOs"
            << ", generate_at_a_time"
                << std::endl
            << "(Parameter names are not case sensitive and underscores are ignored in parameter names, and thus OutputFolderRoot is equivalent to output_folder_root.)"
                << std::endl << std::endl;
    return o.str();
}

std::pair<bool, std::string>  // <true,value> or <false,error message>
ivy_engine::get(const std::string& thingee)
{
    std::string t {};

    try
    {
        t = normalize_identifier(thingee);
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> ivy engine get(thing=\"" << thingee << "\") - thing must be an identifier, that is, starts with an alphabetic character and continues with alphabetics, numerics, and underscores."
            << std::endl << std::endl
            << valid_get_parameters();
        return std::make_pair(false,o.str());
    }

    if (0 == t.compare(normalize_identifier("version")))
    {
        return std::make_pair(true,ivy_version);
    }


    if (0 == t.compare(normalize_identifier("summary_csv")))
    {
        return std::make_pair(true,testFolder + "/all/"s + testName + ".all=all.summary.csv"s);
    }


    if (0 == t.compare(normalize_identifier("output_folder_root")))
    {
        return std::make_pair(true,outputFolderRoot);
    }


    if (0 == t.compare(normalize_identifier("test_name")))
    {
        return std::make_pair(true,testName);
    }


    if (0 == t.compare(normalize_identifier("last_result")))
    {
        return std::make_pair(true,last_result());
    }


    if (0 == t.compare(normalize_identifier("step_NNNN")))
    {
        return std::make_pair(true,stepNNNN);
    }


    if (0 == t.compare(normalize_identifier("step_name")))
    {
        return std::make_pair(true,stepName);
    }


    if (0 == t.compare(normalize_identifier("step_folder")))
    {
        return std::make_pair(true,stepFolder);
    }


    if (0 == t.compare(normalize_identifier("test_folder")))
    {
        return std::make_pair(true,testFolder);
    }



    if (0 == t.compare(normalize_identifier("masterlogfile")))
    {
        return std::make_pair(true,masterlogfile.logfilename);
    }


    if (0 == t.compare(normalize_identifier("rollup_structure")))
    {
        return std::make_pair(true,show_rollup_structure());
    }


    if (0 == t.compare(normalize_identifier("achieved_IOPS_tolerance")))
    {
        std::ostringstream o;
        o << (100.0 * achieved_IOPS_tolerance) << "%";

        return std::make_pair(true,o.str());
    }

    if (0 == t.compare(normalize_identifier("max_active_core_busy")))
    {
        std::ostringstream o;
        o << std::fixed << std::setprecision(2) << (100.0 * max_active_core_busy) << "%";

        return std::make_pair(true,o.str());
    }

    {
        std::ostringstream o;
        o << "<Error> Unknown ivy engine get parameter \"" << thingee << "\"." << std::endl << std::endl
            << valid_get_parameters();
        return std::make_pair(false,o.str());
    }
}

    std::pair<bool, std::string>  // <true,possibly in future some additional info if needed> or <false,error message>
ivy_engine::set(const std::string& thingee,
    const std::string& value)
{
    std::cout << std::endl << "ivy_engine_set(\"" << thingee << "\", \"" << value << "\");" << std::endl << std::endl;

    std::string t {};

    try
    {
        t = normalize_identifier(thingee);
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> ivy engine set(thing=\"" << thingee << "\", value = \"" << value
            << "\") - thing must be an identifier, that is, starts with an alphabetic character and continues with alphabetics, numerics, and underscores."
            << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }


    if (0 == t.compare(normalize_identifier("achieved_IOPS_tolerance")))
    {
        ivy_float v;

        try
        {
            v = number_optional_trailing_percent(value,"achieved_IOPS_tolerance");
        }
        catch (const std::invalid_argument& ia)
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"achieved_IOPS_tolerance\", \"" << value << "\") - achieved_IOPS_tolerance value must be a positive number with optional trailing %."
                << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }

        if (v <= 0.0)
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"achieved_IOPS_tolerance\", \"" << value << "\") - achieved_IOPS_tolerance value must be a positive number with optional trailing %."
                << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }

        achieved_IOPS_tolerance = v;
        return std::make_pair(true,"");
    }

    if (0 == t.compare(normalize_identifier("critical_temp")))
    {
        if (stringCaseInsensitiveEquality(value,"warn") || stringCaseInsensitiveEquality(value,"warning"))
        {
            warn_on_critical_temp=true;
            return std::make_pair(true,"");
        }
        else if (stringCaseInsensitiveEquality(value,"error"))
        {
            warn_on_critical_temp=false;
            return std::make_pair(true,"");
        }
        else
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"critical_temp\", \"" << value << "\") - may only be set to \"warn\" or \"error\"."
                << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }
    }

    if (0 == t.compare(normalize_identifier("fruitless_passes_before_wait"))) { return set_ivydriver_unsigned_parameter_value          ("fruitless_passes_before_wait"s, value); }
    if (0 == t.compare(normalize_identifier("sqes_per_submit_limit"s)))       { return set_ivydriver_positive_unsigned_parameter_value ("sqes_per_submit_limit"s       , value); }
    if (0 == t.compare(normalize_identifier("generate_at_a_time"s)))          { return set_ivydriver_positive_unsigned_parameter_value ("generate_at_a_time"s          , value); }
    if (0 == t.compare(normalize_identifier("track_long_running_IOs")))       { return set_ivydriver_boolean_parameter_value           ("track_long_running_IOs"s      , value); }
    if (0 == t.compare(normalize_identifier("spinloop")))                     { return set_ivydriver_boolean_parameter_value           ("spinloop"s                    , value); }
    if (0 == t.compare(normalize_identifier("max_wait_seconds")))             { return set_ivydriver_positive_ivy_float_parameter_value("max_wait_seconds"s            , value); }

    if (0 == t.compare(normalize_identifier("log")))
    {
        try
        {
            routine_logging = parse_boolean(value);
        }
        catch (std::exception& e)
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"log\", \"" << value << "\") - may only be set to true/false, yes/no, or on/off."
                << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }

        try
        {
            issue_set_command_to_ivydriver("log",value);
        }
        catch (std::exception& e)
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"log\", \"" << value << "\") - failed sending set command to ivydriver(s) - " << e.what() << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }
        return std::make_pair(true,"");
    }


    if (0 == t.compare(normalize_identifier("provisional_csv_lines")))
    {
        try
        {
            provisional_csv_lines = parse_boolean(value);
        }
        catch (const std::invalid_argument& ia)
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"provisional_csv_lines\", \"" << value << "\") - provisional_csv_lines may only be set to \"on\"/\"off\", or \"true\"/\"false\"."
                << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }

        std::cout << "ivy_engine_set(\"provisional_csv_lines\", \"" << value << "\") was successful." << std::endl;

        return std::make_pair(true,"");
    }

    if (0 == t.compare(normalize_identifier("max_active_core_busy")))
    {
        ivy_float v;

        try
        {
            v = number_optional_trailing_percent(value,"max_active_core_busy");
        }
        catch (const std::invalid_argument& ia)
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"max_active_core_busy\", \"" << value << "\") - max_active_core_busy value must be a positive number with optional trailing %.  The value must be less than or equal to 1.0 or 100%."
                << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }

        if (v <= 0.0 || v > 1.0)
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"max_active_core_busy\", \"" << value << "\") - max_active_core_busy value must be a positive number with optional trailing %.  The value must be less than or equal to 1.0 or 100%."
                << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }

        max_active_core_busy = v;

        return std::make_pair(true,"");
    }

    if (0 == t.compare(normalize_identifier("log")))
    {
        try
        {
            routine_logging = parse_boolean(value);
        }
        catch (const std::invalid_argument& ia)
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"log\", \"" << value << "\") - must be yes/no true/false on/off."
                << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }

        try
        {
            issue_set_command_to_ivydriver("log",value);
        }
        catch (std::exception& e)
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"log\", \"" << value << "\") - failed sending set command to ivydriver(s) - " << e.what() << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }

        return std::make_pair(true,"");
    }


    {
        std::ostringstream o;
        o << "<Error> Unknown ivy engine set parameter \"" << thingee << "\"." << std::endl << std::endl;
        o << "Valid set() parameters are \"achieved_IOPS_tolerance\", \"critical_temp\", \"max_active_core_busy\", and \"provisional_csv_lines\"." << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }
}


void ivy_engine::issue_set_command_to_ivydriver(const std::string& attribute, const std::string& value)
{
    std::ostringstream set_command;

    set_command << "set \"" << attribute << "\" to \"" << value << "\"";

    for (auto& pear: host_subthread_pointers)
    {
        pipe_driver_subthread* p_host = pear.second;
		// get lock on talking to pipe_driver_subthread for this remote host
		{
			std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);
			// tell slave driver thread to talk to the other end and make the thread

			p_host->commandString=set_command.str();

			p_host->command=true;
			p_host->commandSuccess=false;
			p_host->commandComplete=false;
			p_host->commandErrorMessage.clear();
		}
		p_host->master_slave_cv.notify_all();
    }

    for (auto& pear: host_subthread_pointers)
    {
        pipe_driver_subthread* p_host = pear.second;
		{
			std::unique_lock<std::mutex> u_lk(p_host->master_slave_lk);
			while (!p_host->commandComplete) p_host->master_slave_cv.wait(u_lk);

			if (!p_host->commandSuccess)
			{
				ostringstream o;
				o << "Failed sending <" << set_command.str() << "> to \"" << p_host->ivyscript_hostname << " - " << p_host->commandErrorMessage;
                throw std::runtime_error(o.str());
			}
		}
    }
}

std::pair<bool,std::string>
ivy_engine::set_ivydriver_positive_unsigned_parameter_value(const std::string& parameter_name, const std::string& value)
{
    string s = value;
    trim(s);

    if (!isalldigits(s))
    {
        std::ostringstream o;
        o << "<Error> ivy engine set(\"" << parameter_name << "\", \"" << value << "\") - value must be a positive number expressed as all decimal digits 0-9."
            << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }

    {
        unsigned long ul;
        std::istringstream is {s};
        is >> ul;
        if (is.fail() || ul == 0 || !is.eof())
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"" << parameter_name << "\", \"" << value << "\") - value must be a positive number expressed as all decimal digits 0-9."
                << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }
    }

    try
    {
        issue_set_command_to_ivydriver(parameter_name,s);
    }
    catch (std::exception& e)
    {
        std::ostringstream o;
        o << "<Error> ivy engine set(\"" << parameter_name << "\", \"" << value << "\") - failed sending set command to ivydriver(s) - " << e.what() << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }

    return std::make_pair(true,"");
}

std::pair<bool,std::string>
ivy_engine::set_ivydriver_unsigned_parameter_value(const std::string& parameter_name, const std::string& value)
{
    string s = value;
    trim(s);

    if (!isalldigits(s))
    {
        std::ostringstream o;
        o << "<Error> ivy engine set(\"" << parameter_name << "\", \"" << value << "\") - value must be a number expressed as all decimal digits 0-9."
            << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }

    {
        unsigned long ul;
        std::istringstream is {s};
        is >> ul;
        if (is.fail() || !is.eof())
        {
            std::ostringstream o;
            o << "<Error> ivy engine set(\"" << parameter_name << "\", \"" << value << "\") - value must be a number expressed as all decimal digits 0-9."
                << std::endl << std::endl;
            return std::make_pair(false,o.str());
        }
    }

    try
    {
        issue_set_command_to_ivydriver(parameter_name,s);
    }
    catch (std::exception& e)
    {
        std::ostringstream o;
        o << "<Error> ivy engine set(\"" << parameter_name << "\", \"" << value << "\") - failed sending set command to ivydriver(s) - " << e.what() << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }

    return std::make_pair(true,"");
}


std::pair<bool,std::string>
ivy_engine::set_ivydriver_boolean_parameter_value(const std::string& parameter_name, const std::string& value)
{
    try
    {
        working_bit = parse_boolean(value);
    }
    catch (std::exception& e)
    {
        std::ostringstream o;
        o << "<Error> ivy engine set(\"" << parameter_name << "\", \"" << value << "\") - may only be set to true/false, yes/no, or on/off."
            << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }

    try
    {
        issue_set_command_to_ivydriver(parameter_name,value);
    }
    catch (std::exception& e)
    {
        std::ostringstream o;
        o << "<Error> ivy engine set(\"" << parameter_name << "\", \"" << value << "\") - failed sending set command to ivydriver(s) - " << e.what() << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }

    return std::make_pair(true,"");
}


std::pair<bool,std::string>
ivy_engine::set_ivydriver_positive_ivy_float_parameter_value(const std::string& parameter_name, const std::string& value)
{
    ivy_float v;

    try
    {
        v = number_optional_trailing_percent(value,parameter_name);
    }
    catch (const std::invalid_argument& ia)
    {
        std::ostringstream o;
        o << "<Error> ivy engine set(\"" << parameter_name << "\", \"" << value << "\") - value must be a positive number with optional trailing %."
            << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }

    if (v <= 0.0)
    {
        std::ostringstream o;
        o << "<Error> ivy engine set(\"" << parameter_name << "\", \"" << value << "\") - value must be a positive number with optional trailing %."
            << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }

    try
    {
        issue_set_command_to_ivydriver(parameter_name,value);
    }
    catch (std::exception& e)
    {
        std::ostringstream o;
        o << "<Error> ivy engine set(\"" << parameter_name << "\", \"" << value << "\") - failed sending set command to ivydriver(s) - " << e.what() << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }

    return std::make_pair(true,"");
}







