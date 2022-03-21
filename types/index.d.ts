declare interface IPlayer {
  name: string,
  appID: number,
  connect: string
}

export default function(): Promise<IPlayer[]>;