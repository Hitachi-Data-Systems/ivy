//Copyright (c) 2016-2020 Hitachi Vantara Corporation
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

#include <cpprest/http_listener.h>
#include <rapidjson/error/en.h>
#include <rapidjson/schema.h>
#include <rapidjson/writer.h>
#include <RestBaseUri.h>
#include <RestEngineUri.h>
#include <RestLogsUri.h>
#include <RestWorkloadsUri.h>
#include <RestCsvqueryUri.h>
#include <RestRollupsUri.h>
#include <RestSessionsUri.h>

#include "ivytypes.h"

using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;
using namespace web::http::details;

using namespace std;
using namespace rapidjson;


class RestHandler {
public:
    RestHandler();
    RestHandler(std::string host, std::string port) :
                              m_host(host), m_port(port) {};
    static void wait_and_serve();
    static void shutdown();
private:

    std::string m_host {"0.0.0.0"};
    std::string m_port {"9000"};
    std::string base_url {"http://" + m_host + ":" + m_port}; // @suppress("Invalid arguments")

    http_listener m_ivy_engine_listener {base_url + "/ivy_engine"}; // @suppress("Invalid arguments")
    http_listener m_rollups_listener {base_url + "/ivy_engine/rollups"}; // @suppress("Invalid arguments")
    http_listener m_workloads_listener {base_url + "/ivy_engine/workloads"}; // @suppress("Invalid arguments")
    http_listener m_csvquery_listener {base_url + "/ivy_engine/csvquery"}; // @suppress("Invalid arguments")
    http_listener m_logs_listener {base_url + "/ivy_engine/logs"}; // @suppress("Invalid arguments")
    http_listener m_sessions_listener {base_url + "/ivy_engine/sessions"}; // @suppress("Invalid arguments")

    // URI operations handlers
    RestEngineUri    *m_ivyengine;
    RestWorkloadsUri *m_workloads;
    RestRollupsUri   *m_rollups;
    RestCsvqueryUri  *m_csvquery;
    RestLogsUri      *m_logs;
    RestSessionsUri  *m_sessions;

    static std::mutex              m_rest_handler_mutex;
    static std::condition_variable m_rest_handler_cv;
    static bool                    m_rest_handler_done;
}; 
