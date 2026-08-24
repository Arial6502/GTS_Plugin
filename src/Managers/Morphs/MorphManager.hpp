#pragma once

namespace GTS {

	class MorphManager : public EventListener, public CInitSingleton<MorphManager> {

		public:

		enum Category : uint8_t {
			kBreasts,
			kBelly,
			kAll,
		};

		enum Action : uint8_t {
			kSet,
			kModify,
		};

		enum UpdateKind : uint8_t {
			kInstant,
			kGradual,
		};

		static void AlterMorph(Actor* a_actor, Category a_type, Action a_action, float a_value, UpdateKind a_kind = kInstant, float a_transitionTime = 1.0f);
		static void ResetMorphs(Actor* a_actor);
		static void HandleCategoryDataChange(Category a_type);

		private:
		static void UpdateMorphsImediate(Actor* a_actor, Category a_type);
		static void SetMorph(Actor* a_actor, Category a_type, float a_scale);
		static const char* GetMorphKey(Category a_type);

		virtual void OnGameActorReset(RE::Actor* a_actor) override;
		virtual void OnActorLoad3D(RE::Actor* a_actor) override;
		virtual void OnActorUpdate(RE::Actor* a_actor) override;
	};
}