import lobby from '../lib/index.js';

setInterval(function(){ 
  lobby()
  .then(console.log)
  .catch(console.error);
}, 3000);
