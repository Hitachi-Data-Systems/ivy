#include "RestHandler.h"
#include "ivy_engine.h"
#include "rapidjson/document.h"  
#include "rapidjson/schema.h"  

extern ivy_engine m_s;

void
RestEngineUri::handle_post(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    std::cout << request.method() << " : " << request.headers()["Cookie"] << std::endl;
    int rc = 0;
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};

    std::string cookie = request.headers()["Cookie"];

    if (cookie != "session_cookie=" +  _active_session_token)
    {
        //http_response response(status_codes::Locked);
        //make_response(response, resultstr, result);
        //request.reply(response);
        //return;
    }

    if (!is_session_owner(request))
    {
        send_busy_response(request);
        return;
    }

    ostringstream o;
    o << "COOKIE:" << cookie << std::endl;
    o << "ACTIVE COOKIE:" << _active_session_token << std::endl;
    std::cout << o.str();
    log (m_s.masterlogfile,o.str());

    char json[1024];

    // extract json from request
    snprintf(json, sizeof(json), "%s", request.extract_string(true).get().c_str());
    std::cout << "JSON:\n" << json << std::endl;

    rapidjson::Document document; 
    if (document.Parse(json).HasParseError())
    {
        rc = 1;
        resultstr += "document invalid";
    }

    rapidjson::SchemaValidator validator(*_schema);
    if (!document.Accept(validator)) {
        rc = 1;
        resultstr += get_schema_validation_error(&validator);
    }

    std::string select_str;
    std::string hosts_list;

    const Value& hosts = document["Hosts"];
    json_to_host_list(hosts, hosts_list); 

    const Value& select = document["Select"];
    json_to_select_str(select, select_str); 

    // Execute Ivy Engine command
    if (rc == 0)
    {
        std::unique_lock<std::mutex> u_lk(goStatementMutex);
        m_s.shutdown_subthreads(); // reset before each run

        std::string outputfolder = m_s.get("output_folder_root").second;
        std::string testname = m_s.get("test_name").second;
        //std::string testname = "MP_50";
        //m_s.startup(outputfolder->value.GetString(), testname->value.GetString(), "", hosts_list, select_str);
        result = m_s.startup(outputfolder, testname, "", hosts_list, select_str);
    }

    http_response response(status_codes::OK);
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestEngineUri::handle_get(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK);
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};
    bool get_hosts {false};
    bool get_luns {false};
    bool get_subsystems {false};

    uri req_uri = request.absolute_uri();
    
    std::map<utility::string_t, utility::string_t>  qmap = req_uri.split_query(req_uri.query());

    for (auto& kv : qmap)
    {
        std::cout << "key: " << kv.first << ", value: " << kv.second << std::endl;
        if (kv.first == "hosts" && kv.second == "true")
            get_hosts = true;
        else if (kv.first == "luns" && kv.second == "true")
            get_luns = true;
        else if (kv.first == "subsystems" && kv.second == "true")
            get_subsystems = true;
        else if (kv.first == "test_folder" || kv.first == "test_name")
        {
                if (kv.first == "test_folder")
                    resultstr = m_s.outputFolderRoot + "/" + m_s.testName;
                else
                    resultstr = m_s.testName;
                std::cout << "resultstr = " << resultstr << std::endl;

                make_response(response, resultstr, result); 
                request.reply(response);
                return;
        }
    }

    // Hosts list
    rapidjson::Document jsonDoc;
    jsonDoc.SetObject();
    rapidjson::Value hosts_array(rapidjson::kArrayType);
    rapidjson::Value subsystems_array(rapidjson::kArrayType);
    rapidjson::Value allluns_array(rapidjson::kArrayType);
    rapidjson::Value testluns_array(rapidjson::kArrayType);
    rapidjson::Value cmdluns_array(rapidjson::kArrayType);
    rapidjson::Document::AllocatorType& allocator = jsonDoc.GetAllocator();

    if (get_hosts)
    {
    std::list<std::string>::iterator iter = m_s.hosts.begin();
    std::list<std::string>::iterator eiter = m_s.hosts.end();
    for (; iter != eiter; ++iter)
    {
        rapidjson::Value val((*iter).c_str(), jsonDoc.GetAllocator());
        hosts_array.PushBack(val, allocator);
    } 
    } 

    // LUNS
    // allDiscoveredLUNs, availableTestLUNs, commandDeviceLUNs;
    if (get_luns)
    {
    for (auto it = m_s.allDiscoveredLUNs.LUNpointers.begin(); it != m_s.allDiscoveredLUNs.LUNpointers.end(); ++it)
    {
        rapidjson::Value val((*it)->toString().c_str(), jsonDoc.GetAllocator());
        allluns_array.PushBack(val, allocator);
    } 

    for (auto it = m_s.availableTestLUNs.LUNpointers.begin(); it != m_s.availableTestLUNs.LUNpointers.end(); ++it)
    {
        rapidjson::Value val((*it)->toString().c_str(), jsonDoc.GetAllocator());
        testluns_array.PushBack(val, allocator);
    } 

    for (auto it = m_s.commandDeviceLUNs.LUNpointers.begin(); it != m_s.commandDeviceLUNs.LUNpointers.end(); ++it)
    {
        rapidjson::Value val((*it)->toString().c_str(), jsonDoc.GetAllocator());
        cmdluns_array.PushBack(val, allocator);
    } 
    } 

    // Subsystems list
    if (get_subsystems)
    {
    for (auto& kv : m_s.subsystems) {
        //std::cout << kv.first << " has value " << kv.second << std::endl;
        rapidjson::Value val(kv.first.c_str(), jsonDoc.GetAllocator());
        subsystems_array.PushBack(val, allocator);
    } 
    }

    if (get_hosts) jsonDoc.AddMember("Hosts", hosts_array, allocator);
    if (get_subsystems) jsonDoc.AddMember("Subsystems", subsystems_array, allocator);
    if (get_luns) jsonDoc.AddMember("AllDiscoveredLUNs", allluns_array, allocator);
    if (get_luns) jsonDoc.AddMember("AvailableTestLUNs", testluns_array, allocator);
    if (get_luns) jsonDoc.AddMember("CommandDeviceLUNs", cmdluns_array, allocator);
    rapidjson::StringBuffer strbuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strbuf);
    jsonDoc.Accept(writer);

    response.headers().add(header_names::content_type, mime_types::application_json);
    response.headers().add(header_names::content_type, charset_types::utf8);
    response.set_body(strbuf.GetString());

    request.reply(response);
}

void
RestEngineUri::handle_put(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    int rc = 0;
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};

    uri req_uri = request.absolute_uri();
    std::map<utility::string_t, utility::string_t>  qmap = req_uri.split_query(req_uri.query());

    for (auto& kv : qmap)
    {
        if (kv.first == "output_folder_root" || kv.first == "test_name")
        {
            std::cout << kv.first << " = " << kv.second << std::endl;
            // Execute Ivy Engine command
            if (rc == 0)
            {
                std::unique_lock<std::mutex> u_lk(goStatementMutex);
                if (kv.first == "output_folder_root")
                    m_s.outputFolderRoot = kv.second;
                if (kv.first == "test_name")
                    m_s.testName = kv.second;

                http_response response(status_codes::OK);
                make_response(response, resultstr, result); 
                request.reply(response);
                return;
            }
        }
    }

    char json[1024];

    // extract json from request
    snprintf(json, sizeof(json), "%s", request.extract_string(true).get().c_str());
    std::cout << "JSON:\n" << json << std::endl;

    rapidjson::Document document; 
    if (document.Parse(json).HasParseError())
    {
        rc = 1;
        resultstr += "document invalid";
    }

    rapidjson::SchemaValidator validator(*_schema);
    if (!document.Accept(validator)) {
        rc = 1;
        resultstr += get_schema_validation_error(&validator);
    }

    rapidjson::Value::MemberIterator step = document.FindMember("stepname");
    rapidjson::Value::MemberIterator warmup = document.FindMember("warmup_seconds");
    rapidjson::Value::MemberIterator measure_seconds = document.FindMember("measure_seconds");
    rapidjson::Value::MemberIterator subinterval = document.FindMember("subinterval_seconds");
    rapidjson::Value::MemberIterator measure = document.FindMember("measure");
    rapidjson::Value::MemberIterator accuracy = document.FindMember("accuracy_plus_minus");
    rapidjson::Value::MemberIterator timeout_seconds = document.FindMember("timeout_seconds");
    rapidjson::Value::MemberIterator max_wp = document.FindMember("max_wp");
    rapidjson::Value::MemberIterator min_wp = document.FindMember("min_wp");
    rapidjson::Value::MemberIterator max_wp_change = document.FindMember("max_wp_change");
    rapidjson::Value::MemberIterator dfc = document.FindMember("dfc");
    rapidjson::Value::MemberIterator low_IOPS = document.FindMember("low_IOPS");
    rapidjson::Value::MemberIterator low_target = document.FindMember("low_target");
    rapidjson::Value::MemberIterator high_IOPS = document.FindMember("high_IOPS");
    rapidjson::Value::MemberIterator high_target = document.FindMember("high_target");
    rapidjson::Value::MemberIterator target_value = document.FindMember("target_value");

    std::ostringstream parameters;
    parameters << "stepname=\"" << step->value.GetString() << "\"";
    if (warmup != document.MemberEnd())
        parameters << ", warmup_seconds=" << warmup->value.GetInt();
    if (measure_seconds != document.MemberEnd())
        parameters << ", measure_seconds=" <<  measure_seconds->value.GetInt();
    if (subinterval != document.MemberEnd())
        parameters << ", subinterval_seconds=" << subinterval->value.GetInt();
    if (measure != document.MemberEnd())
        parameters << ", measure=" << measure->value.GetString();
    if (accuracy != document.MemberEnd())
        parameters << ", accuracy_plus_minus=\"" << accuracy->value.GetString() << "\"";
    if (timeout_seconds != document.MemberEnd())
        parameters << ", timeout_seconds=\"" << timeout_seconds->value.GetString() << "\"";
    if (max_wp != document.MemberEnd())
        parameters << ", max_wp=\"" << max_wp->value.GetString() << "\"";
    if (min_wp != document.MemberEnd())
        parameters << ", min_wp=\"" << min_wp->value.GetString() << "\"";
    if (max_wp_change != document.MemberEnd())
        parameters << ", max_wp_change=\"" << max_wp_change->value.GetString() << "\"";

    if (dfc != document.MemberEnd())
        parameters << ", dfc=" << dfc->value.GetString();

    if (low_IOPS != document.MemberEnd())
        parameters << ", low_IOPS=\"" << low_IOPS->value.GetString() << "\"";

    if (low_target != document.MemberEnd())
        parameters << ", low_target=\"" << low_target->value.GetString() << "\"";
    if (high_IOPS != document.MemberEnd())
        parameters << ", high_IOPS=\"" << high_IOPS->value.GetString() << "\"";

    if (high_target != document.MemberEnd())
        parameters << ", high_target=\"" << high_target->value.GetString() << "\"";
    if (target_value != document.MemberEnd())
        parameters << ", target_value=\"" << target_value->value.GetString() << "\"";

    // Execute Ivy Engine command
    if (rc == 0)
    {
        std::unique_lock<std::mutex> u_lk(goStatementMutex);
        result = m_s.go(parameters.str());
    }

    //resultstr += (result.first ? "Successful" : "Failed");
    //resultstr += " Reason: ";
    //resultstr += result.second;

    http_response response(status_codes::OK);
    make_response(response, resultstr, result); 
    request.reply(response);
}

void
RestEngineUri::handle_patch(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK); 
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestEngineUri::handle_delete(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK); 
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
}
