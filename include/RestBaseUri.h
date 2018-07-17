#pragma once

#include <iostream>
#include <cpprest/http_listener.h>
#include <cpprest/details/http_helpers.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <ivy_engine.h>

using namespace web;
using namespace http;
using namespace utility;
using namespace http::experimental::listener;
using namespace web::http::details;

class RestBaseUri
{
public:
    virtual void handle_post(http_request request) = 0;
    virtual void handle_get(http_request request) = 0;
    virtual void handle_put(http_request request) = 0;
    virtual void handle_patch(http_request request) = 0;
    virtual void handle_delete(http_request request) = 0;

    virtual std::string get_schema_validation_error(rapidjson::SchemaValidator *validator)
    {
        std::ostringstream o;
        rapidjson::StringBuffer sb;
         (*validator).GetInvalidSchemaPointer().StringifyUriFragment(sb);
         o << "Invalid schema: " << sb.GetString() << std::endl ;
         o << "Invalid keyword: " << (*validator).GetInvalidSchemaKeyword() << std::endl;
         sb.Clear();
         (*validator).GetInvalidDocumentPointer().StringifyUriFragment(sb);
         o << "Invalid document: " << sb.GetString() << std::endl;
         return o.str();
    }

    virtual void make_response(http_response &response, std::string &resultstr, std::pair<bool, std::string>&  result)
    {
        // Response JSON
        rapidjson::StringBuffer JSONStrBuffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(JSONStrBuffer);
        std::string rslt;

        rslt += (result.first ? "Successful, " : "Failed, ");
        rslt += result.second;

        writer.StartObject();
        writer.Key("retval"); writer.Bool(result.first);
        writer.Key("status"); writer.String(resultstr.c_str());
        writer.Key("result"); writer.String(rslt.c_str());
        writer.EndObject();

        response.headers().add(header_names::content_type, mime_types::application_json);
        response.headers().add(header_names::content_type, charset_types::utf8);
        response.set_body(JSONStrBuffer.GetString());
    }

    virtual void json_to_host_list(const rapidjson::Value& hosts, std::string& hosts_list)
    {
        if (hosts.IsString())
        {
            // opaque string passthrough
            hosts_list = hosts.GetString();
            return;
        }
        for (rapidjson::Value::ConstValueIterator itr = hosts.Begin(); itr != hosts.End(); ++itr)
        {
            if (itr != hosts.Begin())
                hosts_list += ", ";
            hosts_list += itr->GetString();
        }
    }

    virtual void json_to_select_str(const rapidjson::Value& select, std::string& select_str)
    {
        if (select.IsString())
        {
            // opaque string passthrough
            select_str = select.GetString();
            return;
        }
        for (rapidjson::SizeType i = 0; i < select.Size(); ++i)
        {
            const rapidjson::Value& elem = select[i];

            for (rapidjson::Value::ConstMemberIterator itr = elem.MemberBegin(); itr != elem.MemberEnd(); ++itr)
            {
                select_str += itr->name.GetString();
                select_str += " : ";
                if (itr->value.IsArray())
                {
                    select_str += "[ ";
                    const rapidjson::Value& val_elem = itr->value;
                    for (rapidjson::SizeType i = 0; i < val_elem.Size(); ++i)
                    {
                       if (i > 0 && i < val_elem.Size())
                           select_str += ", ";
                       select_str += val_elem[i].GetString();
                    }
                    select_str += " ]";
                } else
                    select_str += itr->value.GetString();

            }
            if (i < (select.Size() - 1))
                select_str += ", ";
        }
    }

    virtual bool is_session_owner(http_request& request)
    {
        std::string cookie = request.headers()["Cookie"];

        if (_session_state && (cookie != "session_cookie=" +  _active_session_token))
        {
            return false;
        }

        return true;
    }

    virtual void send_busy_response(http_request& request)
    {
        std::string resultstr = "notSessionOwner";
        std::pair<bool, std::string> result = std::make_pair(false, std::string("Another session is running."));

        http_response response(status_codes::Locked);
        make_response(response, resultstr, result);
        request.reply(response);
        return;
    }

    static void initialize_ivy_schema();
    static rapidjson::SchemaDocument* _schema;
    static bool _schema_initialized;
    static std::mutex goStatementMutex;
    static std::map<std::string, ivy_engine*> _ivy_engine_instances;

    // session management
    static std::string _active_session_token;
    static bool _session_state;
    static std::mutex _session_mutex;
};