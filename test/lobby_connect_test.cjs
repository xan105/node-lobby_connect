"use strict";

const lobby = require('../lib/lobby_connect.cjs');

setInterval(function(){ 
  lobby().then(console.log).catch(console.error);
}, 3000);
