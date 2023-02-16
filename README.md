About
=====

FFI bindings to `lobby_connect` from [Goldberg SteamEmu](https://gitlab.com/Mr_Goldberg/goldberg_emulator) [recompiled as a dll](https://gitlab.com/Mr_Goldberg/goldberg_emulator/-/issues/96).

Discover people playing on the network using the Goldberg SteamEmu with the launch parameter to connect to their game.

This can be used in Node.js/Electron.<br/>
💡 Usage without Node/JavaScript is also [explained down below](https://github.com/xan105/node-lobby_connect#usage-without-node--javascript).

Feel free to make a cool looking GUI for this 😃
<p align="center">
<img src="https://github.com/xan105/node-lobby_connect/raw/master/screenshot/gui.png"><br />
<em>Example build with Electron</em>
</p>

Example
=======

```js
import { lobbyPlayerState } from "@xan105/lobby_connect";

setInterval(function(){ 
  lobbyPlayerState()
  .then(console.log)
  .catch(console.error);
}, 3000);

/*output example
[] //Nobody on the network
[ { name: 'Xan', appID: 466560, connect: '' } ] //In game
[ { name: 'Xan', appID: 466560, connect: '+connect_lobby 109212296511539930' } ] //lobby available
*/
```

Install
=======

```
npm install @xan105/lobby_connect
```

API
===

⚠️ This module is only available as an ECMAScript module (ESM) starting with version 2.0.0.<br />
Previous version(s) are CommonJS (CJS) with an ESM wrapper.

## Named export

### `lobbyPlayerState(): Promise<obj[]>`

Discover people playing on the network.

Return an array of 'Player' object as follow:

- name (string): player's name
- appID (number): game's appid
- connect (string): the launch parameter to connect to the player's game

Build
=====

The dll src is located at `lib\src\goldberg_emulator\lobby_connect_dll.cpp`

If you want to build the dll yourself please see the build instructions for the goldberg emulator (https://gitlab.com/Mr_Goldberg/goldberg_emulator#windows).
When your env is set. Run the following npm script in the root dir of this package :

```js
npm run-script build_x86 //for x86
npm run-script build_x64 //for x64

//or

npm run-script build //for both x86 & x64
```

NB: `npm run-script update` will `git pull` ./lib/src/goldberg_emulator

Usage without Node / JavaScript
===============================

You can bind directly to the dynamic link library (dll) found in `lib/dist` with any lang that allows for FFI (_Foreign Function Interface_) with standard C declaration (c-shared).

Have a look at the bindings in `lib/lobby.js` to get a feel for it. Even if you don't know any JavaScript. It's rather simple.

There are 3 functions exported : 
 - lobby_ready(int delay): int
 - lobby_player_count(): int
 - lobby_player_info(int playerIndex): `<Player>`

 `<Player>` being the following Struct:
```c
struct {
  const char *name;
  int appID;
  const char *connect;
  uint64 lobby;
};
```

1. Use `lobby_ready()` with a delay to wait for a few seconds for connections (_delay * 10_) (Originally Mr. Goldberg uses 200ms).<br/>
It returns 1 (true) if SteamAPI is initialized; 0 (false) otherwise.

2. Use `lobby_player_count()` to get how many people they are on the network.

3. Loop until max number of people from the above (_step2_) using `lobby_player_info()` to get each player information (Player Struct).

If you have a `connect` string then that's your connect argument to pass to the game;
Otherwise transform the uint64 lobby to a string and append "+connect_lobby" to it.

Dont' forget credits and license and you are done :wink:
