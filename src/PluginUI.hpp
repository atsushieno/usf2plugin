#pragma once

#include "distrho/DistrhoUI.hpp"

namespace usf2 {

    class PluginUI : public DISTRHO::UI {
    public:
        PluginUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {

        }
    };

}

namespace DISTRHO {
    UI *createUI() { return new usf2::PluginUI; }
}
