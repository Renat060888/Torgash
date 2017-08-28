#pragma once

#include <string>
#include <atomic>
#include <vector>


enum class ItemProcessType_en {
    RACE,
    BUY_WEBSOCKET,
    BUY_HTTPS,

    UNDEFINED
};

struct Item_t {

    Item_t() :   pkey(0),
                            classId(0),
                            instanceId(0),
                            url(""),
                            processType(ItemProcessType_en::UNDEFINED),
                            hash(""),
                            active(false),

                            stopPrice(0),
                            ourPrice(0),
                            buyersCount(0),
                            maxPriceOnMarket(0),
                            priceBeforeMax(0),
                            priceBeforeMaxExist(false),
                            buyOrderCreated(false),

                            botID(0),
                            itemStatus(0),
                            maxPriceForBuy(0),
                            minPriceOnMarket(0),
                            buyedByPrice(0),
                            upperBound(0),
                            lowerBound(0),
                            buy(false),
                            purchasesNumber(0),
                            buysPerformed(0),

                            catchedViaWebsocket(false),
                            requestedToSteam(false)
    { }

    Item_t & operator=( Item_t & _inst ){

        // common
        this->pkey = _inst.pkey;
        this->fk_profile = _inst.fk_profile;
        this->classId = _inst.classId;
        this->instanceId = _inst.instanceId;
        this->url = _inst.url;
        this->processType = _inst.processType;
        this->hash = _inst.hash;
        this->active.store( _inst.active.load() );
        // race
        this->stopPrice = _inst.stopPrice;
        this->ourPrice = _inst.ourPrice;
        this->buyersCount = _inst.buyersCount;
        this->maxPriceOnMarket = _inst.maxPriceOnMarket;
        this->priceBeforeMax = _inst.priceBeforeMax;
        this->priceBeforeMaxExist = _inst.priceBeforeMaxExist;
        this->buyOrderCreated = _inst.buyOrderCreated;
        // buy
        this->botID = _inst.botID;
        this->itemStatus = _inst.itemStatus;
        this->maxPriceForBuy = _inst.maxPriceForBuy;
        this->minPriceOnMarket = _inst.minPriceOnMarket;
        this->buyedByPrice = _inst.buyedByPrice;
        this->upperBound = _inst.upperBound;
        this->lowerBound = _inst.lowerBound;
        this->buy.store( _inst.buy.load() );
        this->purchasesNumber = _inst.purchasesNumber;
        this->buysPerformed.store( _inst.buysPerformed.load() );
        // buy websocket
        this->catchedViaWebsocket = _inst.catchedViaWebsocket;
        this->requestedToSteam = _inst.requestedToSteam;

        return * this;
    }

    // common
    int                 pkey;
    int                 fk_profile;
    int64_t             classId;
    int64_t             instanceId;
    std::string         url;
    ItemProcessType_en  processType;
    std::string         hash;
    std::atomic_bool    active;
    // race
    int                 stopPrice;
    int                 ourPrice;
    int                 buyersCount;
    int                 maxPriceOnMarket;
    int                 priceBeforeMax;
    bool                priceBeforeMaxExist;
    bool                buyOrderCreated;
    // buy
    int64_t             botID;
    int                 itemStatus;
    int                 maxPriceForBuy;
    int                 minPriceOnMarket;
    int                 buyedByPrice;
    int                 upperBound;
    int                 lowerBound;
    std::atomic_bool    buy;
    int                 purchasesNumber;
    std::atomic_int     buysPerformed; // покупки идут сразу из https и websocket
    // buy websocket
    bool                catchedViaWebsocket;
    bool                requestedToSteam;
};

struct Profile_t {

    uint32_t            profileID;
    std::string         profileName;

    std::string         httpsAddress;
    std::string         websocketAddress;
    std::string         secretKey;
    std::string         steamKey;

    std::vector<Item_t *> items;
};





















