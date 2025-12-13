# Multiplayer
Use this guide to answer questions regarding and to start using the multiplayer functionality of Eden.

## Multiplayer FAQ
This FAQ will serve as a general quick question and answer simple questions.

**Click [Here](https://evilperson1337.notion.site/Multiplayer-FAQ-2c357c2edaf680fca2e9ce59969a220f) for a version of this guide with images & visual elements.**

### Can Eden Play Games with a Switch Console?
No - The only emulator that has this kind of functionality is *Ryujinx* and it's forks.  This solution requires loading a custom module on a modded switch console to work.

### Can I Play Online Games?
No - This would require hijacking requests to Nintendo's official servers to a custom server infrastructure built to emulate that functionality.  This is how services like [*Pretendo*](https://pretendo.network/) operate.  As such, you would not be able to play “Online”, you can however play multiplayer games.

### What's the Difference Between Online and Multiplayer?
I have chosen the wording carefully here for a reason.  

- Online: Games that connect to Nintendo's Official servers and allow games/functionality using that communication method are unsupported on any emulator currently (for obvious reasons).
- Multiplayer:  The ability to play with multiple players on separate systems.  This is supported for games that support LDN Local Wireless.

The rule of thumb here is simple: If a game supports the ability to communicate without a server (Local Wireless, LAN, etc.) you will be able to play with other users.  If it requires a server to function - it will not.  You will need to look up if your title support Local Wireless/LAN play as an option.

### How Does Multiplayer Work on Eden Exactly?
Eden's multiplayer works by emulating the Switch's local wireless (LDN) system, then tunneling that traffic over the internet through “rooms” that act like lobbies. Each player runs their own instance of the emulator, and as long as everyone joins the same room and the game supports local wireless multiplayer, the emulated consoles see each other as if they were on the same local network. This design avoids typical one-save netplay issues because every user keeps an independent save and console state while only the in-game wireless packets are forwarded through the room server.  In practice, you pick or host a room, configure your network interface/port forwarding if needed, then launch any LDN-capable game; from the game's perspective it is just doing standard local wireless, while the emulator handles discovery and communication over the internet or LAN.

### What Do I Need to Do?
That depends entirely on what your goal is and your level of technical ability, you have a 2 options on how to proceed.  

1. Join a Public Lobby.
    1. If you just want to play *Mario Kart 8 Deluxe* with other people and don't care how it works or about latency - this is the option for you. See the *Joining a Multiplayer Room* section for instructions on how to do this.
2. Host Your Own Room.
    1. This option will require you to be comfortable with accessing your router's configuration, altering firewall rules, and troubleshooting when things (inevitably) don't work out perfectly on the first try.  Use this option if you want to control the room entirely, are concerned about latency issues, or just want to run something for your friends. See the *Hosting a Multiplayer Room* section for next steps*.*

### Can Other Platforms Play Together?
Yes - the platform you choose to run the emulator on does not matter.  Steam Deck users can play with Windows users, Android users can play with MacOS users, etc.  Furthermore different emulators can play together as well (Eden/Citron/Ryubing, etc.) - but be you may want to all go to the same one if you are having issues.

### What Pitfalls Should I Look Out For?
While it would be nice if everything always worked perfectly - that is not reality.  Here are some things you should watch out for when attempting to play multiplayer.

1. Emulator Version Mismatches
    1. Occasionally updates to the emulator of choice alter how the LDN functionality is handled.  In these situations, unexpected behavior can occur when trying to establish LDN connections.  This is a good first step to check if you are having issues playing a game together, but can join the same lobby without issue.
2. Game Version Mismatches
    1. It is best practice to have the game version be identical to each other in order to ensure that there is no difference in how the programs are handling the LDN logic.  Games are black boxes that the dev team cannot see into to ensure the logic handling operates the same way.  For this reason, it is highly advised that the game versions match across all the players.  This would be a good 2nd step to check if you are having issues playing a game together, but can join the same lobby without issue.
3. Latency
    1. Because this implementation is emulating a LAN/Local Wireless connection - it is extremely sensitive to network latency and drops.  Eden has done a good job of trying to account for this and not immediately drop users out - but it is not infallible.  If latency is a concern or becomes an issue - consider hosting a room.

---

## Joining a Multiplayer Room
Use this when you need to connect to a multiplayer room for LDN functionality inside of Eden.  This does not cover how to host a room, only joining existing ones.

**Click [Here](https://evilperson1337.notion.site/Access-Your-Multiplayer-Room-Externally-2c357c2edaf681c0ab2ce2ee624d809d) for a version of this guide with images & visual elements.**

### Pre-Requisites
- Eden set up and functioning
- Multiplayer Options Configured in Eden Settings
- Network Access

## Steps
There are 2 primary methods that you can use to connect to an existing room, depending on how the room is hosted.

- Joining a Public Lobby
    - This option allows you to view publicly hosted lobbies and join them easily.  Use this option if you just want to join a room and play quickly.
- Directly Connecting to a Room
    - Use this option if the hosted room is not on the public lobby list (private, internal network only, etc.)

<aside>

***NOTE:*** Just because a lobby appears on the public lobby list, does not mean that the hoster has properly configured the necessary port forwarding/firewall rules to allow a connection.  If you cannot connect to a lobby, move onto another entry as the issue is probably not on your end.  Start looking at your environment if you are unable to connect to multiple/any lobbies.

</aside>

### Joining a Public Lobby
1. Open Eden and navigate to *Multiplayer → Browse Public Game Lobby*.
2. The **Public Room Browser** will now open and display a list of publicly accessible rooms.  Find one you want to connect to and double click it.
    
    <aside>
    
    ***NOTE***: Just because a room is set for a specific game, does not mean that you ***have*** to play that game in that lobby.  It is generally good practice to do so, but there is no enforcement of that. 
    
    </aside>
3. You will now see a window showing everyone on the lobby, or an error message.

### Direct Connecting to a Room
If the hoster has not made the lobby public, or you don't want to find it in the public game browser - use this option to connect.

1. Open Eden and navigate to *Multiplayer → Direct Connect*.
2. Enter the *Server Address, Port*, *Nickname* (what your user will be called in the room), and a *Password* (if the hoster set one, otherwise leave it blank) and hit **Connect.**
3. You will now see a window showing everyone on the lobby, or an error message.

---

# Hosting a Multiplayer Room
Use this guide for when you want to host a multiplayer lobby to play with others in Eden.  In order to have someone access the room from outside your local network, see the *Access Your Multiplayer Room Externally* section for next steps.

**Click [Here](https://evilperson1337.notion.site/Hosting-a-Multiplayer-Room-2c357c2edaf6819481dbe8a99926cea2) for a version of this guide with images & visual elements.**

### Pre-Requisites
- Eden set up and Functioning
- Network Access
- Ability to allow programs through the firewall on your device.

## Steps
1. Open Eden and navigate to *Emulation → Multiplayer → Create Room.*    
2. Fill out the following information in the popup dialog box.
    
    
    | **Option** | **Acceptable Values** | **Default** | **Description** |
    | --- | --- | --- | --- |
    | Room Name | *Any string between 4 - 20 characters.* | *None* | Controls the name of the room and how it would appear in the Public Game Lobby/Room screen. |
    | Username | *Any string between 4 - 20 characters.* | *None* | Controls the name your user will appear as to the other users in the room. |
    | Preferred Game | *Any Game from your Game List.* | The first game on your game list | What game will the lobby be playing?  You are not forced to play the game you choose and can switch games without needing to recreate the room. |
    | Password | *None or any string* | *None* | What password do you want to secure the room with, if any. |
    | Max Players | 2 - 16 | 8 | How many players do you want to allow in the room at a time? |
    | Port | 1024 - 65535 | 24872 | What port do you want to run the lobby on?  Could technically be any port number, but it's best to choose an uncommon port to avoid potential conflicts.  See [*Well-Known Ports*](https://en.wikipedia.org/wiki/List_of_TCP_and_UDP_port_numbers#Well-known_ports) for more information on ports commonly used. |
    | Room Description | *None or any string* | *None* | An optional message that elaborates on what the room is for, or for a makeshift message of the day presented to users in the lobby. |
    | Load Previous Ban List | [Checked, Unchecked] | Checked | Tells Eden to load the list containing users you have banned before. |
    | Room Type | [Public, Unlisted] | Public | Specifies whether you want the server to appear in the public game lobby browser |
3. Click **Host Room** to start the room server.  You may get a notice to allow the program through the firewall from your operating system.   Allow it and then users can attempt to connect to your room.

---

# Access Your Multiplayer Room Externally
Quite often the person with whom you want to play is located off of your internal network (LAN).  If you want to host a room and play with them you will need to get your devices to communicate with each other.  This guide will go over your options on how to do this so that you can play together.

**Click [Here](https://evilperson1337.notion.site/Access-Your-Multiplayer-Room-Externally-2c357c2edaf681c0ab2ce2ee624d809d) for a version of this guide with images & visual elements.**

### Pre-Requisites
- Eden set up and Functioning
- Network Access

## Options

### Port Forwarding

- **Difficulty Level**: High
    
    <aside>
    
    Use this option if you want the greatest performance/lowest latency, don't want to install any software, and have the ability to modify your networking equipment's configuration (most notably - your router).  Avoid this option if you cannot modify your router's configuration or are uncomfortable with looking up things on your own.
    
    </aside>
    

Port forwarding is a networking technique that directs incoming traffic arriving at a specific port on a router or firewall to a designated device and port inside a private local network. When an external client contacts the public IP address of the router on that port, the router rewrites the packet's destination information (IP address and sometimes port number) and forwards it to the internal host that is listening on the corresponding service. This allows services such as web servers, game servers, or remote desktop sessions hosted behind NAT (Network Address Translation) to be reachable from the wider Internet despite the devices themselves having non-routable private addresses.

The process works by creating a static mapping—often called a “port-forward rule”—in the router's configuration. The rule specifies three pieces of data: the external (public) port, the internal (private) IP address of the target machine, and the internal port on which that machine expects the traffic. When a packet arrives, the router checks its NAT table, matches the external port to a rule, and then translates the packet's destination to the internal address before sending it onward. Responses from the internal host are similarly rewritten so they appear to come from the router's public IP, completing the bidirectional communication loop. This mechanism enables seamless access to services inside a protected LAN without exposing the entire network. 

For our purposes we would pick the port we want to expose (*e.g. 24872*) and we would access our router's configuration and create a port-forward rule to send the traffic from an external connection to your local machine over our specified port (*24872)*.  The exact way to do so, varies greatly by router manufacturer - and sometimes require contacting your ISP to do so depending on your agreement.  You can look up your router on [*portforward.com*](https://portforward.com/router.htm) which may have instructions on how to do so for your specific equipment.  If it is not there, you will have to use Google/ChatGPT to determine the steps for your equipment.


### Use a Tunnelling Service
- **Difficulty Level**: Easy
    
    <aside>
    
    Use this option if you don't want to have to worry about other users machine/configuration settings, but also cannot do port forwarding.  This will still require that you as the hoster install a program and sign up for an account - but will prevent you from having to deal with port forwards or networking equipment. Avoid this option if there is not a close relay and you are getting issues with latency.
    
    </aside>
    

Using a Tunnelling service may be the solution to avoid port forward, but also avoid worrying about your users setup. A tunnelling service works by having a lightweight client run on the machine that hosts the game server. That client immediately opens an **outbound** encrypted connection (typically over TLS/QUIC) to a relay node operated by the tunnel provider's cloud infrastructure. Because outbound traffic is almost always allowed through NAT routers and ISP firewalls, the tunnel can be established even when the host sits behind carrier-grade NAT or a strict firewall. The tunnel provider then assigns a public address (e.g., `mygame.playit.gg:12345`). When a remote player connects to that address, the traffic reaches the the tunnel provider relay, which forwards it through the already-established tunnel back to the client on the private network, and finally onto the local game server's port. In effect, the server appears to the Internet as if it were listening on the public address, while the host never needs to configure port-forwarding rules or expose its own IP directly.

For our purposes we would spawn the listener for the port that way chose when hosting our room.  The user would connect to our assigned public address/port combination, and it would be routed to our machine.  The tunnel must remain active for as long as you want the connection to remain open.  Closing the terminal will kill the tunnel and disconnect the users.

**Recommended Services:**
- [*Playit.GG*](https://playit.gg/)


### Use a VPN Service

- **Difficulty**: Easy

<aside>

Use this option if you don't want to use a tunnelling service, or don't want to manage the overhead of the playit gg solution, don't want to send data through the relay, or any other reason.  Avoid this method if you do not want to have to manage VPN installation/configuration for your users.

</aside>

The VPN solution is a good compromise between the tunnelling solution and port forwarding.  You do not have to port forward or touch your networking equipment at all - but also don't need to send all your data connections through a 3rd party relay.  The big downside is that you will have to ensure all of your users have your VPN solution installed *and* that they have a valid configuration.  When looking for a solution, it is advised to find one that uses the WireGuard protocol for speed, and does not require communication with a server beyond the initial handshake.

**Recommended Services:**
- [*Tailscale*](https://tailscale.com/)
- [*ZeroTier*](https://www.zerotier.com/)
- *Self-hosted VPN Solution*
    - This is so far out of the scope of this document it has a different postal code.

*Check with the provider you select on the sign up and installation process specific to that provider.*

---

# Finding the Server Information for a Multiplayer Room
Use this guide when you need to determine the connection information for the Public Multiplayer Lobby you are connected to.

**Click [Here](https://evilperson1337.notion.site/Finding-the-Server-Information-for-a-Multiplayer-Room-2c557c2edaf6809e94e8ed3429b9eb26) for a version of this guide with images & visual elements.**

### Pre-Requisites
- Eden set up and configured
- Internet Access

## Steps

### Method 1: Grabbing the Address from the Log File
1. Open Eden and Connect to the room you want to identify.
    1. See the *Joining a Multiplayer Room* section for instructions on how to do so if you need them.
2. Go to *File → Open Eden Folder*, then open the **config** folder.
3. Open the the **qt-config.ini** file in a text editor.
4. Search for the following keys: 
    1. `Multiplayer\ip=` 
    2. `Multiplayer\port=`
5. Copy the Server Address and Port.

### Method 2: Using a Web Browser
1. Obtain the name of the room you want the information for.
2. Open a Web Browser.
3. Navigate to  [`https://api.ynet-fun.xyz/lobby`](https://api.ynet-fun.xyz/lobby)
4. Press *Ctrl + F* and search for the name of your room.
5. Look for and copy the Server Address and Port.

### Method 3: Using a Terminal (PowerShell or CURL)
1. Obtain the name of the room you want the information for.
2. Open the terminal supported by your operating system.
3. Run one of the following commands, replacing *<Name>* with the name of the server from step 1.
    
    ### PowerShell Command [Windows Users]
    
    ```powershell
    # Calls the API to get the address and port information
    (Invoke-RestMethod -Method Get -Uri "https://api.ynet-fun.xyz/lobby").rooms | Where-Object {$_.Name -eq '<NAME>'} | Select address,port | ConvertTo-Json
    
    # Example Output
    #{
    #  "address": "118.208.233.90",
    #  "port": 5001
    #}
    ```
    
    ### CURL Command [MacOS/Linux Users] **Requires jq*
    
    ```bash
    # Calls the API to get the address and port information
    curl -s "https://api.ynet-fun.xyz/lobby" | jq '.rooms[] | select(.name == "<NAME>") | {address, port}'
    
    # Example Output
    #{
    #  "address": "118.208.233.90",
    #  "port": 5001
    #}
    ```
    
4. Copy the Server Address and Port.

---

# Multiplayer for Local Co-Op Games
Use this guide when you want to play with a friend on a different system for games that only support local co-op.

**Click [Here](https://evilperson1337.notion.site/Multiplayer-for-Local-Co-Op-Games-2c657c2edaf680c59975ec6b52022a2d) for a version of this guide with images & visual elements.**

Occasionally you will want to play a game with a friend on a game that does not support LDN multiplayer, and only offer local co-op (multiple controllers connected to a single console), such as with *New Super Mario Bros. U Deluxe.*  Emulation solutions have developed 2 primary methods for handling these cases.

1. Netplay: Netplay lets two or more players run the same game on their own computers while sharing each other's controller inputs over the internet, so everyone sees the same game world in sync. One player hosts the session, and the others join as guests, sending their button presses back and forth to keep the gameplay coordinated.
    1. This is a huge over-simplification of how it works, but gives you an idea 
2. Low-Latency remote desktop solutions: Using a service like *Parsec*, the host shares his screen to a remote party with an input device connected.   This device sends inputs to the host machine.

In either situation at its core, we are emulating an input device on the host machine, so the game believes 2 controllers are connected.  No current Switch emulator has a Netplay offering, so we use Parsec to accomplish this for us.

### Pre-Requisites
- Eden Set Up and Fully Configured
- A [*Parsec*](https://parsec.app/) Account
    - Parsec is free to use for personal, non-commercial use.  For instructions on how to set up an account and install the client you should refer to the Parsec documentation on it's site.
- Parsec client installed on your machine and remote (friend's) machine

## Steps

<aside>

This guide will assume you are the one hosting the game and go over things *Parsec* specific at a high level, as their system is subject to change.  Follow *Parsec's* documentation where needed.

</aside>

1. Launch Parsec on the host machine.
2. Connect to the other player in Parsec.  You will know it is successful when the other player can see the host's screen.
    1. If you are the one hosting the game, you will have your friend initiate the remote connection you will accept.
    2. If you are joining a game, you will have to send a connection request the host will have to accept.
3. Verify that the remote player can see the screen and that there is no issues with the connection.
4. Launch Eden.
5. Navigate to *Emulation → Configure*.
6. Select the **Controls** tab.
7. Set up your controller, if necessary.
8. Select the **Player 2** tab and select the **Connect Controller** checkbox.  This enables inputs from another device to be seen as a second controller.
9. Dropdown the **Input Device** and select the controller.
    1. What exactly it shows up as depends on the Parsec settings.
10. Set up the remote player's controller.
11. Hit **OK** to apply the changes.
12. Launch the game you want to play and enter the co-op mode.  How this works depends on the game, so you will have to look in the menus or online to find out.
