/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

#include "base.h"

#define SEND_LOBBY_RATE 5.0

#define PENDING_JOIN_TIMEOUT 10.0
#define REQUEST_LOBBY_DATA_TIMEOUT 6.0
#define LOBBY_DELETED_TIMEOUT 2

struct Pending_Joins {
    SteamAPICall_t api_id;
    CSteamID lobby_id;
    std::chrono::high_resolution_clock::time_point joined;
    bool message_sent;
};

struct Pending_Creates {
    SteamAPICall_t api_id;
    std::chrono::high_resolution_clock::time_point created;
    ELobbyType eLobbyType;
    int cMaxMembers;
};

struct Data_Requested {
    CSteamID lobby_id;
    std::chrono::high_resolution_clock::time_point requested;
};

struct Filter_Values {
	std::string key;
	std::string value_string;
	int value_int;
	bool is_int;
	ELobbyComparison eComparisonType;
};

struct Chat_Entry {
    std::string message;
    EChatEntryType type;
    CSteamID lobby_id, user_id;
};

#define FILTER_MAX_DEFAULT 4096


class Steam_Matchmaking :
public ISteamMatchmaking002,
public ISteamMatchmaking003,
public ISteamMatchmaking004,
public ISteamMatchmaking005,
public ISteamMatchmaking006,
public ISteamMatchmaking007,
public ISteamMatchmaking008,
public ISteamMatchmaking
{
    class Settings *settings;
    class Networking *network;
    class SteamCallResults *callback_results;
    class SteamCallBacks *callbacks;
    class RunEveryRunCB *run_every_runcb;

    std::vector<Lobby> lobbies;
    std::chrono::high_resolution_clock::time_point last_sent_lobbies;
    std::vector<struct Pending_Joins> pending_joins;
    std::vector<struct Pending_Creates> pending_creates;

    std::vector<struct Filter_Values> filter_values;
    int filter_max_results;
    std::vector<struct Filter_Values> filter_values_copy;
    int filter_max_results_copy;
    std::vector<CSteamID> filtered_lobbies;
    std::chrono::high_resolution_clock::time_point lobby_last_search;
    SteamAPICall_t search_call_api_id;
    bool searching;

    std::vector<struct Chat_Entry> chat_entries;
    std::vector<struct Data_Requested> data_requested;

    std::map<uint64, ::google::protobuf::Map<std::string, std::string>> self_lobby_member_data;
google::protobuf::Map<std::string,std::string>::const_iterator caseinsensitive_find(const ::google::protobuf::Map< ::std::string, ::std::string >& map, std::string key)
{
    auto x = map.begin();
    while (x != map.end()) {
        if (key.size() == x->first.size() && std::equal(x->first.begin(), x->first.end(), key.begin(),
                      [](char a, char b) {
                          return tolower(a) == tolower(b);
                      })) {
                          break;
                      }
        ++x;
    }

    return x;
}

Lobby *get_lobby(CSteamID id)
{
    if (!id.IsLobby())
        return NULL;

    auto lobby = std::find_if(lobbies.begin(), lobbies.end(), [&id](Lobby const& item) { return (item.room_id() & 0xFFFFFFFF) == (id.GetAccountID()); });
    if (lobbies.end() == lobby)
        return NULL;

    return &(*lobby);
}

void send_lobby_data()
{
    PRINT_DEBUG("Sending lobbies %zu\n", lobbies.size());

    for(auto & l: lobbies) {
        if (get_lobby_member(&l, settings->get_local_steam_id()) && l.owner() == settings->get_local_steam_id().ConvertToUint64() && !l.deleted()) {
            PRINT_DEBUG("Sending lobby %llu\n", l.room_id());
            Common_Message msg = Common_Message();
            msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
            msg.set_allocated_lobby(new Lobby(l));
            network->sendToAllIndividuals(&msg, true);
        }
    }
}

void trigger_lobby_dataupdate(CSteamID lobby, CSteamID member, bool success, double cb_timeout=0.005, bool send_changed_lobby=true)
{
    PRINT_DEBUG("Lobby dataupdate %llu %llu\n", lobby.ConvertToUint64(), member.ConvertToUint64());
    LobbyDataUpdate_t data;
    memset(&data, 0, sizeof(data));

    data.m_ulSteamIDLobby = lobby.ConvertToUint64();
    data.m_bSuccess = success;
    data.m_ulSteamIDMember = member.ConvertToUint64();
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), cb_timeout, true);

    if (lobby != member) {
        data.m_ulSteamIDMember = lobby.ConvertToUint64();
        //Is this really necessary?
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), cb_timeout, true);
    }

    Lobby *l = get_lobby(lobby);
    if (l && l->owner() == settings->get_local_steam_id().ConvertToUint64()) {
        if (send_changed_lobby) {
            Common_Message msg = Common_Message();
            msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
            msg.set_allocated_lobby(new Lobby(*l));
            network->sendToAllIndividuals(&msg, true);
        }
    }
}

void trigger_lobby_member_join_leave(CSteamID lobby, CSteamID member, bool leaving, bool success, double cb_timeout=0.0)
{
    LobbyChatUpdate_t data;
    data.m_ulSteamIDLobby = lobby.ConvertToUint64();
    data.m_ulSteamIDUserChanged = member.ConvertToUint64();
    data.m_ulSteamIDMakingChange = member.ConvertToUint64();
    uint32 member_state_change = 0; //EChatMemberStateChange

    if (!leaving) {
        member_state_change |= k_EChatMemberStateChangeEntered;
    } else {
        member_state_change |= k_EChatMemberStateChangeLeft;
    }

    data.m_rgfChatMemberStateChange = member_state_change;

    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), cb_timeout);
    //trigger_lobby_dataupdate(lobby, member, success, cb_timeout);
    trigger_lobby_dataupdate(lobby, lobby, success, cb_timeout);
}

bool send_owner_packet(CSteamID lobby_id, Lobby_Messages *message)
{
    Lobby *lobby = get_lobby(lobby_id);

    if (!lobby) {
        return false;
    }

    Common_Message msg;
    msg.set_allocated_lobby_messages(message);
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    msg.set_dest_id((uint64)lobby->owner());
    msg.mutable_lobby_messages()->set_id(lobby_id.ConvertToUint64());
    return network->sendTo(&msg, true);
}

bool send_clients_packet(CSteamID lobby_id, Lobby_Messages *message)
{
    Lobby *lobby = get_lobby(lobby_id);

    if (!lobby) {
        return false;
    }

    Common_Message msg;
    msg.set_allocated_lobby_messages(message);
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    msg.mutable_lobby_messages()->set_id(lobby_id.ConvertToUint64());
    return network->sendToAllIndividuals(&msg, true);
}

bool send_lobby_members_packet(CSteamID lobby_id, Lobby_Messages *message)
{
    Lobby *lobby = get_lobby(lobby_id);

    if (!lobby) {
        return false;
    }

    Common_Message msg;
    msg.set_allocated_lobby_messages(message);
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    msg.mutable_lobby_messages()->set_id(lobby_id.ConvertToUint64());

    for (auto & m : lobby->members()) {
        msg.set_dest_id((uint64)m.id());
        network->sendTo(&msg, true);
    }

    return true;
}

bool change_owner(Lobby *lobby, CSteamID new_owner)
{
    Lobby_Messages *message = new Lobby_Messages();
    message->set_type(Lobby_Messages::CHANGE_OWNER);
    message->set_idata(new_owner.ConvertToUint64());
    lobby->set_owner(new_owner.ConvertToUint64());
    send_owner_packet((uint64)lobby->room_id(), message);
    trigger_lobby_dataupdate((uint64)lobby->room_id(), (uint64)lobby->room_id(), true);
    return true;
}

void send_gameservercreated_cb(uint64 room_id, uint64 server_id, uint32 ip, uint16 port)
{
    LobbyGameCreated_t data;
    data.m_ulSteamIDLobby = room_id;
    data.m_ulSteamIDGameServer = server_id;
    data.m_unIP = ip;
    data.m_usPort = port;
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
}

void remove_lobbies()
{
    uint64 current_time = std::chrono::duration_cast<std::chrono::duration<uint64>>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto g = std::begin(lobbies);
    while (g != std::end(lobbies)) {
        if (g->members().size() == 0 || (g->deleted() && (g->time_deleted() + LOBBY_DELETED_TIMEOUT < current_time))) {
            PRINT_DEBUG("REMOVING LOBBY %llu\n", g->room_id());
            self_lobby_member_data.erase(g->room_id());
            g = lobbies.erase(g);
        } else {
            ++g;
        }
    }
}

void on_self_enter_leave_lobby(CSteamID id, int type, bool leaving)
{
    if (type == k_ELobbyTypeInvisible) return;
    if (!leaving) {
        settings->set_lobby(id);
    } else {
        settings->set_lobby(k_steamIDNil);
    }

    //TODO: handle cases where in two lobbies of type not invisible
    //steam says a user can only be in one regular lobby but we all know how well documented steam is
}

public:
static void steam_matchmaking_callback(void *object, Common_Message *msg)
{
    PRINT_DEBUG("steam_matchmaking_callback\n");

    Steam_Matchmaking *steam_matchmaking = (Steam_Matchmaking *)object;
    steam_matchmaking->Callback(msg);
}

static void steam_matchmaking_run_every_runcb(void *object)
{
    PRINT_DEBUG("steam_matchmaking_run_every_runcb\n");

    Steam_Matchmaking *steam_matchmaking = (Steam_Matchmaking *)object;
    steam_matchmaking->RunCallbacks();
}

Steam_Matchmaking(class Settings *settings, class Networking *network, class SteamCallResults *callback_results, class SteamCallBacks *callbacks, class RunEveryRunCB *run_every_runcb)
{
    this->settings = settings;
    this->network = network;
    this->run_every_runcb = run_every_runcb;
    this->network->setCallback(CALLBACK_ID_LOBBY, settings->get_local_steam_id(), &Steam_Matchmaking::steam_matchmaking_callback, this);
    this->network->setCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Matchmaking::steam_matchmaking_callback, this);
    this->run_every_runcb->add(&Steam_Matchmaking::steam_matchmaking_run_every_runcb, this);

    this->callback_results = callback_results;
    this->callbacks = callbacks;
    this->filter_max_results = FILTER_MAX_DEFAULT;
    search_call_api_id = 0;
    searching = false;
}

~Steam_Matchmaking()
{
    //TODO rm network callbacks
    this->run_every_runcb->remove(&Steam_Matchmaking::steam_matchmaking_run_every_runcb, this);
}

static Lobby_Member *get_lobby_member(Lobby *lobby, CSteamID user_id)
{
    if (!lobby) return NULL;

    auto member = std::find_if(lobby->mutable_members()->begin(), lobby->mutable_members()->end(), [&user_id](Lobby_Member const& item) { return item.id() == user_id.ConvertToUint64(); });
    if (lobby->mutable_members()->end() == member)
        return NULL;

    return &(*member);
}

// game server favorites storage
// saves basic details about a multiplayer game server locally

// returns the number of favorites servers the user has stored
int GetFavoriteGameCount()
{
    PRINT_DEBUG("GetFavoriteGameCount\n");
    return 0;
}


// returns the details of the game server
// iGame is of range [0,GetFavoriteGameCount())
// *pnIP, *pnConnPort are filled in the with IP:port of the game server
// *punFlags specify whether the game server was stored as an explicit favorite or in the history of connections
// *pRTime32LastPlayedOnServer is filled in the with the Unix time the favorite was added
bool GetFavoriteGame( int iGame, AppId_t *pnAppID, uint32 *pnIP, uint16 *pnConnPort, uint16 *pnQueryPort, uint32 *punFlags, uint32 *pRTime32LastPlayedOnServer )
{
    PRINT_DEBUG("GetFavoriteGame\n");
    return false;
}


// adds the game server to the local list; updates the time played of the server if it already exists in the list
int AddFavoriteGame( AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags, uint32 rTime32LastPlayedOnServer )
{
    PRINT_DEBUG("AddFavoriteGame %lu %lu %hu %hu %lu %lu\n", nAppID, nIP, nConnPort, nQueryPort, unFlags, rTime32LastPlayedOnServer);
    //TODO: what should this return?
    return 0;
}


// removes the game server from the local storage; returns true if one was removed
bool RemoveFavoriteGame( AppId_t nAppID, uint32 nIP, uint16 nConnPort, uint16 nQueryPort, uint32 unFlags )
{
    PRINT_DEBUG("RemoveFavoriteGame\n");
    return false;
}


///////
// Game lobby functions

// Get a list of relevant lobbies
// this is an asynchronous request
// results will be returned by LobbyMatchList_t callback & call result, with the number of lobbies found
// this will never return lobbies that are full
// to add more filter, the filter calls below need to be call before each and every RequestLobbyList() call
// use the CCallResult<> object in steam_api.h to match the SteamAPICall_t call result to a function in an object, e.g.
/*
    class CMyLobbyListManager
    {
        CCallResult<CMyLobbyListManager, LobbyMatchList_t> m_CallResultLobbyMatchList;
        void FindLobbies()
        {
            // SteamMatchmaking()->AddRequestLobbyListFilter*() functions would be called here, before RequestLobbyList();

            m_CallResultLobbyMatchList.Set( hSteamAPICall, this, &CMyLobbyListManager::OnLobbyMatchList );

        }

        void OnLobbyMatchList( LobbyMatchList_t *pLobbyMatchList, bool bIOFailure )
        {
            // lobby list has be retrieved from Steam back-end, use results
        }
    }
*/
// 
#define LOBBY_SEARCH_TIMEOUT 0.2 //Tested on real steam
STEAM_CALL_RESULT( LobbyMatchList_t )
SteamAPICall_t RequestLobbyList()
{
    PRINT_DEBUG("RequestLobbyList\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    filtered_lobbies.clear();
    lobby_last_search = std::chrono::high_resolution_clock::now();
    filter_values_copy = filter_values;
    filter_max_results_copy = filter_max_results;
    filter_values.clear();
    filter_max_results = FILTER_MAX_DEFAULT;
    searching = true;
    if (search_call_api_id) callback_results->rmCallBack(search_call_api_id, NULL);
    search_call_api_id = callback_results->reserveCallResult();



    
    return search_call_api_id;
}

void RequestLobbyList_OLD()
{
    RequestLobbyList();
}

// filters for lobbies
// this needs to be called before RequestLobbyList() to take effect
// these are cleared on each call to RequestLobbyList()
void AddRequestLobbyListStringFilter( const char *pchKeyToMatch, const char *pchValueToMatch, ELobbyComparison eComparisonType )
{
    PRINT_DEBUG("AddRequestLobbyListStringFilter %s %s %i\n", pchKeyToMatch, pchValueToMatch, eComparisonType);
    if (!pchValueToMatch) return;

    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    struct Filter_Values fv;
    fv.key = std::string(pchKeyToMatch);
    fv.value_string = std::string(pchValueToMatch);
    fv.is_int = false;
    fv.eComparisonType = eComparisonType;
    filter_values.push_back(fv);

}

// numerical comparison
void AddRequestLobbyListNumericalFilter( const char *pchKeyToMatch, int nValueToMatch, ELobbyComparison eComparisonType )
{
    PRINT_DEBUG("AddRequestLobbyListNumericalFilter %s %i %i\n", pchKeyToMatch, nValueToMatch, eComparisonType);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    struct Filter_Values fv;
    fv.key = std::string(pchKeyToMatch);
    fv.value_int = nValueToMatch;
    fv.is_int = true;
    fv.eComparisonType = eComparisonType;
    filter_values.push_back(fv);

}

// returns results closest to the specified value. Multiple near filters can be added, with early filters taking precedence
void AddRequestLobbyListNearValueFilter( const char *pchKeyToMatch, int nValueToBeCloseTo )
{
    PRINT_DEBUG("AddRequestLobbyListNearValueFilter %s %u\n", pchKeyToMatch, nValueToBeCloseTo);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    
}

// returns only lobbies with the specified number of slots available
void AddRequestLobbyListFilterSlotsAvailable( int nSlotsAvailable )
{
    PRINT_DEBUG("AddRequestLobbyListFilterSlotsAvailable %i\n", nSlotsAvailable);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    
}

// sets the distance for which we should search for lobbies (based on users IP address to location map on the Steam backed)
void AddRequestLobbyListDistanceFilter( ELobbyDistanceFilter eLobbyDistanceFilter )
{
    PRINT_DEBUG("AddRequestLobbyListDistanceFilter %i\n", eLobbyDistanceFilter);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    
}

// sets how many results to return, the lower the count the faster it is to download the lobby results & details to the client
void AddRequestLobbyListResultCountFilter( int cMaxResults )
{
    PRINT_DEBUG("AddRequestLobbyListResultCountFilter %i\n", cMaxResults);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    filter_max_results = cMaxResults;
    
}


void AddRequestLobbyListCompatibleMembersFilter( CSteamID steamIDLobby )
{
    PRINT_DEBUG("AddRequestLobbyListCompatibleMembersFilter\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    
}

void AddRequestLobbyListFilter( const char *pchKeyToMatch, const char *pchValueToMatch )
{
	AddRequestLobbyListStringFilter(pchKeyToMatch, pchValueToMatch, k_ELobbyComparisonEqual);
}

void AddRequestLobbyListNumericalFilter( const char *pchKeyToMatch, int nValueToMatch, int nComparisonType )
{
	AddRequestLobbyListNumericalFilter(pchKeyToMatch, nValueToMatch, (ELobbyComparison) nComparisonType );
}

void AddRequestLobbyListSlotsAvailableFilter()
{
    
}

// returns the CSteamID of a lobby, as retrieved by a RequestLobbyList call
// should only be called after a LobbyMatchList_t callback is received
// iLobby is of the range [0, LobbyMatchList_t::m_nLobbiesMatching)
// the returned CSteamID::IsValid() will be false if iLobby is out of range
CSteamID GetLobbyByIndex( int iLobby )
{
    PRINT_DEBUG("GetLobbyByIndex %i\n", iLobby);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    CSteamID id = k_steamIDNil;
    if (0 <= iLobby && iLobby < filtered_lobbies.size()) id = filtered_lobbies[iLobby];
    PRINT_DEBUG("Lobby %llu\n", id.ConvertToUint64());
    return id;
}

static bool enter_lobby(Lobby *lobby, CSteamID id)
{
    if (get_lobby_member(lobby, id)) return false;

    Lobby_Member *member = lobby->add_members();
    member->set_id(id.ConvertToUint64());
    return true;
}

static bool leave_lobby(Lobby *lobby, CSteamID id)
{
    auto member = std::find_if(lobby->mutable_members()->begin(), lobby->mutable_members()->end(), [&id](Lobby_Member const& item) { return item.id() == id.ConvertToUint64(); });
    if (member != lobby->mutable_members()->end()) {
        lobby->mutable_members()->erase(member);
        return true;
    }

    return false;
}


#define LOBBY_CREATE_DELAY 0.07 //artificial delay for lobby creation

void Create_pending_lobbies()
{
    auto p_c = std::begin(pending_creates);
    while (p_c != std::end(pending_creates)) {
        if (check_timedout(p_c->created, LOBBY_CREATE_DELAY)) {
            Lobby lobby;
            CSteamID lobby_id = generate_steam_id_lobby();
            lobby.set_room_id(lobby_id.ConvertToUint64());
            lobby.set_joinable(true);
            lobby.set_member_limit(p_c->cMaxMembers);
            lobby.set_type(p_c->eLobbyType);
            lobby.set_owner(settings->get_local_steam_id().ConvertToUint64());
            lobby.set_appid(settings->get_local_game_id().AppID());
            enter_lobby(&lobby, settings->get_local_steam_id());
            lobbies.push_back(lobby);

            if (settings->disable_lobby_creation) {
                LobbyCreated_t data;
                data.m_eResult = k_EResultFail;
                data.m_ulSteamIDLobby = 0;
                callback_results->addCallResult(p_c->api_id, data.k_iCallback, &data, sizeof(data));
                callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
            } else {
                LobbyCreated_t data;
                data.m_eResult = k_EResultOK;
                data.m_ulSteamIDLobby = lobby.room_id();
                callback_results->addCallResult(p_c->api_id, data.k_iCallback, &data, sizeof(data));
                callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));

                {
                    LobbyEnter_t data;
                    data.m_ulSteamIDLobby = lobby.room_id();
                    data.m_rgfChatPermissions = 0; //Unused - Always 0
                    if (p_c->eLobbyType == k_ELobbyTypePrivate)
                        data.m_bLocked = true;
                    else
                        data.m_bLocked = false;
                    data.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseSuccess;
                    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
                }

                on_self_enter_leave_lobby(lobby_id, p_c->eLobbyType, false);
                trigger_lobby_dataupdate(lobby_id, lobby_id, true);
            }

            p_c = pending_creates.erase(p_c);
        } else {
            ++p_c;
        }
    }
}

// Create a lobby on the Steam servers.
// If private, then the lobby will not be returned by any RequestLobbyList() call; the CSteamID
// of the lobby will need to be communicated via game channels or via InviteUserToLobby()
// this is an asynchronous request
// results will be returned by LobbyCreated_t callback and call result; lobby is joined & ready to use at this point
// a LobbyEnter_t callback will also be received (since the local user is joining their own lobby)
STEAM_CALL_RESULT( LobbyCreated_t )
SteamAPICall_t CreateLobby( ELobbyType eLobbyType, int cMaxMembers )
{
    PRINT_DEBUG("CreateLobby type: %i max_members: %i\n", eLobbyType, cMaxMembers);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    struct Pending_Creates p_c;
    p_c.api_id = callback_results->reserveCallResult();
    p_c.eLobbyType = eLobbyType;
    p_c.cMaxMembers = cMaxMembers;
    p_c.created = std::chrono::high_resolution_clock::now();
    pending_creates.push_back(p_c);
    return p_c.api_id;
}

SteamAPICall_t CreateLobby( ELobbyType eLobbyType )
{
	return CreateLobby(eLobbyType, 0);
}

void CreateLobby_OLD( ELobbyType eLobbyType )
{
    CreateLobby(eLobbyType);
}

void CreateLobby( bool bPrivate )
{
    CreateLobby(bPrivate ? k_ELobbyTypePrivate : k_ELobbyTypePublic);
}

// Joins an existing lobby
// this is an asynchronous request
// results will be returned by LobbyEnter_t callback & call result, check m_EChatRoomEnterResponse to see if was successful
// lobby metadata is available to use immediately on this call completing
STEAM_CALL_RESULT( LobbyEnter_t )
SteamAPICall_t JoinLobby( CSteamID steamIDLobby )
{
    PRINT_DEBUG("JoinLobby %llu\n", steamIDLobby.ConvertToUint64());
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    auto pj = std::find_if(pending_joins.begin(), pending_joins.end(), [&steamIDLobby](Pending_Joins const& item) {return item.lobby_id == steamIDLobby;});
    if (pj != pending_joins.end()) return pj->api_id;

    Pending_Joins pending_join;
    pending_join.api_id = callback_results->reserveCallResult();
    pending_join.lobby_id = steamIDLobby;
    pending_join.joined = std::chrono::high_resolution_clock::now();
    pending_joins.push_back(pending_join);

    Lobby_Messages *message = new Lobby_Messages();
    message->set_type(Lobby_Messages::JOIN);
    pending_join.message_sent = send_owner_packet(steamIDLobby, message);

    return pending_join.api_id;
}

void JoinLobby_OLD( CSteamID steamIDLobby )
{
    JoinLobby(steamIDLobby);
}

// Leave a lobby; this will take effect immediately on the client side
// other users in the lobby will be notified by a LobbyChatUpdate_t callback
void LeaveLobby( CSteamID steamIDLobby )
{
    PRINT_DEBUG("LeaveLobby\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    PRINT_DEBUG("LeaveLobby pass mutex\n");
    Lobby *lobby = get_lobby(steamIDLobby);
    if (lobby) {
        if (!lobby->deleted()) {
            on_self_enter_leave_lobby((uint64)lobby->room_id(), lobby->type(), true);
            self_lobby_member_data.erase(lobby->room_id());
            if (lobby->owner() != settings->get_local_steam_id().ConvertToUint64()) {
                PRINT_DEBUG("LeaveLobby not owner\n");
                leave_lobby(&(*lobby), settings->get_local_steam_id());
                Lobby_Messages *message = new Lobby_Messages();
                message->set_type(Lobby_Messages::LEAVE);
                send_owner_packet(steamIDLobby, message);
            } else {
                PRINT_DEBUG("LeaveLobby owner\n");
                Lobby_Messages *message = new Lobby_Messages();
                message->set_type(Lobby_Messages::LEAVE);

                if (lobby->members().size() > 1) {
                    leave_lobby(&(*lobby), settings->get_local_steam_id());
                    change_owner(&(*lobby), (uint64)lobby->members(0).id());
                    send_owner_packet(steamIDLobby, message);
                } else {
                    send_clients_packet(steamIDLobby, message);
                    lobby->set_deleted(true);
                    lobby->set_time_deleted(std::chrono::duration_cast<std::chrono::duration<uint64>>(std::chrono::system_clock::now().time_since_epoch()).count());
                }
            }
        }
    }

    PRINT_DEBUG("LeaveLobby Done\n");
    
}


// Invite another user to the lobby
// the target user will receive a LobbyInvite_t callback
// will return true if the invite is successfully sent, whether or not the target responds
// returns false if the local user is not connected to the Steam servers
// if the other user clicks the join link, a GameLobbyJoinRequested_t will be posted if the user is in-game,
// or if the game isn't running yet the game will be launched with the parameter +connect_lobby <64-bit lobby id>
bool InviteUserToLobby( CSteamID steamIDLobby, CSteamID steamIDInvitee )
{
    PRINT_DEBUG("InviteUserToLobby\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    if (!lobby) return false;

    Common_Message msg;
    Friend_Messages *friend_messages = new Friend_Messages();
    friend_messages->set_type(Friend_Messages::LOBBY_INVITE);
    friend_messages->set_lobby_id(steamIDLobby.ConvertToUint64());
    msg.set_allocated_friend_messages(friend_messages);
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    msg.set_dest_id(steamIDInvitee.ConvertToUint64());
    return network->sendTo(&msg, true);
}


// Lobby iteration, for viewing details of users in a lobby
// only accessible if the lobby user is a member of the specified lobby
// persona information for other lobby members (name, avatar, etc.) will be asynchronously received
// and accessible via ISteamFriends interface

// returns the number of users in the specified lobby
int GetNumLobbyMembers( CSteamID steamIDLobby )
{
    PRINT_DEBUG("GetNumLobbyMembers %llu\n", steamIDLobby.ConvertToUint64());
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    int ret = 0;
    if (lobby) ret = lobby->members().size();

    PRINT_DEBUG("Number: %i\n", ret);
    return ret;
}

// returns the CSteamID of a user in the lobby
// iMember is of range [0,GetNumLobbyMembers())
// note that the current user must be in a lobby to retrieve CSteamIDs of other users in that lobby
CSteamID GetLobbyMemberByIndex( CSteamID steamIDLobby, int iMember )
{
    PRINT_DEBUG("GetLobbyMemberByIndex %llu %i\n", steamIDLobby.ConvertToUint64(), iMember);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    CSteamID id = k_steamIDNil;
    if (lobby && !lobby->deleted() && lobby->members().size() > iMember && iMember >= 0) id = (uint64)lobby->members(iMember).id();
    PRINT_DEBUG("Member: %llu\n", id.ConvertToUint64());
    return id;
}


// Get data associated with this lobby
// takes a simple key, and returns the string associated with it
// "" will be returned if no value is set, or if steamIDLobby is invalid
const char *GetLobbyData( CSteamID steamIDLobby, const char *pchKey )
{
    PRINT_DEBUG("GetLobbyData %llu %s\n", steamIDLobby.ConvertToUint64(), pchKey);
    if (!pchKey) return "";
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    const char *ret = "";
    if (lobby) {
        auto result = caseinsensitive_find(lobby->values(), pchKey);
        if (result != lobby->values().end()) ret = result->second.c_str();
    }

    PRINT_DEBUG("returned %s\n", ret);
    return ret;
}

// Sets a key/value pair in the lobby metadata
// each user in the lobby will be broadcast this new value, and any new users joining will receive any existing data
// this can be used to set lobby names, map, etc.
// to reset a key, just set it to ""
// other users in the lobby will receive notification of the lobby data change via a LobbyDataUpdate_t callback
bool SetLobbyData( CSteamID steamIDLobby, const char *pchKey, const char *pchValue )
{
    PRINT_DEBUG("SetLobbyData %llu %s %s\n", steamIDLobby.ConvertToUint64(), pchKey, pchValue);
    if (!pchKey) return false;
    char empty_string[] = "";
    if (!pchValue) pchValue = empty_string;
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    Lobby *lobby = get_lobby(steamIDLobby);
    if (!lobby || lobby->deleted()) {
        return false;
    }

    bool changed = true;
    //callback is always triggered when setlobbydata is called from non owner however no data is actually changed.
    if (lobby->owner() == settings->get_local_steam_id().ConvertToUint64()) {
        auto result = caseinsensitive_find(lobby->values(), pchKey);
        if (result == lobby->values().end()) {
            (*lobby->mutable_values())[pchKey] = pchValue;
        } else {
            if (result->second == std::string(pchValue)) changed = false;
            (*lobby->mutable_values())[result->first] = pchValue;
        }
    }

    if (changed)
        trigger_lobby_dataupdate(steamIDLobby, steamIDLobby, true, 0.005, changed);

    return true;
}


// returns the number of metadata keys set on the specified lobby
int GetLobbyDataCount( CSteamID steamIDLobby )
{
    PRINT_DEBUG("GetLobbyDataCount\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    int size = 0;
    if (lobby) size = lobby->values().size();

    
    return size;
}


// returns a lobby metadata key/values pair by index, of range [0, GetLobbyDataCount())
bool GetLobbyDataByIndex( CSteamID steamIDLobby, int iLobbyData, char *pchKey, int cchKeyBufferSize, char *pchValue, int cchValueBufferSize )
{
    PRINT_DEBUG("GetLobbyDataByIndex\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    bool ret = false;

    if (lobby && lobby->values().size() > iLobbyData && iLobbyData >= 0) {
        auto lobby_data = lobby->values().begin();
        for (int i = 0; i < iLobbyData; ++i) ++lobby_data;
        if (cchKeyBufferSize) strncpy(pchKey, lobby_data->first.c_str(), cchKeyBufferSize - 1);
        pchKey[cchKeyBufferSize - 1] = 0;

        if (cchValueBufferSize) strncpy(pchValue, lobby_data->second.c_str(), cchValueBufferSize - 1);
        pchValue[cchValueBufferSize - 1] = 0;

        PRINT_DEBUG("GetLobbyDataByIndex ret %s %s\n", pchKey, pchValue);
        ret = true;
    }

    
    return ret;
}


// removes a metadata key from the lobby
bool DeleteLobbyData( CSteamID steamIDLobby, const char *pchKey )
{
    PRINT_DEBUG("DeleteLobbyData\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    if (!lobby || lobby->owner() != settings->get_local_steam_id().ConvertToUint64() || lobby->deleted()) {
        return false;
    }

    lobby->mutable_values()->erase(pchKey);
    trigger_lobby_dataupdate(steamIDLobby, steamIDLobby, true);
    
    return true;
}


// Gets per-user metadata for someone in this lobby
const char *GetLobbyMemberData( CSteamID steamIDLobby, CSteamID steamIDUser, const char *pchKey )
{
    PRINT_DEBUG("GetLobbyMemberData %s %llu %llu\n", pchKey, steamIDLobby.ConvertToUint64(), steamIDUser.ConvertToUint64());
    if (!pchKey) return "";
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    struct Lobby_Member *member = get_lobby_member(get_lobby(steamIDLobby), steamIDUser);
    const char *ret = "";
    if (member) {
        if (steamIDUser == settings->get_local_steam_id()) {
            auto result = self_lobby_member_data.find(steamIDLobby.ConvertToUint64());
            if (result != self_lobby_member_data.end()) {
                auto value = caseinsensitive_find(result->second, std::string(pchKey));
                if (value != result->second.end()) {
                    ret = value->second.c_str();
                }
            }
        } else {
            auto result = caseinsensitive_find(member->values(), std::string(pchKey));
            if (result == member->values().end()) return "";
            ret = result->second.c_str();
        }
    }

    PRINT_DEBUG("GetLobbyMemberData res %s\n", ret);
    return ret;
}

// Sets per-user metadata (for the local user implicitly)
void SetLobbyMemberData( CSteamID steamIDLobby, const char *pchKey, const char *pchValue )
{
    PRINT_DEBUG("SetLobbyMemberData %s %s\n", pchKey, pchValue);
    if (!pchKey) return;
    char empty_string[] = "";
    if (!pchValue) pchValue = empty_string;

    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    if (!lobby || lobby->deleted()) return;

    Lobby_Member *member = get_lobby_member(lobby, settings->get_local_steam_id());
    if (member) {
        if (lobby->owner() == settings->get_local_steam_id().ConvertToUint64()) {
            auto result = caseinsensitive_find(member->values(), std::string(pchKey));
            if (result == member->values().end()) {
                (*member->mutable_values())[pchKey] = pchValue;
            } else {
                (*member->mutable_values())[result->first] = pchValue;
            }
            trigger_lobby_dataupdate(steamIDLobby, (uint64)member->id(), true);
        } else {
            Lobby_Messages *message = new Lobby_Messages();
            message->set_type(Lobby_Messages::MEMBER_DATA);
            (*message->mutable_map())[pchKey] = pchValue;
            send_owner_packet(steamIDLobby, message);
        }

        {
            auto result = self_lobby_member_data.find(steamIDLobby.ConvertToUint64());
            if (result != self_lobby_member_data.end()) {
                auto value = caseinsensitive_find(result->second, std::string(pchKey));
                if (value != result->second.end()) {
                    self_lobby_member_data[steamIDLobby.ConvertToUint64()][value->first] = pchValue;
                } else {
                    self_lobby_member_data[steamIDLobby.ConvertToUint64()][pchKey] = pchValue;
                }
            } else {
                self_lobby_member_data[steamIDLobby.ConvertToUint64()][pchKey] = pchValue;
            }
        }
    }
}


// Broadcasts a chat message to the all the users in the lobby
// users in the lobby (including the local user) will receive a LobbyChatMsg_t callback
// returns true if the message is successfully sent
// pvMsgBody can be binary or text data, up to 4k
// if pvMsgBody is text, cubMsgBody should be strlen( text ) + 1, to include the null terminator
bool SendLobbyChatMsg( CSteamID steamIDLobby, const void *pvMsgBody, int cubMsgBody )
{
    PRINT_DEBUG("SendLobbyChatMsg %i\n", cubMsgBody);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    if (!lobby || lobby->deleted()) return false;

    Lobby_Messages *message = new Lobby_Messages();
    message->set_type(Lobby_Messages::CHAT_MESSAGE);
    message->set_bdata(pvMsgBody, cubMsgBody);
    return send_lobby_members_packet(steamIDLobby, message);
}

// Get a chat message as specified in a LobbyChatMsg_t callback
// iChatID is the LobbyChatMsg_t::m_iChatID value in the callback
// *pSteamIDUser is filled in with the CSteamID of the member
// *pvData is filled in with the message itself
// return value is the number of bytes written into the buffer
int GetLobbyChatEntry( CSteamID steamIDLobby, int iChatID, STEAM_OUT_STRUCT() CSteamID *pSteamIDUser, void *pvData, int cubData, EChatEntryType *peChatEntryType )
{
    PRINT_DEBUG("GetLobbyChatEntry %llu %i %p %p %i %p\n", steamIDLobby.ConvertToUint64(), iChatID, pSteamIDUser, pvData, cubData, peChatEntryType);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (iChatID >= chat_entries.size() || iChatID < 0 || cubData < 0) return 0;
    if (chat_entries[iChatID].lobby_id != steamIDLobby) return 0;
    if (pSteamIDUser) *pSteamIDUser = chat_entries[iChatID].user_id;
    if (peChatEntryType) *peChatEntryType = chat_entries[iChatID].type;
    if (pvData) {
        if (chat_entries[iChatID].message.size() <= cubData) {
            cubData = chat_entries[iChatID].message.size();
            memcpy(pvData, chat_entries[iChatID].message.data(), cubData);
            PRINT_DEBUG("Returned chat of len: %i\n", cubData);
            return cubData;
        }
    }

    return 0;
}


// Refreshes metadata for a lobby you're not necessarily in right now
// you never do this for lobbies you're a member of, only if your
// this will send down all the metadata associated with a lobby
// this is an asynchronous call
// returns false if the local user is not connected to the Steam servers
// results will be returned by a LobbyDataUpdate_t callback
// if the specified lobby doesn't exist, LobbyDataUpdate_t::m_bSuccess will be set to false
bool RequestLobbyData( CSteamID steamIDLobby )
{
    PRINT_DEBUG("RequestLobbyData\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    struct Data_Requested requested;
    requested.lobby_id = steamIDLobby;
    requested.requested = std::chrono::high_resolution_clock::now();
    data_requested.push_back(requested);
    return true;
}

// sets the game server associated with the lobby
// usually at this point, the users will join the specified game server
// either the IP/Port or the steamID of the game server has to be valid, depending on how you want the clients to be able to connect
void SetLobbyGameServer( CSteamID steamIDLobby, uint32 unGameServerIP, uint16 unGameServerPort, CSteamID steamIDGameServer )
{
    PRINT_DEBUG("SetLobbyGameServer %llu %llu %hhu.%hhu.%hhu.%hhu:%hu\n", steamIDLobby.ConvertToUint64(), steamIDGameServer.ConvertToUint64(), ((unsigned char *)&unGameServerIP)[3], ((unsigned char *)&unGameServerIP)[2], ((unsigned char *)&unGameServerIP)[1], ((unsigned char *)&unGameServerIP)[0], unGameServerPort);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    if (lobby) {
        if (lobby->deleted()) return;

        lobby->mutable_gameserver()->set_id(steamIDGameServer.ConvertToUint64());
        lobby->mutable_gameserver()->set_ip(unGameServerIP);
        lobby->mutable_gameserver()->set_port(unGameServerPort);
        lobby->mutable_gameserver()->set_num_update(lobby->gameserver().num_update() + 1);

        send_gameservercreated_cb(lobby->room_id(), lobby->gameserver().id(), lobby->gameserver().ip(), lobby->gameserver().port());
        trigger_lobby_dataupdate(steamIDLobby, steamIDLobby, true);
    }

    
}

// returns the details of a game server set in a lobby - returns false if there is no game server set, or that lobby doesn't exist
bool GetLobbyGameServer( CSteamID steamIDLobby, uint32 *punGameServerIP, uint16 *punGameServerPort, STEAM_OUT_STRUCT() CSteamID *psteamIDGameServer )
{
    PRINT_DEBUG("GetLobbyGameServer\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    if (!lobby) {
        
        return false;
    }

    CSteamID server_id((uint64)lobby->gameserver().id());
    if (server_id.IsValid() || lobby->gameserver().port()) {
        if (psteamIDGameServer) *psteamIDGameServer = server_id;
        if (punGameServerIP) *punGameServerIP = lobby->gameserver().ip();
        if (punGameServerPort) *punGameServerPort = lobby->gameserver().port();
        
        return true;
    }

    
    return false;
}


// set the limit on the # of users who can join the lobby
bool SetLobbyMemberLimit( CSteamID steamIDLobby, int cMaxMembers )
{
    PRINT_DEBUG("SetLobbyMemberLimit\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    if (!lobby || lobby->owner() != settings->get_local_steam_id().ConvertToUint64() || lobby->deleted()) {
        
        return false;
    }

    lobby->set_member_limit(cMaxMembers);
    trigger_lobby_dataupdate(steamIDLobby, steamIDLobby, true);
    
    return true;
}

// returns the current limit on the # of users who can join the lobby; returns 0 if no limit is defined
int GetLobbyMemberLimit( CSteamID steamIDLobby )
{
    PRINT_DEBUG("GetLobbyMemberLimit\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    int limit = 0;
    if (lobby) limit = lobby->member_limit();

    PRINT_DEBUG("limit %i\n", limit);    
    return limit;
}

void SetLobbyVoiceEnabled( CSteamID steamIDLobby, bool bVoiceEnabled )
{
    PRINT_DEBUG("SetLobbyVoiceEnabled\n");
}

// updates which type of lobby it is
// only lobbies that are k_ELobbyTypePublic or k_ELobbyTypeInvisible, and are set to joinable, will be returned by RequestLobbyList() calls
bool SetLobbyType( CSteamID steamIDLobby, ELobbyType eLobbyType )
{
    PRINT_DEBUG("SetLobbyType %i\n", eLobbyType);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    if (!lobby || lobby->owner() != settings->get_local_steam_id().ConvertToUint64() || lobby->deleted()) {
        return false;
    }

    if (lobby->type() != eLobbyType) {
        //maybe rename those functions?
        if (lobby->type() == k_ELobbyTypeInvisible) on_self_enter_leave_lobby(steamIDLobby, eLobbyType, false);
        if (eLobbyType == k_ELobbyTypeInvisible) on_self_enter_leave_lobby(steamIDLobby, lobby->type(), true);

        lobby->set_type(eLobbyType);
        trigger_lobby_dataupdate(steamIDLobby, steamIDLobby, true);
    }

    return true;
}


// sets whether or not a lobby is joinable - defaults to true for a new lobby
// if set to false, no user can join, even if they are a friend or have been invited
bool SetLobbyJoinable( CSteamID steamIDLobby, bool bLobbyJoinable )
{
    PRINT_DEBUG("SetLobbyJoinable %u\n", bLobbyJoinable);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    if (!lobby || lobby->owner() != settings->get_local_steam_id().ConvertToUint64() || lobby->deleted()) {
        return false;
    }

    if (lobby->joinable() != bLobbyJoinable) {
        lobby->set_joinable(bLobbyJoinable);
        trigger_lobby_dataupdate(steamIDLobby, steamIDLobby, true);
    }

    return true;
}


// returns the current lobby owner
// you must be a member of the lobby to access this (Mr_Goldberg note: This is a lie)
// there always one lobby owner - if the current owner leaves, another user will become the owner
// it is possible (bur rare) to join a lobby just as the owner is leaving, thus entering a lobby with self as the owner
CSteamID GetLobbyOwner( CSteamID steamIDLobby )
{
    PRINT_DEBUG("GetLobbyOwner %llu\n", steamIDLobby.ConvertToUint64());
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    if (!lobby || lobby->deleted()) return k_steamIDNil;

    //TODO: might be better to require the lobby info to be at least requested first.
    return (uint64)lobby->owner();
}

// asks the Steam servers for a list of lobbies that friends are in
// returns results by posting one RequestFriendsLobbiesResponse_t callback per friend/lobby pair
// if no friends are in lobbies, RequestFriendsLobbiesResponse_t will be posted but with 0 results
// filters don't apply to lobbies (currently)
bool RequestFriendsLobbies()
{
    PRINT_DEBUG("RequestFriendsLobbies\n");
    RequestFriendsLobbiesResponse_t data = {};
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
    return true;
}

float GetLobbyDistance( CSteamID steamIDLobby )
{
    PRINT_DEBUG("GetLobbyDistance %llu\n", steamIDLobby.ConvertToUint64());
    return 0.0;
}

// changes who the lobby owner is
// you must be the lobby owner for this to succeed, and steamIDNewOwner must be in the lobby
// after completion, the local user will no longer be the owner
bool SetLobbyOwner( CSteamID steamIDLobby, CSteamID steamIDNewOwner )
{
    PRINT_DEBUG("SetLobbyOwner\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Lobby *lobby = get_lobby(steamIDLobby);
    if (!lobby || lobby->owner() != settings->get_local_steam_id().ConvertToUint64() || lobby->deleted()) return false;
    Lobby_Member *member = get_lobby_member(lobby, steamIDNewOwner);
    if (member) {
        change_owner(&(*lobby), (uint64)member->id());
        trigger_lobby_dataupdate(steamIDLobby, steamIDLobby, true);
        
        return true;
    }

    
    return false;
}


// link two lobbies for the purposes of checking player compatibility
// you must be the lobby owner of both lobbies
bool SetLinkedLobby( CSteamID steamIDLobby, CSteamID steamIDLobbyDependent )
{
    PRINT_DEBUG("SetLinkedLobby\n");
    return false;
}

void RunBackground()
{
    remove_lobbies();
    Create_pending_lobbies();

    if (check_timedout(last_sent_lobbies, SEND_LOBBY_RATE)) {
        send_lobby_data();
        last_sent_lobbies = std::chrono::high_resolution_clock::now();
    }
}

void RunCallbacks()
{
    RunBackground();

    if (searching) {
        PRINT_DEBUG("Searching for lobbies %zu\n", lobbies.size());
        for(auto & l: lobbies) {
            bool use = l.joinable() && (l.type() == k_ELobbyTypePublic || l.type() == k_ELobbyTypeInvisible || l.type() == k_ELobbyTypeFriendsOnly) && !l.deleted();
            PRINT_DEBUG("use lobby: %u, filters: %zu, joinable: %u, type: %u, deleted: %u\n", use, filter_values_copy.size(), l.joinable(), l.type(), l.deleted());
            for (auto & f : filter_values_copy) {
                PRINT_DEBUG("%s:%s/%i %u %i\n", f.key.c_str(), f.value_string.c_str(), f.value_int, f.is_int, f.eComparisonType);
                auto value = caseinsensitive_find(l.values(), f.key);
                if (value != l.values().end()) {
                    //TODO: eComparisonType
                    if (!f.is_int) {
                        PRINT_DEBUG("Compare Values %s %s\n", value->second.c_str(), f.value_string.c_str());
                        if (f.eComparisonType == k_ELobbyComparisonEqual) {
                            if (value->second == f.value_string) {
                                PRINT_DEBUG("Equal\n");
                                //use = use;
                            } else {
                                PRINT_DEBUG("Not Equal\n");
                                use = false;
                            }
                        }
                    } else {
                        try {
                            PRINT_DEBUG("%s\n", value->second.c_str());
                            int compare_to = 0;
                            //TODO: check if this is how real steam behaves
                            if (value->second.size()) {
                                compare_to = std::stoll(value->second, 0, 0);
                            }
                            PRINT_DEBUG("Compare Values %i %i\n", compare_to, f.value_int);
                            if (f.eComparisonType == k_ELobbyComparisonEqual) {
                                if (compare_to == f.value_int) {
                                    PRINT_DEBUG("Equal\n");
                                    //use = use;
                                } else {
                                    PRINT_DEBUG("Not Equal\n");
                                    use = false;
                                }
                            }
                        } catch (...) {
                            //Same case as if the key is not in the lobby?
                            use = false;
                        }
                        //TODO: add more comparisons
                    }
                } else {
                    PRINT_DEBUG("Compare Key not in lobby\n");
                    if (f.eComparisonType == k_ELobbyComparisonEqual) {
                        //If the key is not in the lobby do we take it into account?
                        use = false;
                    }
                }
            }

            PRINT_DEBUG("Lobby %llu use %u\n", l.room_id(), use);
            if (use) PUSH_BACK_IF_NOT_IN(filtered_lobbies, (uint64)l.room_id());
            if (filtered_lobbies.size() >= filter_max_results_copy) {
                searching = false;
                LobbyMatchList_t data;
                data.m_nLobbiesMatching = filtered_lobbies.size();
                callback_results->addCallResult(search_call_api_id, data.k_iCallback, &data, sizeof(data));
                callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
                search_call_api_id = 0;
            }
        }
    }

    if (searching && check_timedout(lobby_last_search, LOBBY_SEARCH_TIMEOUT)) {
        PRINT_DEBUG("LOBBY_SEARCH_TIMEOUT %zu\n", filtered_lobbies.size());
        LobbyMatchList_t data;
        data.m_nLobbiesMatching = filtered_lobbies.size();
        callback_results->addCallResult(search_call_api_id, data.k_iCallback, &data, sizeof(data));
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
        searching = false;
        search_call_api_id = 0;
    }

    auto g = std::begin(pending_joins);
    while (g != std::end(pending_joins)) {
        if (!g->message_sent) {
            PRINT_DEBUG("Resend join lobby\n");
            Lobby_Messages *message = new Lobby_Messages();
            message->set_type(Lobby_Messages::JOIN);
            g->message_sent = send_owner_packet(g->lobby_id, message);
        }

        Lobby *lobby = get_lobby(g->lobby_id);
        if (lobby && lobby->deleted()) {
            LobbyEnter_t data;
            data.m_ulSteamIDLobby = lobby->room_id();
            data.m_rgfChatPermissions = 0; //Unused - Always 0
            data.m_bLocked = false;
            data.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseDoesntExist;
            callback_results->addCallResult(g->api_id, data.k_iCallback, &data, sizeof(data));
            callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
            g = pending_joins.erase(g);
        } else

        if (get_lobby_member(lobby, settings->get_local_steam_id())) {
            LobbyEnter_t data;
            data.m_ulSteamIDLobby = lobby->room_id();
            data.m_rgfChatPermissions = 0; //Unused - Always 0
            data.m_bLocked = false;
            data.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseSuccess;
            callback_results->addCallResult(g->api_id, data.k_iCallback, &data, sizeof(data));
            callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
            g = pending_joins.erase(g);
            trigger_lobby_dataupdate((uint64)lobby->room_id(), (uint64)lobby->room_id(), true);
        } else

        if (check_timedout(g->joined, PENDING_JOIN_TIMEOUT)) {
            LobbyEnter_t data;
            data.m_ulSteamIDLobby = g->lobby_id.ConvertToUint64();
            data.m_rgfChatPermissions = 0; //Unused - Always 0
            data.m_bLocked = false;
            data.m_EChatRoomEnterResponse = k_EChatRoomEnterResponseDoesntExist;
            callback_results->addCallResult(g->api_id, data.k_iCallback, &data, sizeof(data));
            callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
            g = pending_joins.erase(g);
            PRINT_DEBUG("PENDING_JOIN_TIMEOUT\n");
        } else {
            ++g;
        }
    }


    auto dr = std::begin(data_requested);
    while (dr != std::end(data_requested)) {
        if (get_lobby(dr->lobby_id)) {
            trigger_lobby_dataupdate(dr->lobby_id, dr->lobby_id, true);
            dr = data_requested.erase(dr);
            continue;
        }

        if (check_timedout(dr->requested, REQUEST_LOBBY_DATA_TIMEOUT)) {
            trigger_lobby_dataupdate(dr->lobby_id, dr->lobby_id, false);
            dr = data_requested.erase(dr);
            continue;
        }

        ++dr;
    }
}



void Callback(Common_Message *msg)
{
    if (msg->has_lobby()) {
        PRINT_DEBUG("GOT A LOBBY appid: %lu\n", msg->lobby().appid());
        if (msg->lobby().owner() != settings->get_local_steam_id().ConvertToUint64() && msg->lobby().appid() == settings->get_local_game_id().AppID()) {
            Lobby *lobby = get_lobby((uint64)msg->lobby().room_id());
            if (!lobby) {
                size_t old_size = lobbies.size();
                lobbies.resize(old_size + 1);
                lobbies[old_size].set_room_id(msg->lobby().room_id());
                lobby = &(lobbies[old_size]);
            }

            if (!lobby->deleted()) {
                if (!protobuf_message_equal(*lobby, msg->lobby())) {
                    bool we_are_in_lobby = !!get_lobby_member(lobby, settings->get_local_steam_id());
                    if (we_are_in_lobby) trigger_lobby_dataupdate((uint64)lobby->room_id(), (uint64)lobby->room_id(), true);

                    for (auto & m : lobby->members()) {
                        int count = 0;
                        Lobby_Member *member = get_lobby_member(msg->mutable_lobby(), (uint64)m.id());

                        if (we_are_in_lobby) {
                            if (!member) {
                                trigger_lobby_member_join_leave((uint64)lobby->room_id(), (uint64)m.id(), true, true, 0.2);
                            } else if (!protobuf_message_equal(*member, m)) {
                                trigger_lobby_dataupdate((uint64)lobby->room_id(), (uint64)m.id(), true);
                            }
                        }
                    }

                    bool joined = false;
                    for (auto & m : msg->lobby().members()) {
                        Lobby_Member *member = get_lobby_member(lobby, (uint64)m.id());
                        if (!member) {
                            if (m.id() == settings->get_local_steam_id().ConvertToUint64()) {
                                CSteamID id((uint64)lobby->room_id());
                                auto pd = pending_joins.begin();
                                while (pd != pending_joins.end()) {
                                    if (pd->lobby_id == id) {
                                        bool success = true;
                                        LobbyEnter_t data;
                                        data.m_ulSteamIDLobby = lobby->room_id();
                                        data.m_rgfChatPermissions = 0; //Unused - Always 0
                                        data.m_bLocked = false;
                                        data.m_EChatRoomEnterResponse = success ? k_EChatRoomEnterResponseSuccess : k_EChatRoomEnterResponseError;
                                        callback_results->addCallResult(pd->api_id, data.k_iCallback, &data, sizeof(data));
                                        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
                                        pd = pending_joins.erase(pd);
                                        joined = true;
                                    } else {
                                        ++pd;
                                    }
                                }
                                if (joined) {
                                    on_self_enter_leave_lobby((uint64)lobby->room_id(), lobby->type(), false);
                                    trigger_lobby_dataupdate((uint64)lobby->room_id(), (uint64)lobby->room_id(), true);
                                }
                            } else {
                                if (we_are_in_lobby) trigger_lobby_member_join_leave((uint64)lobby->room_id(), (uint64)m.id(), false, true);
                            }
                        }
                    }

                    if (joined) {
                        for (auto & m : msg->lobby().members()) {
                            if (m.id() != settings->get_local_steam_id().ConvertToUint64()) {
                                //TODO: is this good?
                                //trigger_lobby_member_join_leave((uint64)lobby->room_id(), (uint64)m.id(), false, true);
                                if (m.values().size()) {
                                    //TODO: check if this is what steam does
                                    //trigger_lobby_dataupdate((uint64)lobby->room_id(), (uint64)m.id(), true);
                                }
                            }
                        }
                    }

                    if ((joined && msg->lobby().gameserver().num_update()) || (we_are_in_lobby && (lobby->gameserver().num_update() != msg->lobby().gameserver().num_update()))) {
                        send_gameservercreated_cb(lobby->room_id(), msg->lobby().gameserver().id(), msg->lobby().gameserver().ip(), msg->lobby().gameserver().port());
                        trigger_lobby_dataupdate((uint64)lobby->room_id(), (uint64)lobby->room_id(), true);
                    }

                    *lobby = msg->lobby();
                }
            }
        }
    }


    if (msg->has_lobby_messages()) {
        PRINT_DEBUG("LOBBY MESSAGE %u %llu\n", msg->lobby_messages().type(), msg->lobby_messages().id());
        Lobby *lobby = get_lobby((uint64)msg->lobby_messages().id());
        if (lobby && !lobby->deleted()) {
            bool we_are_in_lobby = !!get_lobby_member(lobby, settings->get_local_steam_id());
            if (lobby->owner() == settings->get_local_steam_id().ConvertToUint64()) {
                if (msg->lobby_messages().type() == Lobby_Messages::JOIN) {
                    PRINT_DEBUG("LOBBY MESSAGE: JOIN\n");
                    if (enter_lobby(lobby, (uint64)msg->source_id())) {
                        trigger_lobby_member_join_leave((uint64)lobby->room_id(), (uint64)msg->source_id(), false, true, 0.01);
                    }
                }

                if (msg->lobby_messages().type() == Lobby_Messages::MEMBER_DATA) {
                    PRINT_DEBUG("LOBBY MESSAGE: MEMBER_DATA\n");
                    Lobby_Member *member = get_lobby_member(lobby, (uint64)msg->source_id());
                    if (member) {
                        for (auto const &p : msg->lobby_messages().map()) {
                            PRINT_DEBUG("member data %s:%s\n", p.first.c_str(), p.second.c_str());
                            auto result = caseinsensitive_find(member->values(), p.first);
                            if (result == member->values().end()) {
                                (*member->mutable_values())[p.first] = p.second;
                            } else {
                                (*member->mutable_values())[result->first] = p.second;
                            }
                        }

                        trigger_lobby_dataupdate((uint64)lobby->room_id(), (uint64)member->id(), true);
                    }
                }
            }

            if (msg->lobby_messages().type() == Lobby_Messages::LEAVE) {
                PRINT_DEBUG("LOBBY MESSAGE: LEAVE %llu\n", msg->source_id());
                leave_lobby(lobby, (uint64)msg->source_id());
                if (we_are_in_lobby) trigger_lobby_member_join_leave((uint64)lobby->room_id(), (uint64)msg->source_id(), true, true, 0.2);
            }

            if (msg->lobby_messages().type() == Lobby_Messages::CHANGE_OWNER) {
                PRINT_DEBUG("LOBBY MESSAGE: CHANGE OWNER\n");
                lobby->set_owner(msg->lobby_messages().idata());
                if (we_are_in_lobby) trigger_lobby_dataupdate((uint64)lobby->room_id(), (uint64)lobby->room_id(), true);
            }

            if (msg->lobby_messages().type() == Lobby_Messages::CHAT_MESSAGE) {
                PRINT_DEBUG("LOBBY MESSAGE: CHAT MESSAGE\n");
                if (we_are_in_lobby) {
                    struct Chat_Entry entry;
                    entry.type = k_EChatEntryTypeChatMsg;
                    entry.message = msg->lobby_messages().bdata();
                    entry.lobby_id = CSteamID((uint64)msg->lobby_messages().id());
                    entry.user_id = CSteamID((uint64)msg->source_id());
                    LobbyChatMsg_t data;
                    data.m_ulSteamIDLobby = msg->lobby_messages().id();
                    data.m_ulSteamIDUser = msg->source_id();
                    data.m_eChatEntryType = entry.type;
                    data.m_iChatID = chat_entries.size();
                    chat_entries.push_back(entry);
                    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));
                }
            }
        }
    }

    if (msg->has_low_level()) {
        if (msg->low_level().type() == Low_Level::CONNECT) {
            
        }

        if (msg->low_level().type() == Low_Level::DISCONNECT) {
            for (auto & l: lobbies) {
                if (leave_lobby(&(l), (uint64)msg->source_id()))
                    trigger_lobby_member_join_leave((uint64)l.room_id(), (uint64)msg->source_id(), true, true, 0.0);
            }
        }
    }

    remove_lobbies();
}


};
