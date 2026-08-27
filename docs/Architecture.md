# Architecture

A high level map of the plugin: what the systems are, what the managers do, and how a frame flows
through them. Conventions and rules for writing code here live in [CLAUDE.md](../CLAUDE.md); this
document is about what exists and why.

Everything below lives under `src/`. Sibling documents:
[Implemented Features.md](Implemented%20Features.md) for the player-facing feature list, and
[AnimationTriggers.md](AnimationTriggers.md) for the behaviour names behind the animations.

---

## 1. Layers

| Layer | Directory | Role |
| --- | --- | --- |
| Entry point | `xSEPlugin.cpp` | SKSE load: logger, listener registration, Papyrus, trampoline, hooks. Nothing else. |
| Hooks | `Hooks/` | The only place the game engine is patched. Turns engine calls into events. |
| Systems | `Systems/` | Engine-agnostic machinery: event bus, task queue, plugin state, springs/smoothing, raycasts, collider wrappers. |
| Data | `Data/` | Form resolution (`Runtime`), saved state (`Persistent`), session state (`Transient`), the co-save record types. |
| Config | `Config/` | TOML settings, keybind table, per-window UI settings. |
| Managers | `Managers/` | The gameplay layer. One singleton listener per subject. |
| Magic | `Magic/` | Magic effect implementations (spells, potions, poisons, shouts, enchantments). |
| Scale | `Scale/` | The scale primitives every manager reads and writes. |
| Utils | `Utils/` | Shared helpers: actor queries, nodes, units, sounds, actions, looting, item distribution. |
| UI | `UI/` | ImGui framework, windows, settings pages, HUD widgets. |
| Papyrus / API | `Papyrus/`, `API/` | Script bindings, mod events, the native interface for other SKSE plugins, RaceMenu and SmoothCam bridges. |
| Debug | `Debug/` | Profiler, debug draw, projector. |

The dependency direction is one way: `Managers` -> `Systems` / `Utils` / `Data` / `Config` / `Scale`.
Hooks depend on managers only to dispatch into them.

---

## 2. The frame

There is no update loop of the plugin's own. Everything is driven by hooks that dispatch events, and
`EventDispatcher` fans each one out to the managers subscribed to it, **in registration order**.
That order is set once in [EventRegistry.cpp](../src/Systems/Events/EventRegistry.cpp) and is the
backbone of the whole design.

```
Engine call  ->  Hook thunk  ->  EventDispatcher::Dispatch*  ->  listeners, in registration order
```

The five update events, and what each is for:

| Event | Source | Use for |
| --- | --- | --- |
| `OnMainUpdate` | `Main::Update` | Most per-frame work. Live gameplay only. |
| `OnActorUpdate` | same, per 3D-loaded actor | Per-actor work. Runs for every loaded actor, every frame. |
| `OnHavokUpdate` | Havok hit-job processing | Anything that must see or write physics state. |
| `OnPostSMPUpdate` | after HDT-SMP wrote world transforms | The only point where those transforms are valid. |
| `OnCameraUpdate` | `TESCameraState` | Camera math. |

Within a frame the registration groups run in this order, and each group assumes the previous one
has finished:

1. **API bridges** (`Racemenu`, `SmoothCam`) - hand off to other plugins first.
2. **Core** (`HookManager`, `State`, `InitUtils`, `Runtime`, `Config`, `Keybinds`,
   `ConfigModHandler`, `Persistent`, `Transient`) - everything else reads these.
3. **Utilities** (`TaskManager`, `SpringHolder`, `CooldownManager`, `InputManager`,
   `ConsoleManager`, `ItemDistributor`) - advance timers and sample input before anything reacts.
4. **Managers** (perks, magic, hits, crush, deaths, footstep feedback, furniture) - decisions.
5. **Actions and animation** (`AnimationManager`, the controllers, then `AIManager`, `AttackManager`).
6. **Apply layer** (`DynamicScale`, `SizeManager`, `GTSManager`, `AttributeManager`,
   `HighHeelManager`, `MorphManager`, `Headtracking`) - writes the final scale and everything
   derived from it.
7. **Collision** (`ContactManager`, `DynamicCollisionManager`) - works on final data.
8. **Presentation** (`SpectatorManager`, `CameraManager`, `GTSMenu`).

Moving a listener between groups changes behaviour. A manager that reads scale must sit after
`GTSManager`; one that requests a scale change must sit before it.

---

## 3. Core systems

### EventDispatcher (`Systems/Events/`)

The single fan-out point. `AddListener<T>` inspects `T` at compile time and subscribes it only to
the events it overrides, so unused callbacks cost nothing. Sources are the SKSE messaging and
serialization interfaces, the `RE::ScriptEventSourceHolder` sinks, `RE::UI`, and the hooks. The
event list is one X-macro, `GTS_EVENT_LIST`, from which the enum, the subscriber tables and the
override traits are generated.

### TaskManager (`Systems/Misc/Tasks.hpp`)

Deferred and repeating work, each task bound to one of the four update kinds. `RunOnce` for next
tick, `Run` for "until it returns false", `RunFor` for a duration with progress. Named tasks replace
an existing task of the same name, which is how per-actor work stays non-duplicating. Every task
records its `std::source_location` for the profiler.

### State (`Systems/Misc/State.hpp`)

The plugin's global gate: `Enabled`, `InGame`, `Ready`, `Live`, `IsInRaceMenu`, `IsInBlockingMenu`,
`OnMainThread`. Managers check `State::Live()` before doing gameplay work rather than each inventing
its own idea of "the game is running".

### Motion (`Systems/Motion/`)

`Spring` and `Smooth` are the smoothing primitives used everywhere a value must not snap: high heel
offsets, camera transitions, room-height limits, rumble intensity. `SpringHolder` advances every
spring once per frame, before anything reads them.

### Timers, time, rays, colliders

`Timer` and `Time` for cooldown and elapsed checks. `Systems/Rays/` wraps Havok raycasting, with a
dedicated camera collision collector. `Systems/Colliders/` wraps Havok bodies and phantoms
(`ColliderData`, `ActorCollisionData`, `CharacterController`, `Ragdoll`) so collision filters and
ragdoll state can be manipulated without touching `hkp*` types directly in gameplay code.

---

## 4. Data

### Runtime forms (`Data/Runtime*`)

Every form the plugin uses is declared once in `Data/Runtime/L*.hpp` (perks, spells, magic effects,
globals, quests, factions, keywords, races, sounds, explosions, impact data sets, containers,
leveled items) with the `Entry(name, plugin, id)` macro. `Runtime` resolves them all in
`OnSKSEDataLoaded`, logs each failure, and exposes typed getters plus helpers like
`PlaySoundAtNode`, `HasMagicEffect` and `AddPerk`. No `LookupForm` anywhere else.

### Persistent, Transient, and manager-local state

-   `Persistent` - per-actor and global state that survives a save, backed by the co-save record
    types in `Data/Util/`. Kill counts, quest progression counters, crawl toggles, tracked camera
    state, the compressed settings blob.
-   `Transient` - per-actor state for this session, created on demand, never serialized, wiped on
    reset. Cached base height, buff and penalty timers, animation speeds, emotion ramp state.
-   A manager's own FormID-keyed map, for data nothing else needs.

Details and the rules for choosing are in CLAUDE.md.

### Config (`Config/`)

One struct per settings page, serialized to TOML by reflection. `Config::General`, `Gameplay`,
`Balance`, `AutoAim`, `KillMove`, `Audio`, `AI`, `Camera`, `UI`, `Collision`, `Advanced`, `Hidden`,
`Persistent`, plus a non-serialized `Experiments`. Managers read these fields directly.
`OnModConfigRefresh` and `OnModConfigReset` exist for code that must react to a change.

---

## 5. The scale pipeline

This is the heart of the mod. Everything else is either feeding it or reacting to it.

`Scale/` holds the primitives, and the distinction between them matters:

| Function | Meaning |
| --- | --- |
| `get_target_scale` | what the actor is being scaled towards. What gameplay code writes. |
| `get_max_scale` | the actor's own ceiling. |
| `get_visual_scale` | target scale after bonuses, limits and game overrides. What most systems should read. |
| `get_natural_scale` | the actor's inherent size, race and NIF derived, cached. |
| `get_giantess_scale` | visual scale relative to natural scale. |
| `get_3d_scale`, `get_model_scale`, `get_npcnode_scale` | what is actually written to the 3D. |
| `Height.hpp` | the same set expressed in game units instead of multipliers. |

The per-frame flow through the apply layer:

1. **`DynamicScale`** computes a ceiling from the room the actor is in (`GetCeilingHeight`,
   `GetMaxRoomScale`), smoothed by a spring so doorways do not snap the player.
2. **`SizeManager`** (`GTSSizeManager`) holds the per-actor size bonuses and multipliers: aspect of
   giantess, growth spurt, size hunger, hit growth, enchantment bonus, the damage multipliers by
   source, size vulnerability, and the camera bone being tracked. `MaxSizeManager` computes the
   effective maximum from perks, potions, globals and game mode.
3. **`GTSManager`** writes the final scale to the actor and updates animation and movement speed.
   This is the authority. `reapply` / `reapply_actor` exist for after load and 3D reload.
4. **`AttributeManager`** derives attributes from the scale just written: carry weight, health,
   damage, movement speed, and the AV overrides the hooks route through it.
5. **`HighHeelManager`** applies the heel offset, spring-smoothed so enabling and disabling is not a
   jump, and exposes the heel height that damage bonuses read.
6. **`MorphManager`** drives body morphs through RaceMenu/SKEE (breasts, belly), instant or gradual.
7. **`Headtracking`** adjusts where an oversized actor looks.
8. **`ClothManager`** (`RipClothManager`) tears and unequips clothing past a size threshold and
   blocks the game from re-equipping it.
9. **`DynamicCollisionManager`** rescales the character controller so collision follows the body,
   one `DynamicCollisionController` per `bhkCharacterController`.

`GameModeManager` sits above all of this: it applies the selected game mode's growth and shrink
rates per actor, which is what turns a passive size mod into a progression one.

---

## 6. Actions and animation

The largest subsystem, and the one most work touches. `Managers/Animation/` is around seventy files.
This section covers how they hang together.

### 6.1 The round trip

An action is never one function. It is a loop out to Skyrim's behaviour graph and back, and the
plugin only gets control at the points where the animation fires an event:

```
input or AI decision
  -> AnimationManager::StartAnim("StompRight", giant)
    -> triggers["StompRight"] -> behaviour "GtsModStompAnimRight"
      -> Actor::NotifyAnimationGraph(behaviour)
        [ Skyrim plays the animation ]
      -> graph fires "GTSstompstartR", "GTSstompimpactR", "GTSstomplandR", "GTSBEH_Exit", ...
        -> BSAnimationGraphEvent vfunc hook (Hooks/Actor/BSAnimationGraph.cpp)
          -> EventDispatcher::DispatchActorAnimationEvent(actor, tag, payload)
            -> AnimationManager::OnActorAnimationChange
              -> eventCallbacks[tag].callback(AnimationEventData&)
```

The hook is installed into `VTABLE_Character[2]` and `VTABLE_PlayerCharacter[2]`, the actor's
`BSTEventSink<BSAnimationGraphEvent>` slot, with `write_vfunc_unique` and IDs 1 and 2. This is the
canonical example of why `_unique` exists.

Note what this means in practice: **the plugin cannot drive an animation frame by frame.** Damage,
sounds, camera shake and cleanup all happen at the discrete tags the animation author placed in the
behaviour file. If something needs to happen at a moment with no tag, either the behaviour needs a
new one, or the action has to schedule a `TaskManager` task from the nearest tag.

### 6.2 The three registries

`AnimationManager` holds exactly three maps, and everything else is built on them:

| Map | Key | Value | Filled by |
| --- | --- | --- | --- |
| `eventCallbacks` | animation event tag | callback + group name | `RegisterEvent` |
| `triggers` | friendly trigger name | behaviour name(s) + group name | `RegisterTrigger` |
| `data` | actor, then group | live `AnimationEventData` | created on demand |

`RegisterEvent(tag, group, callback)` binds one behaviour graph tag to one function.
`RegisterTrigger(trigger, group, behavior)` maps a friendly name to a behaviour name, so gameplay
code says `StartAnim("StompRight", giant)` and never types a graph name.

Registration happens once, from `AnimationManager::OnSKSEDataLoaded`, which calls
`RegisterEvents()` and `RegisterTriggers()` on every action class in turn. Adding an action means
adding those two calls there; forgetting them is the usual reason a new animation does nothing.

### 6.3 Groups and AnimationEventData

The **group** is the unit of shared state. Every tag belonging to one action registers under the
same group name (`"Stomp"`), and all of them see the same `AnimationEventData` for that actor:

```cpp
struct AnimationEventData {
    Actor& giant;              // the performer
    TESObjectREFR* tiny;       // the target, may be null; a REFR so objects can be picked up too
    std::size_t currentTrigger;
    std::size_t stage;         // 0 means finished
    float animSpeed;
    bool  canEditAnimSpeed;
    bool  disableHH;
    float HHspeed;
};
```

Lifecycle:

-   Created by `StartAnim`, or lazily by `OnActorAnimationChange` if a tag arrives with no data yet
    (which is how an animation started by something other than `StartAnim`, a console `sae` for
    instance, still works).
-   Passed by reference to every callback in the group, so one tag writes what a later tag reads.
-   **Destroyed as soon as a callback returns with `stage == 0`.** That is the "I am done" signal.
    An action that sets `stage = 1` in its start tag must set it back to 0 in its end tag, or the
    data leaks until the actor resets.

`data` is cleared wholesale on `OnPluginReset` and per actor on `OnGameActorReset`.

### 6.4 A worked example

`Stomp_Normal.cpp` is the shape to copy for a simple action. Internals go in an anonymous namespace,
and the only public surface is the two registration functions:

```cpp
void AnimationStomp::RegisterEvents() {
    AnimationManager::RegisterEvent("GTSstompstartR",  "Stomp", GTSstompstartR);
    AnimationManager::RegisterEvent("GTSstompimpactR", "Stomp", GTSstompimpactR);
    AnimationManager::RegisterEvent("GTSstomplandR",   "Stomp", GTSstomplandR);
    AnimationManager::RegisterEvent("GTSStompendR",    "Stomp", GTSStompendR);
    AnimationManager::RegisterEvent("GTSBEH_Exit",     "Stomp", GTSBEH_Exit);

    InputManager::RegisterInputEvent("Stomp", StompEvent, StompCondition);
}

void AnimationStomp::RegisterTriggers() {
    AnimationManager::RegisterTrigger("StompRight", "Stomp", "GtsModStompAnimRight");
    AnimationManager::RegisterTrigger("StompLeft",  "Stomp", "GtsModStompAnimLeft");
}
```

The callbacks read as a timeline: the start tag raises `animSpeed` and sets `canEditAnimSpeed`, the
impact tag does the damage, launch, sound and rumble, the land tag stops the looping rumble, the end
tag resets the speed data, and `GTSBEH_Exit` is the catch-all cleanup, draining stamina and
releasing the camera. Registering a callback for `GTSBEH_Exit` is close to mandatory: it is the tag
that fires when the animation is interrupted rather than completed.

### 6.5 Animation speed and high heels

Speed is multiplicative across every live group on the actor. `GetAnimSpeed` multiplies the saved
per-actor speed from `Persistent` by every group's `animSpeed`; `GetBonusAnimationSpeed` and
`GetHighHeelSpeed` do the same for their fields. `AdjustAnimSpeed(bonus)` applies a player-driven
nudge to every group that opted in with `canEditAnimSpeed`, clamped to 0.33 to 3.0 normally, or 0.50
to 1.75 during breast strangling. `Config::General.bDynamicAnimspeed` switches the whole thing off.

`disableHH` on any live group makes `HHDisabled(actor)` true, which is how an animation suppresses
the high heel offset for its duration without `HighHeelManager` knowing what is playing.

### 6.6 What StartAnim refuses

`StartAnim` returns without doing anything when `AnimationVars::General::IsTransitioning(actor)` is
true, and, for the player, when in first person or in RaceMenu. First person is not supported and is
a deliberate refusal, not an oversight. An unknown trigger name logs
`Requested play of unknown animation named: {}` and returns. Callers do not get a return value, so
an action that must know whether it started has to check those conditions itself first.

### 6.7 Behaviour graph variables

`Utils/Animation/AnimationVars.hpp` wraps every graph variable the mod reads or writes, grouped by
subject (`Stomp`, `Cleavage`, `General`, `Other`, `Utility`), each with a getter and a setter. Use
these rather than raw `GetGraphVariableBool` / `SetGraphVariableFloat` calls: they name the variable
once, in one place, and several are how the mod detects whether the behaviour patches are installed
at all (`BehaviorsInstalled`, `IsNemesisGenerated`, `IsPandoraGenerated`). Blend values such as
`GTS_StompBlend` are what make one behaviour cover a range of stomp distances.

### 6.8 Positioning the target

`Utils/AttachPoint.hpp` is what keeps a grabbed, vored or sandwiched actor glued to the right place
on the giantess: `AttachTo` for a world point or a named bone, `AttachToObjectA/B/R` for the
animation object nodes the animations themselves move, plus purpose-built ones like
`AttachToUnderFoot`, `AttachToHand`, `HugAttach` and `AttachToCleavage`. They handle the ragdoll and
havok side of forcing another actor's position, which is why gameplay code should never set a target
actor's position directly. `TurnTowards` and the raycast helpers in the same folder orient the pair
before an action starts.

### 6.9 Shared action helpers

`Managers/Animation/Utils/AnimationUtils.hpp` is the common vocabulary of the action files, and is
worth reading before writing a new one: `DoLaunch` and `LaunchTask` for knockback,
`DoFootGrind`/`DoFingerGrind` and the `*Check` variants for area damage,
`ApplyThighDamage`/`ApplyFingerDamage`/`DoDamageAtPoint_Cooldown` for targeted damage,
`DrainStamina`, `ManageCamera` for handing a bone to the camera system, `AdjustFacialExpression`
for the emotion layer, `SpawnHurtParticles`, and the hug and vore state fixups
(`AbortHugAnimation`, `Anims_FixAnimationDesync`, `RestoreBreastAttachmentState`).

### 6.10 Plain actions and controllers

Most actions are stateless between tags: everything they need lives in `AnimationEventData` and on
the actors themselves. Actions that must track state across frames get a controller in
`Controllers/`, registered as its own listener: `VoreController`, `GrabAnimationController`,
`HugController`, `ButtCrushController`, `ThighCrushController`, `ThighSandwichController`.

`VoreController` is representative. It owns a `VoreData` per predator holding the set of actors
being eaten as handles, and drives them through grab, swallow, digest and kill, updating their
attachment and node scaling every frame from `OnMainUpdate`. Multi-target actions, anything with a
timer that outlives one animation, and anything that must survive an interruption cleanly belong
here rather than in tag callbacks.

`ThighCrush` is the one action using staged triggers: `RegisterTriggerWithStages("ThighCrush",
"ThighCrush", {"GTSBeh_TriggerSitdown", "GTSBeh_StartThighCrush", "GTSBeh_LeaveSitdown"})`, with the
callbacks setting `data.currentTrigger` to pick the next behaviour.

### 6.11 Cooldowns

`Managers/Animation/Utils/CooldownManager.hpp` gates repeat use. `CooldownSource` is one flat enum
covering everything that needs rate limiting: actions (butt crush, hugs, the breast actions, health
gate), damage kinds (launch, hand, thigh), emotes (laugh, moan, voice), footsteps, and the tiny
calamity mechanics. Each source has a `CooldownConfig` that is either a constant or a function of
the actor, so cooldowns can scale with size or perks (`Calculate_ButtCrushTimer`,
`Calculate_HugCrushCooldown`, `Calculate_BreastActionCooldown`). The API is three calls:
`ApplyActionCooldown`, `IsActionOnCooldown`, `GetRemainingCooldown`. Base durations are `constexpr`
in `Constants.hpp`.

### 6.12 Input (`Managers/Input/`, `Utils/Actions/`)

`InputManager` filters raw input, resolves it against the keybind table, and dispatches by name:

```cpp
InputManager::RegisterInputEvent("Stomp", StompEvent, StompCondition);
```

The condition is a plain predicate, and lives with the others in `InputConditions.hpp`; the callback
receives a `ManagedInputEvent` carrying the hold duration, whether it is an on-up event, and the key
set, so hold-to-charge and tap-versus-hold actions are expressible. Keys come from `Config::Keybinds`
via the `Keybinds` table, which the settings UI edits. Registration happens inside the action's own
`RegisterEvents()`, next to its animation events, so an action is one file end to end.

Player-facing actions usually go through a small dispatcher first: `StompEvent` asks the auto-aim
system which foot and whether the target is under the giantess, then calls `StartAnim` with one of
four trigger names. Auto-aim itself (`Utils/Actions/AutoAim/`) is what picks the target and side.

### 6.13 AI (`Managers/AI/`)

NPCs enter through the same door. `AIManager` runs on `OnMainUpdate`, and on a timer
(`BeginNewActionTimer`, 3 s) calls `TryStartAction`, which consults the per-action deciders:
`VoreAI`, `GrabAI`, `HugAI`, `ButtCrushAI`, `ThighCrushAI`, `ThighSandwichAI`, `StompKickSwipeAI`,
`DevourmentAI`. A decider that accepts calls the same `StartAnim` triggers the player uses, so an
action written for the player is available to NPCs once a decider exists for it. `CombatSteering`
and `Headtracking` handle positioning and gaze; `AIFunctions` holds the shared outcomes
(`KillActor`, `ForceFlee`, `ScareActors`).

`AIManager` is registered after the animation systems and controllers so it decides against
up-to-date state. `AttackManager` is its counterpart, keeping oversized actors from drawing or
swinging a weapon when they should be using size actions.

### 6.14 Third party animations

`Custom_Events_ModSupport.cpp` registers tags meant for animations this repository does not ship:
`GTScrush_caster` and `GTScrush_victim` for any mod that wants to trigger a crush, the
`GTS_CustomDamage_*` on and off pairs that let an external animation enable butt, leg, full body or
cleavage damage windows, foot swipe windows, attachment switches, and Modern Combat Overhaul dodge
compatibility. Treat these tag names as a published contract, like the Papyrus and native APIs.

### 6.15 Things to know before editing

-   `AnimationManager::data`, `HighHeelManager::data` and `CooldownManager::_lastActionTimes` are
    keyed on `Actor*`, not `FormID`, and rely on the reset events to clear entries. This predates
    the rule in CLAUDE.md and is existing debt, not a pattern to copy. New maps key on `FormID`.
-   Most of `AnimationManager` is wrapped in `try` / `catch (const std::out_of_range&)` around `.at`
    lookups, and the catch is usually empty. A silently dropped animation event often means a lookup
    missed, not that the tag never fired.
-   `NextAnim` currently has no callers, and its bounds check reads `behavors.size() < currentTrigger`,
    which is inverted. Staged triggers work through `currentTrigger` being set in callbacks, not
    through this function. Do not build on `NextAnim` without fixing it first.
-   `docs/AnimationTriggers.md` lists the behaviour names for playing these from the console
    (`player.sae GTSBEH_HeavyKickLow_R`), which is the fastest way to test a tag callback without
    the input and AI layers in the way.

---

## 7. Damage, death and reactions

-   **`ContactManager`** runs on the Havok update and owns a `ContactListener`, so body-to-body
    contact can be detected without polling.
-   **`CollisionDamage`** is the size-damage core: foot collision checks against nearby actors,
    scaled damage, crush thresholds, and the `DamageSource` that caused it. Everything that hurts
    someone by size goes through `DoSizeDamage` or `DoFootCollision`.
-   **`SizeHitEffects`**, **`LaunchActor`**, **`LaunchObject`**, **`LaunchPower`** apply the
    secondary effects: knockback, ragdolls, sending objects flying.
-   **`HitManager`** handles Papyrus hit events; `Hooks/Actor/Damage.cpp` handles the engine side.
-   **`CrushManager`** performs the crush itself, with a per-actor state machine and a delay timer.
    **`OverkillManager`** handles gib and overkill on a lethal crush, **`ShrinkToNothingManager`**
    the shrink-to-nothing death, and `TinyCalamity` the instakill and shrink variants.
-   **`Size_Killmoves/`** is the cinematic layer: a staged state machine (move to enemy, rise above,
    look at face or node, death fly-off, return camera) shared by the breast absorb, breast
    suffocate, calamity and wrathful calamity kill moves.
-   **`KillDataUtils`**, **`DeathReport`** and **`Looting`** record what happened, feed the kill feed
    UI, and handle the victim's inventory.

## 8. Footsteps and feedback

A footstep is one event with many consumers, which is why it is dispatched rather than handled in
place:

```
BGSImpactManager hook -> ImpactManager -> EventDispatcher::DispatchImpactEvent -> OnImpact
```

Consumers: **`FootStepManager`** (`Managers/Audio/`) for the sound, chosen by size and surface, with
`PitchShifter`, `Stomps`, `GoreAudio` and `MoansLaughs` alongside it; **`TremorManager`** for screen
shake; **`ExplosionManager`** for dust and debris; **`Rumbling`** for sustained controller and
camera rumble, itself spring-driven with ramp-up and ramp-down states.

`FurnitureManager` handles actors entering and leaving furniture at odd sizes.

## 9. Magic

`Magic/Magic.hpp` defines a small lifecycle base class (`OnStart`, `OnUpdate`, `OnFinish`, polled)
that every effect derives from, with the shared size math in `Magic/Effects/Common.hpp` (`Grow`,
`ShrinkActor`, `Steal`, `TransferSize`, `ShrinkToNothing`, efficiency and power calculations, size
experience). Effects are grouped by delivery: `Spells/`, `Potions/`, `Poisons/`, `Shouts/`,
`Enchantments/`. `MagicManager` registers them and drives the polling.

## 10. Camera and spectating

`CameraManager` runs on `OnCameraUpdate` and composes the final camera transform from the active
state. The states are per view mode and per tracked bone (`Cameras/TP/Normal`, `Alt`, `Foot`,
`FootL`, `FootR`, `Cameras/FP/Normal`), with `Trans` handling transitions between them and
`CamUtil` the shared math. `SpectatorManager` decides whose camera it is, which is what lets the
player watch an NPC giantess instead of themselves. `Systems/Rays/Camera/` keeps the camera out of
geometry.

## 11. Progression

`PerkHandler` reacts to perks being added and removed and to GTS level-ups, and holds the per-perk
bonus logic (cataclysmic stacks, kick speed, and so on). `SkillUtils` and the quest counters in
`Persistent` drive progression. `ItemDistributor` places mod items into containers.
`Perks/ShrinkingGaze` is a standalone perk mechanic.

## 12. UI

ImGui compiled into the DLL, presented through a Scaleform `IMenu`. `UI/Core/` is the framework
(window manager, window base classes, context and style management, fonts, graphics, input,
utilities). `UI/Windows/` is one class per window: the settings window with a category per settings
page, HUD widgets (size bars, status bar, kill feed), the debug window and overlay, and the splash.
`UI/Controls/` holds shared widgets, including the dynamic buff and cooldown icons.

The frame is built in `GTSMenu::AdvanceMovie` and presented from either `PostDisplay` or the D3D11
`Present` hook depending on the active context. See the UI section of CLAUDE.md.

## 13. External surfaces

### Papyrus (`src/Papyrus/`, `distribution/PapyrusSource/`)

Traffic runs both ways, by two different mechanisms.

**Script calls the DLL.** Each C++ file binds its functions to one Papyrus script name, held in a
`PapyrusClass` constant, and `register_papyrus` in [Papyrus.cpp](../src/Papyrus/Papyrus.cpp) calls
every `register_papyrus_*` in turn from `SKSEPluginLoad`. The script side is a `hidden` script of
`global native` declarations with no body. **The C++ file names do not match the script names**, so
go by `PapyrusClass`:

| Script source | `PapyrusClass` | C++ |
| --- | --- | --- |
| `GTSScale.psc` | `GTSScale` | [Scale.cpp](../src/Papyrus/Scale.cpp) |
| `GTSHeight.psc` | `GTSHeight` | [Height.cpp](../src/Papyrus/Height.cpp) |
| `GTSPlugin.psc` | `GTSPlugin` | [Plugin.cpp](../src/Papyrus/Plugin.cpp) |
| `GTSControl.psc` | `GTSControl` | [TotalControl.cpp](../src/Papyrus/TotalControl.cpp) |
| `GTSEvent.psc` | `GTSEvent` | [ModEvents.cpp](../src/Papyrus/ModEvents.cpp) |

A native function exists in three places at once: the `global native` line in the `.psc`, the
`RegisterFunction` call, and the C++ implementation. All three have to agree on name, parameters and
return type, and a mismatch is a runtime failure in the VM, not a compile error.

**DLL calls script.** For work that is easier or only possible in Papyrus, the plugin calls into a
proxy quest: `CallVMFunctionOn(quest, "GTSProxy", "Proxy_*")`, with the quest resolved through
`Runtime::QUST.GTSQuestProxy`. `GTSProxy.psc` holds those entry points, all named `Proxy_*`, and its
job is to reach things the DLL cannot: sending Devourment's `ModEvent`s, satisfying the vampire feed
quest, teaching and unlocking the Tiny Calamity shout. The wrappers are in
[ProxyFunctions.cpp](../src/Papyrus/ProxyFunctions.cpp). Some carry a "Ported From Papyrus" comment;
the direction of travel is script logic moving into C++, not the other way.

**The rest of the scripts are game-side.** `GTSProgressionQuest`, `QF_GTSProgressionQuest`,
`GTSProxyPlayerAlias`, `GTSPileCleaner`, `GTSStartProgressionQuest` and
`GTSUtilTalkToActorFragment` extend `Quest`, `ObjectReference`, `ReferenceAlias` or `Perk` and are
attached to forms in the plugin's ESP. The DLL does not bind them; it reaches them through the forms
in `Data/Runtime/`.

Note that this repository ships `.psc` sources only. No `.pex` is built here, and nothing in CMake
compiles Papyrus. Editing a script means recompiling it and shipping the result with the mod's
archive, outside this repo.

`GTSEvent`'s `RegisterOnFootstep` / `UnRegisterOnFootstep` are for other mods: a form registers and
then receives the plugin's footstep mod events. Treat those, and every function in the tables above,
as a published contract.

-   **Native API** (`API/GTSPluginInterface.hpp`): the versioned interface other SKSE plugins query.
-   **Bridges** (`API/Racemenu.cpp`, `API/SmoothCam.cpp`): morph application through SKEE, and
    handing camera control to and from SmoothCam. Both degrade to no-ops when the other mod is
    absent.
-   **Console** (`Managers/Console/`): the `gts` command set, with argument parsing, actor
    resolution and per-command usage text.

## 14. Debug tooling

`Debug/Profilers.hpp` provides `GTS_PROFILE_ENTRYPOINT`, compiled out unless `GTS_PROFILER_ENABLED`
is defined in `PCH.hpp`. Dispatch sites and tasks carry profiler slots, so the debug menu can show
where frame time goes per listener and per task. `DebugDraw` and `DebugProjector` draw shapes and
overlays in world space for collision and positioning work.

---

## 15. Where to add what

| Adding | Goes in |
| --- | --- |
| A new size action with animation | `Managers/Animation/<Action>`, triggers registered in `AnimationManager`, input in `Utils/Actions/InputFunctions.cpp` |
| A new spell, potion or enchantment | `Magic/Effects/<Kind>/`, registered with `MagicManager` |
| A new NPC behaviour | `Managers/AI/<Action>/`, called from `AIManager` |
| A new engine interception | `Hooks/<Subject>/`, installed from `HookManager::InstallNormal` |
| A new per-frame system | A manager, registered in the right group in `EventRegistry.cpp` |
| A new setting | A field in the matching `Config/Settings/Settings*.hpp` struct, a control in the matching `UI/Windows/Settings/Categories/` page |
| A new saved value | A record on `Persistent`, cleared on reset |
| A new session-only per-actor value | `TransientActorData`, or the manager's own map |
| A new form reference | An `Entry` in the matching `Data/Runtime/L*.hpp` |
