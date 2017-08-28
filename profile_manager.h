#pragma once

// std
#include <vector>
// project
#include <types_common.h>

class ProfileManager{

public:

    ProfileManager();
    ~ProfileManager();

    Profile_t * findProfile( const uint32_t _profileID ); // TODO ?
    std::vector<Profile_t *> getProfiles();


    void createProfile();
    void commitProfile( const uint32_t _profileID );
    bool removeProfile( const uint32_t _profileID );


    void createItem( const uint32_t _profileID );
    void commitItem( const uint32_t _profileID, Item_t * _item );
    bool removeItem( const uint32_t _profileID, const uint32_t _itemID );

private:

    void createDatabase();
    void updatesProfiles();


    std::vector<Profile_t *>    m_profiles;

    class sqlite3 *             m_sqlClient;
};

