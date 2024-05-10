/*
Copyright (c) Anthony Beaumont
This source code is licensed under the GNU LESSER GENERAL PUBLIC LICENSE Version 3
found in the LICENSE file in the root directory of this source tree.
*/

import { arch } from "node:process";
import { join } from "node:path";
import { dlopen, struct } from "@xan105/ffi/koffi";
import { Failure } from "@xan105/error";

const file = join(import.meta.dirname, "dist", 
             `lobby_connect_${{ia32: "x86"}[arch] ?? arch}.dll`)
             .replace("app.asar", "app.asar.unpacked"); //electron asar friendly
             
const Player = struct({
    name: "string",
    appID: "int",
    connect: "string",
    lobby: "uint64"
});

const { 
  lobby_ready, 
  lobby_player_count, 
  lobby_player_info 
} = dlopen(file, {
  "lobby_ready": {
    result: "int",
    parameters: ["int"]
  },
  "lobby_player_count": {
    result: "int"
  },
  "lobby_player_info": {
    result: Player,
    parameters: ["int"]
  }
},{
  nonblocking: true,
  errorAtRuntime: true
});

async function lobbyPlayerState(){
  
  const isReady = await lobby_ready(200);
  if(!isReady) throw new Failure("SteamAPI not ready", "ERR_STEAM_EMU");

  const players = [];
  const playerCount = await lobby_player_count();
      
  for (let i=0; i < playerCount; i++) 
  {
    const player = await lobby_player_info(i);
    players.push({
      name: player.name,
      appID: player.appID,
      connect: (player.connect.length == 0 && player.lobby) ? `+connect_lobby ${player.lobby}` : player.connect
    });
  }

  return players;
}

export { lobbyPlayerState }