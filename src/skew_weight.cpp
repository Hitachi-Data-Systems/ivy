//Copyright (c) 2018 Hitachi Vantara Corporation
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

#include "skew_weight.h"

skew_LUN::skew_LUN()
{
}

void skew_LUN::clear()
{
    workload_skew_weight.clear();
}

ivy_float skew_LUN::total_abs_skew()
{
    ivy_float total {0.0};

    for (auto& w: workload_skew_weight)
    {
        total += abs(w.second);
    }

    return total;
}

ivy_float skew_LUN::normalized_skew_factor(const std::string& wID)
{
    auto iter = workload_skew_weight.find(wID);

    if (workload_skew_weight.end() == iter)
    {
        std::ostringstream o;
        o << "At line " << __LINE__ << " of " << __FILE__ << ", "
        << "in LUN::normalized_skew_factor(const std::string& wID), workload ID \"" << wID << "\" not found.";
        throw std::runtime_error(o.str());
    }

    return abs(iter->second) / total_abs_skew();
}

void skew_LUN::push(const std::string& id, ivy_float weight)
{
    workload_skew_weight[id] = weight;
    // Here so as to not lose any information
    // we preserve the sign of the weight.
}

std::string skew_LUN::toString(std::string indent)
{
    std::ostringstream o;
    o << indent << "skew_LUN = { " << std::endl;

    for (auto& pear : workload_skew_weight)
    {
        o << indent << "   " << pear.first << " : skew_weight = " << pear.second << ", normalized_skew_factor = " << normalized_skew_factor(pear.first) << std::endl;
    }
    o << indent << "total_abs_skew = " << total_abs_skew() <<   " }" << std::endl;

    return o.str();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////



void skew_data::clear()
{
    LUNs.clear();
}


void skew_data::push(const WorkloadID& wID, ivy_float skew_weight)
{
    std::string host_lun = wID.ivyscript_hostname + std::string("+") + wID.LUN_name;

    LUNs[host_lun].push(wID.workloadID,skew_weight);
}

ivy_float skew_data::fraction_of_total_IOPS(const std::string& wID)
{
    WorkloadID w;
    if ( ! w.set(wID) )
    {
        std::ostringstream o;
        o << "At line " << __LINE__ << " of " << __FILE__ << ", "
        << "in skew_data::fraction_of_total_IOPS(const std::string& wID), workload ID \"" << wID << "\" doesn\'t look like a WorkloadID'.";
        throw std::runtime_error(o.str());
    }

    std::string host_lun = w.ivyscript_hostname + std::string("+") + w.LUN_name;

    auto iter = LUNs.find(host_lun);

    if (LUNs.end() == iter)
    {
        std::ostringstream o;
        o << "At line " << __LINE__ << " of " << __FILE__ << ", "
        << "in skew_data::fraction_of_total_IOPS(const std::string& wID), workload ID \"" << wID << "\" not found in \"LUNs\".";
        throw std::runtime_error(o.str());
    }

    if ( 0 == LUNs.size())
    {
        std::ostringstream o;
        o << "At line " << __LINE__ << " of " << __FILE__ << ", "
        << "in skew_data::fraction_of_total_IOPS(const std::string& wID = \"" << wID << "\") LUNs.size() == 0. ";
        throw std::runtime_error(o.str());
    }

    return iter->second.normalized_skew_factor(wID) / ((ivy_float)LUNs.size());
}


std::string skew_data::toString()
{
    std::ostringstream o;

    o << "skew_data = {" << std::endl;

    for (auto& pear : LUNs)
    {
        o << "LUN \"" << pear.first << "\":";
        o << pear.second.toString("   ");
    }

    ivy_float t {0.0};
    for (auto& pear : LUNs)
    {
        for (auto& peach : pear.second.workload_skew_weight)
        {
            ivy_float x = fraction_of_total_IOPS(peach.first);
            t += x;
            o << "fraction_of_total_IOPS(\"" << peach.first << "\") = " << x << std::endl;
        }
    }
    o << "sum over fraction_of_total_IOPS = " << t << std::endl;
    o << "}" << std::endl;

    return o.str();
}






















































