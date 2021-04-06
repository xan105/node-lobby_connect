import lobby from '../lib/esm.mjs';

setInterval(function(){ 
  lobby().then(console.log).catch(console.error);
}, 3000);
