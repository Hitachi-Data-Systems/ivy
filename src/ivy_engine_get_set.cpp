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
    o << "Valid ivy engine get parameters are:"
            << " output_folder_root"
            << ", test_name"
            << ", last_result"
            << ", step_NNNN"
            << ", step_name"
            << ", step_folder"
            << ", and achieved_IOPS_tolerance"
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


    {
        std::ostringstream o;
        o << "<Error> Unknown ivy engine set parameter \"" << thingee << "\"." << std::endl << std::endl;
        o << "The valid set() parameter is \"achieved_IOPS_tolerance\"." << std::endl << std::endl;
        return std::make_pair(false,o.str());
    }
}


