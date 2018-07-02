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
RestWorkloadsUri::handle_post(http_request request)
{
    std::cout << "POST /ivy_engine/workloads\n";
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

    std::string selstr;

    rapidjson::Value::MemberIterator workload = document.FindMember("workload");
    rapidjson::Value::MemberIterator iosequencer = document.FindMember("iosequencer");
    rapidjson::Value::MemberIterator parameters = document.FindMember("parameters");
    rapidjson::Value::MemberIterator IOPS = document.FindMember("IOPS");
    rapidjson::Value::MemberIterator fractionRead = document.FindMember("fractionRead");
    rapidjson::Value::MemberIterator blocksize = document.FindMember("blocksize");
    rapidjson::Value::MemberIterator maxtags = document.FindMember("maxtags");

    rapidjson::Value::MemberIterator select = document.FindMember("select");
    if (select == document.MemberEnd())
        selstr = "";
    else
        selstr = select->value.GetString();

    std::ostringstream params;

    if (parameters->value.IsString())
    {
        params << parameters->value.GetString();
    } else
    {
        params << "fractionRead=" << fractionRead->value.GetInt();
        params << "%, blocksize=" << blocksize->value.GetInt();
        params << "KiB, IOPS=" << IOPS->value.GetString();
        params << ", maxtags=" << maxtags->value.GetInt();
    }

    // Execute Ivy Engine command
    if (rc == 0)
    {
        std::unique_lock<std::mutex> u_lk(goStatementMutex);
        m_s.createWorkload(workload->value.GetString(), selstr.c_str(), iosequencer->value.GetString(), params.str());
    }

    http_response response(status_codes::OK);
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestWorkloadsUri::handle_get(http_request request)
{
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestWorkloadsUri::handle_put(http_request request)
{
    std::cout << "PUT /ivy_engine/workloads\n";
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

    rapidjson::Value::MemberIterator iosequencer_template = document.FindMember("iosequencer");
    rapidjson::Value::MemberIterator IOPS = document.FindMember("IOPS");
    rapidjson::Value::MemberIterator fractionRead = document.FindMember("fractionRead");
    rapidjson::Value::MemberIterator blocksize = document.FindMember("blocksize");
    rapidjson::Value::MemberIterator maxtags = document.FindMember("maxtags");

    std::ostringstream params;

    params << "fractionRead=" << fractionRead->value.GetInt();
    params << "%, blocksize=" << blocksize->value.GetInt();
    params << "KiB, IOPS=" << IOPS->value.GetString();
    params << ", maxtags=" << maxtags->value.GetInt();

    // Execute Ivy Engine command
    if (rc == 0)
    {
        std::unique_lock<std::mutex> u_lk(goStatementMutex);
        m_s.set_iosequencer_template(iosequencer_template->value.GetString(), params.str());
    }

    http_response response(status_codes::OK);

    response.headers().add(header_names::content_type, mime_types::application_json);
    response.headers().add(header_names::content_type, charset_types::utf8);

    response.set_body("{\"status\": 1}");
    make_response(response, resultstr, result); 
    request.reply(response);
}

void
RestWorkloadsUri::handle_patch(http_request request)
{
    http_response response(status_codes::OK);
    std::string resultstr("Not Supported");
    std::pair<bool, std::string> result {true, std::string()};
    make_response(response, resultstr, result);
    request.reply(response);
}

void
RestWorkloadsUri::handle_delete(http_request request)
{
    std::cout << "DELETE /ivy_engine/workloads\n";

    http_response response(status_codes::OK);
    std::string resultstr;
    std::pair<bool, std::string> result {true, std::string()};

    char json[1024];
    // extract json from request
    snprintf(json, sizeof(json), "%s", request.extract_string(true).get().c_str());
    printf("JSON:\n %s\n", json);

    rapidjson::Document document; 
    if (document.Parse(json).HasParseError())
    {
        resultstr += "document invalid";
    }

    rapidjson::SchemaValidator validator(*_schema);
    if (!document.Accept(validator)) {
        resultstr += get_schema_validation_error(&validator);
    }

    rapidjson::Value::MemberIterator workload = document.FindMember("workload");
    //rapidjson::Value::MemberIterator select = document.FindMember("select");

    std::string select_str;

    const Value& select = document["select"];
    json_to_select_str(select, select_str);

    {
        std::unique_lock<std::mutex> u_lk(goStatementMutex);
        result = m_s.deleteWorkload(workload->value.GetString(),
                                 //select->value.GetString());
                                 select_str);
    }
    make_response(response, resultstr, result); 
    request.reply(response);
}
