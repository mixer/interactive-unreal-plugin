#pragma once

#include "http_client.h"

namespace mixer_internal
{

class winapp_http_client : public http_client
{
public:
	winapp_http_client();
	~winapp_http_client();
	
	int make_request(const std::string& uri, const std::string& requestType, const std::map<std::string, std::string>* headers, const std::string& body, http_response& response, unsigned long timeoutMs = 5000) const;
};
}