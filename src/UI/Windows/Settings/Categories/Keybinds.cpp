#include "UI/Windows/Settings/Categories/Keybinds.hpp"
#include "UI/Windows/Settings/SettingsWindow.hpp"

#include "UI/Controls/Button.hpp"
#include "UI/Controls/CheckBox.hpp"
#include "UI/Controls/ComboBox.hpp"
#include "UI/Controls/Misc.hpp"
#include "UI/Controls/ToolTip.hpp"

#include "UI/Core/ImUtil.hpp"
#include "UI/Core/ImInput.hpp"

#include "Config/Keybinds.hpp"
#include "Config/Util/KeybindHandler.hpp"

#include "Config/Config.hpp"

#include "UI/GTSMenu.hpp"
#include "UI/Controls/Text.hpp"

namespace {

    PSString T0 = "Disable this input event.\n"
        "Disabled events are completely ignored by the game and will never trigger.";

    PSString T1 = "When an action is marked as exclusive it means it will only activate if its exact key combination is being pressed.\n"
        "(eg. If an action requires ALT+E to activate and you're also holding W while trying to trigger it with this flag set, nothing will happen unless you stop pressing W.)";

    PSString T2 = "The action trigger type modifies the activation behavior for an action.\n\n"
        "- Once: Trigger an action once upon pressing its key combo.\n"
        "- Release: The action will only trigger when you release its keys after pressing them.\n"
        "- Continuous: As long as you are holding down the key combination the action event will be fired every game frame.";

    PSString T7 = "This keybind already uses the Target Self keys, so holding them changes nothing here.\n"
        "The action still works normally, but it cannot be turned on your own character.\n\n"
        "Rebind either this action or Target Self if you want both.";

    PSString T3 = "Normaly when you press a key combo. whatever keys you are holding down are sent to the mod and the game at the same time\n"
        "Depending on what keys you press this may have undesired effects. Which is why this option exists.\n\n"
        "- Automatic: Prevent the game from reading the pressed action keys only when said GTS action would be valid. (eg. When you have the relevant perk/the action is possible to do).\n"
        "  (NOTE: Some actions are not compatible with this setting. These are by default set to \"Never\" On purpose.)\n"
        "- Never: Never prevent the game from reading the pressed keys for this action even if the action would be valid.\n"
        "- Always: Will always prevent the game from reading this key combination regardless if the action would trigger/do something or not.";

    PSString T4 = "This adds a time delay before an action gets triggerd if its keys are pressed.\n"
        "(eg. if the trigger type is once and this value is set to 1.0 you'd need to hold the correct key combination for atleast 1 second before this event's action will fire.)";

    PSString T5 = "Change the key combination to trigger this event.\n"
        "You don't have to hold down the keys if creating a key combination. Pressing a key once will append it to the list\n"
        "After entering the new key combination press this button again to save it.\n"
        "Pressing ESC will cancel the keybind reassignment.";

    PSString T6 = "Click to open advanced settings for this keybind.";


    PSString TH0 = "Filter based on an actions' name.";
    PSString TH1 = "Reset all keybinds and their settings to default.";

    PSString TH2 = "Name each key the way your current keyboard layout does.\n"
        "Off shows the US names a keybind is stored under, which is what guides and screenshots use.\n"
        "This only changes how keys are displayed. What a keybind is actually bound to never changes.";

}

namespace {

    // The three bands the list is split into. Attacks first, then what supports them, then the parts
    // of the mod that are not gameplay.
    struct Band {
        const char* Label;
        GTS::BindGroup First;
        GTS::BindGroup Last;
    };

    constexpr std::array<Band, 3> Bands{{
        { "Actions",    GTS::BindGroup::kCrush,    GTS::BindGroup::kVore },
        { "Support",    GTS::BindGroup::kMovement, GTS::BindGroup::kAbilities },
        { "Misc",       GTS::BindGroup::kInterface, GTS::BindGroup::kOther },
    }};
}

namespace GTS {

    CategoryKeybinds::CategoryKeybinds() {
        m_name = "Keybinds";
    }

    const InputDef* CategoryKeybinds::NodeDefFor(const std::string& a_name) const {
        return KeybindHandler::Find(a_name);
    }

    // Built on demand rather than in the constructor: half of these binds are declared on the action
    // node tables, and the nodes do not register until after this page is constructed.
    bool CategoryKeybinds::ShadowsModifier(const InputBind& a_Bind) const {

        if (ModifierKeys.empty() || a_Bind.Name == Keybinds::TargetSwitchBind) {
            return false;
        }

        if (!Keybinds::StartsAction.contains(a_Bind.Name)) {
            return false;
        }

        return std::ranges::all_of(ModifierKeys, [&](const std::string& a_Key) {
            return std::ranges::find(a_Bind.Keys, a_Key) != a_Bind.Keys.end();
        });
    }

    void CategoryKeybinds::RefreshModifierKeys() {

        const auto it = std::ranges::find(Keybinds::InputEvents, Keybinds::TargetSwitchBind, &InputBind::Name);

        if (it == Keybinds::InputEvents.end()) {
            ModifierKeys.clear();
            return;
        }

        if (ModifierKeys != it->Keys) {
            ModifierKeys = it->Keys;
        }
    }

    void CategoryKeybinds::RebuildGroups() {

        RefreshModifierKeys();

        Groups.assign(static_cast<std::size_t>(BindGroup::kTotal), Group{});

        for (std::size_t i = 0; i < Groups.size(); ++i) {
            Groups[i].Id = static_cast<BindGroup>(i);
        }

        const bool advanced = Config::Hidden.IKnowWhatImDoing;

        for (auto& bind : Keybinds::InputEvents) {

            const InputDef* def = KeybindHandler::Find(bind.Name);

            if (def && def->Advanced && !advanced) {
                continue;
            }

            const std::string label = def && !def->UIName.empty() ? std::string(def->UIName) : bind.Name;

            if (!SearchRes.empty() && !ContainsString(label, SearchRes) && !ContainsString(bind.Name, SearchRes)) {
                continue;
            }

            Group& group = Groups[static_cast<std::size_t>(def ? def->Group : BindGroup::kOther)];

            (Keybinds::StartsAction.contains(bind.Name) ? group.Starts : group.During).push_back(&bind);
        }

        // Land on something populated rather than an empty pane.
        if (Groups[static_cast<std::size_t>(Selected)].Count() == 0) {
            for (const auto& group : Groups) {
                if (group.Count() > 0) {
                    Selected = group.Id;
                    break;
                }
            }
        }

        m_groupsBuilt = true;
    }

    void CategoryKeybinds::Draw() {

        static std::string lastSearch;
        static std::size_t lastCount = 0;

        if (!m_groupsBuilt || lastSearch != SearchRes || lastCount != Keybinds::InputEvents.size()) {
            lastSearch = SearchRes;
            lastCount = Keybinds::InputEvents.size();
            RebuildGroups();
        }

        // A capture that ended elsewhere leaves the row stuck showing the prompt.
        if (RebindIndex != 0 && !ImInput::IsCapturingKeys()) {
            RebindIndex = 0;
        }

        // Every frame, not with the groups. Rebinding Target Self changes neither the search nor the
        // number of binds, so the groups are not rebuilt and the warning kept comparing against the
        // keys it was bound to when the page opened.
        RefreshModifierKeys();

        CurEventIndex = UINT16_MAX;

        DrawOptions();

        // Sized to the longest label rather than a fixed share of the window, so the list is only as
        // wide as it has to be.
        float listWidth = 0.0f;
        {
            ImGui::PushFont(nullptr, 21.0f);

            for (const auto& group : Groups) {
                listWidth = std::max(listWidth, ImGui::CalcTextSize(BindGroupName(group.Id).data()).x);
            }

            listWidth += ImGui::CalcTextSize("  000").x + ImGui::GetStyle().FramePadding.x * 4.0f + ImGui::GetStyle().ScrollbarSize;

            ImGui::PopFont();
        }

        DrawGroupList(listWidth);

        ImGui::SameLine(0.0f, 8.0f);
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical, 1.0f);
        ImGui::SameLine(0.0f, 8.0f);

        if (ImGui::BeginChild("##BindPane", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders)) {
            DrawBinds();
        }
        ImGui::EndChild();
    }

    void CategoryKeybinds::DrawOptions() {

        if (ImGui::BeginChild("##KeybindOptions", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY)) {

            ImGui::BeginDisabled(RebindIndex > 0);
            {
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
                ImGui::InputTextWithHint("##Search", "Search keybinds...", &SearchRes);
                ImGuiEx::Tooltip(TH0);
                ImGui::PopItemWidth();

                ImGuiEx::SeperatorV();

                ImGuiEx::CheckBox("Show localized key names", &Config::UI.bLocalizedKeyNames, TH2);

                ImGuiEx::SeperatorV();

                if (ImGuiEx::ImageButton("##ResetKeybinds", ImageList::Generic_Reset, 18, TH1)) {
                    Keybinds::ResetKeybinds();
                    m_groupsBuilt = false;
                }
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();

        ImGui::Spacing();
    }

    void CategoryKeybinds::DrawGroupList(float a_width) {

        ImGui::BeginDisabled(RebindIndex > 0);

        if (ImGui::BeginChild("##GroupList", ImVec2(a_width, 0.0f), ImGuiChildFlags_Borders)) {

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 4.0f));
            ImGui::PushFont(nullptr, 21.0f);

            for (const auto& band : Bands) {

                const auto first = static_cast<std::size_t>(band.First);
                const auto last = static_cast<std::size_t>(band.Last);

                bool anyShown = false;

                for (std::size_t i = first; i <= last; ++i) {
                    anyShown = anyShown || Groups[i].Count() > 0;
                }

                if (!anyShown) {
                    continue;
                }

                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImUtil::Colors::Subscript);
                    ImGui::SeparatorText(band.Label);
                    ImGui::PopStyleColor();
                }

                for (std::size_t i = first; i <= last; ++i) {

                    const Group& group = Groups[i];

                    if (group.Count() == 0) {
                        continue;
                    }

                    const std::string_view label = BindGroupName(group.Id);
                    const std::string count = std::format("{}", group.Count());

                    if (ImGui::Selectable(std::format("  {}##grp{}", label, i).c_str(), Selected == group.Id)) {
                        Selected = group.Id;
                    }

                    // Right aligned so the eye can run down the counts.
                    {
                        const float offset = ImGui::GetWindowWidth() - ImGui::CalcTextSize(count.c_str()).x - ImGui::GetStyle().FramePadding.x * 3.0f;
                        ImGui::SameLine(offset);
                        ImGuiEx::TextColorShadow(ImUtil::Colors::Subscript, "%s", count.c_str());
                    }
                }

                ImGui::Spacing();
            }

            ImGui::PopFont();
            ImGui::PopStyleVar();
        }

        ImGui::EndChild();
        ImGui::EndDisabled();
    }

    void CategoryKeybinds::DrawBinds() {

        const Group& group = Groups[static_cast<std::size_t>(Selected)];

        if (group.Count() == 0) {
            ImGui::TextUnformatted("No results matching search string.");
            return;
        }

        {
            ImGui::PushFont(nullptr, 26.0f);
            ImGuiEx::TextShadow("%s", BindGroupName(group.Id).data());
            ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();
        }

        // Measured once for the group on screen rather than over every bind in the mod every frame.
        {
            KeyColumnWidth = 0.0f;

            ImGui::PushFont(nullptr, 21.0f);

            const auto measure = [&](const std::vector<InputBind*>& a_binds) {
                for (const auto* bind : a_binds) {

                    const InputDef* def = NodeDefFor(bind->Name);
                    const std::string_view label = def && !def->UIName.empty() ? def->UIName : std::string_view(bind->Name);

                    float width = ImGui::CalcTextSize(label.data(), label.data() + label.size()).x;

                    // The warning marker is drawn on the same line and is part of what has to fit.
                    if (ShadowsModifier(*bind)) {
                        width += ImGui::GetStyle().ItemSpacing.x + ImGui::CalcTextSize("(!)").x;
                    }

                    KeyColumnWidth = std::max(KeyColumnWidth, width);
                }
            };

            measure(group.Starts);
            measure(group.During);

            // The shadow draws the same text a pixel out, so the widest label still needs somewhere
            // to put its own edge.
            KeyColumnWidth += 2.0f;

            ImGui::PopFont();
        }

        // Only worth splitting when there is something on both sides of it.
        const bool split = !group.Starts.empty() && !group.During.empty();

        DrawBindList(split ? "Starts the action" : "", group.Starts);
        DrawBindList(split ? "While running" : "", group.During);
    }

    void CategoryKeybinds::DrawBindList(std::string_view a_heading, const std::vector<InputBind*>& a_binds) {

        if (a_binds.empty()) {
            return;
        }

        if (!a_heading.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImUtil::Colors::Subscript);
            ImGui::SeparatorText(std::string(a_heading).c_str());
            ImGui::PopStyleColor();
        }

        for (auto* bind : a_binds) {

            const InputDef* def = NodeDefFor(bind->Name);

            std::string label(bind->Name);
            const char* description = nullptr;

            if (def) {
                if (!def->UIName.empty()) {
                    label = std::string(def->UIName);
                }
                description = def->UIDescription.empty() ? nullptr : def->UIDescription.data();
            }

            DrawInputEvent(*bind, label, description);
        }

        ImGui::Spacing();
    }

    void CategoryKeybinds::SetWindowBusy(const bool a_busy) {
        if (SettingsWindow* Window = dynamic_cast<SettingsWindow*>(GTSMenu::WindowManager->wSettings)) {
            Window->m_busy = a_busy;
            Window->m_disableUIInteraction = a_busy;
        }
    }

    bool CategoryKeybinds::DrawInputEvent(InputBind& a_bind, const std::string& a_name, const char* a_description) {

        const float ButtonImageSize = 18 * ImGui::GetStyle().FontScaleMain;
        const float ButtonSize = ButtonImageSize + ImGui::GetStyle().ItemSpacing.x + (ImGui::GetStyle().FramePadding.x * 2.0f);
        const bool IsRebinding = (RebindIndex == CurEventIndex && RebindIndex != 0);

        // Extra padding between name and key input
        const float nameToKeyPadding = ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x * 2;

        // Get available region width inside the child
        float contentWidth = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2 - ImGui::GetStyle().CellPadding.x * 2;
        float nameColWidth = KeyColumnWidth + nameToKeyPadding;

        // Keys column takes remaining space
        float keysColWidth = contentWidth - nameColWidth - (ButtonSize * 2);

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

        ImGui::BeginChild(CurEventIndex, { 0.0f, 0.0f }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX);
        {
            ImGui::BeginDisabled(RebindIndex != CurEventIndex && RebindIndex != 0);
            {
                ImGui::BeginTable("##KeybindRow", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoPadInnerX);
                {
                    // Setup column widths
                    ImGui::TableSetupColumn("##Name", ImGuiTableColumnFlags_WidthFixed, nameColWidth);
                    ImGui::TableSetupColumn("##Keys", ImGuiTableColumnFlags_WidthFixed, keysColWidth);
                    ImGui::TableSetupColumn("##Rebind", ImGuiTableColumnFlags_WidthFixed, ButtonSize);
                    ImGui::TableSetupColumn("##Options", ImGuiTableColumnFlags_WidthFixed, ButtonSize);

                    // Column 1: Action Name
                    ImGui::TableNextColumn();

                    ImGui::PushFont(nullptr, 21.0f);
                    {
                        // Centred against the row rather than left where text lands by default. The
                        // other three columns are framed widgets and are taller than this line, and
                        // the label is a larger font than the frame, so neither AlignTextToFramePadding
                        // nor plain text puts it level with them.
                        {
                            const float row = ImGui::GetFrameHeight();
                            const float text = ImGui::GetTextLineHeight();
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (row - text) * 0.5f));
                        }

                        ImGuiEx::TextColorShadow(a_bind.Disabled ? ImUtil::Colors::Warning : ImGui::GetStyle().Colors[ImGuiCol_Text] ,"%s", a_name.c_str());
                        if (a_description != nullptr && a_description[0] != '\0') {
                            ImGuiEx::Tooltip(a_description, true);
                        }

                        if (ShadowsModifier(a_bind)) {
                            ImGui::SameLine();
                            ImGuiEx::TextColorShadow(ImUtil::Colors::Warning, "(!)");
                            ImGuiEx::Tooltip(T7, true);
                        }
                    }
                    ImGui::PopFont();

                    // Column 2: Current Keys
                    ImGui::TableNextColumn();
                    std::string VisualKeyString;
                    {
                        const auto append = [&VisualKeyString](const std::string& a_label) {
                            if (!VisualKeyString.empty()) {
                                VisualKeyString += " + ";
                            }
                            VisualKeyString += a_label;
                        };

                        if (IsRebinding) {
                            for (const auto& key : ImInput::CapturedKeys()) {
                                append(InputKeyDisplayName(key, Config::UI.bLocalizedKeyNames));
                            }
                        }
                        else {
                            for (const auto& name : a_bind.Keys) {
                                const auto key = InputKeyFromName(name);
                                append(key ? InputKeyDisplayName(*key, Config::UI.bLocalizedKeyNames) : name);
                            }
                        }
                    }
                    std::string InputText = VisualKeyString.empty() ? "Press any Key or ESC To Cancel" : VisualKeyString;

                    ImGui::SetNextItemWidth(keysColWidth - ImGui::GetStyle().CellPadding.x * 2);
                    ImGui::BeginDisabled(a_bind.Disabled);
                    {
                        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                        ImGui::InputText("##KeyRebind", &InputText, ImGuiInputTextFlags_ReadOnly);
                        ImGui::PopItemFlag();
                    }
                    ImGui::EndDisabled();

                    // Column 3: Rebind Button
                    ImGui::TableNextColumn();
                    SetWindowBusy(RebindIndex > 0);

                    ImGui::BeginDisabled((ImInput::CapturedKeys().empty() && IsRebinding) || a_bind.Disabled);
                    {
                        if (ImGuiEx::ImageButton(("##Rebind" + std::to_string(CurEventIndex)).c_str(), IsRebinding ? ImageList::Generic_OK : ImageList::Keybind_EditKeybind, 18, T5)) {
                            if (IsRebinding) {
                                // Stored under the position name, not the label, so the file stays
                                // portable between users on different layouts.
                                a_bind.Keys.clear();
                                for (const auto& key : ImInput::CapturedKeys()) {
                                    a_bind.Keys.emplace_back(InputKeyStorageName(key));
                                }
                                ImInput::EndKeyCapture();
                                RebindIndex = 0;
                            }
                            else {
                                RebindIndex = CurEventIndex;
                                ImInput::BeginKeyCapture();
                            }
                        }

                        // Left click is bindable like any other button, so the one place it still has
                        // to reach the UI is this button. Read after it is drawn, which means the
                        // pump acts on the previous frame's hover - close enough for a cursor.
                        if (IsRebinding) {
                            ImInput::SetRebindConfirmHovered(ImGui::IsItemHovered());
                        }
                    }
                    ImGui::EndDisabled();

                    // Column 4: Options Button
                    ImGui::BeginDisabled(IsRebinding);
                    ImGui::TableNextColumn();
                    if (ImGuiEx::ImageButton(("##OptionsOpen" + std::to_string(CurEventIndex)).c_str(), ImageList::Keybind_ShowAdvanced, 18, T6)) {
                        ImGui::OpenPopup(("##Options" + std::to_string(CurEventIndex)).c_str());
                    }
                    ImGui::EndDisabled();

                    // Options Popup
                    if (ImGui::BeginPopup(("##Options" + std::to_string(CurEventIndex)).c_str())) {
                        ImGui::Text("Extra Options: %s", a_name.c_str());
                        ImGui::Separator();
                        ImGui::PushItemWidth(250.0f);
                        ImGuiEx::CheckBox("Disabled", &a_bind.Disabled, T0);
                        if (!a_bind.Disabled) {
                            ImGui::BeginDisabled(IsRebinding);
                            ImGuiEx::CheckBox("Exclusive", &a_bind.Exclusive, T1);
                            ImGuiEx::ComboEx<BindTriggerType>("Trigger Type", a_bind.Trigger, T2);
                            ImGuiEx::ComboEx<BindBlockType>("Block Input", a_bind.Block, T3);
                            ImGui::InputFloat("Trigger After", &a_bind.Duration, 0.1f, 0.01f, "%.2f Seconds");
                            ImGuiEx::Tooltip(T4);
                            a_bind.Duration = std::clamp(a_bind.Duration, 0.0f, 10.0f);
                            ImGui::EndDisabled();
                        }
                        ImGui::PopItemWidth();
                        ImGui::EndPopup();
                    }
                }
                ImGui::EndTable();
                ImGui::PopStyleVar(2);

                // ImInput collects the presses, so nothing here reads ImGui's key state. That is
                // the whole layout fix: ImGui only ever sees a virtual key mapped through the
                // active layout, and the physical key is gone by then.
                if (IsRebinding && ImInput::KeyCaptureCancelled()) {
                    ImInput::EndKeyCapture();
                    RebindIndex = 0;
                }
                CurEventIndex++;
            }
            ImGui::EndDisabled();
        }
        ImGui::EndChild();
        return true;
    }
}