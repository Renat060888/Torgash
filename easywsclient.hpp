
#ifndef EASYWSCLIENT_HPP_20120819_MIOFVASDTNUASZDQPLFD
#define EASYWSCLIENT_HPP_20120819_MIOFVASDTNUASZDQPLFD

// This code comes from:
// https://github.com/dhbaird/easywsclient
//
// To get the latest version:
// wget https://raw.github.com/dhbaird/easywsclient/master/easywsclient.hpp
// wget https://raw.github.com/dhbaird/easywsclient/master/easywsclient.cpp

#if 0
// Factory method to create a WebSocket:
static pointer from_url(std::string url);
// Factory method to create a dummy WebSocket (all operations are noop):
static pointer create_dummy();

// Function to perform actual network send()/recv() I/O:
// (note: if all you need is to recv()/dispatch() messages, then a
// negative timeout can be used to block until a message arrives.
// By default, when timeout is 0, poll() will not block at all.)
void poll(int timeout = 0); // timeout in milliseconds

// Receive a message, and pass it to callable(). Really, this just looks at
// a buffer (filled up by poll()) and decodes any messages in the buffer.
// Callable must have signature: void(const std::string & message).
// Should work with C functions, C++ functors, and C++11 std::function and
// lambda:
template<class Callable>
void dispatch(Callable callable);

// Sends a TEXT type message (gets put into a buffer for poll() to send
// later):
void send(std::string message);

// Close the WebSocket (send a CLOSE message over WebSocket, then close() the
// actual socket when the send buffer becomes empty):
void close();
#endif

#include <string>
#include <vector>

namespace easywsclient {

struct Callback_Imp { virtual void operator()(const std::string& message) = 0; };
struct BytesCallback_Imp { virtual void operator()(const std::vector<uint8_t>& message) = 0; };

class WebSocket {
  public:
    typedef WebSocket * pointer;
    typedef enum readyStateValues { CLOSING, CLOSED, CONNECTING, OPEN } readyStateValues;

    // Factories:
    static pointer create_dummy();
    static pointer from_url(const std::string& url, const std::string& origin = std::string());
    static pointer from_url_no_mask(const std::string& url, const std::string& origin = std::string());

    // Interfaces:
    virtual ~WebSocket() { }
    virtual void poll(int timeout = 0) = 0; // timeout in milliseconds
    virtual void send(const std::string& message) = 0;
    virtual void sendBinary(const std::string& message) = 0;
    virtual void sendBinary(const std::vector<uint8_t>& message) = 0;
    virtual void sendPing() = 0;
    virtual void close() = 0;
    virtual readyStateValues getReadyState() const = 0;

    template<class Callable>
    void dispatch(Callable callable)
        // For callbacks that accept a string argument.
    { // N.B. this is compatible with both C++11 lambdas, functors and C function pointers
        struct _Callback : public Callback_Imp {
            Callable& callable;
            _Callback(Callable& callable) : callable(callable) { }
            void operator()(const std::string& message) { callable(message); }
        };
        _Callback callback(callable);
        _dispatch(callback);
    }

    template<class Callable>
    void dispatchBinary(Callable callable)
        // For callbacks that accept a std::vector<uint8_t> argument.
    { // N.B. this is compatible with both C++11 lambdas, functors and C function pointers
        struct _Callback : public BytesCallback_Imp {
            Callable& callable;
            _Callback(Callable& callable) : callable(callable) { }
            void operator()(const std::vector<uint8_t>& message) { callable(message); }
        };
        _Callback callback(callable);
        _dispatchBinary(callback);
    }

  protected:
    virtual void _dispatch(Callback_Imp& callable) = 0;
    virtual void _dispatchBinary(BytesCallback_Imp& callable) = 0;
};

} // namespace easywsclient

#endif /* EASYWSCLIENT_HPP_20120819_MIOFVASDTNUASZDQPLFD */
