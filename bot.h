#pragma once

// std
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
// project
#include "types_common.h"

namespace easywsclient{
    class WebSocket;
}

enum class TradeBotState_en {

    START,
    STOP,

    UNDEFINED
};

class Bot{

public:

    Bot();
    ~Bot();

    // init
    bool init();

    // control
    void startTrades( Profile_t * _currentProfile );
    void stopTrades();

    // profile
    Profile_t *                     m_currentProfile;

private:

    // process
    void run();
    bool enablePersonalNotifications();
    void sendMessageToWebSocket( const std::string & _msg, std::string & _resp );
    bool makeHttpsRequest( const std::string & _req, std::string * & _response );

    // threads
    void mainLoopThreadFunc();
    void pingPongThreadFunc();
    void websocketNoticesStreamThreadFunc();
    void websocketNewitemsStreamThreadFunc();

    // trade
    bool needMakeOrder( Item_t * _item );
    bool getItemInfo( Item_t * _item );
    bool createItemOrder( Item_t * _item );
    bool updateItemOrder( Item_t * _item );
    bool buyItem( Item_t * _item );
    bool checkTrades();
    bool makeItemRequest( Item_t * _item  );
    bool deleteAllOrders();

    // utils


    class ThreadPool *              m_threadPool;

    // network
    class easywsclient::WebSocket * m_websocketClient;
    class easywsclient::WebSocket * m_websockForNewitemsStream;
    class Network *                 m_httpsClient;


    // state
    TradeBotState_en                m_botState;

    // threads
    std::thread *                   m_mainLoopThread;
    std::thread *                   m_pingPongThread;
    std::thread *                   m_websocketNoticesStreamThread;
    std::thread *                   m_websocketNewitemsStreamThread;

    // synch
    std::condition_variable         m_cvForPingPongThread;
    std::mutex                      m_mutexForWebSocket;
    std::atomic_bool                m_programShutdown;
    std::atomic_bool                m_tradesRun;
};











