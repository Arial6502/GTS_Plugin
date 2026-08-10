#pragma once

#include "UI/Core/ImCategorySplit.hpp"

namespace GTS {

    class CategoryKillMove final : public ImCategorySplit {
        public:
        CategoryKillMove();
        void DrawLeft() override;
        void DrawRight() override;
    };

}