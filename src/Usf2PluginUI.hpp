#pragma once

#include "distrho/DistrhoUI.hpp"

namespace usf2 {

    class Usf2PluginUI : public DISTRHO::UI {
    public:
        Usf2PluginUI() : UI(DISTRHO_UI_DEFAULT_WIDTH, DISTRHO_UI_DEFAULT_HEIGHT) {

        }
    };

}

namespace DISTRHO {
    UI *createUI() { return new usf2::Usf2PluginUI; }
}
