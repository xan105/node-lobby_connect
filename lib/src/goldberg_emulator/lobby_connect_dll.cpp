/* 
xan105 <https://github.com/xan105>
Modified from https://gitlab.com/Mr_Goldberg/goldberg_emulator/-/blob/master/lobby_connect.cpp
*/

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

/*
*/

#include "sdk_includes/steam_api.h"
#include "dll/common_includes.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else

#endif

extern "C"
{
  
  typedef struct {
    const char *name;
    int appID;
    const char *connect;
    uint64 lobby;
  }Player;
  
    __declspec(dllexport) int lobby_ready(int delay) 
    {
      if (SteamAPI_Init()) 
      {
          SteamAPI_RestartAppIfNecessary(LOBBY_CONNECT_APPID); //Set appid to: LOBBY_CONNECT_APPID

          for (int i = 0; i < 10; ++i) { //Waiting a few seconds for connections
              SteamAPI_RunCallbacks();
              std::this_thread::sleep_for(std::chrono::milliseconds(delay));
          }
          
          return 1;
          
       } else {
       
          return 0;
          
       }
    }
    
    __declspec(dllexport) int lobby_player_count()
    {
      return SteamFriends()->GetFriendCount(k_EFriendFlagAll);
    }
    
    __declspec(dllexport) Player lobby_player_info(int i) 
    {
       CSteamID id = SteamFriends()->GetFriendByIndex(i, k_EFriendFlagAll);
       FriendGameInfo_t friend_info = {};
       SteamFriends()->GetFriendGamePlayed(id, &friend_info);
       
       Player player;      
       
       player.name = SteamFriends()->GetFriendPersonaName(id);
       player.appID = friend_info.m_gameID.AppID();
       player.connect = SteamFriends()->GetFriendRichPresence( id, "connect");
              
       if (friend_info.m_steamIDLobby != k_steamIDNil) {
           player.lobby = friend_info.m_steamIDLobby.ConvertToUint64(); 
       }
       
       return player;
      
    }
}
