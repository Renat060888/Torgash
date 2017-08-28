#pragma once

// std
#include <chrono>
#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <vector>

//=====================
// deletePointersInVector
//=====================
template < typename T >
inline void deletePointersInVector( std::vector< T > & _vector ){

    for( T & pointer : _vector )
        delete pointer;
}

//=====================
// unescape
//=====================
inline void unescape( const char * _escaped, const int _strLen, std::string & _unescaped ){

    _unescaped.resize( _strLen );

    int unescapedIdx = 0;

    for( int i = 0; i < _strLen; i++ ){

        if( '\\' == _escaped[ i ] ){

            switch( _escaped[ ++i ] ){

            case '\\' : _unescaped[ unescapedIdx++ ] = '\\'; break;
            case '\"' : _unescaped[ unescapedIdx++ ] = '\"'; break;
            }
        }
        else{
            _unescaped[ unescapedIdx++ ] = _escaped[ i ];
        }
    }

    _unescaped.shrink_to_fit();
}

//=====================
// isNumber
//=====================
inline bool isNumber( const std::string & _msg ){

    for( int i = 0; i < _msg.size(); i++ ){

        if( ! std::isdigit( _msg[ i ]) ){
            std::cerr << "it's not a number: " << _msg << std::endl;
            return false;
        }
    }

    return true;
}

//=====================
// printCurrentTime
//=====================
inline std::string printCurrentTime(){

    time_t timev;
    time( & timev );
    tm * timeinfo = localtime( & timev );

    std::stringstream time;
    time << "current time: " << timeinfo->tm_hour << "h " << timeinfo->tm_min << "m " << timeinfo->tm_sec << "s";

    return time.str();
}

//=====================
// getCurrentTime (milliseconds)
//=====================
inline long getCurrentTimeMilliSec(){

    using namespace std::chrono;

    auto currentTime = high_resolution_clock::now().time_since_epoch();

    return duration_cast<milliseconds>(currentTime).count();
}

//=====================
// getCurrentTime (seconds)
//=====================
inline long getCurrentTimeSec(){

    using namespace std::chrono;

    auto currentTime = high_resolution_clock::now().time_since_epoch();

    return duration_cast<seconds>(currentTime).count();
}

//=====================
// strToWstr
//=====================
inline std::wstring strToWstr(const std::string _str){

    return std::wstring( _str.begin(), _str.end() );
}

//=====================
// wstrToStr
//=====================
inline std::string wstrToStr(const std::wstring & _wstr){

    char buffer[ _wstr.size() * 2 ];

    int rt = wcstombs( buffer, _wstr.data(), sizeof(buffer) );

    return buffer;
}

//=====================
// shutdownThread
//=====================
inline bool shutdownThread( std::thread * _thread ){

    if( _thread ){
        if( _thread->joinable() ){
            _thread->join();
            delete _thread;
            _thread = nullptr;
            return true;
        }
        else{
            return false;
        }
    }
    else{
        return false;
    }
}

//=====================
// Variant
//=====================
class Variant{

public:
    Variant( const int _digit ) :
        i(_digit),
        s(std::to_string(_digit)),
        c(s.c_str()) {
    }
    Variant( const char * _cstr ) :
        i(atoi(_cstr)),
        s(_cstr),
        c(_cstr) {
    }
    Variant( const std::string _str ) :
        i(atoi(_str.c_str())),
        s(_str),
        c(_str.c_str()) {
    }
    ~Variant(){
    }

    const int i;
    const std::string s;
    const char * c;
};











