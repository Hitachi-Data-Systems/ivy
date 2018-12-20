#include "RestHandler.h"
#include "ivy_engine.h"
#include "rapidjson/document.h"  
#include "rapidjson/schema.h"  

extern ivy_engine m_s;

void
RestLogsUri::handle_post(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    std::pair<bool, std::string> result {true, std::string()};
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    make_response(response, resultstr, result);
    request.reply(response);
    return;
}

void
RestLogsUri::handle_get(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
    return;
}

void
RestLogsUri::handle_put(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    bool rc {true};
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};

    char json[1024];

    // extract json from request
    snprintf(json, sizeof(json), "%s", request.extract_string(true).get().c_str());
    std::cout << "JSON:\n" << json << std::endl;

    //log(m_s.masterlogfile, "Kumaran");

    rapidjson::Document document; 
    if (document.Parse(json).HasParseError())
    {
        rc = false;
        resultstr += "document invalid";
    }

    rapidjson::SchemaValidator validator(*_schema);
    if (!document.Accept(validator)) {
        rc = false;
        resultstr += get_schema_validation_error(&validator);
    }

    rapidjson::Value::MemberIterator logname = document.FindMember("logname");
    rapidjson::Value::MemberIterator logmessage = document.FindMember("message");
    
    if (logname != document.MemberEnd())
        log(logname->value.GetString(), logmessage->value.GetString());
    else
        log(m_s.masterlogfile, logmessage->value.GetString());

    http_response response(status_codes::OK);
    if (!rc) response.set_status_code(status_codes::NotAcceptable);
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestLogsUri::handle_patch(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
    return;
}

void
RestLogsUri::handle_delete(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
    return;
}
