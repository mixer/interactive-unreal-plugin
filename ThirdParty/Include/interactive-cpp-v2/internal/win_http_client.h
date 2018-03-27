#pragma once

#include "http_client.h"
#include <map>
#include <string>

namespace mixer
{

struct hinternet_deleter
{
	void operator()(void* internet);
};

typedef std::unique_ptr<void, hinternet_deleter> hinternet_ptr;

class win_http_client : public http_client
{
public:
	win_http_client();
	~win_http_client();

    int make_request(const std::string& uri, const std::string& requestType, const std::map<std::string, std::string>* headers, const std::string& body, _Out_ http_response& response, unsigned long timeoutMs = 5000) const;

private:
	hinternet_ptr m_internet;
	mutable std::map<std::string, hinternet_ptr> m_sessionsByHostname;
};
}