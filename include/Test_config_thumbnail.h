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
#pragma once

#include "ivytypes.h"
#include "LDEVset.h"

class Test_config_thumbnail
{
//variables:
private:

    std::map
    <
        std::string, // "HM800 - 410034"
        std::map
        <
            std::string, // "DKR5D-J600SS"
            std::map
            <
                std::string, // "RAID-5 7+1"
                std::map
                <
                    std::string, // PG, e.g. "1-1"
                    unsigned int // PG size in drives
                >
            >
        >
    >
    drives_by_PG;

    std::set<std::string> raid_levels_seen;

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

    LDEVset LDEVs;

    std::string error_messages;

// methods
public:
    Test_config_thumbnail(){}
    void clear();

    void add(LUN*);

	friend std::ostream& operator<<(std::ostream&, const Test_config_thumbnail&);

    unsigned int total_drives() const;

    std::string drive_type_column() const;
    std::string drive_quantity_column() const;
    std::string raid_level_column() const;

    static std::string csv_headers();
    std::string csv_columns() const;

};




