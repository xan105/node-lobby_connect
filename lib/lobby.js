import { fileURLToPath } from "node:url";
import { join, dirname } from "node:path";
import { promisify } from "node:util";
import { Failure } from "./util/error.js";
import ffi from "ffi-napi";
import ref from "ref-napi";
import refStruct from "ref-struct-di";
const Struct = refStruct(ref);

const Player = Struct({
    name: ref.types.CString,
    appID: ref.types.int,
    connect: ref.types.CString,
    lobby: ref.types.uint64
});

const file = join(dirname(fileURLToPath(import.meta.url)), "dist", `lobby_connect_${(process.arch === "x64") ? 'x64' : 'x86'}.dll`)
             .replace('app.asar', 'app.asar.unpacked'); //electron asar friendly

const lib = ffi.Library(file, {
   'lobby_ready': ["int", ["int"]],
   'lobby_player_count': ["int", []],
   'lobby_player_info': [Player, ["int"]]
});

const lobby = {
  ready: function(delay = 200){
    return promisify(lib.lobby_ready.async)(delay);
  },
  playerCount: function(){
    return promisify(lib.lobby_player_count.async)();
  },
  playerInfo: function(i){
    return promisify(lib.lobby_player_info.async)(i);
  }
};

async function lobbyPlayerState(){
  if (!await lobby.ready()) throw new Failure("SteamAPI not ready","ERR_STEAM_EMU");
 
  let players = [];
      
  for (let i=0; i < await lobby.playerCount(); i++) 
  {
    const player = await lobby.playerInfo(i);
    players.push({
      name: player.name,
      appID: player.appID,
      connect: (player.connect.length == 0 && player.lobby) ? `+connect_lobby ${player.lobby}` : player.connect
    });
  }

  return players;
}

export { lobbyPlayerState }