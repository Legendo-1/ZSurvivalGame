# Sounds, effects and animations

How to give the game its sounds, particles and character animations: shots, reloads, deaths, taking out a weapon, crafting, footsteps and the rest, so they work for every player.

**Last updated:** 2026-10-26 (first version: events for shots, reloads, deaths, taking out weapons, level-ups, pickups, items and crafting; UI sounds; take-out animations)

## Who does what

| Part | Where | Why |
|---|---|---|
| **When** something happens (a shot, a death, a pickup), and sending it to every player | C++ (done) | It's game logic and networking; the server decides |
| **What it sounds and looks like** | The editor: GameplayCue Notify assets, sounds, Niagara effects | Change and try sounds without touching code |
| **Which weapon, character or item uses which sound** | Their Data Assets and Class Defaults (a tag) | Each weapon can sound different, nothing hard-coded |
| **Sounds that belong to an animation** (footsteps, magazine clicks, a swing) | Anim Notifies in the animation | They stay in time with the movement |
| **Which animation plays** (fire, reload, take out) | The weapon's Data Asset | One place per weapon |

C++ never names a sound file. It only sends an **event** with a **tag**, e.g. `GameplayCue.Weapon.Fire`. A **GameplayCue Notify** asset with the same tag (in `Content/GameplayCues`) says what plays. No asset for a tag means nothing plays, and nothing breaks. So adding a sound is making an asset, and removing it is deleting the asset.

## The events

| Tag | When | Where it plays | Set per thing in |
|---|---|---|---|
| `GameplayCue.Weapon.Fire` | Each shot | At the muzzle | The weapon's Data Asset → **Effects → Fire Cue** |
| `GameplayCue.Weapon.Impact` | A shot hits something | Where it hit | Weapon → **Impact Cue** |
| `GameplayCue.Weapon.Reload` | A reload starts | On the character | Weapon → **Reload Cue** |
| `GameplayCue.Weapon.Equip` | A weapon is taken out | On the character | Weapon → **Equip Cue** (its **Equip Montage** plays with it) |
| `GameplayCue.Character.Hit` | A character takes damage | Where it was hit | The character's Class Defaults → **Effects → Character Cues → Hit Cue** |
| `GameplayCue.Character.Death` | A player or enemy dies | On the character | **Character Cues → Death Cue** |
| `GameplayCue.Character.LevelUp` | A player reaches a new level | On the player | (one for everyone) |
| `GameplayCue.Item.Pickup` | A player picks something up | On the player | The item's Data Asset → **Effects → Pickup Cue** |
| `GameplayCue.Item.Use` | A player uses an item up (a bandage, a tome) | On the player | Item → **Use Cue** |
| `GameplayCue.Crafting.Working` | **Lasts** while a player crafts | At the station (if the recipe needs one), else on the player | The recipe → **Effects → Working Cue** |
| `GameplayCue.Crafting.Done` | A combine made something | On the player | Recipe → **Done Cue** |

**Ready-made tags for one sound per thing:** `GameplayCue.Weapon.Fire.Rifle` / `.Pistol`, `GameplayCue.Weapon.Reload.Rifle` / `.Pistol`, `GameplayCue.Weapon.Equip.Rifle` / `.Pistol`, `GameplayCue.Character.Hit.Player` and `GameplayCue.Character.Death.Player`. DA_Rifle, DA_Pistol and BP_ThirdPersonCharacter already use them.

**Without its own asset, a tag plays its parent's.** The rifle's shot is `GameplayCue.Weapon.Fire.Rifle`. If there's no asset for that tag, the asset for `GameplayCue.Weapon.Fire` plays. So:
- one asset for `GameplayCue.Weapon.Fire` gives every gun a shot sound
- one more for `GameplayCue.Weapon.Fire.Rifle` gives the rifle its own
- the same goes for deaths: `GameplayCue.Character.Death` is every enemy's death, `GameplayCue.Character.Death.Player` the players'

## Making a sound for an event

1. **Import the sound:** drag a `.wav` file into the Content Browser, e.g. into `Content/Audio`.
   - For a few versions played at random (shots, footsteps): select them, right-click → **Create Cue** → open the Sound Cue and put a **Random** node in front of them.
   - **Sounds in the world need an Attenuation** so they get quieter with distance. Open the sound → **Attenuation → Override Attenuation** (or pick a shared **Sound Attenuation** asset). Without one, every player hears it at full volume anywhere on the map.
2. **Make the GameplayCue Notify:** in `Content/GameplayCues` (create the folder if it's not there), right-click → **Blueprint Class** → **All Classes** → search **GameplayCueNotify_Burst** (it shows as **GCN Burst**). Name it after its tag, e.g. `GCN_Weapon_Fire`.
3. Open it → **Class Defaults** → **Gameplay Cue Tag**: `GameplayCue.Weapon.Fire`.
4. **GCN Effects → Burst Effects → Burst Sounds → +** → **Sound**: your sound. On the same page:
   - **Burst Particles:** a Niagara effect, e.g. a muzzle flash or blood
   - **Burst Camera Shake:** a shake, e.g. for the shooter
   - **Burst Decal:** e.g. a bullet hole
5. Save, press Play and shoot.

No Blueprint graph is needed. Useful settings in **GCN Defaults**:
- **Default Placement Info → Attach Policy: Attach To Target:** the sound moves with the character, e.g. a long reload. Add a **Socket Name** (e.g. `head` for a death scream).
- **Default Spawn Condition → Locally Controlled Policy: Local Only** (with **Locally Controlled Source: Instigator Actor**): only the player who did it gets it, e.g. a camera shake on the shooter's screen.
- **Default Spawn Condition → Allowed Surface Types:** only for some surfaces, e.g. one impact asset entry for metal and another for wood. Surface types come from Physical Materials: Project Settings → Physics → **Physical Surface**.
- **Chance To Play:** e.g. 0.3 for a grunt that only plays sometimes.

## The crafting loop (a sound that lasts)

`GameplayCue.Crafting.Working` is on while a recipe with a **Craft Time** is being made, and off when it's done or stopped.
1. In `Content/GameplayCues`: **Blueprint Class** → search **GameplayCueNotify_Looping** (**GCN Looping**) → name it `GCN_Crafting_Working`, tag `GameplayCue.Crafting.Working`.
2. **Looping Effects → Looping Sounds → +** → a sound that loops (open the Sound Wave and tick **Looping**), e.g. hammering.
3. Optional: **Application Effects** play once at the start, **Removal Effects** once at the end (e.g. a finishing clank).

A different loop for some recipes: a child tag (e.g. `GameplayCue.Crafting.Working.Chemistry`), set in those recipes' **Working Cue**, with its own GCN Looping.

## Your own tags (a new weapon, enemy or item)

1. **Project Settings → Gameplay Tags → Manage Gameplay Tags → +**: add one under the event's tag, e.g. `GameplayCue.Weapon.Fire.Shotgun`.
2. Set it in the thing's Data Asset or Class Defaults, e.g. the shotgun's **Fire Cue**. The tag picker only lists fitting tags.
3. Make a GCN for it, as above.

**Enemies:** for a zombie with its own sounds, make a Blueprint child of **ZZombieCharacter** (you need one anyway for a real zombie model) and set its **Effects → Character Cues** to e.g. `GameplayCue.Character.Death.Zombie`. Spawners then use that Blueprint as **Enemy Class**.

**Turning an event off** for one thing: clear its tag. For everything: delete the GCN asset.

## Sounds in animations (footsteps, reloads, swings)

1. Open the animation (e.g. the run animation, or a reload montage).
2. In the **Notifies** track, right-click at the right moment → **Add Notify → Play Sound** → pick the sound in its Details. For footsteps: one at each moment a foot touches the ground.
3. Save.

Every player's machine plays every character's animations, so these sounds play for everyone without any networking. The same works for enemies, e.g. a swing sound in the zombie's attack animation, and for Niagara effects (**Add Notify → Play Niagara Effect**).

**With a reload animation:** put the reload sounds in the animation, where they line up with the hands. Then the weapon's **Reload Cue** isn't needed: clear it, or don't make a GCN for it. Without an animation, the Reload Cue gives a simple reload sound.

## Animations (montages)

| Weapon Data Asset setting | When it plays | How it reaches the others |
|---|---|---|
| **Fire Montage** | Each shot | The ability system plays it for everyone |
| **Reload Montage** | While reloading, sped up or slowed down to the reload time (mods and traits change it) | The ability system |
| **Equip Montage** | When the weapon is taken out | It comes with the Equip Cue, so every machine plays it at that moment |

A montage from an animation: right-click the animation → **Create → Create AnimMontage**.

**The Equip Montage must stay in place.** It starts on every machine by itself, so an animation that moves the character (root motion) would put your character out of step with the server. Such a montage isn't played: the Output Log says why, and **Validate Assets** on the weapon's Data Asset shows an error. To fix one: open its animation → **Asset Details** → untick **Enable Root Motion**, or pick another animation.

So the legs keep walking during reloads and other arm animations, the montages play on the **upper body** only. ANIMATION.md → "Montages on the upper body" has the steps.

## UI sounds (only you hear them)

`BP_ThirdPersonPlayerController` → **Class Defaults → UI → Sounds**:

| Setting | When |
|---|---|
| **Inventory Open Sound / Inventory Close Sound** | The inventory screen opens or closes |
| **Crafted Sound** | A combine made something |
| **Discovery Sound** | A combine made something for the first time (instead of Crafted Sound) |
| **Craft Failed Sound** | A combine made nothing |

These play "in your ears" (2D), not in the world, so they need no attenuation. Buttons in your own Widget Blueprints: select the button → **Style → Pressed Sound / Hovered Sound**.

## Checking which events happen

Console (the key under Esc): `AbilitySystem.DisplayGameplayCues 1`. Every event's tag shows as text in the world, and is written to the Output Log (`LogAbilitySystem`). `0` turns it off. It works before any sound exists, e.g. to see that the client gets the host's reload.

## Multiplayer

- **The server sends the events** (deaths, hits, taking out weapons, crafting, level-ups, pickups) to every player near them. The host plays them too.
- **Your own shots and reloads play on your machine right away**, without waiting for the server; the server sends them to the others.
- **The crafting loop is lasting:** a player who walks up while someone crafts hears it too, and it stops for everyone when the craft ends.
- **One-off events reach only the players near them at that moment**, which is right for sounds. Someone who comes near later doesn't hear an old shot.
- **UI sounds** never leave your machine.

## Adding a new event in C++ (for later)

1. A tag in `ZGameplayTags` (`.h` and `.cpp`), under `GameplayCue`, e.g. `GameplayCue.Character.Dodge`.
2. Where it happens, on the server (or inside an ability): `UZGameplayCueLibrary::ExecuteCue(Actor, Tag, Parameters)` for a one-off; `AddCue` / `RemoveCue` for one that lasts. Fill the parameters the notify can use: Location, Instigator, Source Object, Raw Magnitude.
3. One sound per thing: an `FGameplayTag` setting on its Data Asset with `meta = (Categories = "GameplayCue.Character.Dodge")`, starting as the tag.
4. A row in the table above.

Blueprints can call **Execute Cue** too, e.g. a door Blueprint when it opens (on the server).

## Plan: what isn't made yet

- **Most sounds and effects:** only the shot has one so far (`GCN_Weapon_Fire`). `Content/Audio/FreeWeaponSFX` also has reload and take-out sounds, and a handgun shot for the pistol's own `GameplayCue.Weapon.Fire.Pistol`.
- **Footsteps that depend on the ground** (grass, metal, wood): a notify that checks the surface under the foot.
- **Music and ambience:** ambient sounds in the levels, music that changes in fights.
- **Volume settings:** Sound Classes and a Sound Mix, for music / effects / UI sliders in a settings menu.
- **Enemy idle sounds:** groans while walking around.
