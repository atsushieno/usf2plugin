#pragma once

#include "distrho/DistrhoPlugin.hpp"
#include "SF2Application.h"

namespace usf2 {

    class Usf2Plugin : public DISTRHO::Plugin {
        SF2Application sf2{};

    public:
        Usf2Plugin() : Plugin(/*parameterCount*/0, /*programCount*/0, /*stateCount*/0) {
            sf2.initialize();
        }
        ~Usf2Plugin() override = default;

    protected:
        uint32_t getVersion() const override { return 0; }

        void sampleRateChanged(double newSampleRate) override {
            sf2.sampleRate(newSampleRate);
        }

        void run(const float **inputs, float **outputs, uint32_t frames,
                 const MidiEvent* midiEvents, uint32_t midiEventCount) override {
            sf2.process(outputs[0], outputs[1], frames, midiEvents, midiEventCount);
        }

    public:
    };

}

namespace DISTRHO {
    Plugin *createPlugin() { return new usf2::Usf2Plugin; }
}
