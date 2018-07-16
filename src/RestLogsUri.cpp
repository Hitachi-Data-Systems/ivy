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
    int rc = 0;
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};

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

    rapidjson::Value::MemberIterator name = document.FindMember("name");
    rapidjson::Value::MemberIterator parameters = document.FindMember("parameters");

    // Execute Ivy Engine command
    if (rc == 0)
    {
        std::pair<int, std::string> 
        rslt = m_s.edit_rollup(name->value.GetString(), parameters->value.GetString());
    }

    // TODO: Read the master log file next 10 lines
    // and return as a string

    http_response response(status_codes::OK);
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestLogsUri::handle_put(http_request request)
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
