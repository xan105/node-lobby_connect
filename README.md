Library to use lobby_connect from Goldberg SteamEmu (https://gitlab.com/Mr_Goldberg/goldberg_emulator) in Node.js.
Using 'node-ffi' and 'lobby_connect.exe' recompiled as a dll.

Discovers people playing on the network using the Goldberg SteamEmu with the launch parameters to connect to their games.

cf: https://gitlab.com/Mr_Goldberg/goldberg_emulator/-/issues/96

Install
-------

*Prequisites: VS2017 / Python 2.7(node-gyp)*

```
npm install https://github.com/xan105/node-lobby_connect
//or
npm install xan105/node-loby_connect
```

Usage
-----

```js
"use strict";

const lobby = require('@xan105/lobby_connect');

setInterval(function(){ 
  lobby().then(console.log).catch(console.error);
}, 3000);

/*output example
[] //Nobody on the network
[ { name: 'Xan', appID: 466560, connect: '' } ] //In game
[ { name: 'Xan', appID: 466560, connect: '+connect_lobby 109212296511539930' } ] //lobby available
*/
```

Build
-----
If you want to build the dll yourself please see the build instructions for the goldberg emulator (https://gitlab.com/Mr_Goldberg/goldberg_emulator#windows).
When your env is set. Run the following npm script in the root dir of this package :

```js
npm run-script build_dll_x86 //for x86
npm run-script build_dll_x64 //for x64

```
