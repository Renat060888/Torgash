#pragma once

// -------------------------------
// REQUESTS
// -------------------------------

// информация об изменениях в цене или выставлении на продажу предметов Dota2. (Осторожно, канал генерирует очень много трафика)
static const std::string WS_NEWITEMS_CS         = {"newitems_cs"};
// история продаж которая отображается на каждой странице сайта.
static const std::string WS_HISTORY_CS          = {"history_cs"};
// Получить временный ключ-токен для авторизации на веб-сокет сервере
static const std::string HTTPS_GET_WS_AUTH      = "GetWSAuth/?key=";
// Выход в онлайн, необходимо отправлять раз в 3 минуты.
static const std::string HTTPS_PING_PONG        = "PingPong/?key=";
// Список последних 50 покупок со всей торговой площадки.
static const std::string HTTPS_HISTORY_LAST_50 = "https://market.dota2.net/history/json/";

// -------------------------------
// RESPONSE FIELDS
// -------------------------------

// from GET_WS_AUTH_HTTPS
static const char * SUCCESS = "success";
static const char * WS_AUTH = "wsAuth";

// from
static const char * CLASS_ID = "classid";
static const char * INSTANCE_ID = "instanceid";
static const char * OUR_MARKET_INSTANCEID = "our_market_instanceid";
static const char * MARKET_NAME = "market_name";
static const char * NAME = "name";
static const char * MARKET_HASH_NAME = "market_hash_name";
static const char * RARITY = "rarity";
static const char * QUALITY = "quality";
static const char * TYPE = "type";
static const char * MTYPE = "mtype";
static const char * SLOT = "slot";
static const char * STICKERS = "stickers";
static const char * DESCRIPTION = "description";
static const char * VALUE = "value";
static const char * COLOR = "color";
static const char * TAGS = "tags";
static const char * INTERNAL_NAME = "internal_name";
static const char * CATEGORY = "category";
static const char * CATEGORY_NAME = "category_name";
static const char * HASH = "hash";
static const char * MIN_PRICE = "min_price";
static const char * OFFERS = "offers";
static const char * BUY_OFFERS = "buy_offers";
static const char * PRICE = "price";
static const char * O_PRICE = "o_price";
static const char * C = "c";
static const char * MY_COUNT = "my_count";

// delete orders
static const char * ONLINE = "online";
static const char * DELETED_ORDERS = "deleted_orders";


