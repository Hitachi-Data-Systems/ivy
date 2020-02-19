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
    std::string base_url {"http://" + m_host + ":" + m_port};

    http_listener m_ivy_engine_listener {base_url + "/ivy_engine"};
    http_listener m_rollups_listener {base_url + "/ivy_engine/rollups"};
    http_listener m_workloads_listener {base_url + "/ivy_engine/workloads"};
    http_listener m_csvquery_listener {base_url + "/ivy_engine/csvquery"};
    http_listener m_logs_listener {base_url + "/ivy_engine/logs"};
    http_listener m_sessions_listener {base_url + "/ivy_engine/sessions"};

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
