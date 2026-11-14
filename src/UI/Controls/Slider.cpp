
#include "UI/Controls/Slider.hpp"
#include "UI/Controls/ToolTip.hpp"

#include "Config/Config.hpp"

namespace ImGuiEx {

    bool SliderF(const char* a_label, float* a_value, float a_min, float a_max, const char* a_tooltip, const char* a_cfmt, const bool a_disabled, const bool a_alwaysClamp) {

    	ImGui::BeginDisabled(a_disabled);

        auto flags = GTS::Config::Advanced.bEnforceUIClamps ? ImGuiSliderFlags_AlwaysClamp : 0;

        if (a_alwaysClamp && flags) {
            *a_value = std::min(*a_value, a_max);
            *a_value = std::max(*a_value, a_min);
        }

        const bool res = ImGui::SliderFloat(a_label, a_value, a_min, a_max, a_cfmt, flags);

        Tooltip(a_tooltip);

        ImGui::EndDisabled();

        return res;
    }

    bool SliderF2(const char* a_label, float* a_value, float a_min, float a_max, const char* a_tooltip, const char* a_cfmt, const bool a_disabled, const bool a_alwaysClamp) {

        ImGui::BeginDisabled(a_disabled);

        auto flags = GTS::Config::Advanced.bEnforceUIClamps ? ImGuiSliderFlags_AlwaysClamp : 0;

        if (a_alwaysClamp && flags) {
            a_value[0] = std::clamp(a_value[0], a_min, a_max);
            a_value[1] = std::clamp(a_value[1], a_min, a_max);
        }

        const bool res = ImGui::SliderFloat2(a_label, a_value, a_min, a_max, a_cfmt, flags);

        Tooltip(a_tooltip);

        ImGui::EndDisabled();
        return res;
    }

    bool SliderF3(const char* a_label, float* a_value, float a_min, float a_max, const char* a_tooltip, const char* a_cfmt, const bool a_disabled, const bool a_alwaysClamp) {

        ImGui::BeginDisabled(a_disabled);

        auto flags = GTS::Config::Advanced.bEnforceUIClamps ? ImGuiSliderFlags_AlwaysClamp : 0;

        if (a_alwaysClamp && flags) {
            a_value[0] = std::clamp(a_value[0], a_min, a_max);
            a_value[1] = std::clamp(a_value[1], a_min, a_max);
            a_value[2] = std::clamp(a_value[2], a_min, a_max);
        }

        const bool res = ImGui::SliderFloat3(a_label, a_value, a_min, a_max, a_cfmt, flags);

        Tooltip(a_tooltip);

        ImGui::EndDisabled();

        return res;
    }

    bool SliderU16(const char* a_label, uint16_t* a_value, uint16_t a_min, uint16_t a_max, const char* a_tooltip, const char* a_cfmt, bool a_disabled, bool a_alwaysClamp) {
        ImGui::BeginDisabled(a_disabled);

        auto flags = GTS::Config::Advanced.bEnforceUIClamps ? ImGuiSliderFlags_AlwaysClamp : 0;

        if (a_alwaysClamp && flags) {
            *a_value = std::clamp(*a_value, a_min, a_max);
        }

        const bool res = ImGui::SliderScalar(a_label, ImGuiDataType_U16, a_value, &a_min, &a_max, a_cfmt, flags);

        Tooltip(a_tooltip);

        ImGui::EndDisabled();

        return res;
    }

    bool SliderU8(const char* a_label, uint8_t* a_value, uint8_t a_min, uint8_t a_max, const char* a_tooltip, const char* a_cfmt, bool a_disabled, bool a_alwaysClamp) {
        ImGui::BeginDisabled(a_disabled);

        auto flags = GTS::Config::Advanced.bEnforceUIClamps ? ImGuiSliderFlags_AlwaysClamp : 0;

        if (a_alwaysClamp && flags) {
            *a_value = std::clamp(*a_value, a_min, a_max);
        }

        const bool res = ImGui::SliderScalar(a_label, ImGuiDataType_U8, a_value, &a_min, &a_max, a_cfmt, flags);

        Tooltip(a_tooltip);

        ImGui::EndDisabled();

        return res;
    }
    

    float WeightTotal(std::initializer_list<float> a_weights) {

        float total = 0.0f;

        for (const float weight : a_weights) {
            total += std::max(weight, 0.0f);
        }

        return total;
    }

    bool SliderWeight(const char* a_label, float* a_value, const float a_total,
        const char* a_tooltip, const bool a_disabled, const float a_max) {

        char cfmt[48] = {};

        if (!a_value || *a_value <= 0.0f || a_total <= 0.0f) {
            std::snprintf(cfmt, sizeof(cfmt), "%%.0f  (never)");
        }
        else {
            // Two printf passes: this one bakes the share into the format, then ImGui runs that format
            // over the value. The four percent signs survive the first pass as two and render as one.
            std::snprintf(cfmt, sizeof(cfmt), "%%.0f  (%.0f%%%%)", 100.0f * *a_value / a_total);
        }

        return SliderF(a_label, a_value, 0.0f, a_max, a_tooltip, cfmt, a_disabled);
    }
}
