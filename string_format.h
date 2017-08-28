#pragma once

#include <string>
#include <vector>
#include <sstream>

template<typename T>
std::string to_string( const T & _arg ){

    std::ostringstream ss;
    ss << _arg;
    return ss.str();
}

static std::string vtformat_impl( const std::string & _fmt, const std::vector<std::string> & _strs ){

    static const char FORMAT_SYMBOL = '%';
    std::string res;
    std::string buf;
    bool arg = false;

    for( int i = 0; i <= static_cast<int>(_fmt.size()); ++i ){

        bool last = ( i == static_cast<int>(_fmt.size()) );
        char ch = _fmt[i];
        if( arg ){

            if( ch >= '0' && ch <= '9' ){
                buf += ch;
            }
            else{

                int num = 0;
                if( ! buf.empty() && buf.length() < 10 )
                    num = atoi(buf.c_str());
                if( num >= 1 && num <= static_cast<int>(_strs.size()) )
                    res += _strs[num - 1];
                else
                    res += FORMAT_SYMBOL + buf;
                buf.clear();
                if( ch != FORMAT_SYMBOL ){

                    if( ! last )
                        res += ch;
                    arg = false;
                }
            }
        }
        else{

            if( ch == FORMAT_SYMBOL ){
                arg = true;
            }
            else{
                if( ! last )
                    res += ch;
            }
        }
    }
    return res;
}

template<typename Arg, typename ...Args>
std::string vtformat_impl( const std::string & _fmt, std::vector<std::string> & _strs, Arg _arg, Args... _args ){

    _strs.push_back( to_string(_arg) );
    return vtformat_impl( _fmt, _strs, _args... );
}

template<typename ... Args>
std::string vtformat( const std::string & _fmt, Args ... _args ){

    std::vector<std::string> strs;
    return vtformat_impl( _fmt, strs, _args... );
}
