#pragma once

// std
#include <vector>
// project
#include "bot.h"
#include "profile_manager.h"

class Torgash{

public:

    Torgash();
    ~Torgash();

    bool init();


    // profiles
    std::vector<Profile_t *> getProfiles();
    uint32_t getProfileIDByName( const std::string & _profileName );

    void createProfile();
    void commitProfile( const uint32_t _profileID );
    bool removeProfile( const uint32_t _profileID );

    // items
    void createItem( const uint32_t _profileID );
    bool removeItem( const uint32_t _profileID, const uint32_t _itemPkey );

    // bot
    bool runTrades( const uint32_t _profileID );
    void stopTrades();


private:

    Bot                     m_bot;

    ProfileManager          m_profileManager;


};
