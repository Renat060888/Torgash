#pragma once

#include <vector>
#include <string>

class Network{

    typedef void CURL;

public:

    Network();
    ~Network();

    // GET
    bool sendRequest( const std::string & _url );
    // POST
    bool sendRequest( const std::string & _url, const std::string & _data );
    // HTTPS
    bool sendHTTPSRequest(const std::string &_url);
    // CHECK
    bool checkServerAlive( const std::string & _url );

    std::string * receiveAnswer();

    uint64_t getResponseSize();

private:

    CURL *              m_curl;

    std::string         m_resultStr;

    struct curl_slist * m_slist;
};


