
// project
#include "torgash.h"

using namespace std;

extern void g_logger( const string _msg );

Torgash::Torgash(){

}

Torgash::~Torgash(){


}


bool Torgash::init(){

    Profile_t * profile = m_profileManager.findProfile( 1 );

    if( profile ){
        if( ! m_bot.init() ){
            return false;
        }
    }

    return true;
}

bool Torgash::runTrades( const uint32_t _profileID ){

    Profile_t * profile = m_profileManager.findProfile( _profileID );

    if( ! profile ){
        g_logger( string("ERROR: this profile: [") + to_string(_profileID) + "] not found" );
        return false;
    }

    m_bot.startTrades( profile );
    return true;
}

void Torgash::stopTrades(){

    m_bot.stopTrades();
}

std::vector<Profile_t *> Torgash::getProfiles(){

    return m_profileManager.getProfiles();
}

uint32_t Torgash::getProfileIDByName( const string & _profileName ){

    for( Profile_t * profile : m_profileManager.getProfiles() ){

        if( profile->profileName == _profileName ){
            return profile->profileID;
        }
    }

    return -1;
}

void Torgash::createProfile(){

    m_profileManager.createProfile();
}

void Torgash::commitProfile( const uint32_t _profileID ){

    m_profileManager.commitProfile( _profileID );
}

bool Torgash::removeProfile( const uint32_t _profileID ){

    if( ! m_profileManager.removeProfile(_profileID) ){
        return false;
    }

    return true;
}

void Torgash::createItem( const uint32_t _profileID ){

    m_profileManager.createItem( _profileID );
}

bool Torgash::removeItem( const uint32_t _profileID, const uint32_t _itemPkey ){

    m_profileManager.removeItem( _profileID, _itemPkey );

    return true;
}


















