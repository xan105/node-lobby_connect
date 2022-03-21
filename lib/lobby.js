/*
Copyright (c) Anthony Beaumont
This source code is licensed under the GNU LESSER GENERAL PUBLIC LICENSE Version 3
found in the LICENSE file in the root directory of this source tree.
*/

import { fileURLToPath } from "node:url";
import { join, dirname } from "node:path";
import { promisify } from "node:util";
import { Failure } from "@xan105/error";
import ffi from "ffi-napi";
import ref from "ref-napi";
import refStruct from "ref-struct-di";
const Struct = refStruct(ref);

const file = join(dirname(fileURLToPath(import.meta.url)), "dist", `lobby_connect_${(process.arch === "x64") ? 'x64' : 'x86'}.dll`)
             .replace('app.asar', 'app.asar.unpacked'); //electron asar friendly
             
const Player = Struct({
    name: ref.types.CString,
    appID: ref.types.int,
    connect: ref.types.CString,
    lobby: ref.types.uint64
});

const lib = ffi.Library(file, {
   'lobby_ready': ["int", ["int"]],
   'lobby_player_count': ["int", []],
   'lobby_player_info': [Player, ["int"]]
});

async function lobbyPlayerState(){
  
  const isReady = await promisify(lib.lobby_ready.async)(200);
  if(!isReady) throw new Failure("SteamAPI not ready","ERR_STEAM_EMU");

  let players = [];
  
  const playerCount = await promisify(lib.lobby_player_count.async)();
      
  for (let i=0; i < playerCount; i++) 
  {
    const player = await promisify(lib.lobby_player_info.async)(i);
    players.push({
      name: player.name,
      appID: player.appID,
      connect: (player.connect.length == 0 && player.lobby) ? `+connect_lobby ${player.lobby}` : player.connect
    });
  }

  return players;
}

export { lobbyPlayerState }