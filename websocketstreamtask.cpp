
// std
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/istreamwrapper.h"
// project
#include "websocketstreamtask.h"
#include "utils.h"

using namespace std;
using namespace rapidjson;

extern void g_logger( const string _msg );
itemFromNewitems_t g_itemFromNewitems;

WebsocketStreamTask::WebsocketStreamTask( const int _responsesCount, vector<Item_t *> & _items ) :
    m_done(false),
    m_profileItems(_items) {

    m_responesFromWebsocket.resize( _responsesCount );
}

WebsocketStreamTask::~WebsocketStreamTask(){


}

void WebsocketStreamTask::clear(){

    m_done.store(false);

    m_responesFromWebsocket.clear();
}

bool WebsocketStreamTask::process(){

    for( string row : m_responesFromWebsocket ){

        if( "pong" == row ){
            g_logger( "pong received" );
            continue;
        }

        Document doc;
        if( doc.ParseInsitu<0>( & row.front() ).HasParseError() ){

            g_logger( "ERROR: json parse error, message (note: modifyed by RapidJSON): " + row );
            g_logger( "---------------------------------------------------" );
            return false;
        }

        for( Value::ConstMemberIterator iter = doc.MemberBegin(); iter != doc.MemberEnd(); ++iter ){

            if( ! strcmp("type", iter->name.GetString()) && ! strcmp("newitems_cs", iter->value.GetString()) ){

                // -------------------------------------------
                // NEWITEMS JSON Record
                // -------------------------------------------
//                {
//                    "type":"newitems_cs",
//                    "data":"{
//                        \"i_quality\":\"Genuine\",
//                        \"i_name_color\":\"4D7455\",
//                        \"i_classid\":\"519295759\",
//                        \"i_instanceid\":\"93973071\",
//                        \"i_market_hash_name\":\"Genuine Weather Rain\",
//                        \"i_market_name\":\"Genuine Weather Rain\",
//                        \"ui_price\":23.35,
//                        \"app\":\"cs\"
//                    }"
//                }

                ++iter;
                const Value & dataVal = doc[ iter->name.GetString() ];

                Document dataDoc;
                if( dataDoc.Parse<0>( dataVal.GetString() ).HasParseError() ){

                    g_logger( string("ERROR: json parse error, message (note: modifyed by RapidJSON): ") + dataVal.GetString() );
                }

                for( Value::ConstMemberIterator iter = dataDoc.MemberBegin(); iter != dataDoc.MemberEnd(); ++iter ){

                    const Value & val = dataDoc[ iter->name.GetString() ];

                    if( val.IsString() ){

                        if( ! strcmp(iter->name.GetString(), "i_market_hash_name") ){

                            g_itemFromNewitems.i_marketHashName = val.GetString();
                        }
                        else if( ! strcmp(iter->name.GetString(), "i_market_name") ){

                            g_itemFromNewitems.i_marketName = val.GetString();
                        }
                        else if( ! strcmp(iter->name.GetString(), "i_classid") ){

                            g_itemFromNewitems.i_classId = atoll( val.GetString() );
                        }
                        else if( ! strcmp(iter->name.GetString(), "i_instanceid") ){

                            g_itemFromNewitems.i_instanceId = atoll( val.GetString() );
                        }
                    }
                    else if( val.IsInt() ){

                    }
                    else if( val.IsFloat() ){

                        if( ! strcmp(iter->name.GetString(), "ui_price") ){

                            g_itemFromNewitems.ui_price = val.GetFloat();
                        }
                    }
                    else{

                        g_logger( string("WARN: unknown value type of key: ") + iter->name.GetString() );
                    }
                }

                for( Item_t * item : m_profileItems ){

                    if( ItemProcessType_en::BUY_WEBSOCKET == item->processType ){

                        if( item->classId == g_itemFromNewitems.i_classId &&
                            item->instanceId == g_itemFromNewitems.i_instanceId ){

                            if( item->maxPriceForBuy <= g_itemFromNewitems.ui_price ){

                                g_logger( "WEBSOCKET: found item (classID " + to_string( g_itemFromNewitems.i_classId ) +
                                          ", instanceID " + to_string( g_itemFromNewitems.i_instanceId ) +
                                          ", our max price " + to_string( item->maxPriceForBuy ) +
                                          ", founded item price " + to_string( g_itemFromNewitems.ui_price ) +
                                          ". TRY TO BUY");

                                item->catchedViaWebsocket = true;
                            }
                        }
                    }
                }

//                cout << "------------------------------------------------------------------------" << endl;
//                cout << row << endl;
//                cout << "------------------------------------------------------------------------" << endl;
//                cout << "i_market_hash_name: " << g_itemFromNewitems.i_marketHashName << endl;
//                cout << "i_market_name: " << g_itemFromNewitems.i_marketName << endl;
//                cout << "i_classid: " << g_itemFromNewitems.i_classId << endl;
//                cout << "i_instanceid: " << g_itemFromNewitems.i_instanceId << endl;
//                cout << "ui_price: " << g_itemFromNewitems.ui_price << endl;
//                cout << endl;

                // TODO check item for itemProcessType_t::BUY_WEBSOCKET

                // TODO check lower + upper bounds
            }
            else if( ! strcmp("type", iter->name.GetString()) && ! strcmp("webnotify", iter->value.GetString()) ){

                ++iter;
                const Value & dataVal = doc[ iter->name.GetString() ];

                // TODO какая та хуита. почему то не убираются escape-последовательности
                string unescaped;
                unescape( dataVal.GetString(), dataVal.GetStringLength(), unescaped );

                unescaped.erase( unescaped.find_first_of("\""), 1 ); // TODO неэффективно
                unescaped.erase( unescaped.find_last_of("\""), 1 );

                Document cyrillicStrDoc;
                if( cyrillicStrDoc.Parse<0>( & unescaped.front() ).HasParseError() ){

                    g_logger( "ERROR: json parse error, message (note: modifyed by RapidJSON): " + unescaped );
                }

                for( Value::ConstMemberIterator iter = cyrillicStrDoc.MemberBegin(); iter != cyrillicStrDoc.MemberEnd(); ++iter ){

                    const Value & a = cyrillicStrDoc[ iter->name.GetString() ];

                    if( a.IsString() ){

                        g_logger( string(iter->name.GetString()) + " : " + a.GetString() );
                    }
                    else if( a.IsInt() ){

                        g_logger( string(iter->name.GetString()) + " : " + to_string(a.GetInt()) );
                    }
                    else{

                        g_logger( string("ERROR: unknown value type of key: ") + iter->name.GetString() );
                    }
                }
            }            
            else if( ! strcmp("type", iter->name.GetString()) && ! strcmp("history_cs", iter->value.GetString()) ){

                ++iter;
                const Value & dataVal = doc[ iter->name.GetString() ];

                // TODO какая та хуита. почему то не убираются escape-последовательности
                string unescaped;
                unescape( dataVal.GetString(), dataVal.GetStringLength(), unescaped );

                unescaped.erase( unescaped.find_first_of("\""), 1 ); // TODO неэффективно
                unescaped.erase( unescaped.find_last_of("\""), 1 );

                Document dataDoc;
                if( dataDoc.Parse<0>( & unescaped.front() ).HasParseError() ){

                    g_logger( "ERROR: json parse error, message (note: modifyed by RapidJSON): " + unescaped );
                }

                for( auto & member : dataDoc.GetArray() ){

                    if( member.IsString() ){

//                        cout << member.GetString() << endl;
                    }
                    else if( member.IsInt() ){

//                        cout << iter->name.GetInt() << endl;
                    }
                    else{

                        g_logger( string("WARN: unknown value type of key: ") + iter->name.GetString() );
                    }
                }
            }
            else{
                g_logger( string("WARN: type of websocket is not \"webnotify\": ") + iter->name.GetString() + row );
            }
        }
    }

    m_done.store( true );
    return true;
}

