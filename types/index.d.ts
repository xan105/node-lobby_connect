declare interface Player {
  name: string,
  appID: number,
  connect: string
}

export lobbyPlayerState(): Promise<Player[]>;