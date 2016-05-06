//
// Author Allart Ian Vogelesang, Hitachi Data Systems
// Copyright Hitachi Data Systems 2015
//
#pragma once

#include <map>

#include "ivydefines.h"
#include "RunningStat.h"

class master_stuff;

class subsystem_summary_data
{
    public:

    // variables

    std::map
    <
        std::string, // element,  e.g. MP_core
        std::map
        <
            std::string, // metric, e.g. busy_percent
            RunningStat<ivy_float, ivy_int>
        >
    > data;

    ivy_float repetition_factor {1.0};
    // addIn() adds the repetition factor of the other thing you are adding in.
    // Thus if you look at the sum() function, you need to divide sum values by the repetition factor.


    // methods
    subsystem_summary_data(){};
    void addIn(const subsystem_summary_data&);
    static std::string csvHeadersPartOne(std::string np = std::string(""));
    static std::string csvHeadersPartTwo(std::string np = std::string(""));
    std::string csvValuesPartOne(unsigned int divide_count_by = 1);
    std::string csvValuesPartTwo(unsigned int divide_count_by = 1);
    std::string thumbnail() const;
    void derive_metrics();
    ivy_float IOPS();  // these three methods return -1.0 if no data.
    ivy_float decimal_MB_per_second();
    ivy_float service_time_ms();
};
