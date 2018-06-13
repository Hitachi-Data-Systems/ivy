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
#pragma once

#include "LUN.h"
#include "LDEVset.h"


class PG_set
{
    public:
        unsigned int pg_size {0};
        std::set<std::string> pgs; // PG names
        LDEVset internal_ldevs;
        std::map
        <
            std::string, // Pool ID, e.g. "0"
            LDEVset
        > pool_vols_by_pool_ID;
};

class Test_config_thumbnail
{
public:
    std::map
    <
        std::string,
        LDEVset
    > dp_vols_by_pool_ID;

    std::map
    <
        std::string, // "410034"
        std::map
        <
            std::string, // "DKR5D-J600SS"
            std::map
            <
                std::string, // "RAID-5 7+1"
                PG_set
            >
        >
    >
    pgs_by_subsystem__drive_type__PG_layout;


    std::map
    <
        std::string, // "subsystem_type#serial_number"
        std::map
        <
            std::string, //  "DKR5D-J600SS" or "Pool ID 0 DP-Vols"
            LDEVset
        >
    >
    available_LDEVs;

    std::map
    <
        std::string,  // "subsystem_type#serial_number"
        std::map
        <
            std::string, // Pool ID (in string form)
            std::map
            <
                std::string, // drive type
                LDEVset
            >
        >
    >
    available_Pool_Vols;  // this is built by looking at the command device configuration data

    std::map
    <
        std::string, // ivyscript_hostname
        std::map
        <
            std::string, // "subsystem_type#serial_number"
            LDEVset
        >
    >
    LDEVs_by_host;

    std::map
    <
        std::string, // "subsystem_type#serial_number"
        std::map
        <
            std::string, // port
            LDEVset
        >
    >
    LDEVs_by_subsystem_port;

    std::set<std::string> seen_raid_level;
    std::set<std::string> seen_pg_layout;

    Test_config_thumbnail(){}
    void clear();

    void add(LUN*);

	friend std::ostream& operator<<(std::ostream&, const Test_config_thumbnail&);

    unsigned int total_drives() const;

    std::string drive_type_column() const;
    std::string drive_quantity_column() const;
    std::string raid_level_column() const;
    std::string pg_layout_column() const;

    static std::string csv_headers();
    std::string csv_columns() const;

};




