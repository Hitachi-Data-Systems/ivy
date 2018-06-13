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

#include "ivy_engine.h"

#include "Test_config_thumbnail.h"

void Test_config_thumbnail::clear()
{
    dp_vols_by_pool_ID.clear();
    pgs_by_subsystem__drive_type__PG_layout.clear();
    available_LDEVs.clear();
    available_Pool_Vols.clear();
    LDEVs_by_host.clear();
    LDEVs_by_subsystem_port.clear();
    seen_raid_level.clear();
    seen_pg_layout.clear();
}

std::ostream& operator<<(std::ostream& o, const Test_config_thumbnail& tct)
{
    o << std::endl << "Host-mapped LDEVs by subsystem and drive type or pool ID:" << std::endl;
    for (auto& pear : tct.available_LDEVs)
    {
        for (auto& peach : pear.second)
        {
            const std::string& subsystem = pear.first;
            const std::string& drivetype = peach.first;
            const LDEVset& lset = peach.second;

            o << subsystem << ", " << drivetype << ", LDEVs = " << lset.toString() << " (" << lset.size() << ")"<< std::endl;
        }
    }


    if (tct.available_Pool_Vols.size() > 0 )
    {
        o << std::endl << "Pool volume LDEVs by subsystem, pool ID, and drive type:" << std::endl;

        for (auto& pear : tct.available_Pool_Vols)
        {
            for (auto& peach : pear.second)
            {
                for (auto& banana : peach.second)
                {
                    const std::string& subsystem = pear.first;
                    const std::string& pool = peach.first;
                    const std::string& drivetype = banana.first;
                    const LDEVset& lset = banana.second;

                    o << subsystem << " Pool ID = " << pool << ", Drive_type = " << drivetype << ", LDEVs = " << lset.toString() << " (" << lset.size() << ")"<< std::endl;
                }
            }
        }
        o << std::endl;
    }

    o << std::endl << "LDEVs by test host:" << std::endl;
    for (auto& pear : tct.LDEVs_by_host)
    {
        for (auto& peach : pear.second)
        {
            const std::string& host = pear.first;
            const std::string& subsystem = peach.first;
            const LDEVset& lset = peach.second;

            o << host << ", " << subsystem << ", LDEVs = " << lset.toString() << " (" << lset.size() << ")"<< std::endl;
        }
    }

    o << std::endl << "LDEVs by subsystem port:" << std::endl;
    for (auto& pear : tct.LDEVs_by_subsystem_port)
    {
        for (auto& peach : pear.second)
        {
            const std::string& subsystem = pear.first;
            const std::string& port = peach.first;
            const LDEVset& lset = peach.second;

            o << subsystem << ", port = " << port << ", LDEVs = " << lset.toString() << " (" << lset.size() << ")"<< std::endl;
        }
    }

    if (tct.pgs_by_subsystem__drive_type__PG_layout.size() == 0)
    {
        o << std::endl << "<No drive counts since no RAID_subsystem command devices are available.>" << std::endl << std::endl;
    }
    else
    {
        o << std::endl << "Drive counts:" << std::endl;
        for (auto& pear : tct.pgs_by_subsystem__drive_type__PG_layout)
        {
            unsigned int subsystem_drives = 0;
            const std::string& subsystem_key = pear.first; // "HM800 - 410034"
            for (auto& peach : pear.second)
            {
                const std::string& drive_type = peach.first;

                unsigned int drive_type_drives = 0;

                for (auto& banana : peach.second)
                {
                    const std::string& raid_layout = banana.first;
                    const PG_set& pg_set           = banana.second;
                    unsigned int banana_drives = pg_set.pg_size * pg_set.pgs.size();
                    drive_type_drives+= banana_drives;
                    subsystem_drives += banana_drives;

                    o << subsystem_key << ", " << drive_type << ", " << raid_layout << ", " << banana_drives << " drives in " << pg_set.pgs.size() << " PG"; if (pg_set.pgs.size() != 1) o << "s" << ".";

                    if (pg_set.pgs.size()>0)
                    {
                        o << "   ";
                        bool need_comma = false;
                        for (auto& x : pg_set.pgs)
                        {
                            if (need_comma) o << ", ";
                            need_comma = true;
                            o << x;
                        }
                        o << std::endl;
                    }
                }

                o << subsystem_key << ", " << drive_type << " total " << drive_type_drives << " drives." << std::endl;

            }
            o << subsystem_key << " total " << subsystem_drives << " drives." << std::endl << std::endl;
        }
    }

    return o;

}

unsigned int Test_config_thumbnail::total_drives() const
{

    unsigned int subsystem_drives = 0;

    for (auto& pear : pgs_by_subsystem__drive_type__PG_layout)
    {
//        const std::string& subsystem_key = pear.first; // "HM800 - 410034"
        for (auto& peach : pear.second)
        {
//            const std::string& drive_type = peach.first;

//            unsigned int drive_type_drives = 0;

            for (auto& banana : peach.second)
            {
//                const std::string& raid_layout = banana.first;
                const PG_set& pg_set           = banana.second;
                unsigned int banana_drives = pg_set.pg_size * pg_set.pgs.size();
//                drive_type_drives+= banana_drives;
                subsystem_drives += banana_drives;
            }

        }
    }
    return subsystem_drives;
}


void Test_config_thumbnail::add(LUN* pLUN)
{
    std::string pg;

    std::string host            = pLUN->attribute_value("ivyscript_hostname"); if (host.size() == 0)            host            = "null_ivyscript_host";
    std::string hitachi_product = pLUN->attribute_value("hitachi_product");    if (hitachi_product.size() == 0) hitachi_product = "null_hitachi_product";
    std::string serial          = pLUN->attribute_value("serial_number");      if (serial.size() == 0)          serial          = "null_serial_number";
    std::string port    = toUpper(pLUN->attribute_value("port"));              if (port.size() == 0)            port            = "null_port";
    std::string LDEV    = toUpper(pLUN->attribute_value("LDEV"));
    std::string drive_type      = pLUN->attribute_value("drive_type");
    std::string raid_level      = pLUN->attribute_value("RAID_level");
    std::string pg_size         = pLUN->attribute_value("pg_size");
    std::string pg_layout       = pLUN->attribute_value("pg_layout");
    std::string pool_id         = pLUN->attribute_value("pool_id");
    std::string pg_concat       = pLUN->attribute_value("PG_concat");  // 0 means not an internal lun.

    if (LDEV.size() > 0)
    {
        std::string s = hitachi_product + std::string(" = ") + serial;

        LDEVs_by_subsystem_port
            [s][port]
                .add(LDEV,m_s.masterlogfile);

        LDEVs_by_host
            [host][s]
                .add(LDEV,m_s.masterlogfile);

        std::string drivetype_or_poolid {""};

        if (pLUN->contains_attribute_name("drive_type"))
        {
            drivetype_or_poolid = pLUN->attribute_value("drive_type");
        }
        else if (pLUN->contains_attribute_name("DP_Vol"))
        {
            drivetype_or_poolid = std::string("Pool ID = ") + pLUN->attribute_value("Pool_ID");
        }

        available_LDEVs
            [s][drivetype_or_poolid]
                .add(LDEV,m_s.masterlogfile);

        std::string pool {""};
        if (pLUN->contains_attribute_name("drive_type"))
        {
            unsigned int pgs {0};
            {
                std::ostringstream o;
                o << "Test_config_thumbnail::add() for internal LDEV " << LDEV << " Drive_type " << drive_type << " parsing PG_concat \"" << pg_concat << "\" as unsigned integer." << std::endl;
                pgs = unsigned_int(pg_concat, o.str());
            }

            if (drive_type.size() > 0 && pgs !=1 && pgs !=2 && pgs != 4)
            {
                std::ostringstream o;
                o << "Test_config_thumbnail::add() for internal LDEV " << LDEV << " Drive_type " << drive_type << " parsing PG_concat \"" << pg_concat << "\" as unsigned integer." << std::endl;
                o << "PG_concat is the number of concatenated PGs, which must be 1, 2, or 4." << std::endl;
                log (m_s.masterlogfile,o.str());
                throw std::runtime_error(o.str());
            }

            for (unsigned int c = 0; c < pgs; c++)
            {
                std::ostringstream attribute;
                attribute << "PG_concat_" << (c+1);
                std::string internal_LDEV_PG_name    = pLUN->attribute_value(attribute.str()); // PG_concat_1, etc.

                PG_set& pg_set = pgs_by_subsystem__drive_type__PG_layout[s][drive_type][raid_level + std::string(" - ") + pg_layout];

                {
                    std::ostringstream o;
                    o << "Test_config_thumbnail::add() for LDEV " << LDEV << " drive_type " << drive_type << " pg_size = \"" << pg_size << "\"." << std::endl;
                    pg_set.pg_size = unsigned_int(pg_size,o.str());
                }
                pg_set.pgs.insert(internal_LDEV_PG_name);
            }
            seen_raid_level.insert(raid_level);
            seen_pg_layout.insert(pg_layout);
        }

        if (pLUN->contains_attribute_name("DP_Vol"))
        {
            dp_vols_by_pool_ID[pool_id].add(LDEV, m_s.masterlogfile);

            auto s_it = m_s.subsystems.find(serial);
            if (s_it != m_s.subsystems.end())
            {
                // safe to cast as the serial number is for a RAID subsystem with DP_vols
                Hitachi_RAID_subsystem* pHRs = (Hitachi_RAID_subsystem*) s_it -> second;

                //{std::ostringstream o; o << pHRs->pool_vols_by_pool_ID(); std::cout << o.str(); log(m_s.masterlogfile, o.str());}

                for (auto& pool_LDEV_pair_pointer : pHRs->pool_vols[pool_id])
                {
                    const std::string& pool_LDEV_name = pool_LDEV_pair_pointer->first;
                    std::map
                    <
                        std::string, // Metric, e.g. "total I/O count"
                        metric_value
                    >&
                    metrics = pool_LDEV_pair_pointer->second;

                    std::string dt = metrics["Drive_type"].string_value();

                    LDEVset& pool_lset = available_Pool_Vols[s][pool_id][dt];
                    pool_lset.add(pool_LDEV_name,m_s.masterlogfile);

                    // Drive counts

                    unsigned int pgs {0};
                    {
                        std::ostringstream o;
                        o << "Test_config_thumbnail::add() for pool ID " << pool_id << " pool LDEV " <<  pool_LDEV_name << " Drive_type " << dt << " parsing pool vol's PG_concat \"" << metrics["PG_concat"].string_value() << "\" as unsigned integer." << std::endl;
                        pgs = unsigned_int(metrics["PG_concat"].string_value(), o.str());
                    }

                    if (dt.size() > 0 && (pgs !=1 && pgs !=2 && pgs != 4))
                    {
                        std::ostringstream o;
                        o << "Test_config_thumbnail::add() for pool ID " << pool_id << " pool LDEV " <<  pool_LDEV_name << " Drive_type " << dt << " parsing pool vol's PG_concat \"" << metrics["PG_concat"].string_value() << "\" as unsigned integer." << std::endl;
                        o << "pg_concat is the number of concatenated PGs, which must be 1, 2, or 4." << std::endl;
                        log (m_s.masterlogfile,o.str());
                        throw std::runtime_error(o.str());
                    }

                    for (unsigned int c = 0; c < pgs; c++)
                    {
                        std::ostringstream attribute;
                        attribute << "PG_concat_" << (c+1);
                        std::string pool_vol_PG_name    = metrics[attribute.str()].string_value(); // PG_concat_1, etc.
                        std::string pool_vol_PG_size    = metrics["PG_size"].string_value();
                        std::string pool_vol_RAID_level = metrics["RAID_level"].string_value();
                        std::string pool_vol_PG_layout  = metrics["PG_layout"].string_value();
                        std::string pool_vol_Drive_type = metrics["Drive_type"].string_value();
                        std::string pool_vol_Pool_ID    = metrics["Pool_ID"].string_value();

                        seen_raid_level.insert(pool_vol_RAID_level);
                        seen_pg_layout.insert(pool_vol_PG_layout);

                        PG_set& pg_set = pgs_by_subsystem__drive_type__PG_layout[s][pool_vol_Drive_type][pool_vol_RAID_level + std::string(" = ") + pool_vol_PG_layout];

                        {
                            std::ostringstream o;
                            o << "Test_config_thumbnail::add() for pool ID " << pool_id << " pool LDEV " <<  pool_LDEV_name << " Drive_type " << dt << " parsing pool vol PG_size = \"" << pool_vol_PG_size << "\"." << std::endl;
                            pg_set.pg_size = unsigned_int(pool_vol_PG_size,o.str());
                        }
                        pg_set.pgs.insert(pool_vol_PG_name);
                        pg_set.pool_vols_by_pool_ID[pool_vol_Pool_ID].add(pool_LDEV_name,m_s.masterlogfile);
                    }
                }
            }
        }
    }
 }

std::string Test_config_thumbnail::drive_type_column() const
{
    std::ostringstream o;

    bool need_plus {false};
    for (auto& pear : pgs_by_subsystem__drive_type__PG_layout)
    {
        const std::string& subsystem_key = pear.first; // "HM800 - 410034"

        for (auto& peach : pear.second)
        {
            const std::string& drive_type = peach.first;

            if (need_plus) o << "+";
            need_plus=true;
            if (pgs_by_subsystem__drive_type__PG_layout.size()>1) o << subsystem_key << ":";
            o << drive_type;
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
    for (auto& pear : pgs_by_subsystem__drive_type__PG_layout)
    {
        const std::string& subsystem_key = pear.first; // "HM800 - 410034"
        for (auto& peach : pear.second)
        {
//            const std::string& drive_type = peach.first;
            drive_type_drives = 0;

            for (auto& banana : peach.second)
            {
//                const std::string& raid_layout = banana.first;
                const PG_set& pg_set           = banana.second;
                unsigned int banana_drives = pg_set.pg_size * pg_set.pgs.size();
                drive_type_drives+= banana_drives;
                subsystem_drives += banana_drives;
            }

            if (need_plus)
            {
                o << "+";
                saw_second = true;
            }
            need_plus=true;

            if (pgs_by_subsystem__drive_type__PG_layout.size()>1) o << subsystem_key << ":";

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
    for (auto& x : seen_raid_level)
    {
        if (need_slash) o << "/";
        need_slash=true;
        o << x;
    }
    return o.str();
}

std::string Test_config_thumbnail::pg_layout_column() const
{
    std::ostringstream o;

    bool need_slash {false};
    for (auto& x : seen_pg_layout)
    {
        if (need_slash) o << "/";
        need_slash=true;
        o << x;
    }
    return o.str();
}

/*static*/ std::string Test_config_thumbnail::csv_headers()
{
    return ",drive type,drive quantity,RAID level,PG layout";
}

std::string Test_config_thumbnail::csv_columns() const
{
    std::ostringstream o;
    o
        << "," << drive_type_column()
        << "," << drive_quantity_column()
        << "," << raid_level_column()
        << "," << pg_layout_column();
    return o.str();
}
