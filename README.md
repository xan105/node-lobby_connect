About
=====

Library to use `lobby_connect` from [Goldberg SteamEmu](https://gitlab.com/Mr_Goldberg/goldberg_emulator) in Node.js.<br/>
Using 'node-ffi-napi' and 'lobby_connect.exe' recompiled as a dll.

Discover people playing on the network using the Goldberg SteamEmu with the launch parameter to connect to their game.

cf: https://gitlab.com/Mr_Goldberg/goldberg_emulator/-/issues/96

Example
=======

```js
import lobby from "@xan105/lobby_connect";

setInterval(function(){ 
  lobby()
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

`npm install @xan105/lobby_connect`

_Prerequisite: C/C++ build tools and Python 3.x (node-gyp) in order to build [node-ffi-napi](https://www.npmjs.com/package/ffi-napi)_

API
===

⚠️ This module is only available as an ECMAScript module (ESM) starting with version 2.0.0.<br />
Previous version(s) are CommonJS (CJS) with an ESM wrapper.

## Default export

### `function(): obj[]`

Discover people playing on the network.

Return an array of 'Player' object as follow:

- name (string): player's name
- appID (number): game's appid
- connect (string): the launch parameter to connect to the player's game

Build
=====
If you want to build the dll yourself please see the build instructions for the goldberg emulator (https://gitlab.com/Mr_Goldberg/goldberg_emulator#windows).
When your env is set. Run the following npm script in the root dir of this package :

```js
npm run-script build_x86 //for x86
npm run-script build_x64 //for x64

//or

npm run-script build //for both x86 & x64
```

NB: `npm run-script update` will `git pull` ./lib/src/goldberg_emulator
