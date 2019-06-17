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
#include <string>
using namespace std::string_literals;

#include "ivy_engine.h"

#include "Test_config_thumbnail.h"

void Test_config_thumbnail::clear()
{
    drives_by_PG.clear();
    LDEVs_by_host.clear();
    LDEVs_by_subsystem_port.clear();
    error_messages.clear();
}

std::ostream& operator<<(std::ostream& o, const Test_config_thumbnail& tct)
{
    if (tct.LDEVs_by_host.size() > 0)
    {
        o << std::endl << "LDEVs by test host:" << std::endl;
        for (auto& pear : tct.LDEVs_by_host)
        {
            for (auto& peach : pear.second)
            {
                {
                    const std::string& host = pear.first;
                    const std::string& subsystem = peach.first;
                    const LDEVset& lset = peach.second;

                    o << host << ", " << subsystem << ", LDEVs = " << lset.toString() << " (" << lset.size() << ")"<< std::endl;
                }
            }
        }
    }

    if (tct.LDEVs_by_subsystem_port.size() > 0)
    {
        o << std::endl << "LDEVs by subsystem port:" << std::endl;
        for (auto& pear : tct.LDEVs_by_subsystem_port)
        {
            for (auto& peach : pear.second)
            {
                {
                    const std::string& subsystem = pear.first;
                    const std::string& port = peach.first;
                    const LDEVset& lset = peach.second;

                    o << subsystem << ", port = " << port << ", LDEVs = " << lset.toString() << " (" << lset.size() << ")"<< std::endl;
                }
            }
        }
    }

    if (tct.drives_by_PG.size() > 0)
    {
        o << std::endl << "Drive counts:" << std::endl;
        for (auto& pear /* "HM800 - 410034" */: tct.drives_by_PG)
        {
            unsigned int subsystem_drives = 0;

            for (auto& peach /* "DKR5D-J600SS" */: pear.second)
            {
                unsigned int drive_type_drives = 0;

                for (auto& banana /* "RAID-5 7+1" */: peach.second)
                {
                    unsigned int raid_level_drives = 0;

                    for (auto& kumquat /* "1-1" */: banana.second)
                    {
                        drive_type_drives+= kumquat.second;
                        subsystem_drives += kumquat.second;
                        raid_level_drives += kumquat.second;
                    }

                    o << pear.first /* "HM800 - 410034" */
                        << ", " << peach.first /* "DKR5D-J600SS" */
                            << ", " << banana.first /* "RAID-5 7+1" */
                        << ", " << raid_level_drives << " drives in "
                        << banana.second.size() << " PG"; if (banana.second.size() > 1) o << "s" << ".";

                    if (banana.second.size() > 0)
                    {
                        o << "   ";
                        bool need_comma = false;
                        for (auto& x : banana.second)
                        {
                            if (need_comma) o << ", ";
                            need_comma = true;
                            o << x.first;
                        }
                        o << std::endl;
                    }
                }

                o << pear.first << ", " << peach.first << " total " << drive_type_drives << " drives." << std::endl;
            }

            o << pear.first << " total " << subsystem_drives << " drives." << std::endl << std::endl;
        }
    }

    o << tct.error_messages;

    return o;
}

unsigned int Test_config_thumbnail::total_drives() const
{
    unsigned int drives = 0;

    for (auto& pear /* HM800 - 410116 */: drives_by_PG)
    {
        for (auto& peach /* "DKR5D-J600SS" */: pear.second)
        {
            for (auto& banana /*"RAID-5 7+1"*/ : peach.second)
            {
                for(auto& kumquat /* 1-1 */ : banana.second)
                {
                    drives += kumquat.second;
                }
            }

        }
    }

    return drives;
}


void Test_config_thumbnail::add(LUN* pLUN)
{
    std::string pg;

    std::string host            = pLUN->attribute_value("ivyscript hostname"); if (host.size() == 0)            host            = "null_ivyscript_host";
    std::string hitachi_product = pLUN->attribute_value("hitachi product");    if (hitachi_product.size() == 0) hitachi_product = "null_hitachi_product";
    std::string serial          = pLUN->attribute_value("serial number");      if (serial.size() == 0)          serial          = "null_serial_number";
    std::string port    = toUpper(pLUN->attribute_value("port"));              if (port.size() == 0)            port            = "null_port";
    std::string LDEV    = toUpper(pLUN->attribute_value("LDEV"));
    std::string pg_names        = pLUN->attribute_value("PG Names");


    if (LDEV.size() > 0)
    {
        std::string s = hitachi_product + std::string(" = ") + serial;

        LDEVs_by_subsystem_port
            [s][port]
                .add(LDEV,m_s.masterlogfile);

        LDEVs_by_host
            [host][s]
                .add(LDEV,m_s.masterlogfile);

        unsigned int pg_names_cursor = 0;

        while (true)
        {
            auto rc = get_next_field(pg_names,pg_names_cursor,'/');

            if (!rc.first) break;

            std::string pg = rc.second;

            auto s_it = m_s.subsystems.find(serial);
            if (s_it != m_s.subsystems.end() && s_it->second->type() == "Hitachi RAID"s)
            {
                Hitachi_RAID_subsystem* pHRs = (Hitachi_RAID_subsystem*) s_it -> second;
                auto cg_it = pHRs->configGatherData.data.find("PG"s);
                if (cg_it == pHRs->configGatherData.data.end())
                {
                    std::ostringstream o;
                    o << "<Warning> Test_config_thumbnail::add() - did not find config element type \"PG\" for subsystem " << serial << "." << std::endl;
                    error_messages += o.str();
                    log(m_s.masterlogfile,o.str());
                    continue;
                }

                auto pg_name_it = cg_it->second.find(pg);
                if (pg_name_it == cg_it->second.end())
                {
                    std::ostringstream o;
                    o << "<Warning> Test_config_thumbnail::add() - did not find parity group \"" << pg
                        << "\" in config element type \"PG\" for subsystem " << serial << "." << std::endl;
                    error_messages += o.str();
                    log(m_s.masterlogfile,o.str());
                    continue;
                }

                auto concat_set_it = pg_name_it->second.find("Concat Set");
                if (concat_set_it == pg_name_it->second.end())
                {
                    std::ostringstream o;
                    o << "<Warning> Test_config_thumbnail::add() - did not find the \"Concat Set\" attribute for parity group \""
                        << pg << "\" in config element type \"PG\" for subsystem " << serial << "." << std::endl;
                    error_messages += o.str();
                    log(m_s.masterlogfile,o.str());
                    continue;
                }

                std::string concat_set = concat_set_it->second.value;

                // The reason we iterate over the concatenation PG set is that we may only
                // encounter an LDEV in one PG in the set, yet all the drives in the set are in use.

                unsigned int concat_set_cursor = 0;

                while (true)
                {
                    auto concat_rc = get_next_field(concat_set,concat_set_cursor,'/');

                    if (!concat_rc.first) break;

                    std::string c_pg = concat_rc.second;

                    auto cpg_it = cg_it->second.find(c_pg);
                    if (cpg_it == cg_it->second.end())
                    {
                        std::ostringstream o;
                        o << "<WarningTest_config_thumbnail::add() - did not find concatenation set parity group \""
                            << c_pg << "\" in config element type \"PG\" for subsystem " << serial << ".";
                        error_messages += o.str();
                        log(m_s.masterlogfile,o.str());
                        break;
                    }

                    auto drive_type_it = cpg_it->second.find("Drive Type");
                    if (drive_type_it == cpg_it->second.end())
                    {
                        std::ostringstream o;
                        o << "<Warning> Test_config_thumbnail::add() - did not find the \"Drive Type\" attribute for parity group \""
                            << c_pg << "\" in config element type \"PG\" for subsystem " << serial << "." << std::endl;
                        error_messages += o.str();
                        log(m_s.masterlogfile,o.str());
                        break;
                    }
                    std::string drive_type = drive_type_it->second.value;

                    auto raid_level_it = cpg_it->second.find("RAID Level");
                    if (raid_level_it == cpg_it->second.end())
                    {
                        std::ostringstream o;
                        o << "<Warning> Test_config_thumbnail::add() - did not find the \"RAID Level\" attribute for parity group \""
                            << c_pg << "\" in config element type \"PG\" for subsystem " << serial << "." << std::endl;
                        error_messages += o.str();
                        log(m_s.masterlogfile,o.str());
                        break;
                    }
                    std::string raid_level = raid_level_it->second.value;
                    raid_levels_seen.insert(raid_level);

                    auto size_it = cpg_it->second.find("Size");
                    if (size_it == cpg_it->second.end())
                    {
                        std::ostringstream o;
                        o << "<Warning> Test_config_thumbnail::add() - did not find the \"Size\" attribute for parity group \""
                            << c_pg << "\" in config element type \"PG\" for subsystem " << serial << "." << std::endl;
                        error_messages += o.str();
                        log(m_s.masterlogfile,o.str());
                        break;
                    }
                    std::string size_string = size_it->second.value;

                    if (!std::regex_match(size_string, unsigned_int_regex))
                    {
                        std::ostringstream o;
                        o << "<Warning> Test_config_thumbnail::add() - does not look like an unsigned integer - \"Size\" attribute value \""
                            << size_string << "\" for parity group \""
                            << c_pg << "\" in config element type \"PG\" for subsystem " << serial << "." << std::endl;
                        error_messages += o.str();
                        log(m_s.masterlogfile,o.str());
                        break;
                    }
                    unsigned int pg_size;
                    {
                        std::istringstream is(size_string);
                        is >> pg_size;
                    }

                    drives_by_PG[hitachi_product + " - "s + serial][drive_type][raid_level][c_pg] = pg_size;
                }
            }
        }
    }
 }

std::string Test_config_thumbnail::drive_type_column() const
{
    std::ostringstream o;

    bool need_plus {false};
    for (auto& pear /* "HM800 - 410034" */: drives_by_PG)
    {
        for (auto& peach /* "DKR5D-J600SS" */: pear.second)
        {
            if (need_plus) o << "+";
            need_plus=true;
            if (drives_by_PG.size()>1) o << pear.first << ":";
            o << peach.first;
        }
    }
    return o.str();
}


std::string Test_config_thumbnail::drive_quantity_column() const
{
    std::ostringstream o;

    unsigned int subsystem_drives = 0;
    unsigned int drive_type_drives = 0;

    bool need_plus {false};
    bool saw_second {false};

    for (auto& pear /* "HM800 - 410034" */: drives_by_PG)
    {
        for (auto& peach /* "DKR5D-J600SS" */: pear.second)
        {
            drive_type_drives = 0;

            for (auto& banana /* "RAID-5 7+1" */: peach.second)
            {
                for (auto& pg_pair /* "1-1" */: banana.second)
                {
                    unsigned int pg_size = pg_pair.second;

                    drive_type_drives+= pg_size;
                    subsystem_drives += pg_size;
                }
            }

            if (need_plus)
            {
                o << "+";
                saw_second = true;
            }
            need_plus=true;

            if (drives_by_PG.size()>1) o << pear.first << ":";

            o << drive_type_drives;
        }
    }

    if (saw_second)
    {
        o << "=" << subsystem_drives;
    }

    return o.str();
}


std::string Test_config_thumbnail::raid_level_column() const
{
    std::ostringstream o;

    bool need_slash {false};
    for (auto& x : raid_levels_seen)
    {
        if (need_slash) o << "/";
        need_slash=true;
        o << x;
    }
    return o.str();
}


/*static*/ std::string Test_config_thumbnail::csv_headers()
{
    return ",drive type,drive quantity,RAID level";
}


std::string Test_config_thumbnail::csv_columns() const
{
    std::ostringstream o;
    o
        << "," << drive_type_column()
        << "," << drive_quantity_column()
        << "," << raid_level_column();
    return o.str();
}
