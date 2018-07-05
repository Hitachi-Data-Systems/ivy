#pragma once

#include "RestBaseUri.h"


class RestRollupsUri final : public RestBaseUri
{
public:
    RestRollupsUri(http_listener& listener) : _listener(listener) { };

    virtual void handle_post(http_request request) override;
    virtual void handle_get(http_request request) override;
    virtual void handle_put(http_request request) override;
    virtual void handle_patch(http_request request) override;
    virtual void handle_delete(http_request request) override;
private:
    http_listener& _listener;
};
