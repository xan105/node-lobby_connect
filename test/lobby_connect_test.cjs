"use strict";

const lobby = require('../lobby_connect.cjs');

setInterval(function(){ 
  lobby().then(console.log).catch(console.error);
}, 3000);
