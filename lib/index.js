/*
Copyright (c) Anthony Beaumont
This source code is licensed under the GNU LESSER GENERAL PUBLIC LICENSE Version 3
found in the LICENSE file in the root directory of this source tree.
*/

import { arch } from "node:process";
import { fileURLToPath } from "node:url";
import { join, dirname } from "node:path";
import koffi from "koffi";
import { dlopen } from "@xan105/ffi/koffi";
import { Failure } from "@xan105/error";

const file = join(dirname(fileURLToPath(import.meta.url)), "dist", 
             `lobby_connect_${(arch === "x64") ? "x64" : "x86"}.dll`)
             .replace('app.asar', 'app.asar.unpacked'); //electron asar friendly
             
const Player = koffi.struct("Player",{
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
    parameters: ["int"],
    nonblocking: true
  },
  "lobby_player_count": {
    result: "int",
    nonblocking: true
  },
  "lobby_player_info": {
    result: Player,
    parameters: ["int"],
    nonblocking: true
  }
});

async function lobbyPlayerState(){
  
  const isReady = await lobby_ready(200);
  if(!isReady) throw new Failure("SteamAPI not ready","ERR_STEAM_EMU");

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