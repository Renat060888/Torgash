#pragma once

// std
#include <string>
#include <vector>
#include <atomic>
// project
#include "task.h"
#include "torgash.h"

struct itemFromNewitems_t {

    int64_t         i_classId;
    int64_t         i_instanceId;
    float           ui_price;
    std::string     i_marketHashName;
    std::string     i_marketName;
};

class WebsocketStreamTask : public Task {

public:

    WebsocketStreamTask( const int _responsesCount, std::vector<Item_t *> & _items );

    virtual ~WebsocketStreamTask();

    virtual bool process() override;

    void clear();

    std::vector<std::string>            m_responesFromWebsocket;

    std::atomic_bool                    m_done;

    std::vector<Item_t * > &            m_profileItems;
};

