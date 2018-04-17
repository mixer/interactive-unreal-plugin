#pragma once

#include <string>
#include <memory>
#include <map>

namespace mixer_internal
{

struct http_response
{
	unsigned int statusCode;
	std::string body;
};

class http_client
{
public:
	virtual ~http_client() = 0 {};

	virtual int make_request(const std::string& uri, const std::string& requestType, const std::map<std::string, std::string>* headers, const std::string& body, _Out_ http_response& response, unsigned long timeoutMs = 5000) const = 0;
};

class http_factory
{
public:
	static std::unique_ptr<http_client> make_http_client();
};

}