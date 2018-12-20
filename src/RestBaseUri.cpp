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

#include "RestHandler.h"
#include "ivy_engine.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"  
#include "rapidjson/schema.h"  
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

bool RestBaseUri::_schema_initialized = false;
rapidjson::SchemaDocument* RestBaseUri::_schema = nullptr;
std::mutex RestBaseUri::goStatementMutex;
std::map<std::string, ivy_engine*> _ivy_engine_instances;

std::string RestBaseUri::_active_session_token;
bool RestBaseUri::_session_state {false};
std::mutex RestBaseUri::_session_mutex;

std::string JSON_SCHEMA = R"( {
    "$schema": "http://json-schema.org/draft-04/schema#",

    "definitions": {
        "percentageschema": {
            "anyof": [
                {  "type":     "number",
                  "minimum":  0,
                  "maximum":  100
                },
                {  "type":     "number",
                  "minimum":  0,
                  "maximum":  1
                }]
        },
        "binarysizeschema" : {
            "type": "object", 
	    "properties": {
                "value" : { "type":     "number" },
                "suffix" : { "type": { "enum": [ "KiB", "MiB", "GiB", "TiB" ] } }
            }
        },
        "hostlistschema": {
            "anyof" : [
                {"type" : "string"},
                {
                "type": "array",
                "items":{
                    "type": "string"
                }
                } ]
        },
        "selectschema": {
            "anyof" : [
                {"type" : "string"},
                {
                "type": "array",
                "items":{
                    "type": "object", 
                    "properties": {
                        "key" : { "type": "string" },
                        "val" : {
                            "anyof": [
                                { "type": "array",
                                "items":{
                                    "type": "string"
                                }},
                                {"type": "string"}
                             ]
                        }
                    }
                }
                } ]
        },
        "ioparametersschema":  {
            "anyof": [ 
            {"type" : "string"},
            {
            "type": "object", 
	    "properties": {
                "skew": { "$ref": "#/definitions/percentageschema" },
                "hot_zone_size_bytes" : {
                    "anyof": [
                        { "type": "string" },
                        { "$ref": "#/definitions/binarysizeschema" }
                    ]
                },
                "hot_zone_read_fraction" : { "type" : "boolean" },
                "hot_zone_write_fraction" : { "type" : "boolean" },
                "hot_zone_IOPS_fraction": { "$ref": "#/definitions/percentageschema" },
                "fractionRead": { "$ref": "#/definitions/percentageschema" },
                "VolumeCoverageFractionStart": { "$ref": "#/definitions/percentageschema" },
                "VolumeCoverageFractionEnd": { "$ref": "#/definitions/percentageschema" },
                "blocksize": { "type" : "integer" },
                "IOPS": {
                    "anyof": [
                          { "type": {
                              "enum": [ "max", "min" ] }
                          },
                          { "type": "object",
	                      "properties": {
                                  "allof": [
                                      { "anyof": [
                                            {  "incr+": { "type": "string" } },
                                            {  "decr-": { "type": "string" } }
                                       ] }, 
                                      { "amount": { "type": "integer" } }
                                  ] 
	                      }
	                  }
                    ] 
                },
                "pattern": {
                    "anyof": [
                          { "type": {
                              "enum": [ "random", "ascii", "gobbledgook" ] }
                          },
                          { "type": "object",
	                      "properties": {
                                  "variable_type": { "type": { 
                                      "enum": [ "trailing_zeros" ] }
                                  },
                                  "compressibility": { "$ref": "#/definitions/percentageschema" }
                              }
	                  }
                    ] 
                },
                "maxTags": {"type": "integer" }
            }
            } ]
        },
        "iosequencertemplateschema":  {
            "type": "object", 
	    "properties": {
                "iosequencer": { "type": {
                            "enum": [ "random_steady", "random_independent", "sequential" ] }},
                "parameters" : { "$ref": "#/definitions/ioparametersschema" }
	    }
        },
        "loggingchema":  {
            "type": "object", 
	    "properties": {
                "logname": { "type": "string" },
                "message": { "type": "string" },
	    "required": ["logname", "message"]
	    }
        },
        "hostsstatementschema":  {
            "type": "object", 
	    "properties": {
                "hosts" : { "$ref": "#/definitions/hostlistschema" },
                "select" : { "$ref": "#/definitions/selectschema" },
	    "required": ["hosts", "select"]
            }
        },
        "setiosequencertemplateschema": {
            "iosequencer" : { "$ref": "#/definitions/iosequencertemplateschema" }
        },
        "createworkloadschema":  {
            "type": "object", 
	    "properties": {
                "name": { "type": "string" },
                "select" : { "$ref": "#/definitions/selectschema" },
                "iosequencer": { "type": {
                                    "enum": [ "random_steady", "random_independent", "sequential"]}},
                "parameters" : { "$ref": "#/definitions/ioparametersschema" }
	    },
	    "required": ["name"]
        },
        "deleteworkloadschema":  {
            "type": "object", 
	    "properties": {
                "name": { "type": "string" },
                "select" : { "$ref": "#/definitions/selectschema" }
	    },
	    "required": ["name"]
        },
        "createrollupschema":  {
            "type": "object", 
	    "properties": {
                "name": { "type": "string" },
                "nocsv": { "type": "boolean" },
                "maxDroopMaxtoMinIOPSSection": { "type": "boolean" },
                "quantity": { "type": "integer" },
                "maxDroop": { "type": "number" }
	    },
	    "required": ["name"]
        },
        "editrollupschema":  {
            "type": "object", 
	    "properties": {
                "name": { "type": "string" },
                "parameters" : {
                    "anyof": [
                        { "type": "string" },
                        { "$ref": "#/definitions/ioparametersschema" }
                    ]
                }
	    },
	    "required": ["name", "parameters"]
        },
        "deleterollupschema":  {
            "type": "object", 
	    "properties": {
                "name": { "type": "string" }
	    },
	    "required": ["name"]
        },
        "csvqueryschema":  {
            "type": "object", 
	    "properties": {
                "filename": { "type": "string" },
                "row": { "type": "integer" },
                "col": { 
                    "anyof": [
                        { "type": "integer" },
                        { "type": "string" }
                    ]
	        }
	    },
	    "required": ["filename", "row", "col"]
        },
        "configschema":  {
            "type": "object", 
	    "properties": {
                "Hosts": {
                    "type": "array",
                    "items":{
                        "type": "string"
                    }
                },
                "allDiscoveredLUNs": {
                    "type": "array",
                    "items":{
                        "type": "string"
                    }
                },
                "availableTestLUNs": {
                    "type": "array",
                    "items":{
                        "type": "string"
                    }
                },
                "commandDeviceLUNs": {
                    "type": "array",
                    "items":{
                        "type": "string"
                    }
                },
                "Subsystems": {
                    "type": "array",
                    "items":{
                        "type": "string"
                    }
                }
	    }
        },
        "sessionsschema":  {
            "type": "object", 
	    "properties": {
                "api_token": { "type": "string" }
	    },
	    "required": ["api_token"]
        },
        "gostatementschema":  {
            "type": "object", 
	    "properties": {
                "stepname": { "type": "string" },
                "subinterval_seconds" : { "type": "integer" },
                "warmup_seconds" : { "type": "integer" },
                "measure_seconds" : { "type": "integer" },
                "cooldown_by_wp": { "type": "boolean" },
                "measure": { "type": {
                            "enum": ["MP_core_busy_percent", "PG_busy_percent",
                                     "CLPR_WP_percent", "MB_per_second", "IOPS",
                             "service_time_seconds", "response_time_seconds"]
                }},
                "accuracy_plus_minus": { "type": "string" },
                "timeout_seconds": { 
                    "anyof": [
                        { "type": "integer" },
                        { "type": "string" }
                    ]
	        },
                "max_wp": { "$ref": "#/definitions/percentageschema" },
                "min_wp": { "$ref": "#/definitions/percentageschema" },
                "max_wp_change": { "$ref": "#/definitions/percentageschema" },
                "dfc": { "type": {
                            "enum": ["PID"]
                }},
                "low_IOPS": { "type": "string" },
                "low_target": { "type": "string" },
                "high_IOPS": { "type": "string" },
                "high_target": { "type": "string" },
                "target_value": { "type": "string" },
                "focus_rollup": { "type": {
                            "enum": ["all"]
                }},
                "source": { "type": {
                            "enum": ["workload", "RAID_subsystem"] }
                 },
                "subsystem_element": { "type": "string" },
                "element_metric": { "type": "string" },
                "category": { "type": {
                            "enum": ["overall", "read", "write", "random", "sequential",
                                     "random_read", "random_write", "sequential_read",
                                     "sequential_write"] }
                 },
                "accumulator_type": { "type": {
                            "enum": ["bytes_transferred", "service_time", "response_time"] }
                 },
                "accessor": { "type": {
                            "enum": ["avg", "count", "min","max", "sum", "variance",
                                     "standardDeviation"] }
                 }
	    },
	    "required": ["stepname"]
        },
        "responseschema":  {
            "type": "object", 
	    "properties": {
                "retval": { "type": "string" },
                "status": { "type": "string" },
                "result": { "type": "string" }
	    },
	    "required": ["retval", "status", "result"]
        }
    },

    "anyof": [
        { "$ref": "#/definitions/responseschema" },
        { "$ref": "#/definitions/percentageschema" },
        { "$ref": "#/definitions/binarysizeschema" },
        { "$ref": "#/definitions/hostlistschema" },
        { "$ref": "#/definitions/selectschema" },
        { "$ref": "#/definitions/ioparametersschema" },
        { "$ref": "#/definitions/iosequencertemplateschema" },
        { "$ref": "#/definitions/hostsstatementschema" },
        { "$ref": "#/definitions/setiosequencertemplateschema" },
        { "$ref": "#/definitions/createworkloadschema" },
        { "$ref": "#/definitions/deleteworkloadschema" },
        { "$ref": "#/definitions/createrollupschema" },
        { "$ref": "#/definitions/editrollupschema" },
        { "$ref": "#/definitions/deleterollupschema" },
        { "$ref": "#/definitions/sessionsschema" },
        { "$ref": "#/definitions/gostatementschema" },
        { "$ref": "#/definitions/configschema" },
        { "$ref": "#/definitions/loggingschema" },
        { "$ref": "#/definitions/csvquerychema" }
    ]
})";

void
RestBaseUri::initialize_ivy_schema()
{
    if (_schema_initialized)
        return;

    rapidjson::Document schema_doc;
#if 0
    char buffer[4096];
    std::string schema_file {"/usr/local/bin/ivyschema.json"};
    FILE *fp = fopen(schema_file.c_str(), "r");
    if (!fp) {
        std::ostringstream err;
        err << "<Error> Schema file: " << schema_file << " not found" << std::endl;
        log(m_s.masterlogfile, err.str());
        throw std::runtime_error(err.str());
    }

    rapidjson::FileReadStream fs(fp, buffer, sizeof(buffer));
    schema_doc.ParseStream(fs);
    if (schema_doc.HasParseError()) {
        std::ostringstream err;
        err << "<Error> Schema file: " << schema_file << " is not a valid JSON." << std::endl;
        err << "<Error> Error(offset " << static_cast<unsigned>(schema_doc.GetErrorOffset()) << "): " << GetParseError_En(schema_doc.GetParseError()) << std::endl;
        log(m_s.masterlogfile, err.str());
        fclose(fp);
        throw std::runtime_error(err.str());
    }
    fclose(fp);
#endif
     
    schema_doc.Parse(JSON_SCHEMA.c_str());
    if (schema_doc.HasParseError()) {
        std::ostringstream err;
        err << "<Error> Schema :  " << JSON_SCHEMA  << " is not a valid JSON." << std::endl;
        err << "<Error> Error(offset " << static_cast<unsigned>(schema_doc.GetErrorOffset()) << "): " << GetParseError_En(schema_doc.GetParseError()) << std::endl;
        log(m_s.masterlogfile, err.str());
        throw std::runtime_error(err.str());
    }

    _schema = new rapidjson::SchemaDocument(schema_doc);
    _schema_initialized = true;
}
