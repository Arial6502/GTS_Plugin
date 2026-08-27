# CLAUDE.md

Guidance for Claude Code (claude.ai/code) when working in this repository.

`GtsPlugin` is the SKSE64 plugin DLL behind the *Size Matters NG* Skyrim SE/AE mod. One DLL, one
hook layer, one event bus, one ImGui UI, and roughly fifty *managers* that hang off the event bus.

## What this project is

A released mod with users, not a scratch project. See
[docs/Architecture.md](docs/Architecture.md) for what the systems are and how they fit together.

**This is an adult mod, and most of the gameplay layer is explicit.** Size fetish content is the
subject matter, not an incidental corner of it: vore and its variants, breast crush, suffocation and
absorption, butt crush, thigh crush and thigh sandwich, hugs and hug crushing, stomping and
crushing, gore and gib effects, body morphs, clothes tearing, moans and other reactive audio, and a
camera system built to frame all of it. File and function names say exactly what they do
(`BoobCrush`, `CleavageStrangle`, `Grab_Sneak_Vore`, `SizeKillMove_BreastAbsorb`, `MoansLaughs`).

Treat all of it as ordinary code. Implement, refactor, debug and review it on the same technical
terms as the engine-side work, name things as plainly as the existing code does, and do not soften a
UI string, a log message or an identifier to avoid the subject. There is no need to comment on the
content or to check whether a request in this repository is appropriate; it is the point of the mod.

-   `src/Managers/`, `src/Magic/`, `src/Utils/Actions/` are the gameplay layer.
-   `src/Systems/`, `src/Hooks/`, `src/Data/`, `src/Config/`, `src/UI/Core/` are shared
    infrastructure. A change there affects every manager.
-   Do not remove or disable an existing manager, hook or setting because it looks unused or
    unfinished. Ask first.

## Building

**Find out what the developer is building before you build anything.** Every preset has its own
binary directory and its own vcpkg tree, so picking the wrong one is not a neutral act: it triggers
a full dependency build, including CommonLibSSE, and leaves a second multi-gigabyte tree behind.

The active target is Visual Studio's, in `.vs/ProjectSettings.json`:

```json
{ "CurrentProjectSetting": "Build AE/SE (Debug)" }
```

That string is a build preset's `displayName`. Map it back and use that preset:

| `CurrentProjectSetting` | Build preset | Configure preset | Binary dir |
| --- | --- | --- | --- |
| Build AE/SE (Debug) | `build-debug` | `config-debug` | `build/Debug` |
| Build AE/SE (Release) | `build-release` | `config-release` | `build/Release` |
| Build AE/SE (Release AVX) | `build-release-avx` | `config-release-avx` | `build/Release-AVX` |
| Build AE/SE (Release AVX2) | `build-release-avx2` | `config-release-avx2` | `build/Release-AVX2` |
| Build AE/SE (Release AVX512) | `build-release-avx512` | `config-release-avx512` | `build/Release-AVX512` |
| Build AE/SE (Debug Profiler) | `build-debug-profiler` | `config-debug-profiler` | `build/Debug-Profiler` |
| Build AE/SE (Release Profiler) | `build-release-profiler` | `config-release-profiler` | `build/Release-Profiler` |
| Build AE/SE (Release Plugin Disabled) | `build-release-disabled` | `config-release-disabled` | `build/Release-Disabled` |

Then:

```bash
cmake --build --preset build-debug
```

Configure only when the binary dir has no `CMakeCache.txt`, or when CMake asks for it:

```bash
cmake --preset config-debug
```

Rules:

-   If `.vs/ProjectSettings.json` is missing or names something not in the table, ask. Do not guess,
    and do not fall back to release.
-   Do not switch the developer's active target, and do not configure a preset they have not used.
    `ls build/` shows which ones actually exist; those are cheap, the rest are not.
-   Debug and the release presets differ by more than optimisation. The release presets set
    `/arch:` (SSE4.2, AVX, AVX2, AVX512), `/GL` + `/LTCG` and `/fp:fast`. A bug that only appears in
    one of them is plausible; say which preset a result came from.
-   The last three presets exist to carry the project-wide macros declared in `src/PCH.hpp`. They
    come from CMake options of the same name, so **do not uncomment the `#define` in `PCH.hpp` to
    turn one on**; configure the matching preset instead and let it build in its own folder.

    | Macro | Effect | Presets |
    | --- | --- | --- |
    | `GTS_PROFILER_ENABLED` | Compiles in `GTS_PROFILE_ENTRYPOINT` and the per-listener and per-task timing shown in the debug menu. Off, the macros expand to nothing. | debug and release |
    | `GTS_DISABLE_PLUGIN` | Intended to disable the plugin while keeping serialization alive so cosave data survives. **Nothing reads this macro yet**, so the build is currently an ordinary release. | release only |
-   `_CompileDLL.bat` and `_BuildAndPackage.bat` always use `config-release` / `build-release`,
    whatever the active target is. Use them only when release is what is wanted.
-   `out/build/x64-debug` is Visual Studio's non-preset CMake directory. The presets do not use it.

**A build writes outside the build tree.** Post-build steps copy the DLL and PDB into
`distribution/Package-<BuildFolder>/`, and, when `GTSPLUGIN_COPY_DIR` is set in the environment,
into that directory as well, which is a live game install. Building is therefore a deploy. Say so
when you have done it, and do not build to "just check it compiles" while the game is running.

## Toolchain

-   C++23 (`CMAKE_CXX_STANDARD 23`, required), MSVC only (>= 14.39 enforced in CMake), Windows x64.
-   CMake + Ninja, presets in `CMakePresets.json`, vcpkg manifest (`vcpkg.json`) with overlay ports
    in `cmake/ports/`, plus `FetchContent` for `reflect`, `glaze` and `cmake-git-version-tracking`.
-   Sources are globbed (`cmake/AddCXXFiles.cmake`, `CONFIGURE_DEPENDS`), so new `.cpp`/`.hpp` under
    `src/` need no CMake edit.
-   ImGui is built into this DLL from `lib/imgui`. There is no SKSEMenuFramework dependency.
-   `src/PCH.hpp` is the precompiled header and is force-included: the whole STL, `RE/`, `REL/`,
    `SKSE/`, Win32, D3D11, spdlog (`logger::`), ImGui, oneTBB, Abseil containers, glm, toml11,
    magic_enum, reflect, glaze, RE2, lz4, lunasvg, Detours, plus the project's own core headers
    (`Singleton`, `EventListener`, `EventDispatcher`, `State`, `Tasks`, `Timer`, `Spring`,
    `Runtime`, `Persistent`, `Transient`, the actor/scale utils, `Constants*`).
    **Do not re-include anything PCH.hpp already provides.**
-   Prefer `absl::flat_hash_map` / `absl::btree_map` / `absl::InlinedVector` over `std::unordered_*`
    for new containers; use `tbb::concurrent_*` only where cross-thread access is real.
-   `namespace GTS` is the project namespace and does `using namespace RE`, so `Actor`, `FormID` and
    `NiAVObject` are unqualified inside it. `namespace Hooks` pulls in both.

## CommonLibSSE-NG

The reverse-engineered interface to the game engine. This project uses a **fork**, not upstream:
`BingusEx/CommonLibSSE-NG-GTS`, pinned by REF in
[portfile.cmake](cmake/ports/CommonlibSSE-NG-GTS/portfile.cmake) and pulled in with
`find_package(CommonLibSSE CONFIG REQUIRED)`.

Headers land in the build tree:

```
build/<Preset>/vcpkg_installed/x64-windows-static-md/include/{RE,REL,SKSE}
```

Read that tree before writing engine-facing code, to confirm class layouts, vtable indices, member
offsets and helper signatures. Do not guess an RE type's members.

The installed headers are not the full fork: no `.cpp` sources, no git history. When the answer
needs those, **ask where the fork is checked out. Never assume a path.**

**Namespaces**: `RE::` game classes, `REL::` relocation/version handling, `SKSE::` plugin interfaces.

The plugin is built version-independent (`SKSE::VersionIndependence::AddressLibrary`), so SE/AE
differences are resolved at *runtime*:

-   `REL::RelocationID(seID, aeID)` / `REL::VariantID(se, ae, vr)`: address resolution.
-   `REL::VariantOffset(se, ae, vr)`: per-runtime offset added to a resolved address.
-   `REL::RelocateMember<T>()` / `RelocateMemberIfNewer<T>()`: members that moved between runtimes.
-   `REL::RelocateVirtual<T>()`, `REL::Module::IsAE()/IsSE()/IsVR()`.

`REL::Relocate(...)` calls `REL::Module::get()`, which is unreliable in a static member initialiser
running at DLL load. `SKSEPluginLoad` calls `REL::Module::reset()` first thing as the workaround;
constant vtable indices that are identical across runtimes should still be plain `constexpr`.

### Settling an RE question

When a layout, offset or relocation ID is in doubt, do not reason from a stubbed
`RELOCATION_ID(x, 0)` or an assumed struct. Ask, or use whatever decompiler and reference sources
the developer points you at.

## Startup order

[xSEPlugin.cpp](src/xSEPlugin.cpp) is the whole entry point and does this, in this order:

```cpp
REL::Module::reset();                                //Clib init bug workaround
Init(a_skse);
logger::Initialize();
GTS::RegisterEventListeners();                       //src/Systems/Events/EventRegistry.cpp
GTS::EventDispatcher::Init(_byteswap_ulong('GTSP'));
InitializePapyrus();
SKSE::GetTrampoline().create(384);                   //raise when adding call hooks
Hooks::HookManager::InstallNormal();
```

Nothing else runs at DLL load. Everything a manager needs to do happens in a listener callback.

**Timing ladder**: `SKSEPluginLoad` (registration and hooks only) -> `OnSKSEDataLoaded` (forms
resolve here, config loads) -> `OnGameMainMenuFullyLoaded` (late hooks, anything that must outlast
other plugins' installs) -> `OnSKSEPostLoadGame` / `OnSerdePostLoad` (per-save state).

## Event dispatching

[EventDispatcher](src/Systems/Events/EventDispatcher.hpp) is the single fan-out point. Managers
consume events by overriding virtuals on [EventListener](src/Systems/Events/EventListener.hpp) and
nothing else.

**Implementation**: the dispatcher keeps one fixed-size subscriber list per event
(`MaxListeners = 256`, atomic slots). `AddListener<T>` detects at compile time which virtuals `T`
overrides and subscribes it only to those, so an unused callback costs nothing at dispatch. It owns
the SKSE messaging registration, the SKSE serialization interface (unique ID `'GTSP'`), the
`RE::ScriptEventSourceHolder` sinks and `RE::UI`'s `MenuOpenCloseEvent`. Hooks feed it the rest
through the static `Dispatch*` entry points.

**Adding an event** means three edits, all in `Systems/Events/`: the virtual on `EventListener`, a
line in the `GTS_EVENT_LIST(X)` X-macro right below it (the enum, the subscriber lists and the
override traits are generated from that list), and a static `Dispatch*` on `EventDispatcher` with a
`.cpp` body. Then call it from the hook or sink that produces it.

**Registration** happens exclusively in [EventRegistry.cpp](src/Systems/Events/EventRegistry.cpp):

```cpp
EventDispatcher::AddListener<GTS::MyManager>();   //takes T::GetSingleton()
```

**Registration order is execution order, and that ordering is load-bearing.** The file is grouped
API bridges, core components, utilities, managers, action/animation, game-facing apply layer,
collision, UI, so that decisions resolve before scale is written, attributes derive from the scale
just written, and collision sees final data. A new listener goes in the group that matches what it
does, not at the end.

**Event families** on `EventListener` (all default to no-op, override only what is needed):

-   Frame: `OnMainUpdate` (end of the game loop, live gameplay only), `OnActorUpdate` (per loaded
    actor, per frame), `OnPapyrusUpdate`, `OnHavokUpdate`, `OnPostSMPUpdate` (after SMP wrote world
    transforms, the only place those are valid), `OnCameraUpdate`.
-   Actor: `OnActorLoad3D`, `OnActor3DUnload` (fires *before* teardown, release cached node pointers
    here), `OnActorPerkAdded`/`OnActorPerkRemoved`, `OnLethalHit`, `OnImpact`,
    `OnActorAnimationChange`, `OnHighHeelEquiped`, `OnGTSLevelUp`.
-   Game sinks: `OnGameActorReset`, `OnGameActorEquip`, `OnGameActorLoaded`, `OnGameDeath`,
    `OnGameHit`, `OnGameMenuChange`, `OnGameFurnitureChange`, `OnGameContainerChanged`,
    `OnGameDragonSoulAbsorb`, `OnGameFormDelete`, `OnGameMainMenuFullyLoaded`.
-   Config: `OnModConfigReset`, `OnModConfigRefresh`.
-   Lifecycle: `OnPluginReset` (load started, or new game; clear per-save state here).
-   SKSE: `OnSKSEPostLoad`, `OnSKSEPostPostLoad`, `OnSKSEInputLoaded`, `OnSKSEDataLoaded`,
    `OnSKSEPreLoadGame`, `OnSKSEPostLoadGame`, `OnSKSENewGame`, `OnSKSESaveGame`, `OnSKSEDeleteGame`.
-   Serialization: `OnSerdePreSave`/`OnSerdeSave`/`OnSerdePostSave`,
    `OnSerdePreLoad`/`OnSerdeLoad`/`OnSerdePostLoad`, `OnSerdeRevert`, `OnSKSEFormDelete`.

## Hooking

Hooks live under [src/Hooks/](src/Hooks) grouped by subject (`Actor/`, `Animation/`, `Camera/`,
`Engine/`, `Havok/`, `Papyrus/`, `Projectile/`, `Sound/`, `UI/`). Each file exposes a
`Hook_<Thing>` struct with a static `Install()`; [Hooks.cpp](src/Hooks/Hooks.cpp) calls them all
from `HookManager::InstallNormal()`. `InstallLate()` runs from `OnGameMainMenuFullyLoaded` and is
for hooks that must be applied after other plugins have installed theirs.

The helpers are in [HookUtil.hpp](src/Hooks/Util/HookUtil.hpp), namespace `Hooks::stl`. A hook is a
struct with a static `thunk` plus a `func` slot declared with one of the boilerplate macros; the
installer templates fill `func` with the original.

| Macro | Use with |
| --- | --- |
| `FUNCTYPE_CALL` | `write_call` / `write_jmp` |
| `FUNCTYPE_CALL_UNIQUE` | `write_call_unique` |
| `FUNCTYPE_VFUNC` | `write_vfunc` |
| `FUNCTYPE_VFUNC_UNIQUE` | `write_vfunc_unique` |
| `FUNCTYPE_DETOUR` | `write_detour` (MS Detours) |

Installers (all log their resolved addresses at `debug`):

```cpp
stl::write_call<T, Size = 5>(uintptr_t | REL::RelocationID | REL::VariantID, REL::VariantOffset = {});
stl::write_call_unique<T, ID, Size = 5>(REL::RelocationID | REL::VariantID, REL::VariantOffset = {});
stl::write_jmp<T, Size = 5>(uintptr_t | REL::RelocationID, REL::VariantOffset = {});
stl::write_vfunc<T>(REL::VariantID);                 // T::funcIndex selects the slot
stl::write_vfunc<F, vtblIndex = 0, T>();             // F::VTABLE[vtblIndex]
stl::write_vfunc_unique<T, ID>(REL::VariantID);
stl::write_detour<T>(REL::RelocationID);
```

Rules and gotchas:

-   **`_unique<ID>`**: one hook struct installed into several vtables needs a distinct `ID` per
    installation, because `func` is a template variable keyed on it. Both `thunk` and `func` must be
    `template<int ID>`.
-   `write_vfunc` requires `static constexpr std::size_t funcIndex` on the hook struct.
-   Secondary vtables (`RE::VTABLE_Character[2]` is the actor's `BSTEventSink<BSAnimationGraphEvent>`)
    do **not** give a usable `this` for the primary type. Take the object from the event payload.
-   Call the original before or after the dispatch deliberately, and say which in one line.
-   `SKSE::GetTrampoline().create(384)` in `xSEPlugin.cpp` sizes the trampoline. Raise it when
    adding call hooks; `InstallNormal()` logs used/total bytes.
-   Anything touching game state from a hook that fires mid-transaction goes through the task queue
    (see [Tasks](#tasks)) or `SKSE::GetTaskInterface()`.
-   Prefer ending a hook in an `EventDispatcher::Dispatch*` call and doing the work in a listener.
    That is the default, and it is what an event with more than one consumer needs. A **new** hook
    whose job is narrow and has exactly one consumer may call that manager's function straight from
    the thunk instead; a dedicated event for a single caller is just indirection. Keep the thunk to
    the call itself, and put the logic in the manager. Do not retrofit this onto existing
    dispatch-based hooks.

## Managers

`src/Managers/<Name>.hpp|.cpp`, or `src/Managers/<Group>/` for anything with more than a couple of
files. Every manager is a singleton listener:

```cpp
namespace GTS {
	class MyManager : public EventListener, public CInitSingleton<MyManager> {

		public:
		void OnActorUpdate(RE::Actor* a_Actor) override;
		void OnPluginReset() override;
	};
}
```

-   `CInitSingleton<T>` gives `T::GetSingleton()`; shared state is `static inline` on the class.
-   Managers must not include each other's headers where a shared util or an event would do. The
    dependency direction is `Managers` -> `Systems`/`Utils`/`Data`/`Config`, one way.
-   **Prefer splitting a manager across several files over one `.hpp`/`.cpp` pair.** One file per
    concern. `Managers/Animation/`, `Managers/Console/` and `Managers/AI/` are the shape to copy.
-   Guard everything against null actors and null 3D. Managers run against every loaded actor every
    frame, including during cell transitions.
-   Per-actor state goes in `Transient`, in `Persistent`, or in the manager's own FormID-keyed
    container. See [Per-actor data](#per-actor-data) and
    [Keys](#keys-never-store-game-object-pointers).

**Checklist for a new manager**

1. `src/Managers/<Name>.hpp` + `.cpp` with the class above.
2. Override only the listener virtuals you need. If the trigger is narrow enough that it needs a new
   hook and nothing else consumes it, calling the manager from that thunk is fine (see [Hooking](#hooking)).
3. Resolve forms through `Runtime`, never a hardcoded `LookupForm`.
4. If it persists anything, declare records on `Persistent` and clear them on reset.
5. `#include` it and add one `EventDispatcher::AddListener<GTS::MyManager>();` in
   [EventRegistry.cpp](src/Systems/Events/EventRegistry.cpp), **in the right ordering group**.

## Forms

Never hardcode `LookupForm`. Forms are declared in `src/Data/Runtime/L*.hpp` as `RuntimeEntry`
members of an `iListable<T>` struct, via the `Entry(name, plugin, id)` macro:

```cpp
struct Perks : iListable<RE::BGSPerk> {
	Entry(GTSPerkHugs, GTSP, 0x60656E); //Gentle Hugs (25)
};
```

[Runtime](src/Data/Runtime.hpp) resolves them all in `OnSKSEDataLoaded` and logs a warning per
failed lookup. Reach them through `Runtime::Get*` (`GetPerk`, `GetSpell`, `GetMagicEffect`,
`GetSound`, `GetGlobal`, `GetKeyword`, ...) or the helper wrappers (`PlaySoundAtNode`,
`HasMagicEffect`, ...). A form can be looked up by tag string or by the entry itself; prefer the
entry, it cannot typo.

## Tasks

[TaskManager](src/Systems/Misc/Tasks.hpp) runs deferred and repeating work on a chosen update
(`UpdateKind::Main`, `Camera`, `Havok`, `PostPhysics`).

```cpp
TaskManager::RunOnce([](const OneshotUpdate&) { ... });              //next tick
TaskManager::Run("name", [](const TaskUpdate& a_u) { return true; }); //until it returns false
TaskManager::RunFor("name", 3.0f, [](const TaskForUpdate& a_u) { return true; });
TaskManager::Cancel("name");
```

Named tasks replace an existing task of the same name, which is how per-actor tasks stay
non-duplicating; name them with the actor's FormID. The trailing `std::source_location` is filled in
by the compiler and is what the profiler attributes the task to. Do not pass it by hand.

## Configuration

[Config](src/Config/Config.hpp) is a singleton listener holding one `static inline` struct per
settings page (`Config::General`, `Gameplay`, `Balance`, `Audio`, `AI`, `Camera`, `UI`, `Collision`,
`KillMove`, `AutoAim`, `Advanced`, `Hidden`, `Persistent`, `Experiments`). Read settings straight
off those; they are plain data.

-   Structs live in `src/Config/Settings/Settings*.hpp` and are serialized to TOML by reflection
    through the `TOML_SERIALIZABLE()` macro. Files sit in `Data/SKSE/Plugins/GTSPlugin/`.
-   Naming convention inside a settings struct: `b` bool, `i` int, `f` float, `s` string, nested
    structs plain. Keep it.
-   Reflection limits: no C arrays, no enums (store a string and convert with `magic_enum`), no
    `std::tuple`, nested structs must themselves be `TOML_SERIALIZABLE()`, and **64 members maximum
    per struct** (a `reflect` limit). Group new settings into a nested struct rather than piling
    them on at the top level.
-   `Config::Experiments` is deliberately not serialized.
-   Renaming a member drops its saved value. Treat member names as part of the file format.
-   Code that must react to a settings change overrides `OnModConfigRefresh` / `OnModConfigReset`.
-   Keybinds are separate: [Keybinds](src/Config/Keybinds.cpp) owns the table, `InputManager`
    dispatches, and actions register with
    `InputManager::RegisterInputEvent("Name", callback, condition)` from
    [InputFunctions.cpp](src/Utils/Actions/InputFunctions.cpp), with the gate in
    [InputConditions.hpp](src/Utils/Actions/InputConditions.hpp).

## Serialization

Co-save data goes through `src/Data/Util/`. There is no direct `SKSE::SerializationInterface` use in
manager code: declare a record, then forward the `OnSerde*` callbacks to it.

### Record types

| Type | Storage | Use for |
| --- | --- | --- |
| `Serialization::BasicRecord<T, 'TAG', ver>` | one fixed-size `T`, memcpy'd | a single value or a POD blob. Rejects the record if `size != sizeof(T)`, so **any change to `T` needs a `ver` bump**. |
| `Serialization::MapRecord<Value, 'TAG', ver>` | `absl::flat_hash_map<RE::FormID, Value>` | per-actor state. |
| `Serialization::VectorRecord<Entry, 'TAG', ver>` | `std::vector<Entry>` | ordered lists. No FormID handling. |
| `Serialization::TLVRecord<T, 'TAG', ver>` | reflection-driven tag/length/value | anything that will keep changing shape. |
| `Serialization::CompressedStringRecord<'TAG', ver>` | lz4-compressed text | large text payloads, e.g. the embedded settings blob. |
| `Serialization::StringRecord` / `StringViewRecord` | length-prefixed text | `StringViewRecord` is save-only by design (`Load` is `= delete`). |
| `Serialization::TLVFile` | the same TLV payloads on disk | presets, import/export. |

### Declaring and wiring one

Records are `static inline` members of [Persistent](src/Data/Persistent.hpp), or of the manager that
owns them. Never globals or locals.

```cpp
static inline Serialization::MapRecord<MyActorData, 'MYAD'> ActorData {};

void MyManager::OnSerdeSave(SKSE::SerializationInterface* a_this) {
	ActorData.Save(a_this);
}

void MyManager::OnSerdeLoad(SKSE::SerializationInterface* a_this, std::uint32_t a_type, std::uint32_t a_version, std::uint32_t a_size) {
	ActorData.Load(a_this, a_type, a_version, a_size);
}
```

-   `OnSerdeLoad` fires once **per record in the co-save**; every record's `Load` self-filters on
    `type != ID` and returns. Hand each call to every record you own. Do not branch by hand.
-   **Clearing on reset is not optional.** `OnPluginReset` / `OnSerdeRevert` fire before a load and
    on returning to the main menu. A manager that does not clear there leaks the previous save's
    state into the next one.
-   `Save` on an empty `MapRecord` writes nothing, which is correct. The loader keeps defaults.
-   `MapRecord::Load` runs every key through form-ID resolution and drops entries whose form is
    gone, so saved FormIDs survive load order changes. Nothing else you store gets that treatment.

### Tags and versioning

-   Tags are four-character literals (`'ACT_'`), byteswapped into the record ID and registered at
    static-init by `detail::IDRegistrar`. A duplicate **hard-fails the plugin at startup**. Pick a
    fresh one.
-   `ver` is the record's format version. A mismatch means the record is skipped and defaults are
    kept. There is no migration path, so bumping `ver` **drops that record's data for every existing
    save**. This mod ships to users; treat a bump as a deliberate, stated decision, not a reflex.
-   `TLVRecord` is how to avoid that: fields are keyed by an FNV-1a hash of the **member name**, so
    adding, removing and reordering members are backward compatible within the same `ver`. Renaming
    a member loses its stored value.

### TLV constraints

-   `T` must be standard-layout or an aggregate. Members may be arithmetic types, enums,
    `std::string`, `std::vector` of those, and nested reflectable aggregates. Anything else fails a
    `static_assert`.
-   Member-name hash collisions inside one struct are a compile error.
-   `reflect` generates bindings up to exactly 64 members. Nest structs rather than growing the top
    level.
-   `std::vector<bool>` is the packed specialisation and does not round-trip. Use
    `std::vector<int32_t>` for flag arrays.

### Keys: never store game object pointers

**`RE::Actor*`, `RE::TESObjectREFR*`, `RE::NiAVObject*` and every other engine pointer are forbidden
as map/set keys, and forbidden as stored state generally.** They are not stable: the object is
destroyed on cell change, 3D reload, save load or actor unload, and the address is reused by an
unrelated object. A map keyed on one silently starts answering for the wrong actor.

-   Key on `RE::FormID`. That is what `MapRecord`, `Persistent` and `Transient` store.
-   To hold an actor across frames or into a task, use `RE::ActorHandle` (`a_Actor->GetHandle()`,
    then `handle.get().get()` at the point of use, null-checked).
-   Cached `NiAVObject*` must be released in `OnActor3DUnload`. That event exists for this.
-   Raw pointers are fine as *function arguments* within a single dispatch, and nowhere else.

## Per-actor data

Three places, and which one a new field belongs in is a real decision.

-   [Persistent](src/Data/Persistent.hpp) - per-actor state that must survive a save.
    `Persistent::GetActorData(actor)` returns a `PersistentActorData*`, backed by the `'ACT_'`
    `MapRecord`. Only put a field here if losing it on reload would be a bug.
-   [Transient](src/Data/Transient.hpp) - per-actor state for this session only.
    `Transient::GetActorData(actor)` returns a `TransientActorData*` and creates the entry on first
    use. It is never serialized, it is wiped on `OnPluginReset`, and an actor's entry is dropped on
    `OnGameActorReset`. This is where derived and in-flight values live: cached base height, buff
    timers, animation speeds, emotion ramp state, per-frame flags.
-   A container owned by the manager itself. Perfectly fine, and often the better answer when the
    data means nothing outside that manager. Key it on `RE::FormID` and clear it in `OnPluginReset`
    plus `OnGameActorReset` or `OnActor3DUnload`, exactly as `Transient` does.

`Transient::GetActorData` **can return null**: it refuses to create an entry for an actor with no
3D loaded or a negative scale. Check it. Do not cache the returned pointer past the current
dispatch either - the map can rehash.

Prefer `Transient` when several managers read the same value, or when the value is per-actor state
rather than bookkeeping. Prefer a local container when it is one manager's private business; adding
a field to `TransientActorData` grows a struct every loaded actor pays for.

## UI

ImGui, compiled into this DLL. [GTSMenu](src/UI/GTSMenu.cpp) is a Scaleform `IMenu`
(`GTSPlugin/OverlayMenu`) that owns the ImGui and DX11 context, input filtering, pause/blur/slow
motion and the layout INI (`Data/SKSE/Plugins/GTSPlugin/GTSPluginImGui.ini`).

**Frame building and presenting are two separate steps on two different paths:**

-   `GTSMenu::AdvanceMovie` builds the frame. It is the menu's update callback, driven by Scaleform
    (`UI_MENU_FLAGS::kRequiresUpdate`), and does `ImGui_ImplDX11_NewFrame` + `NewFrame`, drains the
    input queue, runs `ImWindowManager::Update` (which is where every window's `Draw` runs), then
    sets `m_frameReady`. All UI drawing happens here, never in the `Present` hook.
-   `GTSMenu::Present` (`ImGui::Render` + `RenderDrawData`) runs from **either**
    `GTSMenu::PostDisplay`, the menu's own draw slot, **or** the D3D11 `Present` hook
    ([Hooks/Engine/Present.cpp](src/Hooks/Engine/Present.cpp)), depending on `m_DrawOnPresent`.
-   `m_DrawOnPresent` comes from the active context's `drawOnPresent` flag in
    [ImContextManager](src/UI/Core/ImContextManager.cpp), which also carries `depthPriority` and the
    cursor state. Only `kMainMenu` presents from the hook, because the main menu's message boxes
    hijack input at any depth priority high enough to draw normally. Changing a context's
    `depthPriority` or `drawOnPresent` changes who gets input; read the comments there first.

Structure:

-   `UI/Core/` is the framework: `ImWindowManager`, `ImWindow`/`ImConfigurableWindow`, `ImCategory`,
    `ImStyleManager`, `ImFontManager`, `ImGraphics`, `ImInput`, `ImUtil`.
-   `UI/Windows/` is one class per window (`Settings/`, `Widgets/`, `Overlay/`, `Debug/`, `Other/`).
    Windows are constructed in `ImWindowManager::Init` with
    `AddWindow(std::make_unique<T>(), &wT)`.
-   `UI/Windows/Settings/Categories/` is one file per settings page, deriving `ImCategory` or
    `ImCategorySplit`, registered in `SettingsWindow::Init` with `AddCategory`. Order there is tab
    order.
-   `UI/Controls/` holds shared widgets (`Button`, `CheckBox`, `Slider`, `ComboBox`, `ProgressBar`,
    `ActorInfoCard`, `KillEntry`, the `Icons/` dynamic buff icons).

Practical rules:

-   A window that persists position or size derives `ImConfigurableWindow<T>`, which registers its
    settings holder automatically. Do not write window geometry into `Config` by hand.
-   Draw code runs on the render thread. Call `ImUtil::ValidState()` where the framework expects it,
    null-check every actor and 3D pointer, and never block.
-   Reuse `UI/Controls/` and `ImUtil::Colors` instead of writing fresh `ImVec4` literals or raw
    `ImGui::` widget calls.
-   Every `Begin*`/`Push*` needs its matching `End*`/`Pop*` on every path, including early returns.
    Scope them (see Formatting) so the pairing is visible.

## Papyrus and the modder API

-   Native functions are registered in [Papyrus.cpp](src/Papyrus/Papyrus.cpp), one
    `register_papyrus_*` per file. Script sources live in `distribution/PapyrusSource/`. See
    [docs/Architecture.md](docs/Architecture.md) for which `.psc` pairs with which `.cpp`; the file
    names do not match, the `PapyrusClass` constant is what binds them.
-   A native function lives in three places: the `global native` declaration in the `.psc`, the
    `vm->RegisterFunction` call, and the C++ body. **Change all three together.** A mismatch in
    name, parameter list or return type fails in the VM at runtime, with nothing to catch it at
    compile time.
-   `.psc` sources are shipped, not built. Nothing in CMake compiles Papyrus, so editing a script
    means recompiling the `.pex` outside this repo. Prefer solving something in C++ over adding
    script logic.
-   The DLL calls back into Papyrus only through the proxy quest,
    `CallVMFunctionOn(quest, "GTSProxy", "Proxy_*")`. Add to `GTSProxy.psc` when script is genuinely
    the only way to reach something, such as another mod's `ModEvent`.
-   [GTSPluginInterface](src/API/GTSPluginInterface.hpp) is the versioned native interface other
    SKSE plugins query. Treat it as a published contract: add to it, do not reshape it.
-   `src/API/` also holds the outbound integrations (RaceMenu/SKEE morphs, SmoothCam). Both register
    first in `EventRegistry.cpp` and must degrade to no-ops when the other plugin is absent.

## Code generation rules

These are absolute and apply to every change made in this repository.

-   **Keep code comments to an absolute minimum. This is the rule most often broken. Treat a comment
    as something that must earn its place, and default to writing none.** Comment only what the code
    cannot state itself: a non-obvious engine constraint, an ordering requirement that looks
    arbitrary, a relocation ID's provenance, a workaround for a specific bug. When one is warranted,
    one or two lines is the size, not a paragraph.

    Do not write:

    -   Restatements of what a line or block does. If it needs explaining, rename the thing.
    -   Section banners, `//---- Something ----` dividers, or headings inside a function. The brace
        scopes described under Formatting are what mark a step; that is their whole purpose.
    -   Docblocks on self-evident members, parameters, or functions. `bool bShowIcons` is already
        documented by being called `bShowIcons`.
    -   File-top prose explaining what the file is for.
    -   Narrative about how the code got this way: what it used to do, what was tried first, what
        was removed. Git holds that.
    -   Essays justifying a design. State the constraint in a sentence and stop.

    Existing files contain a great deal of commentary that predates this rule. Do not treat their
    density as the standard to match, and do not add to it when editing them.

-   **Write comments, log messages and UI strings plainly. No AI writing habits.** The test is
    whether it reads like a developer typed it into the file.

    -   No em dashes. Use a plain hyphen with spaces around it, a comma, or a full stop.
    -   No rhetorical constructions: "it is not X, it is Y", "this is the whole point", "which is
        exactly why". Say the thing once.
    -   No paraphrasing the same fact twice in different words.
    -   Plain vocabulary. Avoid "merely", "outright", "silently", "deliberately" unless they carry
        real meaning in that sentence.
    -   Short sentences. If a sentence needs a subclause to survive, split it.
    -   Be concrete. Name the function, the type, the flag or the failure.

    The same applies to `logger::` messages and to any text shown in the UI.

-   **Modern C++23.** `std::string_view`, `std::span`, `std::format`, `constexpr`/`consteval`,
    ranges and views, concepts over SFINAE, structured bindings, `if constexpr`, designated
    initialisers, `[[nodiscard]]`, `enum class`. No raw `new`/`delete`, no C-style casts, no macros
    where a template or `constexpr` will do (the hook boilerplate and `GTS_EVENT_LIST` are the
    deliberate exceptions).
-   **Windows/MSVC only.** No cross-platform scaffolding, no GCC/Clang branches. SEH, Win32 types
    and MSVC intrinsics are fair game.
-   Log through `logger::` (spdlog) at the level that matches: `trace` for flow, `debug` for install
    and resolution details, `warn`/`error` for real problems. Never `printf` or `std::cout`.
-   Never key a container on a game object pointer, and never store one. See
    [Keys](#keys-never-store-game-object-pointers).
-   Anything that runs per-actor per-frame is hot. Do not allocate, build a `std::string`, or look
    up a form there.

### Formatting

Tabs for indentation; match the file you are in, since parts of `src/UI/` and `src/Config/` use four
spaces. `a_` prefix on parameters, `m_` on non-static members, `namespace GTS` for everything except
hooks, which are `namespace Hooks`.

Access specifiers sit at the same indent level as the members below them, not outdented:

```cpp
class MyManager : public EventListener, public CInitSingleton<MyManager> {

	public:
	void OnActorUpdate(RE::Actor* a_Actor) override;

	private:
	static inline std::mutex m_Lock;
};
```

**K&R braces, with Allman applied to the places that normally get no braces at all.** Opening brace
on the same line for functions, types, `if`/`else`, loops and lambdas:

```cpp
void MyManager::OnActorUpdate(RE::Actor* a_Actor) {
	if (!a_Actor) {
		return;
	}
	...
}
```

Every `switch` case gets its own Allman-braced block, including single-statement cases:

```cpp
switch (a_Type) {
	case Type::Growth:
	{
		DoThing();
		break;
	}
	default:
	{
		break;
	}
}
```

**Wrap self-contained segments in their own brace scope.** Any run of statements that forms one
logical step gets `{ }` around it, which keeps its locals from leaking and makes the step visible
without a comment. This is mandatory in UI code, where the scope also carries the `Push`/`Pop` and
`Begin`/`End` pairing:

```cpp
void MyWindow::Draw() {

	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImUtil::Colors::Good);
		ImGui::TextUnformatted("Active");
		ImGui::PopStyleColor();
	}

	if (ImGui::BeginTable("actors", 3)) {
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("Scale");
			ImGui::TableSetupColumn("State");
			ImGui::TableHeadersRow();
		}

		for (auto* actor : FindActors()) {
			...
		}

		ImGui::EndTable();
	}
}
```

The same applies outside the UI. The init blocks in `xSEPlugin.cpp` and the deferred-task blocks in
the managers are the existing pattern.
