#include "Systems/Events/EventRegistry.hpp"

#include "API/Racemenu.hpp"
#include "API/SmoothCam.hpp"
#include "Config/ConfigModHandler.hpp"
#include "Config/Keybinds.hpp"
#include "Hooks/Hooks.hpp"
#include "Magic/Magic.hpp"
#include "Managers/AI/AIManager.hpp"
#include "Managers/AI/headtracking.hpp"
#include "Managers/Animation/AnimationManager.hpp"
#include "Managers/Animation/BoobCrush.hpp"
#include "Managers/Animation/Controllers/ThighSandwichController.hpp"
#include "Managers/Animation/Controllers/VoreController.hpp"
#include "Managers/Animation/Grab.hpp"
#include "Managers/Animation/HugShrink.hpp"
#include "Managers/Animation/Utils/CooldownManager.hpp"
#include "Managers/AttributeManager.hpp"
#include "Managers/Audio/Footstep.hpp"
#include "Managers/CameraManager.hpp"
#include "Managers/Collision/DynamicCollisionManager.hpp"
#include "Managers/Console/ConsoleManager.hpp"
#include "Managers/Contact/ContactManager.hpp"
#include "Managers/CrushManager.hpp"
#include "Managers/Damage/CollisionDamage.hpp"
#include "Managers/ExplosionManager.hpp"
#include "Managers/FurnitureManager.hpp"
#include "Managers/Gamemode/GameModeManager.hpp"
#include "Managers/GTSManager.hpp"
#include "Managers/GTSSizeManager.hpp"
#include "Managers/Highheel.hpp"
#include "Managers/HitManager.hpp"
#include "Managers/Input/InputManager.hpp"
#include "Managers/Morphs/MorphManager.hpp"
#include "Managers/OverkillManager.hpp"
#include "Managers/Perks/PerkHandler.hpp"
#include "Managers/RandomGrowth.hpp"
#include "Managers/Rumble.hpp"
#include "Managers/ShrinkToNothingManager.hpp"
#include "Managers/SpectatorManager.hpp"
#include "Managers/Tremor.hpp"
#include "Scale/DynamicScale.hpp"
#include "UI/GTSMenu.hpp"
#include "Utils/ItemDistributor.hpp"
#include "Utils/Plugin/InitUtils.hpp"

namespace GTS {

	// Registration order is execution order
	// Internal systems should run early, systems that modify the game late. 
	// Decisions and requests resolve first then scale, attributes, model, collision data is written.
	void RegisterEventListeners() {

		// API Bridges
		EventDispatcher::AddListener<Racemenu>();          // RaceMenu morph interface
		EventDispatcher::AddListener<SmoothCam>();         // SmoothCam camera control handoff

		
		// Core Components
		EventDispatcher::AddListener<Hooks::HookManager>(); // Handles Hook Installation
		EventDispatcher::AddListener<State>();              // Handles state management for the whole plugin
		EventDispatcher::AddListener<InitUtils>();          // Version check and plugin info logging
		EventDispatcher::AddListener<Runtime>();            // Handles resolving and managing of game forms.
		EventDispatcher::AddListener<Config>();             // Global settings manager
		EventDispatcher::AddListener<Keybinds>();           // Keybind table; reads Config
		EventDispatcher::AddListener<ConfigModHandler>();   // Propagates config reset/refresh
		EventDispatcher::AddListener<Persistent>();         // Save-backed data storage
		EventDispatcher::AddListener<Transient>();          // Per-actor runtime data that is never saved

		
		// Utility Components
		EventDispatcher::AddListener<TaskManager>();       // Runs queued work on main/havok/camera/SMP
		EventDispatcher::AddListener<SpringHolder>();      // Advances springs others read this frame
		EventDispatcher::AddListener<CooldownManager>();   // Per-action cooldown timers
		EventDispatcher::AddListener<InputManager>();      // Samples input before anything reacts to it
		EventDispatcher::AddListener<ConsoleManager>();    // "gts" console commands
		EventDispatcher::AddListener<ItemDistributor>();   // distributes items to chests

		// Managers
		EventDispatcher::AddListener<PerkHandler>();            // Perk updates
		EventDispatcher::AddListener<MagicManager>();           // Spells and size changes in general
		EventDispatcher::AddListener<HitManager>();             // Papyrus hit events
		EventDispatcher::AddListener<CrushManager>();           // Crushing, after damage sources
		EventDispatcher::AddListener<OverkillManager>();        // Gib/overkill on lethal crush
		EventDispatcher::AddListener<ShrinkToNothingManager>(); // Shrink-to-nothing deaths
		EventDispatcher::AddListener<FootStepManager>();        // Footstep sounds
		EventDispatcher::AddListener<TremorManager>();          // Screen tremors on footsteps
		EventDispatcher::AddListener<ExplosionManager>();       // Dust clouds on footsteps
		EventDispatcher::AddListener<Rumbling>();               // Sustained controller/camera rumble
		EventDispatcher::AddListener<FurnitureManager>();       // Furniture enter/exit handling

		// Action / Animation Systems
		EventDispatcher::AddListener<AnimationManager>();        // Owns the anim events controllers respond to
		EventDispatcher::AddListener<RandomGrowth>();            // Random growth perk
		EventDispatcher::AddListener<VoreController>();          // Vore
		EventDispatcher::AddListener<Grab>();                    // Grabbing
		EventDispatcher::AddListener<ThighSandwichController>(); // Thigh sandwiching
		EventDispatcher::AddListener<AnimationBoobCrush>();      // Breast crush
		EventDispatcher::AddListener<HugShrink>();               // Hug shrinking
		EventDispatcher::AddListener<AIManager>();               // Picks GTS actions once everything above is current


		// Game-Facing Apply Layer
		EventDispatcher::AddListener<DynamicScale>();      // Dynamic caps on max scale
		EventDispatcher::AddListener<SizeManager>();       // Max scale the authority clamps against
		EventDispatcher::AddListener<GTSManager>();        // Writes final scale, anim and movement speed
		EventDispatcher::AddListener<AttributeManager>();  // Attributes derived from the scale just written
		EventDispatcher::AddListener<HighHeelManager>();   // Applies high heel offset
		EventDispatcher::AddListener<MorphManager>();      // Body morphs
		EventDispatcher::AddListener<Headtracking>();      // Modifies actor headtracking

		// Collision is last, should work on final data.
		EventDispatcher::AddListener<ContactManager>();          // Collision detection
		EventDispatcher::AddListener<DynamicCollisionManager>(); // Scales character controller collision

		
		// Presentation / UI
		EventDispatcher::AddListener<SpectatorManager>();  // Camera targets
		EventDispatcher::AddListener<CameraManager>();     // Edits the camera
		EventDispatcher::AddListener<GTSMenu>();           // Mod settings menu

		logger::info("Managers Registered");
		EventDispatcher::LogSubscriptions();
	}
}
