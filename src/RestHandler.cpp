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

#include <cpprest/base_uri.h>
#include <cpprest/http_listener.h>
#include <cpprest/http_msg.h>
#include <cpprest/json.h>

#include "ivytypes.h"
#include "RestHandler.h"

std::mutex              RestHandler::m_rest_handler_mutex;
std::condition_variable RestHandler::m_rest_handler_cv;
bool                    RestHandler::m_rest_handler_done {false};

RestHandler::RestHandler()
{
    RestBaseUri::initialize_ivy_schema();

    // URI: /ivy_engine
    m_ivyengine = new RestEngineUri(m_ivy_engine_listener);

    m_ivy_engine_listener.support(methods::POST,  // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestEngineUri::handle_post, m_ivyengine, std::placeholders::_1));

    m_ivy_engine_listener.support(methods::GET,  // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestEngineUri::handle_get, m_ivyengine, std::placeholders::_1));

    m_ivy_engine_listener.support(methods::PUT,  // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestEngineUri::handle_put, m_ivyengine, std::placeholders::_1));

    m_ivy_engine_listener.support(methods::PATCH,  // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestEngineUri::handle_patch, m_ivyengine, std::placeholders::_1));

    m_ivy_engine_listener.support(methods::DEL,  // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestEngineUri::handle_delete, m_ivyengine, std::placeholders::_1));

    // URI: /ivy_engine/workloads
    m_workloads = new RestWorkloadsUri(m_workloads_listener);
    m_workloads_listener.support(methods::POST, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestWorkloadsUri::handle_post, m_workloads, std::placeholders::_1));

    m_workloads_listener.support(methods::GET, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestWorkloadsUri::handle_get, m_workloads, std::placeholders::_1));

    m_workloads_listener.support(methods::PUT, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestWorkloadsUri::handle_put, m_workloads, std::placeholders::_1));

    m_workloads_listener.support(methods::PATCH, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestWorkloadsUri::handle_patch, m_workloads, std::placeholders::_1));

    m_workloads_listener.support(methods::DEL, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestWorkloadsUri::handle_delete, m_workloads, std::placeholders::_1));

    // URI: /ivy_engine/rollups
    m_rollups = new RestRollupsUri(m_rollups_listener);
    m_rollups_listener.support(methods::POST, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestRollupsUri::handle_post, m_rollups, std::placeholders::_1));

    m_rollups_listener.support(methods::GET, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestRollupsUri::handle_get, m_rollups, std::placeholders::_1));

    m_rollups_listener.support(methods::PUT, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestRollupsUri::handle_put, m_rollups, std::placeholders::_1));

    m_rollups_listener.support(methods::PATCH, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestRollupsUri::handle_patch, m_rollups, std::placeholders::_1));

    m_rollups_listener.support(methods::DEL, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestRollupsUri::handle_delete, m_rollups, std::placeholders::_1));

    // URI: /ivy_engine/csvquery
    m_csvquery = new RestCsvqueryUri(m_csvquery_listener);
    m_csvquery_listener.support(methods::POST, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestCsvqueryUri::handle_post, m_csvquery, std::placeholders::_1));

    m_csvquery_listener.support(methods::GET, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestCsvqueryUri::handle_get, m_csvquery, std::placeholders::_1));

    m_csvquery_listener.support(methods::PUT, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestCsvqueryUri::handle_put, m_csvquery, std::placeholders::_1));

    m_csvquery_listener.support(methods::PATCH, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestCsvqueryUri::handle_patch, m_csvquery, std::placeholders::_1));

    m_csvquery_listener.support(methods::DEL, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestCsvqueryUri::handle_delete, m_csvquery, std::placeholders::_1));

    // URI: /ivy_engine/logs
    m_logs = new RestLogsUri(m_logs_listener);
    m_logs_listener.support(methods::POST, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestLogsUri::handle_post, m_logs, std::placeholders::_1));

    m_logs_listener.support(methods::GET, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestLogsUri::handle_get, m_logs, std::placeholders::_1));

    m_logs_listener.support(methods::PUT, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestLogsUri::handle_put, m_logs, std::placeholders::_1));

    m_logs_listener.support(methods::PATCH, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestLogsUri::handle_patch, m_logs, std::placeholders::_1));

    m_logs_listener.support(methods::DEL, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestLogsUri::handle_delete, m_logs, std::placeholders::_1));

    // URI: /ivy_engine/sessions
    m_sessions = new RestSessionsUri(m_sessions_listener);
    m_sessions_listener.support(methods::POST, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestSessionsUri::handle_post, m_sessions, std::placeholders::_1));

    m_sessions_listener.support(methods::GET, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestSessionsUri::handle_get, m_sessions, std::placeholders::_1));

    m_sessions_listener.support(methods::PUT, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestSessionsUri::handle_put, m_sessions, std::placeholders::_1));

    m_sessions_listener.support(methods::PATCH, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestSessionsUri::handle_patch, m_sessions, std::placeholders::_1));

    m_sessions_listener.support(methods::DEL, // @suppress("Symbol is not resolved") // @suppress("Invalid arguments")
        std::bind(&RestSessionsUri::handle_delete, m_sessions, std::placeholders::_1));

    try
    {
        //actively listen at endpoints
        m_ivy_engine_listener.open().wait();
        m_workloads_listener.open().wait();
        m_rollups_listener.open().wait();
        m_csvquery_listener.open().wait();
        m_logs_listener.open().wait();
        m_sessions_listener.open().wait();
    }
    catch (std::exception const & e)
    {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

void RestHandler::wait_and_serve()
{
    std::unique_lock<std::mutex> lk(m_rest_handler_mutex);
    while (!m_rest_handler_done) m_rest_handler_cv.wait(lk, []{return m_rest_handler_done;});
 
    std::pair<bool,std::string> strc1 = m_s.shutdown_subthreads();
    {
        std::ostringstream o;
        o << "ivy engine shutdown subthreads ";
        if (strc1.first) o << "successful"; else o << "unsuccessful";
        o << std::endl;
        std::cout << o.str();
        log (m_s.masterlogfile,o.str());
    }
}

void RestHandler::shutdown()
{
    m_s.overall_success = true;
    m_rest_handler_done = true;
    m_rest_handler_cv.notify_all();
}
