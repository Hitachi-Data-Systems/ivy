#include "RestHandler.h"
#include "ivy_engine.h"
#include "rapidjson/document.h"  
#include "rapidjson/schema.h"  

extern ivy_engine m_s;

void
RestRollupsUri::handle_post(http_request request)
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
    rapidjson::Value::MemberIterator nocsv = document.FindMember("nocsv");
    rapidjson::Value::MemberIterator quantity = document.FindMember("quantity");
    rapidjson::Value::MemberIterator maxdroop = document.FindMember("maxDroop");
    rapidjson::Value::MemberIterator maxsdroop_validation = document.FindMember("maxDroopMaxtoMinIOPSSection");

    // Execute Ivy Engine command
    if (rc == 0)
    {
        std::unique_lock<std::mutex> u_lk(goStatementMutex);
        std::pair<int, std::string> 
        rslt = m_s.create_rollup(name->value.GetString(),
                   (nocsv != document.MemberEnd() ? nocsv->value.GetBool() : false),
                   false /* have_quantity_validation */,
                   (maxsdroop_validation != document.MemberEnd() ? maxsdroop_validation->value.GetBool() : false),
                   (quantity != document.MemberEnd() ? quantity->value.GetInt() : 1),
                   (maxdroop != document.MemberEnd() ? maxdroop->value.GetDouble() : 6.95323e-310));
    }

    http_response response(status_codes::OK);
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestRollupsUri::handle_get(http_request request)
{
    std::cout << request.method() << " : " << request.absolute_uri().path() << std::endl;
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};

    resultstr = m_s.show_rollup_structure();

    make_response(response, resultstr, result);
    request.reply(response);
    return;
}

void
RestRollupsUri::handle_put(http_request request)
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
        std::unique_lock<std::mutex> u_lk(goStatementMutex);
        std::pair<int, std::string> 
        rslt = m_s.edit_rollup(name->value.GetString(), parameters->value.GetString());
    }

    http_response response(status_codes::OK);
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestRollupsUri::handle_patch(http_request request)
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
RestRollupsUri::handle_delete(http_request request)
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

    Value::MemberIterator name = document.FindMember("name");

    // Execute Ivy Engine command
    if (rc == 0)
    {
        std::unique_lock<std::mutex> u_lk(goStatementMutex);
        std::pair<int, std::string> 
        rslt = m_s.delete_rollup(name->value.GetString());
    }

    http_response response(status_codes::OK);
    make_response(response, resultstr, result);
    request.reply(response);
}
