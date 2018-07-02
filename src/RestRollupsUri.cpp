#include "RestHandler.h"
#include "ivy_engine.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/document.h"  
#include "rapidjson/schema.h"  
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

extern ivy_engine m_s;

void
RestRollupsUri::handle_post(http_request request)
{
    std::cout << "POST /ivy_engine/rollups\n";
    int rc = 0;
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};

    char json[1024];

    // extract json from request
    snprintf(json, sizeof(json), "%s", request.extract_string(true).get().c_str());
    printf("JSON:\n %s\n", json);

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
                                 nocsv->value.GetBool(),
                                 false /* have_quantity_validation */,
                                 maxsdroop_validation->value.GetBool(),
                                 quantity->value.GetInt(),
                                 maxdroop->value.GetDouble());
    }

    http_response response(status_codes::OK);
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestRollupsUri::handle_get(http_request request)
{
    std::cout << "GET /ivy_engine/rollups\n";
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
    return;
}

void
RestRollupsUri::handle_put(http_request request)
{
    std::cout << "PUT /ivy_engine/rollups\n";
    int rc = 0;
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};

    char json[1024];

    // extract json from request
    snprintf(json, sizeof(json), "%s", request.extract_string(true).get().c_str());
    printf("JSON:\n %s\n", json);

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
    std::cout << "PATCH /ivy_engine/rollups\n";
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
    std::cout << "DELETE /ivy_engine/rollups\n";
    int rc = 0;
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};

    char json[1024];

    // extract json from request
    snprintf(json, sizeof(json), "%s", request.extract_string(true).get().c_str());
    printf("JSON:\n %s\n", json);

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
