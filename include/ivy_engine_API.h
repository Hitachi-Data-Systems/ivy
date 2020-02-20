//Copyright (c) 2016 Hitachi Data Systems, Inc.
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
//Author: Allart Ian Vogelesang <ian.vogelesang@hds.com>
//
//Support:  "ivy" is not officially supported by Hitachi Data Systems.
//          Contact me (Ian) by email at ian.vogelesang@hds.com and as time permits, I'll help on a best efforts basis.
#pragma once

    std::pair<bool,std::string> // true=success, output or error message
ivy_startup(                    // must be first call
    std::string,                // outputFolderRoot, e.g. "/ivy_output"
    std::string,                // testname - used as subfolder name within outputFolderRoot
    std::string,                // host_spec - names or dotted quad IP addresses of test hosts, e.g. "sun159, cb24-31"
    std::string;                // test LUN select clause - e.g.
		                        //                              { "LDEV_type" : "DP-Vol", "port" : [ "1A", "2A" ] }

// The select clause in ivy_Hosts() specifies "available test LUNs" as a selection out of
// "all discovered LUNs", which has all attribute values seen on all LUNs discovered by
// the external SCSI Inquiry tool on all the test hosts.

// Once we have "available test LUNs", we look to see if we have any command devices going
// to the same subsystem as an available test LUN, and if we do, we fetch the configuration
// data from the subsystem, merging additional attribute values such as "drive_type" that the
// subsystem gives you over and above what you can discover with SCSI Inquiry.

// Over and above automatically being able to match on things like { "port" : [ "1A", "2A" ] },
// ivy has a few custom-matchers for specific attribute names.

// There is a custom matcher for "LDEV" which understands things like "00:1A-00:3F, 01:FF"

// There is a custom matcher for "PG" which understands
// "1-*"	matches PG names starting with 1-
// "1-2:4"	matches 1-2, 1-3, 1-4
// "1-2:"	matches 1-2, 1-3, …
// "1-:2"	matches 1-1, 1-2

    std::pair<bool,std::string> // true=success, output or error message
ivy_SetIogeneratorTemplate(
    std::string,                // e.g. "random_steady"
    std::string);               // parameters, e.g. "IOPS = 20, blocksize = 4KiB"

// ivy iogenerator parameter values don't need quotes in some simple cases,
// for example  iops=max  means  iops="max" and  iops=125.33  means iops="125.33".

// Also saying  fractionRead=25%  means the same as  fractionRead=0.25  and as  fractionRead="0.25".

    std::pair<bool,std::string> // true=success, output or error message
ivy_CreateWorkload(
    std::string                 // workload_name e.g. "cat"
    std::string                 // iogenerator_name, e.g. "sequential"
    std::string);               // parameters, e.g. "IOPS=max, maxTags=1";

    std::pair<bool,std::string> // true=success, output or error message
ivy_CreateRollup(
    std::string,                // rollup name must be LUN attribute names joined with plus signs, e.g.  "host" or "serial_number+port"
    bool,                       // nocsv - true means suppress making csv files
    int,                        // quantity - invalidate result if we don't have this required number of instances. specify -1 if no quantity validation
    double);                    // maxDroop - 0.25 means invalidate if lowest instance IOPS over 25% below highest instance IOPS , or -1 if no maxDroop validation

    std::pair<bool,std::string> // true=success, output or error message
ivy_EditRollup(
    std::string,                // rollup_spec, e.g. "serial_number+Port = { 410123+1A, 410123+2A }"
    std::string);               // parameters], e.g. "maxTags=128"

    std::pair<bool,std::string> // true=success, output or error message
ivy_Go(
    std::string);               // go_parameters, e.g. "stepname=whole_LUN_staggered_start, measure_seconds = 60"

    std::pair<bool,std::string> // true=success, output or error message
ivy_DeleteRollup(
    std::string);               // rollup_spec, e.g. "serial_number+Port";

    std::pair<bool,std::string> // true=success, output or error message
ivy_DeleteWorkload(
    std::string,                // workload_name, e.g. "cat"
    std::string);               // optional test LUN select clause, e.g. ] "{ \"LDEV\" : \"00:04\" }"

    std::pair<bool,std::string> // <true,value> or <false,error message>
ivy_get(
    const std::string&          // thing to be gotten
    );

    std::pair<bool,std::string> // <true,"" or possibly at some point a success string value if some information is required more than just success> or <false,error message>
ivy_put(
    const std::string&,         // thing to update
    const std::string&          // value to set in the thing
    );


    std::pair<bool,std::string> // true=success, output or error message
ivy_shutdown();
