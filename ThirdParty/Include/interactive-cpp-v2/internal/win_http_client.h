#pragma once

#include "http_client.h"
#include <map>

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

	int make_request(const std::string& uri, const std::string& requestType, const std::string& headers, const std::string& body, http_response& response) const;

private:
	hinternet_ptr m_internet;
	mutable std::map<std::string, hinternet_ptr> m_sessionsByHostname;
};
}