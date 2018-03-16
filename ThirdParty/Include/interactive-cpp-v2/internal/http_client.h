#pragma once

#include <string>
#include <memory>

namespace mixer
{

struct http_response
{
	int statusCode;
	std::string body;
};

class http_client
{
public:
	virtual ~http_client() = 0 {};

	virtual int make_request(const std::string& uri, const std::string& requestType, const std::string& headers, const std::string& body, _Out_ http_response& response) const = 0;
};

class http_factory
{
public:
	static std::unique_ptr<http_client> make_http_client();
};

}