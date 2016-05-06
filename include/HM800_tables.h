//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <map>
#include <vector>

extern std::map
<
    std::string, // sub_model HM800S / HM800M / HM800H
    std::map
    <
        std::string, // MPU "0", "1", "2", "3"
        std::vector
        <
            std::pair
            <
                std::string, // MPU#    like MP#00
                std::string  // MP_core like MP10-00
            >
        >
    >
>

HM800_MPU_map;
