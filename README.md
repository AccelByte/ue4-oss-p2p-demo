# UE4 AccelByteOSS P2P Demo

The repository consist the P2P demo using AccelByte OSS.

This P2P uses thirdparty library [libJuice](https://github.com/paullouisageneau/libjuice)

## Prepare the signaling server

This repo have a simple signaling server written in JS. Signaling server use to exchange P2P route info, this P2P info will be use by libJuice to make a peer to peer connection.
To use signaling server:
- Install NodeJS
- Clone this repo, open command line and go to folder ```server/node-signaling-server```
- Type command ```npm install```
- Run the server ```npm start```
 
By default the server will run on port ```8555```, make sure the firewall does not block the port.
In order to make the signaling server accessible from internet, the signaling server must run in machine that has IP public (ex: in cloud).

## How to use in vanilla shootergame
- Clone this repo
- Get the Shooter Game from Unreal Learn Center
- Copy the AccelByteOnlineSubsystem to [Your Shooter Game Folder]/Plugins
- Edit ```Config/DefaultEngine.ini``` and add this to the end of file: 
```ini
[/Script/OnlineSubsystemAccelByte.IpNetDriverAccelByte]
NetConnectionClassName=OnlineSubsystemAccelByte.IpConnectionAccelByte
ReplicationDriverClassName=/Script/ShooterGame.ShooterReplicationGraph

[OnlineSubsystemAccelByte]
bEnabled=true
SignalingServerURL="ws://[your signaling IP / domain]:8555"
TurnServerUrl=""
TurnServerPort=0
TurnServerUsername=""
TurnServerPassword=""
StunServerUrl="stun1.l.google.com"
StunPort=19302
```

- Edit ```Config/Windows/WindowsEngine.ini``` to install ```AccelByteNetDriver``` and use AccelByteOnlineSubsystem as default OSS of the game
```ini
[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="OnlineSubsystemAccelByte.IpNetDriverAccelByte",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")
+NetDriverDefinitions=(DefName="DemoNetDriver",DriverClassName="/Script/Engine.DemoNetDriver",DriverClassNameFallback="/Script/Engine.DemoNetDriver")

[OnlineSubsystem]
DefaultPlatformService=AccelByte
```

## Stun Server

The example above we use Stun server from google, this should be enough if between 2 peers are able to connect directly and the router support nat punch. But if the peer is behind nat that the router not support nat punch, then it will need to use turn server.

## Turn Server

Turn server is a relay server, the libJuice will use relay when it is not possible to make direct p2p connection between 2 peers. Unfortunately, there is no free turn server out there that reliable to use because turn server requires some resources when a lot of peer connected to the relay. We can install our own turn server using Coturn, please check on google how to install it on your cloud.
When the turn server is ready we can set it to the ```Config/DefaultEngine.ini```

## Testing the P2P

- Run 2 instance of the game, can be at the same computer or different computer. 1 instance will be as host and another one as client
- To run host, on the main menu select ```HOST```, on the next menu make sure that the ```LAN``` option is ```OFF```. And select the ```FREE FOR ALL```
- To join the session, on main menu select ```JOIN``` and make sure the ```LAN``` and ```DEDICATED``` options are both ```OFF```. And select ```SERVER``` at top of the menu.