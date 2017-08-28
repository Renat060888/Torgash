
// std
#include <sys/time.h>
#include <iostream>
// 3rd party
#include "curl/curl.h"
// project
//#include "utils/log.h"
#include "network.h"

using namespace std;

extern void g_logger( const string _msg );

/**
 * @brief writer        - функция обратного вызова. вызывается, когда данные по конкретной операции получены
 */
static size_t writer(char *fromWebServer, size_t blockSize, size_t blockCount, void *localContainer){

    std::string  & result = *( std::string  * )localContainer;

    int offset = result.size();

    result.resize( offset + blockCount );

    // считывание по блокам

    for ( size_t blockNumber = 0 ; blockNumber < blockCount ; blockNumber++ ){

        result[ offset + blockNumber ] = fromWebServer[ blockNumber ];
    }

    return blockSize * blockCount;
}

Network::Network(){    

    setlocale( LC_NUMERIC, "C" );

    m_curl = curl_easy_init();

    if( ! m_curl){
       //logger(ERR) << "couldn't init CURL";
       throw;
    }

    //функция обратного вызова
    curl_easy_setopt(m_curl,CURLOPT_WRITEFUNCTION,writer);
    curl_easy_setopt(m_curl,CURLOPT_NOSIGNAL,1); // на Астре CURL кидает сигнал
    //установка указателя контекста (в функции обратного вызова это последний аргумент)
    curl_easy_setopt(m_curl,CURLOPT_WRITEDATA,(void*)&m_resultStr);//массив символов полученных в результате http-запроса
    // or whatever charset your XML is really using...
    m_slist = curl_slist_append( NULL, "Content-Type: text/xml; charset=utf-8" );
}

Network::~Network(){

    curl_slist_free_all( m_slist );
    curl_easy_cleanup( m_curl );
}

/*
 * GET
 */
bool Network::sendRequest(const std::string &_url){

    m_resultStr.clear();

    char error[CURL_ERROR_SIZE] = "";
    long ferror = 0;

    //url-строка, посылаемая на сервер
    curl_easy_setopt(m_curl,CURLOPT_URL,_url.c_str());
    curl_easy_setopt(m_curl,CURLOPT_ERRORBUFFER,&error);
    curl_easy_setopt(m_curl,CURLOPT_FAILONERROR,&ferror);
    curl_easy_setopt( m_curl,CURLOPT_HTTPGET, 1L ); // после POST запроса Curl нужно обратно возвращать в GET режим

    CURLcode curlCodeResult = curl_easy_perform(m_curl);

    long httpCode = 0;

    curl_easy_getinfo( m_curl, CURLINFO_RESPONSE_CODE, httpCode );

    if( curlCodeResult != CURLE_OK ){

        cout << "Curl HTTP-Code = " << httpCode << endl;
        cout <<  "curl - " << curl_easy_strerror(curlCodeResult) << endl;
        cout << " ferror - " << error << " % " << ferror << endl;;
        return false;
    }

    return true;
}

/*
 * GET
 */
bool Network::sendHTTPSRequest( const std::string &_url ){

    m_resultStr.clear();

    char error[CURL_ERROR_SIZE] = "";
    long ferror = 0;

    //url-строка, посылаемая на сервер
    curl_easy_setopt(m_curl, CURLOPT_URL,_url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_ERRORBUFFER,&error);
    curl_easy_setopt(m_curl, CURLOPT_FAILONERROR,&ferror);
    curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1L ); // после POST запроса Curl нужно обратно возвращать в GET режим
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0L );
//    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 1L ); // TODO

    CURLcode curlCodeResult = curl_easy_perform(m_curl);

    long httpCode = 0;

    curl_easy_getinfo( m_curl, CURLINFO_RESPONSE_CODE, httpCode );

    if( curlCodeResult != CURLE_OK ){

        g_logger( string("Curl HTTP-Code: ") + to_string(httpCode) );
        g_logger( string("curl: ") + curl_easy_strerror(curlCodeResult) );
        g_logger( string("ferror: ") + error + " % " + to_string(ferror) );
        g_logger( string("failed url: ") + _url );
        return false;
    }

    return true;
}

/*
 * POST
 */
bool Network::sendRequest( const std::string & _url, const std::string & _data ){

    m_resultStr.clear();

    char error[CURL_ERROR_SIZE] = "";
    long ferror = 0;

    //url-строка, посылаемая на сервер
    curl_easy_setopt( m_curl, CURLOPT_URL, _url.c_str() );
    curl_easy_setopt( m_curl, CURLOPT_ERRORBUFFER, & error );
    curl_easy_setopt( m_curl, CURLOPT_FAILONERROR, & ferror );
    curl_easy_setopt( m_curl, CURLOPT_POST, 1L );
    curl_easy_setopt( m_curl, CURLOPT_POSTFIELDS, _data.c_str() );
    curl_easy_setopt( m_curl, CURLOPT_POSTFIELDSIZE, _data.length() );
    curl_easy_setopt( m_curl, CURLOPT_HTTPHEADER, m_slist );

    CURLcode curlCodeResult = curl_easy_perform(m_curl);

    long httpCode = 0;

    curl_easy_getinfo( m_curl, CURLINFO_RESPONSE_CODE, httpCode );

    if( curlCodeResult != CURLE_OK ){

        //logger(ERR) << "Curl HTTP-Code = " << httpCode;
        //logger(ERR) <<  "curl - " << curl_easy_strerror(curlCodeResult);
        //logger(ERR) << " ferror - " << error << " % " << ferror;
        return false;
    }

    return true;
}

bool Network::checkServerAlive( const std::string & _url ){

    // TODO
    m_resultStr.clear();

    char errorBuffer[CURL_ERROR_SIZE] = "";
    long failOnError = 0;
    long httpCode = 0;

    //url-строка, посылаемая на сервер
    curl_easy_setopt(m_curl,CURLOPT_URL,_url.c_str());
    curl_easy_setopt(m_curl,CURLOPT_ERRORBUFFER, & errorBuffer );
    curl_easy_setopt(m_curl,CURLOPT_FAILONERROR, & failOnError );

    CURLcode curlCodeResult = curl_easy_perform(m_curl);

    curl_easy_getinfo( m_curl, CURLINFO_RESPONSE_CODE, httpCode );

    if( CURLE_OK == curlCodeResult /*&& 200 == httpCode*/ ){

        return true;
    }
    else{
        //logger(ERR) << "CURL Check Server Status FAIL: " << httpCode << " ( " << _url << " )";
        //logger(ERR) << "HTTP-code: " << httpCode;
        //logger(ERR) << "curl code: " << curl_easy_strerror(curlCodeResult);
        //logger(ERR) << "error buffer: " << errorBuffer;
        //logger(ERR) << "fail on error: " << failOnError;
        return false;
    }
}

string * Network::receiveAnswer(){

    return & m_resultStr;
}

uint64_t Network::getResponseSize(){

    return m_resultStr.size();
}


