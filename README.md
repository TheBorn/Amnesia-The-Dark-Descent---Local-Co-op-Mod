# Amnesia: The Dark Descent — Local Co-op Mod

Split-Screen / Dual Monitor Local Co-op for *Amnesia: The Dark Descent*, built on the
[Amnesia64](https://github.com/buzer2020/Amnesia64) 64-bit fork of Frictional Games'
open-source HPL2 engine.

Videos

> These videos are from various **testing runs** and do not show the mod in its fullness.


https://streamable.com/lioopm

https://streamable.com/1kgreu

https://streamable.com/9bkbmg

https://streamable.com/1881zf

https://streamable.com/s8d2nm

Player 1 uses keyboard and mouse. Player 2 uses a gamepad, and has Justine's sounds effects.

> **This is experimental.** The main game and every existing custom story were written
> for exactly one player. Co-op can be forced onto them though, and the compatibility options
> below exist because there is no single correct way to do that. Expect bugs, odd
> behaviour, and the occasional soft lock or even crashes in some stories. Debug teleports and noclip are available by default so you
> can get out of some soft lock scenarios but I have done my best to make sure you do not have to use them, the main game should be playable from start to finish without cheating.

---

## Requirements

- A legitimate copy of *Amnesia: The Dark Descent* and its files. This repository is
  **source only** and contains no game data — built binaries are on the Releases page.
- Visual Studio 2019 (or its Build Tools), x64, I used Rider 2024.
- A gamepad for Player 2, I used an Xbox One controller, other types like PlayStation might work.

## Building

1. Extract `HPL2/dependencies.zip` in place.
2. Open `Amnesia.sln`.
3. Build the `Lux` project, `Release | x64`.
4. Copy the resulting `Lux.exe` into your Amnesia install's `redist` folder.

---

## Playing

Start a new game from the main menu and pick one of:

| Mode | What it does |
|---|---|
| **Co-op Normal** | Simpler co-op. Items, lantern, oil, laudanum, sanity potions and tinderboxes are all shared. |
| **Co-op Hard mode** | Resources must be conserved between you. Item effects are not shared and only one lantern exists — swap it between each other or stay close. Both players must be near a level door to change level. |
| **Co-op Custom** | Uses whatever you set in *Options → Co-op*. Nothing is preset. This is great for a more personal experience and I would recommend taking a look at this instead of doing normal since I have yet to fully find a good balance in it or a preset everyone would agree on. |

Custom stories have a **Start Co-op** button on the story page, all of them can run in co-op mode, but a warning will pop up if the story was not made to be co-op in the first place.

**Continue** and **Load Game** need no co-op choice — a save remembers whether it was a
co-op game, along with the split mode, and restores it.

### Where custom stories are loaded from

`CustomStoryPath` in `redist/config/main_init.cfg` takes a **comma separated list** of
folders, so stories can live anywhere on your machine instead of only in the game's own
`custom_stories`. Out of the box it is just the one folder:

```xml
CustomStoryPath = "custom_stories"
```

To add more, wrap the whole value in **single** quotes and put each folder in double
quotes inside it — the file is XML, so the outer pair has to be the single ones:

```xml
CustomStoryPath = '"custom_stories", "D:\Amnesia Stories", "E:\Steam\steamapps\workshop\content\57300"'
```

Quoting each entry is optional if none of your paths contain a comma:

```xml
CustomStoryPath = "custom_stories, D:\Amnesia Stories"
```

Forward or back slashes both work, folders that do not exist are skipped silently, and
listing the same folder twice will not list its stories twice. Every folder is scanned
one level deeper as well, so a folder holding a *collection* of story folders works just
as well as a story folder itself — that is also how a Steam Workshop content folder gets
picked up, since its subfolders are numeric ids rather than story names.

### Default co-op keys

| Key | Action |
|---|---|
| `F2` | Teleport Player 2 to Player 1 |
| `F1` | Teleport Player 1 to Player 2 |
| Both sticks clicked | Teleport Player 2 to Player 1 (gamepad) |
| `V` | Noclip (Player 1 only) |

These are on by default in forced co-op so you can escape a soft lock. They are off by
default in stories that declare co-op support.

---

## Options → Co-op

**Co-op view** — Left → Right, Top → Bottom, or one entry per connected monitor for
dual-monitor play.

**(Experimental) Allow player model change** — draws each player as one of the game's
character models instead of the default stickman. Lantern-holding and grab animations
are broken on the rigged models; this is off by default for that reason.

### Forced Co-op

These apply only when co-op is forced onto a story that was not built for it. A story
that declares co-op support handles all of this itself and these are ignored.

| Option | Recommended |
|---|---|
| Teleport both players on map teleports | All modes |
| Show flashbacks for both players | All modes |
| Both players required near level doors | Hard mode |
| Shared sanity reward | All modes |
| Shared lantern | Normal mode |
| Shared oil effect | Normal mode |
| Shared laudanum effect | Normal mode |
| Shared sanity potion effect | Normal mode |
| Shared tinderboxes count | Simpler play |
| Player 2 triggers events, look-at, interact, enter & leave callbacks | All modes |
| Full Item Share | Simpler play |

---

## Options → Debug

Off by default. The console, the debug overlay and verbose logging answer their hotkeys
only once switched on here.

- **Enable developer console** — the `~` key
- **Enable ImGui debug menu** — `Insert`
- **Enable verbose logging** — per-frame render and co-op detail to `hpl.log`. Some of it
  costs a full GPU sync every frame, so leave it off unless you were asked for a log.

The first time you switch on the console or the debug menu you get a one-time warning
about what these can do to a save. Verbose logging gets none — it only makes the game slow.

### Enemy Morph

In the debug menu (`Insert`), under **POSSESSION / ENEMY MORPH**. Tick **Enable Morphing**
and Player 1 can become a monster outright — no monster needs to be on the map to take
over, one is spawned for you.

| Key | Become |
|---|---|
| `1` | Servant Grunt |
| `2` | Servant Brute |
| `3` | Suitor (the Justine one, from `entities/ptest`) |
| `4` | Water Lurker |

The same key again changes you back, a different one swaps you straight over, and there
are **Become** buttons in the menu doing the same thing. Controls are the possession ones:
move and look as normal, `Shift` runs, mouse 1 swings, mouse 2 smashes a door in front of
you. The *Possess Cam Distance* and *Height* sliders apply here too.

While morphed your own body is switched off, so Player 2 sees the monster and not you, and
you come back standing where the monster was. A morphed monster is never written to a save,
you are changed back on a map change, and unticking the box changes you back immediately.

Two things worth knowing: a Water Lurker has no visible model unless `showWaterLurkers` is
on, and it only swims — on dry land you get an invisible monster that will not move. The
Suitor needs the Justine content installed; if it is missing the morph is refused and says so.

---

## Console

`~` opens it, `Shift`+`~` opens the log panel. Type-ahead lists commands in blue and
variables with their current value beside them; `Tab` completes and cycles.

### Commands

Aiming is calculated from Player 1 only.

| Command | Example | Description |
|---|---|---|
| `spawn <file.ent>` | `spawn entities/enemy/servant_grunt/servant_grunt.ent` | Spawn an entity in front of you. `Tab` completes paths. |
| `give <item>` | `give tinderbox` | Add an item to the inventory. `Tab` lists what is available. |
| `kill` | `kill` | Kill the monster you are aiming at, might be a bit buggy with its ragdoll physics. |
| `destroy` | `destroy` | Break the prop you are aiming at, playing its real break — broken mesh, debris, sound. Overrules the level editor's *DisableBreakable* tick, but will not break a prop whose type has no break at all; that gets refused rather than silently deleted. |
| `delete` | `delete` | Remove the entity you are aiming at. Script areas need `debugView` on to be targeted. |
| `undo` | `undo` | Restore the most recently deleted entity. |
| `lit` / `unlit` | `lit` | Light or put out the lamp you are aiming at. |
| `lock` / `unlock` | `unlock` | Lock or unlock the door you are aiming at. |
| `noclip` | `noclip` | Toggle noclip bind, press V once enabled to noclip, the command does not enable noclip automatically. Disabling noclip will disarm the noclip button. |
| `loadMap <path.map> [startPos]` | `loadMap maps/main/ch01/07_archives_cellar.map` | Change level immediately to any map found in the games directory. A start position may be named as a second argument. |
| `help` | `help` | List everything registered. |

### Variables

Read with the name alone, set with `name value`.

| Variable | Example | Description |
|---|---|---|
| `coop` | `coop 1` | Local co-op on/off |
| `splitScreenMode` | `splitScreenMode 2` | 1 = left/right, 2 = top/bottom, 3 = dual monitor |
| `fov` | `fov 90` | Field of view, 10–170 |
| `sanityDrainDisabled` | `sanityDrainDisabled 1` | A cheat to stop darkness from draining sanity. |
| `debugView` | `debugView 1` | Draw script areas as coloured boxes, like the level editor. Inactive areas are dashed. |
| `showCollider` | `showCollider 1` | Draw every physics shape as wireframe, both players' own bodies in yellow, very laggy! |
| `showWaterLurkers` | `showWaterLurkers 1` | Draw the water lurker model the game normally hides. |
| `undoArray` | `undoArray 10` | How many deleted entities to keep for `undo` (1–64, default 1) |
| `consoleUnlockSound` | `consoleUnlockSound sounds/door/unlock_door.ogg` | Sound `unlock` falls back to when a door defines none, not really needed but kept for better feedback on some swing doors. |

---

## For custom story authors

Declare co-op support in `custom_story_settings.cfg` so the menu knows before the story
is entered:

```xml
<Main
  ...
  SupportsCoop="true"
/>
```

The same file can set the Co-op Options, so a story that only wants to configure co-op
does not need a `global.hps` to do it:

```xml
<Main
  ...
  SupportsCoop="true"

  CoopAllowCrouchBoost="false"
  CoopAllowPlayerCollision="false"
  CoopAllowMonsterKilling="false"
  CoopMonsterPropDamageMul="10"
  CoopGlobalLantern="true"
/>
```

Those are the defaults, so a settings file that mentions none of them behaves exactly as
if they were absent. What each one does is described under **Co-op options** below.

These are applied as the story starts, *before* the global script runs — so the file is
your starting position and a script that calls the matching `SetCoopAllow*` function
later overrides it. That is what lets you change them mid-play.

A story that declares support is started without the forced-co-op warning, and the
compatibility options above are skipped entirely — the engine assumes the story addresses
each player itself in its maps' HPS scripts, and the Co-op options below become the ones
that count.

### Knowing there are two players

```angelscript
bool GetCoopActive();
```
True when a second player exists at all. Every `...2` function below does nothing when
this is false, so you can call them unguarded, but check this when you want to branch —
for example to place a second key, or to skip a puzzle that needs two people.

```angelscript
int GetTriggerPlayer();
```
Which player is running the callback you are currently inside — `1` or `2`. There was no
way to ask this before, so a callback could not tell whether it was fired by the player
who stepped into the trigger or by the other one. Valid inside collide callbacks,
interact callbacks, look-at callbacks and event timers.

### Co-op options

These are yours, not the player's. In a story that declares co-op support these are the
values that count and the menu's Forced Co-op options are ignored. Settable at
any time — `OnGameStart`, a map's `OnEnter`, or from a timer mid-play — and they take
effect immediately. All of them are saved with the game.

```angelscript
void  SetCoopAllowCrouchBoost(bool abX);        // default false
bool  GetCoopAllowCrouchBoost();
```
Landing on a **crouched** partner's head from above gives real, standable ground — stand
up under them and boost, Counter-Strike style, yay. Walking into each other still goes through, so
this does not make your partner an obstacle in corridors. Use it for intentional
two-player height puzzles.

```angelscript
void  SetCoopAllowPlayerCollision(bool abX);    // default false
bool  GetCoopAllowPlayerCollision();
```
Full player-versus-player collision, always, not just from above. Implies boosting. Be
careful with this one in tight spaces — it is the difference between a partner you can
walk through and one who can wedge you into a corner.

```angelscript
void  SetCoopAllowMonsterKilling(bool abX);     // default false
bool  GetCoopAllowMonsterKilling();
```
Thrown props damage monsters. Non-fatal hits make the monster flinch; at zero health it
dies for good and drops to the floor like a ragdoll. Off by default because Amnesia's whole design is to not fight back after all — turning it on changes that entire aspect of the game, do it if you like your story this way.

```angelscript
void  SetCoopMonsterPropDamageMul(float afMul); // default 10
float GetCoopMonsterPropDamageMul();
```
Damage per kilogram of thrown prop. Monsters have 100 health, so at the default of 10 a
10 kg object kills in one hit. Only relevant when `SetCoopAllowMonsterKilling(true)`.

```angelscript
void  SetCoopGlobalLantern(bool abX);           // default true
bool  GetCoopGlobalLantern();
```
`true` — one found lantern serves both players and either may toggle it at any time.
`false` — a lantern belongs to whoever picked it up. The other player either finds one of
their own or is handed this one: double-click it in the inventory to equip it for giving,
and handing it over snuffs the flame. The oil meter goes with it. You cannot hand one over
if both players already have a lantern. This is what makes a lantern a shared resource to
negotiate over.

### Per-player function variants

Sixty-five of the ordinary player functions have `1` and `2` variants that act on that
player only, formed by adding the number to the end of the name:

```angelscript
SetPlayerHealth1(50.0f);        // Player 1 only
GiveSanityBoost2();             // Player 2 only
TeleportPlayer2("PlayerStart_2");
FadeOut1(1.0f);
```

Covered are checkpoints, health, sanity, lamp oil, damage, teleporting, look-at,
crouching, jumping, movement speed multipliers, FOV and roll fades, insanity events,
messages, death hints, the lantern, fades, and giving or removing items.

Calling a variant for a player who does not exist does nothing, and a getter variant for
a missing player returns zero rather than the other player's value — so a script can
never mistake "there is no Player 2" for a real reading, luckily...

The unsuffixed versions still work. In a story that has **not** declared co-op support
they reach both players, so old stories keep behaving sensibly. In a story that **has**
declared support they address the acting player, on the assumption that you will name
the player you mean when you mean one in particular.

---

## Credits

- **Frictional Games** — *Amnesia: The Dark Descent* and the HPL2 engine, released as
  open source under GPLv3. None of this exists without them putting the source out.
- **[Amnesia64](https://github.com/buzer2020/Amnesia64)** — the 64-bit fork of HPL2 that
  this mod is built on top of. All the work of getting a 2010 32-bit engine building and
  running as x64 on modern toolchains was already done there; this repository started as
  a clone of it.
- **Co-op mod** — The Born.

---

## Licence

GPLv3, the same licence Frictional Games released HPL2 under. See `LICENSE`.

This repository contains engine and game source only. *Amnesia: The Dark Descent* game
assets are not included and are not covered by this licence. Binaries are available on the release page!

---

## FAQ

**1. Is this networked? Do we both need the game?**

Nope! No network code at all. It is all one local instance of the game running on a
single thread, so only **one** of you needs to own Amnesia. Your friend either sits next
to you in real life with a controller, or connects with **Moonlight** (preferably) or
**Parsec** and plays over a stream.

**2. Neither of us has a controller, can we still play?**

Sadly not. The controller is essential — it is what creates a separate input method from
the keyboard and mouse, and those cannot be shared between two players like that. One of
you must have a controller.

**3. I want to play but my friend lives very far from me, can we still do it?**

If they live far away then streaming your game to them introduces latency, and even with
good internet there is no bypassing the speed of light. Distant places can be over 100
milliseconds away, which means input latency and frame latency for your guest. People in
the EU will struggle or be unable to play with people in the US. That is the limitation
of a local co-op game played over the internet like this.

**4. How do I even play with someone who isn't physically here with me?**

Use **Moonlight** or **Parsec** — see the two tutorials below. If you want a full screen
each instead of split-screen, that needs a second display on the host: either a real one,
or a virtual one, which on Parsec means both of you need **Warp** subscriptions.

**5. Can Player 2 noclip?**

Actually no, I didn't implement that. You can still bring them over with `F2`, or have
them teleport to you by pressing both sticks down at the same time.

**6. Can I fork this or make my own edits?**

Any help with further development is appreciated, and you are **always** allowed to make
your own edits privately. I would just like this repository to remain the one public
place to get the mod itself — unless I disappear, refuse to review commits, or set it to
archived, this is the home of the co-op mod.

**7. Is this made by you or just AI slop?**

This is made by me, and it is not *vibe coded*. AI has been used at times, but it is not
the *developer* of the mod.

---

## Playing together over the internet

Both of these send your screen and sound to your friend and send their controller back to
you. The game never knows the difference — as far as it is concerned, Player 2's gamepad
is plugged into your PC.

Whichever you pick, do this first:

- Plug your friend's controller in **on their end** and make sure their app is forwarding
  it. On your PC it will appear as an ordinary virtual gamepad, and that is Player 2.
- Start the game, pick a **Co-op** mode, and set *Options → Co-op → Co-op view* to
  **Left → Right** or **Top → Bottom**. Both of you will be looking at the same split
  screen. That is normal and it works fine.
- A **full screen each** needs a second display on the host, real or virtual, and
  *Co-op view* set to that monitor. See the notes at the end of each tutorial.
- **Give them the pad and nothing else.** Player 2 in this mod is a gamepad, start to
  finish — there is no point in the game where they need your keyboard or your mouse. So
  you can switch both off at the host end and lose nothing at all. Both apps can do this,
  and both can also keep the rest of your desktop out of the stream. How, in each
  tutorial below.

### Moonlight (recommended)

Moonlight is free and open source, and generally gives lower latency than Parsec. It was
originally built for NVIDIA GameStream, which NVIDIA has discontinued — so the host side
is now **Sunshine**, which is also free, open source, and works on AMD and Intel GPUs too,
not just NVIDIA.

**On your PC (the host):**

1. Install [Sunshine](https://github.com/LizardByte/Sunshine).
2. Open its web interface (it opens on `https://localhost:47990` by default) and set a
   username and password.
3. Under *Applications* make sure there is a **Desktop** entry. Streaming the desktop
   rather than the game directly is easier here, because you want your friend to see the
   whole split screen.

**On their PC (the client):**

4. Install [Moonlight](https://moonlight-stream.org/).
5. Both machines need to look like they are on the same network. Over the internet the
   painless way is a mesh VPN — install [Tailscale](https://tailscale.com/) on both PCs,
   sign in to the same account on each, and they will see each other as if they were on
   your LAN. This avoids port forwarding entirely.
6. In Moonlight, add your PC by its Tailscale IP, enter the PIN it shows into Sunshine's
   web interface to pair, then connect.

**Settings worth changing:**

- Set the bitrate as high as their connection will take without stuttering. 20–50 Mbps is
  a reasonable range on a good connection.
- Match the stream resolution and frame rate to what your PC is actually outputting.
  Mismatches cost you latency for nothing.
- Turn **V-Sync** and any frame pacing off on the client. It adds a frame of delay and you
  are already paying for the network.

**For a full screen each:** add a virtual display on the host — the community tools people
usually use are the [Virtual Display Driver](https://github.com/itsmikethetech/Virtual-Display-Driver)
or an HDMI dummy plug — then set *Co-op view* to that display and have Sunshine stream it.
You get your real monitor, they get the virtual one.

**Keeping your PC to yourself:**

- In Sunshine's web interface, *Configuration → Input* has three separate switches:
  *allow keyboard input from the client*, *allow mouse input from the client*, and
  *allow controller input from the client*. All three are **on** by default. Turn the
  first two **off** and leave controller on. Your guest keeps full control of Player 2
  and cannot type, click or alt-tab on your machine at all.
- Stream **one display, not your desktop.** *Configuration → Audio/Video → Output Name*
  (`output_name`) picks which monitor is captured. Point it at the virtual display and
  your real screen is never in the stream in the first place — so it does not matter what
  is on it. On Windows the value is the `device_id` in braces from Sunshine's startup log.
  This pairs naturally with the full-screen-each setup above: the display you hand them is
  the display they get.
- Pairing is a one-time PIN per client and the web interface is behind the username and
  password you set in step 2. Nobody who has not paired can connect, and
  *Troubleshooting → Unpair All Clients* revokes everyone if you ever want a clean slate.

### Parsec

Parsec is easier to set up because it handles the connection for you — no VPN, no port
forwarding — at the cost of somewhat more latency than Moonlight and 20 dollars total of course.

1. Both of you install [Parsec](https://parsec.app/) and make an account.
2. Add each other as friends, then have them join your computer from their Parsec list.
3. In Parsec's host settings, make sure gamepad support is on so their controller reaches
   your PC.

**For a full screen each:** Parsec's virtual display is part of
[Warp](https://parsec.app/warp), their paid tier, and both of you need it for a
multi-display session. Check the [current pricing](https://parsec.app/pricing) — it has
changed over the years. Last I checked its 9.99 USD/EUR for a month. If you would rather not pay, use Moonlight with a free virtual
display driver, or just play split-screen, which costs nothing and works just as fine too but you do get a smaller viewport each.

**Keeping your PC to yourself:**

- The safe setup is already the default here. Parsec gives a guest **controller
  permissions only**; keyboard and mouse have to be handed over deliberately, by clicking
  their profile picture at the bottom of the Parsec window. For this mod there is never a
  reason to — so just do not, and they are a gamepad and nothing else. What guests get by
  default lives under the **Friends** icon if you want to check it.
- **Approved Apps** is the one worth setting up: *Settings cog → Approved Apps → enable
  it*, then launch Amnesia and tick it in the list. If you alt-tab out of the game, the
  guest's screen freezes on *"The host is doing something else right now. Please wait a
  moment!"* and their controller, keyboard and mouse all stop working until you come back.
  Windows only. Run the game **fullscreen** while you use it — in windowed mode the guest
  can still see whatever is around the window.
- A connection has to be accepted by you before it starts, either from the prompt or with
  `Ctrl`+`F1`.
