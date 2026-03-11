# User Handbook - Controllers

Most of the controls should work out of the box. If not, please use a joystick calibrator to ensure it's not an issue with your own controller, for example:

- https://github.com/dkosmari/calibrate-joystick

## Using external controllers on the Steamdeck

In desktop mode ignore your pro controller/xbox contoller external controller and use **Steam Virtual Gamepad 0 as Player 1**. If you have multiple external controllers set **Player 2 to Steam Virtual Gamepad 1**. Steam app must not be closed on desktop mode.

Here's the annoying part of it. When waking up the steam deck from sleep try not to touch any button on the Steamdeck and turn on your external controller. Then open the Eden.AppImage. If you're lucky you can get your external controller to be position 0 and also Steam Virtual Gamepad 0 in desktop mode. If not that is ok too unless you need to configure player 1 to have gyro. You might need to repeat this to get your external controller as Steam Virtual Gamepad 0 so you can config Player 1 having gyro. You might be able to config player 1 to have gyro with the Steamdeck itself. Or you can also config player 1, 2, 3, etc, to have gyro somehow. Make sure they are all using Virtual Gamepads though.

Turn off controller then go to gaming mode. Try not to touch any buttons on the physical Steamdeck. When in gaming mode turn on the external controller. If lucky it will be assigned as Steam Virtual Gamepad 0. If not just use steam Gamemode feature to rearrange controller positions order.

Basically the Steamdeck or the external controller is fighting for position 0 and it depends on what is touched first after waking from sleep.

## Configuring Controller Profiles

Use this guide for when you want to configure specific controller settings to be reused.

**Click [Here](https://evilperson1337.notion.site/Configuring-Controller-Profiles-2be57c2edaf680eabc3ac8c333ec75c4) for a version of this guide with images & visual elements.**

---

#### Pre-Requisites

- Eden Set Up and Configured

---

#### Steps
1. Launch Eden and wait for it to load.
2. Navigate to *Emulation > Configure…*
3. Select **Controls** from the left-hand menu and configure your controller for the way you want it to be in game.
4. Select **New** and enter a name for the profile in the box that appears. Press **OK** to save the profile settings.
5. Select **OK** to close the settings menu.

### Setting Controller Profiles By Game

Use this guide when you want to set up specific controller profiles for specific games. This can be useful for certain games like *Captain Toad Treasure Tracker* where a blue dot appears in the middle of the screen when you have docked mode enabled, but not handheld mode.

**Click [Here](https://evilperson1337.notion.site/Setting-Controller-Profiles-By-Game-2b057c2edaf681658a57f0c199cb6083) for a version of this guide with images & visual elements.**

---

#### Pre-Requisites

- Eden Emulator set up and fully configured
- Controller Profile Created
    - See [*Configuring Controller Profiles*](./ControllerProfiles.md) for instructions on how to do this if needed.

---

#### Steps

1. *Right-Click* the game you want to apply the profile to in the main window and select **Properties.**
2. Navigate to the **Input Profiles** tab in the window that appears. Drop down on *Player 1 profile* (or whatever player profile you want to apply it to) and select the profile you want.

    <aside>

    ***NOTE***: You may have to resize the window to see all tabs, or press the arrows by the tabs to see **Input Profiles**.

    </aside>
1. Click **OK** to apply the profile mapping.
2. Launch the game and confirm that the profile is applied, regardless of what the global configuration is.
