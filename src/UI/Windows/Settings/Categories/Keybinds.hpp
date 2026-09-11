#pragma once

#include "UI/Core/ImCategory.hpp"
#include "Config/Keybinds.hpp"

namespace GTS {

    class CategoryKeybinds final : public ImCategory {
        public:
        CategoryKeybinds();

        void Draw() override;

        private:
        struct Group {
            BindGroup Id = BindGroup::kOther;
            std::vector<InputBind*> Starts;
            std::vector<InputBind*> During;

            [[nodiscard]] std::size_t Count() const { return Starts.size() + During.size(); }
        };

        void RefreshModifierKeys();
        void RebuildGroups();
        void DrawOptions();
        void DrawGroupList(float a_width);
        void DrawBinds();
        void DrawBindList(std::string_view a_heading, const std::vector<InputBind*>& a_binds);
        static void SetWindowBusy(bool a_busy);
        bool DrawInputEvent(InputBind& Event, const std::string& a_name, const char* a_description);

        [[nodiscard]] const InputDef* NodeDefFor(const std::string& a_name) const;

        // Keys of the target switch modifier, refreshed with the groups. A bind covering all of
        // them keeps its plain meaning and cannot be turned on the player, which is worth saying.
        std::vector<std::string> ModifierKeys;
        [[nodiscard]] bool ShadowsModifier(const InputBind& a_Bind) const;

        std::string SearchRes;
        std::vector<Group> Groups;
        BindGroup Selected = BindGroup::kCrush;

        int RebindIndex = 0;
        int CurEventIndex = UINT16_MAX;
        float KeyColumnWidth = 0.0f;
        bool m_groupsBuilt = false;
    };
}
