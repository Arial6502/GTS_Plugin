#pragma once

namespace ImGuiEx {

    bool SliderF (const char* a_label, float* a_value, float a_min = 0.0f, float a_max = 1.0f, 
        const char* a_tooltip = nullptr, const char* a_cfmt = "%.2f", bool a_disabled = false, bool a_alwaysClamp = true);

    bool SliderF2(const char* a_label, float* a_value, float a_min = 0.0f, float a_max = 1.0f, 
        const char* a_tooltip = nullptr, const char* a_cfmt = "%.2f", bool a_disabled = false, bool a_alwaysClamp = true);

    bool SliderF3(const char* a_label, float* a_value, float a_min = 0.0f, float a_max = 1.0f, 
        const char* a_tooltip = nullptr, const char* a_cfmt = "%.2f", bool a_disabled = false, bool a_alwaysClamp = true);

    bool SliderU16(const char* a_label, uint16_t* a_value, uint16_t a_min, uint16_t a_max, const char* a_tooltip, const char* a_cfmt, bool a_disabled = false, bool a_alwaysClamp = true);
    bool SliderU8(const char* a_label, uint8_t* a_value, uint8_t a_min, uint8_t a_max, const char* a_tooltip, const char* a_cfmt, bool a_disabled = false, bool a_alwaysClamp = true);

    // Sum of a group of competing weights, ignoring the rows the picker would skip. Values rather than
    // pointers: a slider drawn below may change one mid frame, and the share catching up next frame is
    // not worth the plumbing.
    [[nodiscard]] float WeightTotal(std::initializer_list<float> a_weights);

    // A slider for one weight in a group. The overlay shows the weight and the share of the group it
    // currently buys, because a weight on its own says nothing. A weight of zero reads as never, since
    // the picker skips those rows rather than rolling them rarely.
    bool SliderWeight(const char* a_label, float* a_value, float a_total,
        const char* a_tooltip = nullptr, bool a_disabled = false, float a_max = 100.0f);
}



