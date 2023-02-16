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

#include "steam_matchmaking_servers.h"


static void network_callback(void *object, Common_Message *msg)
{
    PRINT_DEBUG("steam_matchmaking_servers_callback\n");

    Steam_Matchmaking_Servers *obj = (Steam_Matchmaking_Servers *)object;
    obj->Callback(msg);
}

Steam_Matchmaking_Servers::Steam_Matchmaking_Servers(class Settings *settings, class Networking *network)
{
    this->settings = settings;
    this->network = network;
    this->network->setCallback(CALLBACK_ID_GAMESERVER, (uint64) 0, &network_callback, this);
}

static int server_list_request;

// Request a new list of servers of a particular type.  These calls each correspond to one of the EMatchMakingType values.
// Each call allocates a new asynchronous request object.
// Request object must be released by calling ReleaseRequest( hServerListRequest )
HServerListRequest Steam_Matchmaking_Servers::RequestInternetServerList( AppId_t iApp, STEAM_ARRAY_COUNT(nFilters) MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
    PRINT_DEBUG("RequestInternetServerList\n");
    //TODO
    return RequestLANServerList(iApp, pRequestServersResponse);
}

HServerListRequest Steam_Matchmaking_Servers::RequestLANServerList( AppId_t iApp, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
    PRINT_DEBUG("RequestLANServerList %u\n", iApp);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    struct Steam_Matchmaking_Request request;
    request.appid = iApp;
    request.callbacks = pRequestServersResponse;
    request.old_callbacks = NULL;
    request.cancelled = false;
    request.completed = false;
    requests.push_back(request);
    ++server_list_request;
    requests[requests.size() - 1].id = (void *)server_list_request;
    HServerListRequest id = requests[requests.size() - 1].id;
    PRINT_DEBUG("request id: %p\n", id);
    return id;
}

HServerListRequest Steam_Matchmaking_Servers::RequestFriendsServerList( AppId_t iApp, STEAM_ARRAY_COUNT(nFilters) MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
    PRINT_DEBUG("RequestFriendsServerList\n");
    //TODO
    return RequestLANServerList(iApp, pRequestServersResponse);
}

HServerListRequest Steam_Matchmaking_Servers::RequestFavoritesServerList( AppId_t iApp, STEAM_ARRAY_COUNT(nFilters) MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
    PRINT_DEBUG("RequestFavoritesServerList\n");
    //TODO
    return RequestLANServerList(iApp, pRequestServersResponse);
}

HServerListRequest Steam_Matchmaking_Servers::RequestHistoryServerList( AppId_t iApp, STEAM_ARRAY_COUNT(nFilters) MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
    PRINT_DEBUG("RequestHistoryServerList\n");
    //TODO
    return RequestLANServerList(iApp, pRequestServersResponse);
}

HServerListRequest Steam_Matchmaking_Servers::RequestSpectatorServerList( AppId_t iApp, STEAM_ARRAY_COUNT(nFilters) MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse *pRequestServersResponse )
{
    PRINT_DEBUG("RequestSpectatorServerList\n");
    //TODO
    return RequestLANServerList(iApp, pRequestServersResponse);
}

void Steam_Matchmaking_Servers::RequestOldServerList(AppId_t iApp, ISteamMatchmakingServerListResponse001 *pRequestServersResponse, EMatchMakingType type)
{
    PRINT_DEBUG("RequestOldServerList %u\n", iApp);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    auto g = std::begin(requests);
    while (g != std::end(requests)) {
        if (g->id == ((void *)type)) {
            return;
        }

        ++g;
    }

    struct Steam_Matchmaking_Request request;
    request.appid = iApp;
    request.callbacks = NULL;
    request.old_callbacks = pRequestServersResponse;
    request.cancelled = false;
    request.completed = false;
    requests.push_back(request);
    requests[requests.size() - 1].id = (void *)type;
}

void Steam_Matchmaking_Servers::RequestInternetServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse001 *pRequestServersResponse )
{
    PRINT_DEBUG("%s old\n", __FUNCTION__);
    //TODO
    RequestOldServerList(iApp, pRequestServersResponse, eInternetServer);
}

void Steam_Matchmaking_Servers::RequestLANServerList( AppId_t iApp, ISteamMatchmakingServerListResponse001 *pRequestServersResponse )
{
    PRINT_DEBUG("%s old\n", __FUNCTION__);
    //TODO
    RequestOldServerList(iApp, pRequestServersResponse, eLANServer);
}

void Steam_Matchmaking_Servers::RequestFriendsServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse001 *pRequestServersResponse )
{
    PRINT_DEBUG("%s old\n", __FUNCTION__);
    //TODO
    RequestOldServerList(iApp, pRequestServersResponse, eFriendsServer);
}

void Steam_Matchmaking_Servers::RequestFavoritesServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse001 *pRequestServersResponse )
{
    PRINT_DEBUG("%s old\n", __FUNCTION__);
    //TODO
    RequestOldServerList(iApp, pRequestServersResponse, eFavoritesServer);
}

void Steam_Matchmaking_Servers::RequestHistoryServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse001 *pRequestServersResponse )
{
    PRINT_DEBUG("%s old\n", __FUNCTION__);
    //TODO
    RequestOldServerList(iApp, pRequestServersResponse, eHistoryServer);
}

void Steam_Matchmaking_Servers::RequestSpectatorServerList( AppId_t iApp, MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse001 *pRequestServersResponse )
{
    PRINT_DEBUG("%s old\n", __FUNCTION__);
    //TODO
    RequestOldServerList(iApp, pRequestServersResponse, eSpectatorServer);
}


// Releases the asynchronous request object and cancels any pending query on it if there's a pending query in progress.
// RefreshComplete callback is not posted when request is released.
void Steam_Matchmaking_Servers::ReleaseRequest( HServerListRequest hServerListRequest )
{
    PRINT_DEBUG("ReleaseRequest %p\n", hServerListRequest);
    auto g = std::begin(requests);
    while (g != std::end(requests)) {
        if (g->id == hServerListRequest) {
            //NOTE: some garbage games release the request before getting server details from it.
            //     g = requests.erase(g);
            // } else {
            //TODO: eventually delete the released request.
            g->cancelled = true;
            g->released = true;
        }

        ++g;
    }
}


/* the filter operation codes that go in the key part of MatchMakingKeyValuePair_t should be one of these:

    "map"
        - Server passes the filter if the server is playing the specified map.
    "gamedataand"
        - Server passes the filter if the server's game data (ISteamGameServer::SetGameData) contains all of the
        specified strings.  The value field is a comma-delimited list of strings to match.
    "gamedataor"
        - Server passes the filter if the server's game data (ISteamGameServer::SetGameData) contains at least one of the
        specified strings.  The value field is a comma-delimited list of strings to match.
    "gamedatanor"
        - Server passes the filter if the server's game data (ISteamGameServer::SetGameData) does not contain any
        of the specified strings.  The value field is a comma-delimited list of strings to check.
    "gametagsand"
        - Server passes the filter if the server's game tags (ISteamGameServer::SetGameTags) contains all
        of the specified strings.  The value field is a comma-delimited list of strings to check.
    "gametagsnor"
        - Server passes the filter if the server's game tags (ISteamGameServer::SetGameTags) does not contain any
        of the specified strings.  The value field is a comma-delimited list of strings to check.
    "and" (x1 && x2 && ... && xn)
    "or" (x1 || x2 || ... || xn)
    "nand" !(x1 && x2 && ... && xn)
    "nor" !(x1 || x2 || ... || xn)
        - Performs Boolean operation on the following filters.  The operand to this filter specifies
        the "size" of the Boolean inputs to the operation, in Key/value pairs.  (The keyvalue
        pairs must immediately follow, i.e. this is a prefix logical operator notation.)
        In the simplest case where Boolean expressions are not nested, this is simply
        the number of operands.

        For example, to match servers on a particular map or with a particular tag, would would
        use these filters.

            ( server.map == "cp_dustbowl" || server.gametags.contains("payload") )
            "or", "2"
            "map", "cp_dustbowl"
            "gametagsand", "payload"

        If logical inputs are nested, then the operand specifies the size of the entire
        "length" of its operands, not the number of immediate children.

            ( server.map == "cp_dustbowl" || ( server.gametags.contains("payload") && !server.gametags.contains("payloadrace") ) )
            "or", "4"
            "map", "cp_dustbowl"
            "and", "2"
            "gametagsand", "payload"
            "gametagsnor", "payloadrace"

        Unary NOT can be achieved using either "nand" or "nor" with a single operand.

    "addr"
        - Server passes the filter if the server's query address matches the specified IP or IP:port.
    "gameaddr"
        - Server passes the filter if the server's game address matches the specified IP or IP:port.

    The following filter operations ignore the "value" part of MatchMakingKeyValuePair_t

    "dedicated"
        - Server passes the filter if it passed true to SetDedicatedServer.
    "secure"
        - Server passes the filter if the server is VAC-enabled.
    "notfull"
        - Server passes the filter if the player count is less than the reported max player count.
    "hasplayers"
        - Server passes the filter if the player count is greater than zero.
    "noplayers"
        - Server passes the filter if it doesn't have any players.
    "linux"
        - Server passes the filter if it's a linux server
*/

void Steam_Matchmaking_Servers::server_details(Gameserver *g, gameserveritem_t *server)
{
    uint16 query_port = g->query_port();
    if (g->query_port() == 0xFFFF) {
        query_port = g->port();
    }

    server->m_NetAdr.Init(g->ip(), query_port, g->port());
    server->m_nPing = 10; //TODO
    server->m_bHadSuccessfulResponse = true;
    server->m_bDoNotRefresh = false;
    strncpy(server->m_szGameDir, g->mod_dir().c_str(), k_cbMaxGameServerGameDir - 1);
    strncpy(server->m_szMap, g->map_name().c_str(), k_cbMaxGameServerMapName - 1);
    strncpy(server->m_szGameDescription, g->game_description().c_str(), k_cbMaxGameServerGameDescription - 1);

    server->m_szGameDir[k_cbMaxGameServerGameDir - 1] = 0;
    server->m_szMap[k_cbMaxGameServerMapName - 1] = 0;
    server->m_szGameDescription[k_cbMaxGameServerGameDescription - 1] = 0;

    server->m_nAppID = g->appid();
    server->m_nPlayers = g->num_players();
    server->m_nMaxPlayers = g->max_player_count();
    server->m_nBotPlayers = g->bot_player_count();
    server->m_bPassword = g->password_protected();
    server->m_bSecure = g->secure();
    server->m_ulTimeLastPlayed = 0;
    server->m_nServerVersion = g->version();
    server->SetName(g->server_name().c_str());
    server->m_steamID = CSteamID((uint64)g->id());
    PRINT_DEBUG("server_details %llu\n", g->id());

    strncpy(server->m_szGameTags, g->tags().c_str(), k_cbMaxGameServerTags - 1);
    server->m_szGameTags[k_cbMaxGameServerTags - 1] = 0;
}

// Get details on a given server in the list, you can get the valid range of index
// values by calling GetServerCount().  You will also receive index values in 
// ISteamMatchmakingServerListResponse::ServerResponded() callbacks
gameserveritem_t *Steam_Matchmaking_Servers::GetServerDetails( HServerListRequest hRequest, int iServer )
{
    PRINT_DEBUG("GetServerDetails %p %i\n", hRequest, iServer);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    std::vector <struct Steam_Matchmaking_Servers_Gameserver> gameservers_filtered;
    auto g = std::begin(requests);
    while (g != std::end(requests)) {
        PRINT_DEBUG("equal? %p %p\n", hRequest, g->id);
        if (g->id == hRequest) {
            gameservers_filtered = g->gameservers_filtered;
            PRINT_DEBUG("found %u\n", gameservers_filtered.size());
            break;
        }

        ++g;
    }

    if (iServer >= gameservers_filtered.size() || iServer < 0) {
        return NULL;
    }

    Gameserver *gs = &gameservers_filtered[iServer].server;
    gameserveritem_t *server = new gameserveritem_t(); //TODO: is the new here ok?
    server_details(gs, server);
    PRINT_DEBUG("Returned server details\n");
    return server;
}


// Cancel an request which is operation on the given list type.  You should call this to cancel
// any in-progress requests before destructing a callback object that may have been passed 
// to one of the above list request calls.  Not doing so may result in a crash when a callback
// occurs on the destructed object.
// Canceling a query does not release the allocated request handle.
// The request handle must be released using ReleaseRequest( hRequest )
void Steam_Matchmaking_Servers::CancelQuery( HServerListRequest hRequest )
{
    PRINT_DEBUG("CancelQuery %p\n", hRequest);
    auto g = std::begin(requests);
    while (g != std::end(requests)) {
        if (g->id == hRequest) {
            g->cancelled = true;
        }

        ++g;
    }
}
 

// Ping every server in your list again but don't update the list of servers
// Query callback installed when the server list was requested will be used
// again to post notifications and RefreshComplete, so the callback must remain
// valid until another RefreshComplete is called on it or the request
// is released with ReleaseRequest( hRequest )
void Steam_Matchmaking_Servers::RefreshQuery( HServerListRequest hRequest )
{
    PRINT_DEBUG("RefreshQuery %p\n", hRequest);
}
 

// Returns true if the list is currently refreshing its server list
bool Steam_Matchmaking_Servers::IsRefreshing( HServerListRequest hRequest )
{
    PRINT_DEBUG("IsRefreshing %p\n", hRequest);
    return false;
}
 

// How many servers in the given list, GetServerDetails above takes 0... GetServerCount() - 1
int Steam_Matchmaking_Servers::GetServerCount( HServerListRequest hRequest )
{
    PRINT_DEBUG("GetServerCount %p\n", hRequest);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    int size = 0;
    auto g = std::begin(requests);
    while (g != std::end(requests)) {
        if (g->id == hRequest) {
            size = g->gameservers_filtered.size();
            break;
        }

        ++g;
    }

    return size;
}


// Refresh a single server inside of a query (rather than all the servers )
void Steam_Matchmaking_Servers::RefreshServer( HServerListRequest hRequest, int iServer )
{
    PRINT_DEBUG("RefreshServer %p\n", hRequest);
    //TODO
}
 
static HServerQuery new_server_query()
{
    static int a;
    ++a;
    if (!a) ++a;
    return a;
}

//-----------------------------------------------------------------------------
// Queries to individual servers directly via IP/Port
//-----------------------------------------------------------------------------

// Request updated ping time and other details from a single server
HServerQuery Steam_Matchmaking_Servers::PingServer( uint32 unIP, uint16 usPort, ISteamMatchmakingPingResponse *pRequestServersResponse )
{
    PRINT_DEBUG("PingServer %hhu.%hhu.%hhu.%hhu:%hu\n", ((unsigned char *)&unIP)[3], ((unsigned char *)&unIP)[2], ((unsigned char *)&unIP)[1], ((unsigned char *)&unIP)[0], usPort);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Steam_Matchmaking_Servers_Direct_IP_Request r;
    r.id = new_server_query();
    r.ip = unIP;
    r.port = usPort;
    r.ping_response = pRequestServersResponse;
    r.created = std::chrono::high_resolution_clock::now();
    direct_ip_requests.push_back(r);
    return r.id;
}

// Request the list of players currently playing on a server
HServerQuery Steam_Matchmaking_Servers::PlayerDetails( uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse *pRequestServersResponse )
{
    PRINT_DEBUG("PlayerDetails %hhu.%hhu.%hhu.%hhu:%hu\n", ((unsigned char *)&unIP)[3], ((unsigned char *)&unIP)[2], ((unsigned char *)&unIP)[1], ((unsigned char *)&unIP)[0], usPort);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Steam_Matchmaking_Servers_Direct_IP_Request r;
    r.id = new_server_query();
    r.ip = unIP;
    r.port = usPort;
    r.players_response = pRequestServersResponse;
    r.created = std::chrono::high_resolution_clock::now();
    direct_ip_requests.push_back(r);
    return r.id;
}


// Request the list of rules that the server is running (See ISteamGameServer::SetKeyValue() to set the rules server side)
HServerQuery Steam_Matchmaking_Servers::ServerRules( uint32 unIP, uint16 usPort, ISteamMatchmakingRulesResponse *pRequestServersResponse )
{
    PRINT_DEBUG("ServerRules %hhu.%hhu.%hhu.%hhu:%hu\n", ((unsigned char *)&unIP)[3], ((unsigned char *)&unIP)[2], ((unsigned char *)&unIP)[1], ((unsigned char *)&unIP)[0], usPort);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    Steam_Matchmaking_Servers_Direct_IP_Request r;
    r.id = new_server_query();
    r.ip = unIP;
    r.port = usPort;
    r.rules_response = pRequestServersResponse;
    r.created = std::chrono::high_resolution_clock::now();
    direct_ip_requests.push_back(r);
    return r.id;
}


// Cancel an outstanding Ping/Players/Rules query from above.  You should call this to cancel
// any in-progress requests before destructing a callback object that may have been passed 
// to one of the above calls to avoid crashing when callbacks occur.
void Steam_Matchmaking_Servers::CancelServerQuery( HServerQuery hServerQuery )
{
    PRINT_DEBUG("CancelServerQuery\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    auto r = std::find_if(direct_ip_requests.begin(), direct_ip_requests.end(), [&hServerQuery](Steam_Matchmaking_Servers_Direct_IP_Request const& item) { return item.id == hServerQuery; });
    if (direct_ip_requests.end() == r) return;
    direct_ip_requests.erase(r);
}

void Steam_Matchmaking_Servers::RunCallbacks()
{
    PRINT_DEBUG("Steam_Matchmaking_Servers::RunCallbacks\n");

    {
        auto g = std::begin(gameservers);
        while (g != std::end(gameservers)) {
            if (check_timedout(g->last_recv, SERVER_TIMEOUT)) {
                g = gameservers.erase(g);
                PRINT_DEBUG("SERVER TIMEOUT\n");
            } else {
                ++g;
            }
        }
    }

    PRINT_DEBUG("REQUESTS %zu gs: %zu\n", requests.size(), gameservers.size());

    for (auto &r : requests) {
        if (r.cancelled || r.completed) continue;

        r.gameservers_filtered.clear();
        for (auto &g : gameservers) {
            PRINT_DEBUG("game_server_check %u %u\n", g.server.appid(), r.appid);
            if (g.server.appid() == r.appid) {
                PRINT_DEBUG("REQUESTS server found\n");
                r.gameservers_filtered.push_back(g);
            }
        }
    }

    std::vector <struct Steam_Matchmaking_Request> requests_temp(requests);
    PRINT_DEBUG("REQUESTS_TEMP %zu\n", requests_temp.size());
    for (auto &r : requests) {
        r.completed = true;
    }

    for (auto &r : requests_temp) {
        if (r.cancelled || r.completed) continue;
        int i = 0;

        if (r.callbacks) {
            for (auto &g : r.gameservers_filtered) {
                PRINT_DEBUG("REQUESTS server responded cb %p\n", r.id);
                r.callbacks->ServerResponded(r.id, i);
                ++i;
            }

            if (i) {
                r.callbacks->RefreshComplete(r.id, eServerResponded);
            } else {
                r.callbacks->RefreshComplete(r.id, eNoServersListedOnMasterServer);
            }
        }

        if (r.old_callbacks) {
            for (auto &g : r.gameservers_filtered) {
                PRINT_DEBUG("old REQUESTS server responded cb %p\n", r.id);
                r.old_callbacks->ServerResponded(i);
                ++i;
            }

            if (i) {
                r.old_callbacks->RefreshComplete(eServerResponded);
            } else {
                r.old_callbacks->RefreshComplete(eNoServersListedOnMasterServer);
            }
        }
    }

    std::vector <struct Steam_Matchmaking_Servers_Direct_IP_Request> direct_ip_requests_temp;
    auto dip = std::begin(direct_ip_requests);
    while (dip != std::end(direct_ip_requests)) {
        if (check_timedout(dip->created, DIRECT_IP_DELAY)) {
            direct_ip_requests_temp.push_back(*dip);
            dip = direct_ip_requests.erase(dip);
        } else {
            ++dip;
        }
    }

    for (auto &r : direct_ip_requests_temp) {
        PRINT_DEBUG("dip request: %lu:%hu\n", r.ip, r.port);
        for (auto &g : gameservers) {
            PRINT_DEBUG("server: %lu:%hu\n", g.server.ip(), g.server.query_port());
            uint16 query_port = g.server.query_port();
            if (query_port == 0xFFFF) {
                query_port = g.server.port();
            }

            if (query_port == r.port && g.server.ip() == r.ip) {
                if (r.rules_response) {
                    int number_rules = g.server.values().size();
                    PRINT_DEBUG("rules: %lu\n", number_rules);
                    auto rule = g.server.values().begin();
                    for (int i = 0; i < number_rules; ++i) {
                        PRINT_DEBUG("RULE %s %s\n", rule->first.c_str(), rule->second.c_str());
                        r.rules_response->RulesResponded(rule->first.c_str(), rule->second.c_str());
                        ++rule;
                    }

                    r.rules_response->RulesRefreshComplete();
                    r.rules_response = NULL;
                }

                if (r.ping_response) {
                    gameserveritem_t server;
                    server_details(&(g.server), &server);
                    r.ping_response->ServerResponded(server);
                    r.ping_response = NULL;
                }
                //TODO: players response
            }
        }

        if (r.rules_response) r.rules_response->RulesRefreshComplete();
        //TODO: player response
        if (r.players_response) r.players_response->PlayersRefreshComplete();
        if (r.ping_response) r.ping_response->ServerFailedToRespond();
    }
}

void Steam_Matchmaking_Servers::Callback(Common_Message *msg)
{
    if (msg->has_gameserver()) {
        PRINT_DEBUG("got SERVER %llu, offline:%u\n", msg->gameserver().id(), msg->gameserver().offline());
        if (msg->gameserver().offline()) {
            for (auto &g : gameservers) {
                if (g.server.id() == msg->gameserver().id()) {
                    g.last_recv = std::chrono::high_resolution_clock::time_point();
                }
            }
        } else {
            bool already = false;
            for (auto &g : gameservers) {
                if (g.server.id() == msg->gameserver().id()) {
                    g.last_recv = std::chrono::high_resolution_clock::now();
                    g.server = msg->gameserver();
                    g.server.set_ip(msg->source_ip());
                    already = true;
                }
            }

            if (!already) {
                struct Steam_Matchmaking_Servers_Gameserver g;
                g.last_recv = std::chrono::high_resolution_clock::now();
                g.server = msg->gameserver();
                g.server.set_ip(msg->source_ip());
                gameservers.push_back(g);
                PRINT_DEBUG("SERVER ADDED\n");
            }
        }
    }
}
