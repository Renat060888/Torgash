
// std
#include <string.h>
#include <cassert>
// 3rd party
#include "sqlite3.h"
// project
#include "profile_manager.h"
#include "string_format.h"

using namespace std;

extern void g_logger( const string _msg );
const char * DATABASE_NAME = "profiles_database.sqlite";

enum class RequestType_en {
    DDL,
    PROFILES,
    PROFILES_COUNT,
    ITEMS,

    UNDEFINED
};

struct RequestContainer_t {

    int                 profilesCount;
    RequestType_en      requestType;
    vector<Profile_t>   callbackDataProfiles;
    vector<Item_t *>    callbackDataItems;
};

// --------------------------------------
// SQLITE CALLBACK
// --------------------------------------
static int callbackSqlite( void * userArg, int argc, char ** argv, char ** azColName ){

    RequestContainer_t * callbackAnswer = ( RequestContainer_t * )( userArg );

    switch( callbackAnswer->requestType ) {
    case RequestType_en::PROFILES : {

        if( 6 != argc ){
            g_logger( "profiles request - columns is not 6" );
            break;
        }

        Profile_t p;
        int i = 0;

        p.profileID         = atoi( argv[ i ] );
        p.profileName       = argv[ i + 1 ];
        p.httpsAddress      = argv[ i + 2 ];
        p.websocketAddress  = argv[ i + 3 ];
        p.secretKey         = argv[ i + 4 ];
        p.steamKey          = argv[ i + 5 ];

        callbackAnswer->callbackDataProfiles.push_back( p );
        break;
    }
    case RequestType_en::PROFILES_COUNT : {

        if( 1 != argc ){
            g_logger( "profiles count request - columns is not 1" );
            break;
        }

        callbackAnswer->profilesCount = atoi( argv[ 0 ] );
        break;
    }
    case RequestType_en::ITEMS : {

        if( 10 != argc ){
            g_logger( "items request - columns is not 10" );
            break;
        }

        Item_t * item = new Item_t();
        int i = 0;

        item->pkey              = atoi( argv[ i ] );
        item->fk_profile        = atoi( argv[ i + 1 ] );
        item->active.store( atoi(argv[ i + 2 ]) );
        item->url               = argv[ i + 3 ];
        item->maxPriceForBuy    = atoi( argv[ i + 4 ] );
        item->purchasesNumber   = atoi( argv[ i + 5 ] );
        item->stopPrice         = atoi( argv[ i + 6 ] );
        item->classId           = atoi( argv[ i + 7 ] );
        item->instanceId        = atoi( argv[ i + 8 ] );

        if( ! strcmp( "order", argv[ i + 9 ]) ){
            item->processType = ItemProcessType_en::RACE;
        }
        else if( ! strcmp( "buy", argv[ i + 9 ]) ){
            item->processType = ItemProcessType_en::BUY_HTTPS;
        }
        else if( ! strcmp( "buy_websocket", argv[ i + 9 ]) ){
            item->processType = ItemProcessType_en::BUY_WEBSOCKET;
        }
        else{
            g_logger( "ERROR item trade type is not [Order / Buy / Buy Websocket]" );
        }

        callbackAnswer->callbackDataItems.push_back( item );
        break;
    }
    case RequestType_en::DDL : {
        // dummy
    }
    default:
        g_logger( "unknown request type" );
    }

    return 0;
}


ProfileManager::ProfileManager() :
    m_sqlClient(nullptr){

    //
    int rc = sqlite3_open( DATABASE_NAME, & m_sqlClient );

    if( rc ){
        g_logger( string("WARN: can't open database: ") + sqlite3_errmsg(m_sqlClient) );
        sqlite3_close( m_sqlClient );
        throw;
    }

    //
    createDatabase();

    //
    updatesProfiles();
}

ProfileManager::~ProfileManager(){

    if( m_sqlClient ){
        sqlite3_close( m_sqlClient );
    }
}

void ProfileManager::createItem( const uint32_t _profileID ){

    //
    char * zErrMsg = nullptr;

    // items( id integer PRIMARY KEY, prof_id integer, active boolean, name varchar, price integer, class_id integer, instance_id integer, descr varchar, trade_type varchar
    const string requestTemplate = "INSERT INTO items( prof_id, active, name, price_for_buy, buys_number, price_stop, class_id, instance_id, trade_type ) VALUES ( %1, 0, 'set_me', 0, 0, 0, 0, 0, 'order' );";
    const string request = vtformat( requestTemplate, _profileID );

    int rc = sqlite3_exec( m_sqlClient, request.c_str(), callbackSqlite, 0, & zErrMsg );

    if( SQLITE_OK != rc ){

        g_logger( string("SQL error create new item: ") + zErrMsg );
        sqlite3_free( zErrMsg );
    }

    //
    updatesProfiles();
}

void ProfileManager::commitItem( const uint32_t _profileID, Item_t * _item ){

    //
    char * zErrMsg = nullptr;

    string trade_type;
    if( ItemProcessType_en::BUY_HTTPS == _item->processType ){
        trade_type = "buy";
    }
    else if( ItemProcessType_en::BUY_WEBSOCKET == _item->processType ){
        trade_type = "buy_websocket";
    }
    else if( ItemProcessType_en::RACE == _item->processType ){
        trade_type = "order";
    }
    else{
        g_logger( "ERROR: commit item, unknown item process type " );
    }

    const string requestTemplate = "UPDATE items SET active = %2, name = '%3', price_for_buy = %4, buys_number = %5, price_stop = %6, class_id = %7, instance_id = %8, trade_type = '%9' WHERE prof_id == %1 AND id = %10;";
    const string request = vtformat( requestTemplate, _profileID, _item->active.load(), _item->url, _item->maxPriceForBuy, _item->purchasesNumber, _item->stopPrice, _item->classId, _item->instanceId, trade_type, _item->pkey );

    int rc = sqlite3_exec( m_sqlClient, request.c_str(), callbackSqlite, 0, & zErrMsg );

    if( SQLITE_OK != rc ){

        g_logger( string("SQL error update item: ") + zErrMsg );
        sqlite3_free( zErrMsg );
    }
}

bool ProfileManager::removeItem( const uint32_t _profileID, const uint32_t _itemID ){

    //
    char * zErrMsg = nullptr;

    const string requestTemplate = "DELETE FROM items WHERE prof_id == %1 AND id = %2";
    const string request = vtformat( requestTemplate, _profileID, _itemID );

    int rc = sqlite3_exec( m_sqlClient, request.c_str(), callbackSqlite, 0, & zErrMsg );

    if( SQLITE_OK != rc ){

        g_logger( string("SQL error delete item: ") + zErrMsg );
        sqlite3_free( zErrMsg );
        return false;
    }

    //
    updatesProfiles();

    return true;
}

Profile_t * ProfileManager::findProfile( const uint32_t _profileID ){

    for( Profile_t * profile : m_profiles ){

        if( profile->profileID == _profileID ){
            return profile;
        }
    }

    return nullptr;
}

std::vector<Profile_t *> ProfileManager::getProfiles(){

    return m_profiles;
}

void ProfileManager::createProfile(){

    //
    char * zErrMsg = nullptr;
    RequestContainer_t sqliteCallbackAnswer;

    // load profiles
    const char * query1 = "SELECT COUNT(*) FROM profiles;";

    sqliteCallbackAnswer.requestType = RequestType_en::PROFILES_COUNT;
    int rc = sqlite3_exec( m_sqlClient, query1, callbackSqlite, & sqliteCallbackAnswer, & zErrMsg );

    if( SQLITE_OK != rc ){

        g_logger( string("SQL error select all profiles: ") + zErrMsg );
        sqlite3_free( zErrMsg );
    }

    //
    const string requestTemplate = "INSERT INTO profiles( name, https, websocket, secret_key, steam_key ) VALUES ('project_%1','set_me','set_me','set_me','set_me');";
    const string request = vtformat( requestTemplate, sqliteCallbackAnswer.profilesCount + 1 );

    rc = sqlite3_exec( m_sqlClient, request.c_str(), callbackSqlite, 0, & zErrMsg );

    if( SQLITE_OK != rc ){

        g_logger( string("SQL error create new profile: ") + zErrMsg );
        sqlite3_free( zErrMsg );
    }

    //
    updatesProfiles();
}

void ProfileManager::commitProfile( const uint32_t _profileID ){

    Profile_t * profile = nullptr;
    for( Profile_t * p : m_profiles ){

        if( p->profileID == _profileID ){
            profile = p;
        }
    }

    if( ! profile ){
        assert( 1 == 0 );
    }

    //
    char * zErrMsg = nullptr;

    const string requestTemplate = "UPDATE profiles SET name = '%2', https = '%3', websocket = '%4', secret_key = '%5', steam_key = '%6' WHERE id == %1 ";
    const string request = vtformat( requestTemplate, profile->profileID, profile->profileName, profile->httpsAddress, profile->websocketAddress, profile->secretKey, profile->steamKey );

    int rc = sqlite3_exec( m_sqlClient, request.c_str(), callbackSqlite, 0, & zErrMsg );

    if( SQLITE_OK != rc ){

        g_logger( string("SQL error update profile: ") + zErrMsg );
        sqlite3_free( zErrMsg );
    }

    for( Item_t * item : profile->items ){

        commitItem( profile->profileID, item );
    }

    //
    updatesProfiles();
}

bool ProfileManager::removeProfile( const uint32_t _profileID ){

    //
    char * zErrMsg = nullptr;

    const string requestTemplate = "DELETE FROM profiles WHERE id == %1";
    const string request = vtformat( requestTemplate, _profileID );

    int rc = sqlite3_exec( m_sqlClient, request.c_str(), callbackSqlite, 0, & zErrMsg );

    if( SQLITE_OK != rc ){

        g_logger( string("SQL error create new profile: ") + zErrMsg );
        sqlite3_free( zErrMsg );
        return false;
    }

    //
    updatesProfiles();

    return true;
}

void ProfileManager::createDatabase(){

    char * zErrMsg = nullptr;
    RequestContainer_t callbackAnswer;

    // sqlite create profiles table
    const char * query1 = "CREATE TABLE IF NOT EXISTS profiles( id integer PRIMARY KEY, name varchar, https varchar, websocket varchar, secret_key varchar, steam_key varchar );";

    callbackAnswer.requestType = RequestType_en::DDL;
    int rc = sqlite3_exec( m_sqlClient, query1, callbackSqlite, & callbackAnswer, & zErrMsg );

    if( SQLITE_OK != rc ){

        g_logger( string("SQL error create table profiles: ") + zErrMsg );
        sqlite3_free( zErrMsg );
    }

    // sqlite create items table
    // [ QStringList() << "active" << "object name" << "price" << "class id" << "instance id" << "description" << "mode" ]
    const char * query2 = "CREATE TABLE IF NOT EXISTS items( id integer PRIMARY KEY, prof_id integer, active boolean, name varchar, price_for_buy integer, buys_number integer, price_stop integer, class_id integer, instance_id integer, trade_type varchar, CHECK (trade_type IN('order', 'buy', 'buy_websocket')), FOREIGN KEY(prof_id) REFERENCES profiles(id) );";

    callbackAnswer.requestType = RequestType_en::DDL;
    rc = sqlite3_exec( m_sqlClient, query2, callbackSqlite, & callbackAnswer, & zErrMsg );

    if( SQLITE_OK != rc ){

        g_logger( string("SQL error create table items: ") + zErrMsg );
        sqlite3_free( zErrMsg );
    }
}

void ProfileManager::updatesProfiles(){

    char * errMsg = nullptr;
    RequestContainer_t sqliteCallbackAnswer;

    // load profiles
    const char * query1 = "SELECT * FROM profiles;";

    sqliteCallbackAnswer.requestType = RequestType_en::PROFILES;
    int rc = sqlite3_exec( m_sqlClient, query1, callbackSqlite, & sqliteCallbackAnswer, & errMsg );

    if( SQLITE_OK != rc ){

        g_logger( string("SQL error select all profiles: ") + errMsg );
        sqlite3_free( errMsg );
    }

    // load items
    const char * query2 = "SELECT * FROM items;";

    sqliteCallbackAnswer.requestType = RequestType_en::ITEMS;
    rc = sqlite3_exec( m_sqlClient, query2, callbackSqlite, & sqliteCallbackAnswer, & errMsg );

    if( SQLITE_OK != rc ){

        g_logger( string("SQL error select all items: ") + errMsg );
        sqlite3_free( errMsg );
    }

    // -----------------------------------------
    // temp container -> actual container
    // -----------------------------------------

    // remove old
    for( Profile_t * profile : m_profiles ){

        for( Item_t * item : profile->items ){

            delete item;
        }
        delete profile;
    }

    m_profiles.clear();

    // create new
    for( const Profile_t & requestedProfile : sqliteCallbackAnswer.callbackDataProfiles ){

        Profile_t * p = new Profile_t();

        * p = requestedProfile;

        m_profiles.push_back( p );

        // items
        for( Item_t * requestedItem : sqliteCallbackAnswer.callbackDataItems ){

            if( requestedItem->fk_profile == p->profileID ){

                p->items.push_back( requestedItem );
            }
        }
    }
}










