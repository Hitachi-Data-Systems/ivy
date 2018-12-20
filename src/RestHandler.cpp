#include "RestHandler.h"

std::mutex              RestHandler::m_rest_handler_mutex;
std::condition_variable RestHandler::m_rest_handler_cv;
bool                    RestHandler::m_rest_handler_done {false};

RestHandler::RestHandler()
{
    RestBaseUri::initialize_ivy_schema();

    // URI: /ivy_engine
    m_ivyengine = new RestEngineUri(m_ivy_engine_listener);

    m_ivy_engine_listener.support(methods::POST, 
        std::bind(&RestEngineUri::handle_post, m_ivyengine, std::placeholders::_1));

    m_ivy_engine_listener.support(methods::GET, 
        std::bind(&RestEngineUri::handle_get, m_ivyengine, std::placeholders::_1));

    m_ivy_engine_listener.support(methods::PUT, 
        std::bind(&RestEngineUri::handle_put, m_ivyengine, std::placeholders::_1));

    m_ivy_engine_listener.support(methods::PATCH, 
        std::bind(&RestEngineUri::handle_patch, m_ivyengine, std::placeholders::_1));

    m_ivy_engine_listener.support(methods::DEL, 
        std::bind(&RestEngineUri::handle_delete, m_ivyengine, std::placeholders::_1));

    // URI: /ivy_engine/workloads
    m_workloads = new RestWorkloadsUri(m_workloads_listener);
    m_workloads_listener.support(methods::POST,
        std::bind(&RestWorkloadsUri::handle_post, m_workloads, std::placeholders::_1));

    m_workloads_listener.support(methods::GET,
        std::bind(&RestWorkloadsUri::handle_get, m_workloads, std::placeholders::_1));

    m_workloads_listener.support(methods::PUT,
        std::bind(&RestWorkloadsUri::handle_put, m_workloads, std::placeholders::_1));

    m_workloads_listener.support(methods::PATCH,
        std::bind(&RestWorkloadsUri::handle_patch, m_workloads, std::placeholders::_1));

    m_workloads_listener.support(methods::DEL,
        std::bind(&RestWorkloadsUri::handle_delete, m_workloads, std::placeholders::_1));

    // URI: /ivy_engine/rollups
    m_rollups = new RestRollupsUri(m_rollups_listener);
    m_rollups_listener.support(methods::POST,
        std::bind(&RestRollupsUri::handle_post, m_rollups, std::placeholders::_1));

    m_rollups_listener.support(methods::GET,
        std::bind(&RestRollupsUri::handle_get, m_rollups, std::placeholders::_1));

    m_rollups_listener.support(methods::PUT,
        std::bind(&RestRollupsUri::handle_put, m_rollups, std::placeholders::_1));

    m_rollups_listener.support(methods::PATCH,
        std::bind(&RestRollupsUri::handle_patch, m_rollups, std::placeholders::_1));

    m_rollups_listener.support(methods::DEL,
        std::bind(&RestRollupsUri::handle_delete, m_rollups, std::placeholders::_1));

    // URI: /ivy_engine/csvquery
    m_csvquery = new RestCsvqueryUri(m_csvquery_listener);
    m_csvquery_listener.support(methods::POST,
        std::bind(&RestCsvqueryUri::handle_post, m_csvquery, std::placeholders::_1));

    m_csvquery_listener.support(methods::GET,
        std::bind(&RestCsvqueryUri::handle_get, m_csvquery, std::placeholders::_1));

    m_csvquery_listener.support(methods::PUT,
        std::bind(&RestCsvqueryUri::handle_put, m_csvquery, std::placeholders::_1));

    m_csvquery_listener.support(methods::PATCH,
        std::bind(&RestCsvqueryUri::handle_patch, m_csvquery, std::placeholders::_1));

    m_csvquery_listener.support(methods::DEL,
        std::bind(&RestCsvqueryUri::handle_delete, m_csvquery, std::placeholders::_1));

    // URI: /ivy_engine/logs
    m_logs = new RestLogsUri(m_logs_listener);
    m_logs_listener.support(methods::POST,
        std::bind(&RestLogsUri::handle_post, m_logs, std::placeholders::_1));

    m_logs_listener.support(methods::GET,
        std::bind(&RestLogsUri::handle_get, m_logs, std::placeholders::_1));

    m_logs_listener.support(methods::PUT,
        std::bind(&RestLogsUri::handle_put, m_logs, std::placeholders::_1));

    m_logs_listener.support(methods::PATCH,
        std::bind(&RestLogsUri::handle_patch, m_logs, std::placeholders::_1));

    m_logs_listener.support(methods::DEL,
        std::bind(&RestLogsUri::handle_delete, m_logs, std::placeholders::_1));

    // URI: /ivy_engine/sessions
    m_sessions = new RestSessionsUri(m_sessions_listener);
    m_sessions_listener.support(methods::POST,
        std::bind(&RestSessionsUri::handle_post, m_sessions, std::placeholders::_1));

    m_sessions_listener.support(methods::GET,
        std::bind(&RestSessionsUri::handle_get, m_sessions, std::placeholders::_1));

    m_sessions_listener.support(methods::PUT,
        std::bind(&RestSessionsUri::handle_put, m_sessions, std::placeholders::_1));

    m_sessions_listener.support(methods::PATCH,
        std::bind(&RestSessionsUri::handle_patch, m_sessions, std::placeholders::_1));

    m_sessions_listener.support(methods::DEL,
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
