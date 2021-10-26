import lobby from '../lib/esm.js';

setInterval(function(){ 
  lobby()
  .then(console.log)
  .catch(console.error);
}, 3000);
