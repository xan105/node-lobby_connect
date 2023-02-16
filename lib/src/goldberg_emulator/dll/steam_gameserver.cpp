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

#include "steam_gameserver.h"

#define SEND_SERVER_RATE 5.0

Steam_GameServer::Steam_GameServer(class Settings *settings, class Networking *network, class SteamCallBacks *callbacks)
{
    this->network = network;
    this->settings = settings;
    server_data.set_id(settings->get_local_steam_id().ConvertToUint64());
    this->callbacks = callbacks;
    ticket_manager = new Auth_Ticket_Manager(settings, network, callbacks);
}

Steam_GameServer::~Steam_GameServer()
{
    delete ticket_manager;
}

//
// Basic server data.  These properties, if set, must be set before before calling LogOn.  They
// may not be changed after logged in.
//

/// This is called by SteamGameServer_Init, and you will usually not need to call it directly
bool Steam_GameServer::InitGameServer( uint32 unIP, uint16 usGamePort, uint16 usQueryPort, uint32 unFlags, AppId_t nGameAppId, const char *pchVersionString )
{
    PRINT_DEBUG("InitGameServer\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    if (logged_in) return false; // may not be changed after logged in.
    if (!pchVersionString) pchVersionString = "";

    std::string version(pchVersionString);
    version.erase(std::remove(version.begin(), version.end(), ' '), version.end());
    version.erase(std::remove(version.begin(), version.end(), '.'), version.end());
    try {
        server_data.set_version(std::stoi(version));
    } catch (...) {
        PRINT_DEBUG("InitGameServer: not a number: %s\n", pchVersionString);
        server_data.set_version(0);
    }

    server_data.set_ip(unIP);
    server_data.set_port(usGamePort);
    server_data.set_query_port(usQueryPort);
    server_data.set_offline(false);
    if (!settings->get_local_game_id().AppID()) settings->set_game_id(CGameID(nGameAppId));
    //TODO: flags should be k_unServerFlag
    flags = unFlags;
    policy_response_called = false;
    call_servers_connected = false;
    call_servers_disconnected = false;

    return true;
}


/// Game product identifier.  This is currently used by the master server for version checking purposes.
/// It's a required field, but will eventually will go away, and the AppID will be used for this purpose.
void Steam_GameServer::SetProduct( const char *pszProduct )
{
    PRINT_DEBUG("SetProduct\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_product(pszProduct);
}


/// Description of the game.  This is a required field and is displayed in the steam server browser....for now.
/// This is a required field, but it will go away eventually, as the data should be determined from the AppID.
void Steam_GameServer::SetGameDescription( const char *pszGameDescription )
{
    PRINT_DEBUG("SetGameDescription\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_game_description(pszGameDescription);
}


/// If your game is a "mod," pass the string that identifies it.  The default is an empty string, meaning
/// this application is the original game, not a mod.
///
/// @see k_cbMaxGameServerGameDir
void Steam_GameServer::SetModDir( const char *pszModDir )
{
    PRINT_DEBUG("SetModDir\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_mod_dir(pszModDir);
}


/// Is this is a dedicated server?  The default value is false.
void Steam_GameServer::SetDedicatedServer( bool bDedicated )
{
    PRINT_DEBUG("SetDedicatedServer\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_dedicated_server(bDedicated);
}


//
// Login
//

/// Begin process to login to a persistent game server account
///
/// You need to register for callbacks to determine the result of this operation.
/// @see SteamServersConnected_t
/// @see SteamServerConnectFailure_t
/// @see SteamServersDisconnected_t
void Steam_GameServer::LogOn( const char *pszToken )
{
    PRINT_DEBUG("LogOn %s\n", pszToken);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    call_servers_connected = true;
    logged_in = true;
}

void Steam_GameServer::LogOn(
		const char *pszAccountName,
		const char *pszPassword
	)
{
        PRINT_DEBUG("LogOn %s %s\n", pszAccountName, pszPassword);
        LogOn(pszAccountName);
}

/// Login to a generic, anonymous account.
///
/// Note: in previous versions of the SDK, this was automatically called within SteamGameServer_Init,
/// but this is no longer the case.
void Steam_GameServer::LogOnAnonymous()
{
    PRINT_DEBUG("LogOnAnonymous\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    call_servers_connected = true;
    logged_in = true;
}

void Steam_GameServer::LogOn()
{
    PRINT_DEBUG("LogOn\n");
    LogOnAnonymous();
}

/// Begin process of logging game server out of steam
void Steam_GameServer::LogOff()
{
    PRINT_DEBUG("LogOff\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (logged_in) {
        call_servers_disconnected = true;
    }

    logged_in = false;
}


// status functions
bool Steam_GameServer::BLoggedOn()
{
    PRINT_DEBUG("BLoggedOn\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return logged_in;
}

bool Steam_GameServer::BSecure()
{
    PRINT_DEBUG("BSecure\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (!policy_response_called) return false;
    return !!(flags & k_unServerFlagSecure);
}
 
CSteamID Steam_GameServer::GetSteamID()
{
    PRINT_DEBUG("Steam_GameServer::GetSteamID\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (!logged_in) return k_steamIDNil;
    return settings->get_local_steam_id();
}


/// Returns true if the master server has requested a restart.
/// Only returns true once per request.
bool Steam_GameServer::WasRestartRequested()
{
    PRINT_DEBUG("WasRestartRequested\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return false;
}


//
// Server state.  These properties may be changed at any time.
//

/// Max player count that will be reported to server browser and client queries
void Steam_GameServer::SetMaxPlayerCount( int cPlayersMax )
{
    PRINT_DEBUG("SetMaxPlayerCount\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_max_player_count(cPlayersMax);
}


/// Number of bots.  Default value is zero
void Steam_GameServer::SetBotPlayerCount( int cBotplayers )
{
    PRINT_DEBUG("SetBotPlayerCount\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_bot_player_count(cBotplayers);
}


/// Set the name of server as it will appear in the server browser
///
/// @see k_cbMaxGameServerName
void Steam_GameServer::SetServerName( const char *pszServerName )
{
    PRINT_DEBUG("SetServerName\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_server_name(pszServerName);
}


/// Set name of map to report in the server browser
///
/// @see k_cbMaxGameServerName
void Steam_GameServer::SetMapName( const char *pszMapName )
{
    PRINT_DEBUG("SetMapName\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_map_name(pszMapName);
}


/// Let people know if your server will require a password
void Steam_GameServer::SetPasswordProtected( bool bPasswordProtected )
{
    PRINT_DEBUG("SetPasswordProtected\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_password_protected(bPasswordProtected);
}


/// Spectator server.  The default value is zero, meaning the service
/// is not used.
void Steam_GameServer::SetSpectatorPort( uint16 unSpectatorPort )
{
    PRINT_DEBUG("SetSpectatorPort\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_spectator_port(unSpectatorPort);
}


/// Name of the spectator server.  (Only used if spectator port is nonzero.)
///
/// @see k_cbMaxGameServerMapName
void Steam_GameServer::SetSpectatorServerName( const char *pszSpectatorServerName )
{
    PRINT_DEBUG("SetSpectatorServerName\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_spectator_server_name(pszSpectatorServerName);
}


/// Call this to clear the whole list of key/values that are sent in rules queries.
void Steam_GameServer::ClearAllKeyValues()
{
    PRINT_DEBUG("ClearAllKeyValues\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.clear_values();
}


/// Call this to add/update a key/value pair.
void Steam_GameServer::SetKeyValue( const char *pKey, const char *pValue )
{
    PRINT_DEBUG("SetKeyValue %s %s\n", pKey, pValue);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    (*server_data.mutable_values())[std::string(pKey)] = std::string(pValue);
}


/// Sets a string defining the "gametags" for this server, this is optional, but if it is set
/// it allows users to filter in the matchmaking/server-browser interfaces based on the value
///
/// @see k_cbMaxGameServerTags
void Steam_GameServer::SetGameTags( const char *pchGameTags )
{
    PRINT_DEBUG("SetGameTags\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_tags(pchGameTags);
}


/// Sets a string defining the "gamedata" for this server, this is optional, but if it is set
/// it allows users to filter in the matchmaking/server-browser interfaces based on the value
/// don't set this unless it actually changes, its only uploaded to the master once (when
/// acknowledged)
///
/// @see k_cbMaxGameServerGameData
void Steam_GameServer::SetGameData( const char *pchGameData )
{
    PRINT_DEBUG("SetGameData\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_gamedata(pchGameData);
}


/// Region identifier.  This is an optional field, the default value is empty, meaning the "world" region
void Steam_GameServer::SetRegion( const char *pszRegion )
{
    PRINT_DEBUG("SetRegion\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_region(pszRegion);
}


//
// Player list management / authentication
//

// Handles receiving a new connection from a Steam user.  This call will ask the Steam
// servers to validate the users identity, app ownership, and VAC status.  If the Steam servers 
// are off-line, then it will validate the cached ticket itself which will validate app ownership 
// and identity.  The AuthBlob here should be acquired on the game client using SteamUser()->InitiateGameConnection()
// and must then be sent up to the game server for authentication.
//
// Return Value: returns true if the users ticket passes basic checks. pSteamIDUser will contain the Steam ID of this user. pSteamIDUser must NOT be NULL
// If the call succeeds then you should expect a GSClientApprove_t or GSClientDeny_t callback which will tell you whether authentication
// for the user has succeeded or failed (the steamid in the callback will match the one returned by this call)
bool Steam_GameServer::SendUserConnectAndAuthenticate( uint32 unIPClient, const void *pvAuthBlob, uint32 cubAuthBlobSize, CSteamID *pSteamIDUser )
{
    PRINT_DEBUG("SendUserConnectAndAuthenticate %u %u\n", unIPClient, cubAuthBlobSize);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    return ticket_manager->SendUserConnectAndAuthenticate(unIPClient, pvAuthBlob, cubAuthBlobSize, pSteamIDUser);
}

void Steam_GameServer::SendUserConnectAndAuthenticate( CSteamID steamIDUser, uint32 unIPClient, void *pvAuthBlob, uint32 cubAuthBlobSize )
{
    SendUserConnectAndAuthenticate(unIPClient, pvAuthBlob, cubAuthBlobSize, NULL);
}

// Creates a fake user (ie, a bot) which will be listed as playing on the server, but skips validation.  
// 
// Return Value: Returns a SteamID for the user to be tracked with, you should call HandleUserDisconnect()
// when this user leaves the server just like you would for a real user.
CSteamID Steam_GameServer::CreateUnauthenticatedUserConnection()
{
    PRINT_DEBUG("CreateUnauthenticatedUserConnection\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    return ticket_manager->fakeUser();
}


// Should be called whenever a user leaves our game server, this lets Steam internally
// track which users are currently on which servers for the purposes of preventing a single
// account being logged into multiple servers, showing who is currently on a server, etc.
void Steam_GameServer::SendUserDisconnect( CSteamID steamIDUser )
{
    PRINT_DEBUG("SendUserDisconnect\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    ticket_manager->endAuth(steamIDUser);
}


// Update the data to be displayed in the server browser and matchmaking interfaces for a user
// currently connected to the server.  For regular users you must call this after you receive a
// GSUserValidationSuccess callback.
// 
// Return Value: true if successful, false if failure (ie, steamIDUser wasn't for an active player)
bool Steam_GameServer::BUpdateUserData( CSteamID steamIDUser, const char *pchPlayerName, uint32 uScore )
{
    PRINT_DEBUG("BUpdateUserData %llu %s %u\n", steamIDUser.ConvertToUint64(), pchPlayerName, uScore);
    return true;
}

// You shouldn't need to call this as it is called internally by SteamGameServer_Init() and can only be called once.
//
// To update the data in this call which may change during the servers lifetime see UpdateServerStatus() below.
//
// Input:	nGameAppID - The Steam assigned AppID for the game
//			unServerFlags - Any applicable combination of flags (see k_unServerFlag____ constants below)
//			unGameIP - The IP Address the server is listening for client connections on (might be INADDR_ANY)
//			unGamePort - The port which the server is listening for client connections on
//			unSpectatorPort - the port on which spectators can join to observe the server, 0 if spectating is not supported
//			usQueryPort - The port which the ISteamMasterServerUpdater API should use in order to listen for matchmaking requests
//			pchGameDir - A unique string identifier for your game
//			pchVersion - The current version of the server as a string like 1.0.0.0
//			bLanMode - Is this a LAN only server?
//			
// bugbug jmccaskey - figure out how to remove this from the API and only expose via SteamGameServer_Init... or make this actually used,
// and stop calling it in SteamGameServer_Init()?
bool Steam_GameServer::BSetServerType( uint32 unServerFlags, uint32 unGameIP, uint16 unGamePort, 
                            uint16 unSpectatorPort, uint16 usQueryPort, const char *pchGameDir, const char *pchVersion, bool bLANMode )
{
    PRINT_DEBUG("BSetServerType\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_ip(unGameIP);
    server_data.set_port(unGamePort);
    server_data.set_query_port(usQueryPort);
    server_data.set_spectator_port(unSpectatorPort);

    std::string version(pchVersion);
    version.erase(std::remove(version.begin(), version.end(), ' '), version.end());
    version.erase(std::remove(version.begin(), version.end(), '.'), version.end());
    server_data.set_version(stoi(version));
    flags = unServerFlags;

    //TODO?
    return true;
}

bool Steam_GameServer::BSetServerType( int32 nGameAppId, uint32 unServerFlags, uint32 unGameIP, uint16 unGamePort, 
									uint16 unSpectatorPort, uint16 usQueryPort, const char *pchGameDir, const char *pchVersion, bool bLANMode )
{
    return BSetServerType(unServerFlags, unGameIP, unGamePort, unSpectatorPort, usQueryPort, pchGameDir, pchVersion, bLANMode);
}

// Updates server status values which shows up in the server browser and matchmaking APIs
void Steam_GameServer::UpdateServerStatus( int cPlayers, int cPlayersMax, int cBotPlayers, 
                                    const char *pchServerName, const char *pSpectatorServerName, 
                                    const char *pchMapName )
{
    PRINT_DEBUG("UpdateServerStatus\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    server_data.set_num_players(cPlayers);
    server_data.set_max_player_count(cPlayersMax);
    server_data.set_bot_player_count(cBotPlayers);
    server_data.set_server_name(pchServerName);
    server_data.set_spectator_server_name(pSpectatorServerName);
    server_data.set_map_name(pchMapName);
}

// This can be called if spectator goes away or comes back (passing 0 means there is no spectator server now).
void Steam_GameServer::UpdateSpectatorPort( uint16 unSpectatorPort )
{
    PRINT_DEBUG("UpdateSpectatorPort\n");
    SetSpectatorPort(unSpectatorPort);
}

// Sets a string defining the "gametype" for this server, this is optional, but if it is set
// it allows users to filter in the matchmaking/server-browser interfaces based on the value
void Steam_GameServer::SetGameType( const char *pchGameType )
{
    PRINT_DEBUG("SetGameType\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
}

// Ask if a user has a specific achievement for this game, will get a callback on reply
bool Steam_GameServer::BGetUserAchievementStatus( CSteamID steamID, const char *pchAchievementName )
{
    PRINT_DEBUG("BGetUserAchievementStatus\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return false;
}

// New auth system APIs - do not mix with the old auth system APIs.
// ----------------------------------------------------------------

// Retrieve ticket to be sent to the entity who wishes to authenticate you ( using BeginAuthSession API ). 
// pcbTicket retrieves the length of the actual ticket.
HAuthTicket Steam_GameServer::GetAuthSessionTicket( void *pTicket, int cbMaxTicket, uint32 *pcbTicket )
{
    PRINT_DEBUG("Steam_GameServer::GetAuthSessionTicket\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    return ticket_manager->getTicket(pTicket, cbMaxTicket, pcbTicket);
}


// Authenticate ticket ( from GetAuthSessionTicket ) from entity steamID to be sure it is valid and isnt reused
// Registers for callbacks if the entity goes offline or cancels the ticket ( see ValidateAuthTicketResponse_t callback and EAuthSessionResponse )
EBeginAuthSessionResult Steam_GameServer::BeginAuthSession( const void *pAuthTicket, int cbAuthTicket, CSteamID steamID )
{
    PRINT_DEBUG("Steam_GameServer::BeginAuthSession %i %llu\n", cbAuthTicket, steamID.ConvertToUint64());
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    return ticket_manager->beginAuth(pAuthTicket, cbAuthTicket, steamID );
}


// Stop tracking started by BeginAuthSession - called when no longer playing game with this entity
void Steam_GameServer::EndAuthSession( CSteamID steamID )
{
    PRINT_DEBUG("Steam_GameServer::EndAuthSession %llu\n", steamID.ConvertToUint64());
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    ticket_manager->endAuth(steamID);
}


// Cancel auth ticket from GetAuthSessionTicket, called when no longer playing game with the entity you gave the ticket to
void Steam_GameServer::CancelAuthTicket( HAuthTicket hAuthTicket )
{
    PRINT_DEBUG("Steam_GameServer::CancelAuthTicket\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    ticket_manager->cancelTicket(hAuthTicket);
}


// After receiving a user's authentication data, and passing it to SendUserConnectAndAuthenticate, use this function
// to determine if the user owns downloadable content specified by the provided AppID.
EUserHasLicenseForAppResult Steam_GameServer::UserHasLicenseForApp( CSteamID steamID, AppId_t appID )
{
    PRINT_DEBUG("Steam_GameServer::UserHasLicenseForApp\n");
    return k_EUserHasLicenseResultHasLicense;
}


// Ask if a user in in the specified group, results returns async by GSUserGroupStatus_t
// returns false if we're not connected to the steam servers and thus cannot ask
bool Steam_GameServer::RequestUserGroupStatus( CSteamID steamIDUser, CSteamID steamIDGroup )
{
    PRINT_DEBUG("RequestUserGroupStatus\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return true;
}



// these two functions s are deprecated, and will not return results
// they will be removed in a future version of the SDK
void Steam_GameServer::GetGameplayStats( )
{
    PRINT_DEBUG("GetGameplayStats\n");
}

STEAM_CALL_RESULT( GSReputation_t )
SteamAPICall_t Steam_GameServer::GetServerReputation()
{
    PRINT_DEBUG("GetServerReputation\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return 0;
}


// Returns the public IP of the server according to Steam, useful when the server is 
// behind NAT and you want to advertise its IP in a lobby for other clients to directly
// connect to
uint32 Steam_GameServer::GetPublicIP_old()
{
    PRINT_DEBUG("GetPublicIP_old\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    uint32 ip = network->getOwnIP();
    PRINT_DEBUG("%X\n", ip);
    return ip;
}

SteamIPAddress_t Steam_GameServer::GetPublicIP()
{
    PRINT_DEBUG("GetPublicIP\n");
    SteamIPAddress_t ip = SteamIPAddress_t::IPv4Any();
    ip.m_unIPv4 = GetPublicIP_old();
    return ip;
}

void Steam_GameServer::GetPublicIP_fix(SteamIPAddress_t *out)
{
    PRINT_DEBUG("GetPublicIP_fix\n");
    if (out) *out = GetPublicIP();
}

// These are in GameSocketShare mode, where instead of ISteamGameServer creating its own
// socket to talk to the master server on, it lets the game use its socket to forward messages
// back and forth. This prevents us from requiring server ops to open up yet another port
// in their firewalls.
//
// the IP address and port should be in host order, i.e 127.0.0.1 == 0x7f000001

// These are used when you've elected to multiplex the game server's UDP socket
// rather than having the master server updater use its own sockets.
// 
// Source games use this to simplify the job of the server admins, so they 
// don't have to open up more ports on their firewalls.

// Call this when a packet that starts with 0xFFFFFFFF comes in. That means
// it's for us.
bool Steam_GameServer::HandleIncomingPacket( const void *pData, int cbData, uint32 srcIP, uint16 srcPort )
{
    PRINT_DEBUG("HandleIncomingPacket %i %X %i\n", cbData, srcIP, srcPort);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return true;
}


// AFTER calling HandleIncomingPacket for any packets that came in that frame, call this.
// This gets a packet that the master server updater needs to send out on UDP.
// It returns the length of the packet it wants to send, or 0 if there are no more packets to send.
// Call this each frame until it returns 0.
int Steam_GameServer::GetNextOutgoingPacket( void *pOut, int cbMaxOut, uint32 *pNetAdr, uint16 *pPort )
{
    PRINT_DEBUG("GetNextOutgoingPacket\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    if (outgoing_packets.size() == 0) return 0;

    if (outgoing_packets.back().data.size() < cbMaxOut) cbMaxOut = outgoing_packets.back().data.size();
    if (pOut) memcpy(pOut, outgoing_packets.back().data.data(), cbMaxOut);
    if (pNetAdr) *pNetAdr = outgoing_packets.back().ip;
    if (pPort) *pPort = outgoing_packets.back().port;
    outgoing_packets.pop_back();
    return cbMaxOut;
}


//
// Control heartbeats / advertisement with master server
//

// Call this as often as you like to tell the master server updater whether or not
// you want it to be active (default: off).
void Steam_GameServer::EnableHeartbeats( bool bActive )
{
    PRINT_DEBUG("EnableHeartbeats\n");
}

/// Indicate whether you wish to be listed on the master server list
/// and/or respond to server browser / LAN discovery packets.
/// The server starts with this value set to false.  You should set all
/// relevant server parameters before enabling advertisement on the server.
///
/// (This function used to be named EnableHeartbeats, so if you are wondering
/// where that function went, it's right here.  It does the same thing as before,
/// the old name was just confusing.)
void Steam_GameServer::SetAdvertiseServerActive( bool bActive )
{
    PRINT_DEBUG("SetAdvertiseServerActive\n");
    EnableHeartbeats(bActive);
}

// You usually don't need to modify this.
// Pass -1 to use the default value for iHeartbeatInterval.
// Some mods change this.
void Steam_GameServer::SetHeartbeatInterval( int iHeartbeatInterval )
{
    PRINT_DEBUG("SetHeartbeatInterval\n");
}

void Steam_GameServer::SetMasterServerHeartbeatInterval_DEPRECATED( int iHeartbeatInterval )
{
    PRINT_DEBUG("SetMasterServerHeartbeatInterval_DEPRECATED\n");
}


// Force a heartbeat to steam at the next opportunity
void Steam_GameServer::ForceHeartbeat()
{
    PRINT_DEBUG("ForceHeartbeat\n");
}

void Steam_GameServer::ForceMasterServerHeartbeat_DEPRECATED()
{
    PRINT_DEBUG("ForceMasterServerHeartbeat_DEPRECATED\n");
}


// associate this game server with this clan for the purposes of computing player compat
STEAM_CALL_RESULT( AssociateWithClanResult_t )
SteamAPICall_t Steam_GameServer::AssociateWithClan( CSteamID steamIDClan )
{
    PRINT_DEBUG("AssociateWithClan\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return 0;
}


// ask if any of the current players dont want to play with this new player - or vice versa
STEAM_CALL_RESULT( ComputeNewPlayerCompatibilityResult_t )
SteamAPICall_t Steam_GameServer::ComputeNewPlayerCompatibility( CSteamID steamIDNewPlayer )
{
    PRINT_DEBUG("ComputeNewPlayerCompatibility\n");
    std::lock_guard<std::recursive_mutex> lock(global_mutex);
    return 0;
}

void Steam_GameServer::RunCallbacks()
{
    bool temp_call_servers_connected = call_servers_connected;
    bool temp_call_servers_disconnected = call_servers_disconnected;
    call_servers_disconnected = call_servers_connected = false;

    if (temp_call_servers_connected) {
        PRINT_DEBUG("Steam_GameServer::SteamServersConnected_t\n");
        SteamServersConnected_t data;
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.1);
    }

    if (logged_in && !policy_response_called) {
        PRINT_DEBUG("Steam_GameServer::GSPolicyResponse_t\n");
        GSPolicyResponse_t data;
        data.m_bSecure = !!(flags & k_unServerFlagSecure);
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.11);
        policy_response_called = true;
    }

    if (logged_in && check_timedout(last_sent_server_info, SEND_SERVER_RATE)) {
        PRINT_DEBUG("Steam_GameServer Sending Gameserver\n");
        Common_Message msg;
        msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
        server_data.set_appid(settings->get_local_game_id().AppID());
        msg.set_allocated_gameserver(new Gameserver(server_data));
        msg.mutable_gameserver()->set_num_players(ticket_manager->countInboundAuth());
        network->sendToAllIndividuals(&msg, true);
        last_sent_server_info = std::chrono::high_resolution_clock::now();
    }

    if (temp_call_servers_disconnected) {
        SteamServersDisconnected_t data;
        data.m_eResult = k_EResultOK;
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data));

        if (!logged_in) {
            Common_Message msg;
            msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
            msg.set_allocated_gameserver(new Gameserver(server_data));
            msg.mutable_gameserver()->set_offline(true);
            network->sendToAllIndividuals(&msg, true);
        }
    }
}
