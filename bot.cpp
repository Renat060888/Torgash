
// std
#include <iostream>
#include <algorithm>
// 3rd party
#include "easywsclient.hpp"
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/istreamwrapper.h"
// project
#include "websocketstreamtask.h"
#include "threadpool.h"
#include "string_format.h"
#include "bot.h"
#include "network.h"
#include "utils.h"
#include "tokens.h"

using namespace std;
using namespace easywsclient;
using namespace rapidjson;

// websocket    - ws://wsn.dota2.net/wsn/
// https        - https://market.dota2.net/api/

static const int MAX_PRICE_RUN_AHEAD = 2;

extern void g_logger( const string _msg );

Bot::Bot() :
    m_threadPool(nullptr),
    m_websocketClient(nullptr),
    m_websockForNewitemsStream(nullptr),
    m_httpsClient(nullptr),
    m_currentProfile(nullptr),
    m_botState(TradeBotState_en::UNDEFINED),
    m_mainLoopThread(nullptr),
    m_pingPongThread(nullptr),
    m_websocketNoticesStreamThread(nullptr),
    m_websocketNewitemsStreamThread(nullptr),
    m_programShutdown(false),
    m_tradesRun(false){

}

Bot::~Bot(){

    // delete orders if exists
    deleteAllOrders();

    // kill main thread
    m_programShutdown.store( true );
    m_cvForPingPongThread.notify_one();

    this_thread::sleep_for( chrono::milliseconds(1) );

    shutdownThread( m_mainLoopThread );

    // kill another threads
    m_tradesRun.store( true );

    this_thread::sleep_for( chrono::milliseconds(1) );

    shutdownThread( m_websocketNoticesStreamThread );
    shutdownThread( m_websocketNewitemsStreamThread );
    shutdownThread( m_pingPongThread );

    // crutch !
    if( m_websocketClient )
        m_websocketClient->close();
    if( m_websockForNewitemsStream )
        m_websockForNewitemsStream->close();

    delete m_threadPool;
    m_threadPool = nullptr;

    delete m_websocketClient;
    m_websocketClient = nullptr;

    delete m_websockForNewitemsStream;
    m_websockForNewitemsStream = nullptr;

    delete m_httpsClient;
    m_httpsClient = nullptr;
}

bool Bot::init(){

    // init network
    m_httpsClient = new Network();

    // for websocket
    const int threadsNum = 4;
    m_threadPool = new ThreadPool( threadsNum );

    // threads
    m_mainLoopThread = new thread( & Bot::mainLoopThreadFunc, this );
    m_pingPongThread = new thread( & Bot::pingPongThreadFunc, this );
    m_websocketNoticesStreamThread = new thread( & Bot::websocketNoticesStreamThreadFunc, this );
    m_websocketNewitemsStreamThread = new thread( & Bot::websocketNewitemsStreamThreadFunc, this );

    return true;
}

void Bot::startTrades( Profile_t * _newProfile ){

    // init profile
    m_tradesRun.store( false );
    this_thread::sleep_for( chrono::milliseconds(5) );

    m_currentProfile = _newProfile;

    // close all websockets
    if( m_websocketClient ){
        m_websocketClient->close();
    }

    if( m_websockForNewitemsStream ){
        m_websockForNewitemsStream->close();
    }

    // create again
    m_websocketClient = WebSocket::from_url( m_currentProfile->websocketAddress );

    if( ! m_websocketClient ){
        g_logger( "ERROR: websocket client create fail, websocket address: " + m_currentProfile->websocketAddress );
        return;
    }

    enablePersonalNotifications();

    m_websockForNewitemsStream = WebSocket::from_url( m_currentProfile->websocketAddress );

    if( ! m_websockForNewitemsStream ){
        g_logger( "ERROR: websocket for newitems stream create fail, websocket address: " + m_currentProfile->websocketAddress );
        return;
    }

    m_websockForNewitemsStream->send( WS_NEWITEMS_CS );
//    m_websockForNewitemsStream->send( WS_HISTORY_CS );

    m_tradesRun.store( true );

    // run
    m_botState = TradeBotState_en::START;
}

void Bot::stopTrades(){

    m_botState = TradeBotState_en::STOP;
    m_tradesRun.store( false );
}

void Bot::mainLoopThreadFunc(){

    while( ! m_programShutdown.load() ){

        if( TradeBotState_en::START == m_botState ){
            run();
        }

        this_thread::sleep_for( chrono::milliseconds(5) );
    }
}

void Bot::run(){

    g_logger( ">> begin trades <<" );

    unsigned long currentIndex = 0; // чтобы по истечение секунды на обработку шел ровно тот товар, который был после последнего успешного
    const int itemsCount = m_currentProfile->items.size();

    while( TradeBotState_en::START == m_botState ){

        // TODO
        if( 0 == itemsCount ){
            continue;
        }

        checkTrades();

        Item_t * item = m_currentProfile->items[ currentIndex % itemsCount ];

        switch( item->processType ){
        case ItemProcessType_en::BUY_HTTPS : {

            // если товар уже 1) куплен 2) запрос к Стим отправлен - то не трогать
            if( item->requestedToSteam ){

                currentIndex++;
                break;
            }

            // если товар куплен, нужно запросить передачу стиму
            if( item->botID != 0 && item->itemStatus == 4 ){

                if( makeItemRequest( item ) ){

                    currentIndex++;
                }
                break;
            }

            // попытка купить
            if( (item->buysPerformed.load() < item->purchasesNumber) && item->active && getItemInfo(item) ){

                currentIndex++;

                if( (item->minPriceOnMarket <= item->maxPriceForBuy) ){

                    buyItem( item );
                }
            }
            break;
        }
        case ItemProcessType_en::RACE : {

            if( item->active && getItemInfo(item) ){
                currentIndex++;

                if( needMakeOrder(item) ){
                    if( item->buyOrderCreated ){

                        updateItemOrder( item );
                    }
                    else{
                        createItemOrder( item );
                    }
                }
            }
            break;
        }
        case ItemProcessType_en::BUY_WEBSOCKET : {

            // если товар уже 1) куплен 2) запрос к Стим отправлен - то не трогать
            if( item->requestedToSteam ){
                currentIndex++;
                break;
            }

            // если товар куплен, нужно запросить передачу стиму
            if( item->botID != 0 && item->itemStatus == 4 ){

                if( makeItemRequest( item ) ){

                    currentIndex++;
                }
                break;
            }

            // обрабатываются в потоках, парсящих ответ с ws_newitems
            if( item->catchedViaWebsocket && item->buysPerformed.load() < item->purchasesNumber ){
                currentIndex++;
                buyItem( item );
            }

            break;
        }
        default : {}
        }

        this_thread::sleep_for( chrono::milliseconds(5) );
    }

    g_logger( ">> stop trades <<" );
}

void Bot::websocketNoticesStreamThreadFunc(){

    /*
     * Типы уведомлений, которые могут прийти:
     * additem_cs       - добавление предмета на странице "Мои вещи".
     * itemout_new_cs   - Исчезание предмета на странице "Мои вещи".
     * itemstatus_cs    - Изменение статуса предмета на странице "Мои вещи".
     * money            - Изменение баланса пользователя.
     * webnotify        - Получение уведомлений от администрации, доступности предмета для вывода, о покупках вещей.
     */
    bool responseEvent = false;
    string response;

    g_logger( "entry websocket notices stream thread func" );

    while( ! m_programShutdown.load() ){

        while( ! m_tradesRun.load() ){
            this_thread::sleep_for( chrono::milliseconds(5) );
        }
        // crutch
        if( m_programShutdown.load( )){
            return;
        }

        {
            lock_guard<mutex> lock( m_mutexForWebSocket );

            m_websocketClient->poll();

            m_websocketClient->dispatch(
                                        // lambda for callback
                                        [ & ]( const string & _fromServer ){
                                            response = _fromServer;
                                            responseEvent = true;
                                        });
        }

        if( responseEvent ){

            g_logger( "from websocket notices stream THREAD: " + response );

            WebsocketStreamTask * task = new WebsocketStreamTask( 0, m_currentProfile->items );
            task->m_responesFromWebsocket.push_back( response );
            m_threadPool->enqueue( task );

            responseEvent = false;
        }

        this_thread::sleep_for( chrono::milliseconds(5) );
    }

    g_logger( "quit websocket notices stream thread func" );
}

void Bot::websocketNewitemsStreamThreadFunc(){

    bool responseEvent = false;

    // stream parsing
    const int packagesPerTask = 10;
    vector<string> responesFromWebsocket;
    responesFromWebsocket.resize( packagesPerTask );
    int counter = 0;

    // average json objects in 1 second = 20-30

    g_logger( "entry websocket newitems stream thread func" );

    while( ! m_programShutdown.load() ){

        while( ! m_tradesRun.load() ){
            this_thread::sleep_for( chrono::milliseconds(5) );
        }
        // crutch
        if( m_programShutdown.load( )){
            return;
        }

        m_websockForNewitemsStream->poll();

        m_websockForNewitemsStream->dispatch(
                                    // lambda for callback
                                    [ & ]( const string & _fromServer ){
                                        responesFromWebsocket[ counter ] = _fromServer;
                                        responseEvent = true;
                                    });


        if( responseEvent ){

//            g_logger( "from websocket newitems stream THREAD: " + responesFromWebsocket[ counter ] );

            responseEvent = false;
            counter++;
        }

        if( counter == packagesPerTask ){

            WebsocketStreamTask * task = new WebsocketStreamTask( packagesPerTask, m_currentProfile->items );
            task->m_responesFromWebsocket.swap( responesFromWebsocket );            
            m_threadPool->enqueue( task );
            counter = 0;
        }

        this_thread::sleep_for( chrono::milliseconds(5) );
    }    

    g_logger( "quit websocket newitems stream thread func" );
}

/*
 * ВАЖНО! Если вы отправите больше 5 запросов (https) в секунду к нашему серверу, то ваш ключ будет удален.
 *
 * ВНИМАНИЕ! Для нахождения в онлайне и продажи вещей необходимо отправлять команду PING раз минуту,
 * предварительно единожды отправив запрос к API методом PINGPONG.
 *
 * Если мы подписались на персональные оповещения используя свой ключ - больше ничего делать не надо, просто слушаем входящие пакеты от нашего сервера
 * и раз в 40-50 секунд отправляем «ping» на наш сервер. Все персональные уведомления (баланс, уведомления и прочее) будут приходить автоматический.
 */
void Bot::pingPongThreadFunc(){

    const int pingIntervalSec   = 45;
    const string pingMessage    = "ping";
    const int timeoutToWait     = -10;

    mutex mutexForSleep;

    g_logger( "entry ping pong thread func" );

    while( ! m_programShutdown.load() ){

        while( ! m_tradesRun.load() ){
            this_thread::sleep_for( chrono::milliseconds(5) );
        }
        // crutch
        if( m_programShutdown.load( )){
            return;
        }

        {
            lock_guard<mutex> lk( m_mutexForWebSocket );

            m_websocketClient->send( pingMessage );

            m_websocketClient->poll( timeoutToWait );

            g_logger( "from ping-pong THREAD send ping..." + printCurrentTime() );

            m_websocketClient->dispatch(
                                // lambda for callback
                                []( const string & _fromServer ){

                                    g_logger( "from ping-pong THREAD receive: " + _fromServer );
                                });
        }

        unique_lock<mutex> lk( mutexForSleep );
        m_cvForPingPongThread.wait_for( lk, chrono::seconds( pingIntervalSec ), [&](){return (m_programShutdown.load() == true);} );
    }

    // 2. Init through "pingpong" // TODO непонятно как работает
    // TODO сделать https клиент потокобезопасным
#if 0
    string request = HTTPS_ADDRESS;
    request += HTTPS_PING_PONG;
    request += SECRET_KEY;

    string * response;
    if( ! _inst->makeHttpsRequest(request,response) ){
        return;
    }

    logger() << "ping pong request response: " << * response;
#endif

    g_logger( "quit ping pong thread func" );
}

bool Bot::enablePersonalNotifications(){

    // 1.
    // Получить временный ключ-токен для авторизации на веб-сокет сервере
    // ВАЖНО! Ключ действует ограничено время и через 60 секунд перестает приниматься сервером (послать нужно сразу после получения)
    // При ошибке авторизации на сервере уведомлений сервер пришлет строку: «auth»
    string request = m_currentProfile->httpsAddress;
    request += HTTPS_GET_WS_AUTH;
    request += m_currentProfile->secretKey;

    string * response;
    if( ! makeHttpsRequest(request,response) ){
        return false;
    }

    Document jsonMessage;
    if( jsonMessage.ParseInsitu<0>( & (*response).front() ).HasParseError() ){

        g_logger( "json parse error, message: " + * response );
        return false;
    }

    std::string tempWebsocketKeyToken;
    if( jsonMessage.HasMember( SUCCESS ) && jsonMessage.HasMember( WS_AUTH ) ){

        if( jsonMessage[ SUCCESS ].GetBool() ){

            tempWebsocketKeyToken = jsonMessage[ WS_AUTH ].GetString();
        }
    }
    else{
        g_logger( "something wrong in response (note: modifyed by RapidJSON): " + * response + " %% raised by request: " + request );
        return false;
    }

    // 2. Авторизация на веб сокет
    string webSocketResponse;
    sendMessageToWebSocket( tempWebsocketKeyToken, webSocketResponse );

    // TODO
//    g_logger( "ws auth response: " + webSocketResponse );

    return true;
}

void Bot::sendMessageToWebSocket( const string & _msg, string & _resp ){

    lock_guard<mutex> lk( m_mutexForWebSocket );

    m_websocketClient->send( _msg );

    m_websocketClient->poll();

    m_websocketClient->dispatch(
                // lambda for callback
                [ & _resp ]( const string & _fromServer ){

                    _resp += _fromServer;
                });
}

// --------------------------------------------------------------------------------------------------------
// HTTPS
// --------------------------------------------------------------------------------------------------------

bool Bot::makeHttpsRequest( const string & _req, string * & _response ){

    /*
     * ВАЖНО! Если вы отправите больше 5 запросов в секунду к нашему серверу, то ваш ключ будет удален.
     *
     * Пожалуйста, используйте возможные вебсокеты для получения информации с сайта. У нас стоит ограничение на обычные http запросы - 3 в секунду.
     * Используя вебсокеты, вы будете получать всю нужную информацию для покупки вещей быстрее,
     * чем если вы будете ддосить наш сервер обычными http запросами.
     *
     * ВАЖНО! Чтобы понять Кракозябные ошибки стима, достаточно сделать у себя json_decode или
     * воспользоваться http://rishida.net/tools/conversion/ unicode to utf конвертером.
     */

    // average http request time = 190 millisecond

#define ONE_SECOND_MILLISEC     1001 // +1 погрешность, чтоб наверняка
#define MAX_REQUEST_AT_SECOND   3 // TODO 5

    const long currentTimeMillisec = getCurrentTimeMilliSec();

    static int currentRequestsCount         = 0;
    static long lastRequestTimeMillisec     = currentTimeMillisec;

    // со времени последнего запроса прошло менее секунды
    if( (currentTimeMillisec - lastRequestTimeMillisec) <= ONE_SECOND_MILLISEC ){

        if( currentRequestsCount != MAX_REQUEST_AT_SECOND ){

            lastRequestTimeMillisec = currentTimeMillisec;

//            logger() << "make https request (<1), " << printCurrentTime();

            m_httpsClient->sendHTTPSRequest( _req );
            _response = m_httpsClient->receiveAnswer();

            currentRequestsCount++;

            return true;
        }
        else{
//            logger(WARN) << "exceeded " << MAX_REQUEST_AT_SECOND << " requests at 1 second. Request aborted";
//            logger(WARN) << "current requests: " << currentRequestsCount;
//            logger(WARN) << " %% current time in milliseconds: " << currentTimeMillisec;
//            logger(WARN) << " %% last request at time: " << lastRequestTimeMillisec;

            return false;
        }
    }
    // со времени последнего запроса прошло больше секунды
    else{

        lastRequestTimeMillisec = currentTimeMillisec;

//        logger() << "make https request (>1), " << printCurrentTime();

        m_httpsClient->sendHTTPSRequest( _req );
        _response = m_httpsClient->receiveAnswer();

        currentRequestsCount = 1;

        return true;
    }
}

bool Bot::needMakeOrder( Item_t * _item ){

    /*
     * - если кто то подогнал вместе с собой к цене, различающейся с 3м участником на N копеек, а потом слился
     * то вернуться к 3му участнику на +1 больше его
     */
    if( _item->priceBeforeMaxExist &&

        (_item->ourPrice - _item->priceBeforeMax) > MAX_PRICE_RUN_AHEAD &&

        (_item->ourPrice == _item->maxPriceOnMarket && 1 == _item->buyersCount) ){

        g_logger( "backward order approved: our price: " + to_string(_item->ourPrice) +
                  ", max price on market: " + to_string(_item->maxPriceOnMarket) +
                  ", buyers count: " + to_string(_item->buyersCount) +
                  ", price before max: " + to_string(_item->priceBeforeMax) +
                  ", stop price: " + to_string(_item->stopPrice) );

        _item->ourPrice = _item->priceBeforeMax + 1;
        return true;
    }

    /*
     * - обход последнего покупателя нужен только когда:
     * 1. этот товар видится впервые
     * 2. на его макс. цену подписался кто то еще
     * 3. кто то поставил большую цену
     */
    if( (_item->ourPrice == _item->maxPriceOnMarket && _item->buyersCount > 1) ||

        (_item->ourPrice < _item->maxPriceOnMarket) ||

        (0 == _item->ourPrice) ){

        if( _item->maxPriceOnMarket < _item->stopPrice ){

            g_logger( "forward order approved: our price: " + to_string(_item->ourPrice) +
                      ", max price on market: " + to_string(_item->maxPriceOnMarket) +
                      ", buyers count: " + to_string(_item->buyersCount) +
                      ", stop price: " + to_string(_item->stopPrice) );

            _item->ourPrice = _item->maxPriceOnMarket + 1;
            return true;
        }
        else{
            _item->active = false;
        }
    }

    return false;
}

bool comparePairByFirst( const std::pair<int,int> & _firstItem, const std::pair<int,int> & _secondItem ){

    return ( _firstItem.first > _secondItem.first );
}

bool Bot::getItemInfo( Item_t * _item ){

    const string requestTemplate = m_currentProfile->httpsAddress + "ItemInfo/%1_%2/%3/?key=%4";
    string request = vtformat( requestTemplate, _item->classId, _item->instanceId, "en", m_currentProfile->secretKey );

    string * response;
    if( ! makeHttpsRequest(request, response) ){
        return false;
    }

//    logger() << * response;

    Document doc;
    if( doc.ParseInsitu<0>( & (*response).front() ).HasParseError() ){

        g_logger( "ERROR: json parse error, message (note: modifyed by RapidJSON): " + * response );
        return false;
    }

    // read min price
    if( doc.HasMember( MIN_PRICE ) ){

        const char * minPrice = doc[ MIN_PRICE ].GetString();
        _item->minPriceOnMarket = atoi( minPrice );
    }
    else{
        g_logger( "min price not found for item: " + _item->url );
    }

    // read hash
    if( doc.HasMember( HASH ) ){

        _item->hash = doc[ HASH ].GetString();
    }
    else{
        g_logger( "hash not found for item: " + _item->url );
    }

    // read offers
    if( doc.HasMember( BUY_OFFERS ) ){

        const Value & buy_offers = doc[ BUY_OFFERS ];

        // find 2 last max prices
        vector<pair<int, int>> priceAndBuyersCount;

        for( auto & obj : buy_offers.GetArray() ){

            const char * priceStr = obj[ O_PRICE ].GetString();
            const char * countStr = obj[ C ].GetString();

            priceAndBuyersCount.emplace_back( atoi(priceStr), atoi(countStr) );
        }

        sort( priceAndBuyersCount.begin(), priceAndBuyersCount.end(), comparePairByFirst );

        _item->maxPriceOnMarket = priceAndBuyersCount[0].first;
        _item->buyersCount = priceAndBuyersCount[0].second;


        if( priceAndBuyersCount.size() > 1 ){

            _item->priceBeforeMax = priceAndBuyersCount[1].first;
            _item->priceBeforeMaxExist = true;
        }
    }
    else{
        g_logger( string("field <") + BUY_OFFERS + "> not found in json response (note: modifyed by RapidJSON): " + * response );
        return false;
    }

    return true;
}

bool Bot::createItemOrder( Item_t * _item ){

    const string requestTemplate = m_currentProfile->httpsAddress + "InsertOrder/%1/%2/%3/%4/?key=%5";
    string request = vtformat( requestTemplate, _item->classId, _item->instanceId, _item->ourPrice, _item->hash, m_currentProfile->secretKey );

    string * response;
    if( ! makeHttpsRequest(request, response) ){
        return false;
    }

    Document doc;
    if( doc.ParseInsitu<0>( & (*response).front()).HasParseError() ){

        g_logger( "json parse error, message (note: modifyed by RapidJSON): " + * response );
        return false;
    }

    _item->buyOrderCreated = true;

    g_logger( "response from create item order: " + * response );

    // TODO

    return true;
}

bool Bot::updateItemOrder( Item_t * _item ){

    const string requestTemplate = m_currentProfile->httpsAddress + "UpdateOrder/%1/%2/%3/?key=%4";

    string request = vtformat( requestTemplate, _item->classId, _item->instanceId, _item->ourPrice, m_currentProfile->secretKey );

    string * response;
    if( ! makeHttpsRequest(request, response) ){
        return false;
    }

    Document doc;
    if( doc.ParseInsitu<0>( & (*response).front()).HasParseError() ){

        g_logger( "json parse error, message (note: modifyed by RapidJSON): " + * response );
        return false;
    }

    g_logger( "response from update item order: " + * response );

    // TODO

    return true;
}

bool Bot::buyItem( Item_t * _item ){

    const string requestTemplate = m_currentProfile->httpsAddress + "Buy/%1_%2/%3/%4/?key=%5";

    // покупается по минимальной цене на маркете
    string request = vtformat( requestTemplate, _item->classId, _item->instanceId, _item->minPriceOnMarket, _item->hash, m_currentProfile->secretKey);

    string * response;
    if( ! makeHttpsRequest(request, response) ){
        return false;
    }

    Document doc;
    if( doc.Parse<0>( & (*response).front()).HasParseError() ){

        g_logger( "json parse error, message (RapidJSON): " + * response );
        return false;
    }

    g_logger( "response from buy item: " + * response + " by request: " + request );

    if( doc.HasMember("result") ){

        if( ! strcmp( doc["result"].GetString(), "ok") ){

            _item->buysPerformed++;

            g_logger( "OK - item buy done: url " + _item->url );
            g_logger( "price: " + to_string(_item->minPriceOnMarket) + " buys performed: " + to_string(_item->buysPerformed) + " (max: " + to_string(_item->purchasesNumber) + ")" );
        }
        else{
            g_logger( "item buy failed. Url: " + _item->url );
            g_logger( string("result string: ") + doc["result"].GetString() );
            g_logger( "item switched off from buy mode" );

            _item->active.store( false );
        }
    }

    // TODO

    return true;
}

bool Bot::checkTrades(){

    // trades check's necessity
    bool buyedItemsExist = false;
    for( Item_t * item : m_currentProfile->items ){

        if( ItemProcessType_en::BUY_HTTPS == item->processType || ItemProcessType_en::BUY_WEBSOCKET == item->processType ){
//            if( item->buysPerformed.load() ){
                buyedItemsExist = true;
//            }
        }
    }

    if( ! buyedItemsExist ){
        return false;
    }

    // check every 10 sec if need
    static long lastTradesCheckTime = 0;
    const long tradesCheckInterval = 10000;
    if( (getCurrentTimeMilliSec() - lastTradesCheckTime) <= tradesCheckInterval && lastTradesCheckTime != 0 ){
        return false;
    }
    lastTradesCheckTime = getCurrentTimeMilliSec();

    g_logger( "make trades check, time: " + printCurrentTime() );


    // make check
    const string requestTemplate = m_currentProfile->httpsAddress + "Trades/?key=%1";
    string request = vtformat( requestTemplate, m_currentProfile->secretKey);

    string * response;
    if( ! makeHttpsRequest(request, response) ){
        return false;
    }

    Document doc;
    if( doc.Parse<0>( & (*response).front()).HasParseError() ){
        g_logger( "json parse error, message (RapidJSON): " + * response );
        return false;
    }

    // parse trade items
    for( auto & arrayItem : doc.GetArray() ){

        const int64_t marketID = atoll( arrayItem[ "ui_id" ].GetString() );
        const string itemName = arrayItem[ "i_name" ].GetString();

        const int64_t classID = atoll( arrayItem[ "i_classid" ].GetString() );
        const int64_t instanceID = atoll( arrayItem[ "i_instanceid" ].GetString() );
        const int itemStatus = atoi( arrayItem[ "ui_status" ].GetString() );
        const int64_t botID = atoll( arrayItem[ "ui_bid" ].GetString() );

        for( Item_t * item : m_currentProfile->items ){

            if( item->classId == classID && item->instanceId == instanceID ){
                item->itemStatus = itemStatus;
                item->botID = botID;
            }
        }
    }


    return true;
}

bool Bot::makeItemRequest( Item_t * _item ){

    const string requestTemplate = m_currentProfile->httpsAddress + "ItemRequest/out/%1/?key=%2";
    string request = vtformat( requestTemplate, _item->botID, m_currentProfile->secretKey );

    string * response;
    if( ! makeHttpsRequest(request, response) ){
        return false;
    }

    Document doc;
    if( doc.Parse<0>( & (*response).front()).HasParseError() ){
        g_logger( "json parse error, message (RapidJSON): " + * response );
        return false;
    }

    if( doc.HasMember("success") ){
        if( doc["success"].GetBool() ){
            g_logger( "OK - item request done. Response: " + * response );
            _item->requestedToSteam = true;

            return true;
        }
        else{
            // parse error
            const Value & dataVal = doc[ "error" ];
            g_logger( string("item buy failed. Response error: ") + dataVal.GetString() );
            _item->requestedToSteam = true;

            return false;
        }
    }

    // TODO
//    Пример ответа:
//    {
//      "success": true,
//      "trade": "1704976549",
//      "nick": "NIPFribergEZIO",
//      "botid": "354589802",
//      "profile": "https://steamcommunity.com/profiles/76561198314855530/",
//      "secret": "1J10",
//      "items": [
//        "1812819920_188530170",
//        "2082539396_188530139",
//        "2048839018_902658099"
//      ]
//    }

    return true;
}

bool Bot::deleteAllOrders(){

    if( ! m_currentProfile ){
        return false;
    }

    bool ordersExist = false;
    for( Item_t * item : m_currentProfile->items ){
        if( item->buyOrderCreated ){

            ordersExist = true;
            break;
        }
    }
    if( ! ordersExist ){
        return false;
    }

    const string requestTemplate = m_currentProfile->httpsAddress + "DeleteOrders/?key=%1";
    string request = vtformat( requestTemplate, m_currentProfile->secretKey );

    string * response;
    if( ! makeHttpsRequest(request, response) ){
        return false;
    }

    Document doc;
    if( doc.ParseInsitu<0>( & (*response).front()).HasParseError() ){
        g_logger( "json parse error, message (note: modifyed by RapidJSON): " + * response );
        return false;
    }

    for( rapidjson::Value::ConstMemberIterator itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr ){
//    for( auto & obj : doc.GetObject() ){

        const Value & a = doc[ itr->name.GetString() ];

        if( a.IsObject() ){
            if( a.HasMember(SUCCESS) ){
                g_logger( "delete orders status: " + a[ SUCCESS ].GetBool() );
            }

            if( a.HasMember(DELETED_ORDERS) ){
                g_logger( "deleted orders count: " + a[ DELETED_ORDERS ].GetInt() );
            }
        }
        else{
            g_logger( "delete orders response, one or more target fields not found (note: modifyed by RapidJSON): " + * response );
        }
    }

    return true;
}


/*

Buy

Покупка предмета. В нашей системе возможно покупка только по одному предмету за запрос.

Пример запроса:
https://market.dota2.net/api/Buy/[classid]_[instanceid]/[price]/[hash]/?key=[your_secret_key]

Параметры запроса:
[classid]_[instanceid] — идентификаторы предмета, который можно найти в ссылке на предмет. https://market.dota2.net/item/57939770-57939888-Treasure+Key/ - ключ в Dota2. 57939770 - [classid], 57939888 - [instanceid].
[price] — цена в копейках (целое число), уже какого-то выставленного лота, или можно указать любую сумму больше цены самого дешевого лота, во втором случае будет куплен предмет по самой низкой цене.
[hash] - md5 от описания предмета. Вы можете найти его в ответе метода ItemInfo. Это введено, чтобы вы были уверены в покупке именно той вещи, которую покупаете. Если для Вас это не интересно, просто пришлите пустую строку - то есть сделайте запрос вида ...[price]//?key=...

Пример ответа:
{
  "result": "ok",
  "id": "136256960"
}

Пояснение к ответу:
result = ok — предмет был успешно куплен
id - ID предмета (операции) в нашей системе, так-же можно увидеть в Trades

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ItemInfo

Информация и предложения о продаже конкретной вещи.

Пример запроса:
https://market.dota2.net/api/ItemInfo/[classid]_[instanceid]/[language]/?key=[your_secret_key]

Параметры запроса:
[classid] — ClassID предмета в Steam, можно найти в ссылке на предмет
[instanceid] — InstanceID предмета в Steam, можно найти в ссылке на предмет
[language] — на каком языке получить описание предмета, возможные значения ru или en

Пример ответа:
{
  "classid": "384801319",
  "instanceid": "0",
  "our_market_instanceid": null,
  "market_name": "Оружейный кейс операции «Феникс»",
  "name": "Оружейный кейс операции «Феникс»",
  "market_hash_name": "Operation Phoenix Weapon Case",
  "rarity": "базового класса",
  "quality": "",
  "type": "Контейнер",
  "mtype": "CSGO_Type_WeaponCase",
  "slot": "Обыч.",
  "stickers": null,
  "description": [
    {
      "type": "html",
      "value": "Контейнер тиража #11",
      "color": "99ccff"
    }
  ],
  "tags": [
    {
      "internal_name": "CSGO_Type_WeaponCase",
      "name": "Контейнер",
      "category": "Type",
      "category_name": "Тип"
    }
  ],
  "hash": "8db34e382830ad19effdfeed7cf01e9b",
  "min_price": "196",
  "offers": [
    {
      "price": "196",
      "count": "1",
      "my_count": "0"
    },
    {
      "price": "220",
      "count": "4",
      "my_count": "0"
    }
  ]
}
Поля classid, instanceid, market_name, market_hash_name, description, tags дублируются из инвентаря Steam.
Поле our_market_instanceid - мы используем группировки вещей по совпадению classid и описания и подменяем instanceid у некоторых предметов выставляемых на продажу, в этом поле может отображаться настоящий instanceid если он был изменен (может быть числовым, либо null).
Поле offers - содержит массив текущих предложения на нашем маркете.
Поле min_price - минимальная цена предмета в копейках на нашем маркете.
Поле hash - необходимо для покупки предмета.

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Trades

Список предметов на продаже, готовых к получению после покупки, предметов которые необходимо передать после продажи со страницы "Мои вещи".

Пример запроса:
https://market.dota2.net/api/Trades/?key=[your_secret_key]

Пример ответа:
[
  {
    "ui_id": "136299509",
    "i_name": "Наклейка | Fnatic | Кёльн 2014",
    "i_market_name": "Наклейка | Fnatic | Кёльн 2014",
    "i_name_color": "D2D2D2",
    "i_rarity": "Высшего класса",
    "i_descriptions": null,
    "ui_status": "2",
    "he_name": "Наклейка",
    "ui_price": 40.18,
    "i_classid": "549051152",
    "i_instanceid": "188530139",
    "ui_real_instance": "unknown",
    "i_quality": "",
    "i_market_hash_name": "Sticker | Fnatic | Cologne 2014",
    "i_market_price": 61.96,
    "position": 1,
    "min_price": 0,
    "ui_bid": "0",
    "ui_asset": "0",
    "type": "2",
    "ui_price_text": "40.18",
    "min_price_text": false,
    "i_market_price_text": "61.96",
    "left": 1061,
    "placed": "42 минуты назад"
  }
]

Возможные статусы:
"ui_status" = 1 — Вещь выставлена на продажу.
"ui_status" = 2 — Вы продали вещь и должны ее передать боту.
"ui_status" = 3 — Ожидание передачи боту купленной вами вещи от продавца.
"ui_status" = 4 — Вы можете забрать купленную вещь.

Пояснение к ответу:
ui_id — ID предмета в нашей системе
ui_status — статус предмета (см. выше)
ui_price — ваша цена
position — позиция в очереди продажи (сортировка по наименьшей цене), в момент покупки выбирается самый дешевый предмет.
ui_bid — ID бота, на котором находится предмет в статусе 4.
ui_asset — ID предмета в инвентаре бота.
placed — Время, когда изменился статус предмета/цена или он был выставлен на продажу.
left — Времени осталось на передачу предмета, после этого операция будет отменена и деньги вернутся покупателю. Будут начислены штрафные баллы
i_market_price — Рекомендуемая цена продаже (относительно ТП Steam).

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ItemRequest

С помощью этого методы, Вы можете запросить передачу предмета который был куплен у Вас или куплен Вами. Вам будет отправлен оффлайн трейдоффер в Steam, который необходимо принять в течении 2 минут. В одну операцию может попасть максимум 20 предметов.

Пример запроса на получение купленного предмета:
https://market.dota2.net/api/ItemRequest/out/[botid]/?key=[your_secret_key]

В нашей систему можно создать только один запрос за определенное время. За один запрос можно получить вещи только с одного бота, на котором они в данный момент находятся.

Параметры запроса:
[botid] — Идентификатор бота, который можно узнать c помощью метода Trades - это поле ui_bid у предметов со статусом (ui_status) = 4
Пример ответа:
{
  "success": true,
  "trade": "1704976549",
  "nick": "NIPFribergEZIO",
  "botid": "354589802",
  "profile": "https://steamcommunity.com/profiles/76561198314855530/",
  "secret": "1J10",
  "items": [
    "1812819920_188530170",
    "2082539396_188530139",
    "2048839018_902658099"
  ]
}

Пояснение к ответу:
trade — SteamID трейд оффера, который был отправлен Вам
nick — Никнейм бота в Steam
botid — SteamID бота
profile — Ссылка на профиль бота в Steam
secret — Секретный код сделки, указанный в предложении обмена в Steam
items — Массив данных с предметами, которые бот хочет забрать у Вас (при передаче проданных предметов)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

https://api.steampowered.com/IEconService/GetTradeOffer/v1/?key=14E0B161A51F2856CDE032341F6AE4A7&tradeofferid=1&language=en


*/




















