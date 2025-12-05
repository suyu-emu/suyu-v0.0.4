# Getting Gyro/Motion Controls Working on Steam Deck
Use this guide when you want to use the Steam Deck's native gyro functionality for motion controls in Eden.

**Click [Here](https://evilperson1337.notion.site/Getting-Gyro-Motion-Controls-Working-on-Steam-Deck-2b057c2edaf681a1aaade35db6e0fd1b) for a version of this guide with images & visual elements.**

---

### Pre-Requisites

- Steam Deck Set up and Configured
- Eden set up and Configured
- Internet Access

---

## Steps

1. Go into Steam Deck's Desktop Mode, and use the shortcut to launch EmuDeck.
2. Install [SteamDeckGyroDSU](https://github.com/kmicki/SteamDeckGyroDSU/releases) by going to *3rd Party Tools > Gyroscope* and clicking **Install.**
    a. Alternatively you can install [SteamDeckGyroDSU](https://github.com/kmicki/SteamDeckGyroDSU/releases) manually following the GitHub page instructions. 
3. Upon completion of the installation. You will need to reboot your Steam Deck.  Do so before continuing on.
4. Go back into the Steam Deck Desktop Mode and open the Dolphin File Explorer.
5. Navigate to the following directory to see you controller configuration: `/home/deck/.config/Eden`
6. *Right-Click* the **qt-config.ini** file and open it with ***Kate***
7. Look for the following line: `player_0_motionleft=[empty]`.
8. Change the line to now say: `player_0_motionleft="motion:0,pad:0,port:26760,guid:0000000000000000000000007f000001,engine:cemuhookudp"`
9. Save the file and open Eden.
10. Launch a compatible title, like *The Legend of Zelda: Breath of the Wild*.
11. Test the gyro capabilities, for the above mentioned title, it is accessed by holding down the **R Trigger** and moving the Steam Deck around.