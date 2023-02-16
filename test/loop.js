import { setInterval } from "node:timers/promises";
import { lobbyPlayerState } from "../lib/index.js";

const iterator = setInterval(3 * 1000, lobbyPlayerState);
for await (const value of iterator)
{
  try{
    const lobby = await value();
    console.log(lobby);
  }catch(err){
    console.error(err);
  }
}

