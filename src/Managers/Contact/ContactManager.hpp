#pragma once
#include "Managers/Contact/ContactListener.hpp"

namespace GTS {

	class ContactManager : public EventListener, public CInitSingleton <ContactManager> {
		public:
		virtual void OnHavokUpdate() override;
		void UpdateCameraContacts();

		private:
		ContactListener listener{};
	};
}
