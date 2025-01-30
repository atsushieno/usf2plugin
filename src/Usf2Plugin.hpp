#pragma once

#include "distrho/DistrhoPlugin.hpp"
#include "SF2Application.h"
#include "Usf2EditControllerWebServer.h"

namespace usf2 {

    class Usf2Plugin : public DISTRHO::Plugin {
        SF2Application sf2{};
        Usf2WebUIServer web_server{};

    public:
        Usf2Plugin() : Plugin(/*parameterCount*/130 * 16, /*programCount*/128, /*stateCount*/0) {
            sf2.initialize();
            auto bp = getBundlePath();
            auto bundlePath = bp ? std::string{bp} : "";
#if __APPLE__
            bundlePath = std::format("{}/Contents/Resources", bundlePath);
#endif
            web_server.initialize(bundlePath);
        }
        ~Usf2Plugin() override = default;

    protected:
        uint32_t getVersion() const override { return 0; }

        void sampleRateChanged(double newSampleRate) override {
            sf2.sampleRate(newSampleRate);
        }

        void initProgramName(uint32_t index, String& programName) override {
            static const char* gmInstrumentNames[] {
                    "Acoustic Piano",
                    "Bright Piano",
                    "Electric Grand Piano",
                    "Honky-tonk Piano",
                    "Electric Piano",
                    "Electric Piano 2",
                    "Harpsichord",
                    "Clavi",
                    "Celesta",
                    "Glockenspiel",
                    "Musical Box",
                    "Vibraphone",
                    "Marimba",
                    "Xylophone",
                    "Tubular Bell",
                    "Dulcimer",
                    "Drawbar Organ",
                    "Percussive Organ",
                    "Rock Organ",
                    "Church Organ",
                    "Reed Organ",
                    "Accordion",
                    "Harmonica",
                    "Tango Accordion",
                    "Acoustic Guitar (nylon)",
                    "Acoustic Guitar (steel)",
                    "Electric Guitar (jazz)",
                    "Electric Guitar (clean)",
                    "Electric Guitar (muted)",
                    "Overdriven Guitar",
                    "Distortion Guitar",
                    "Guitar Harmonics",
                    "Acoustic Bass",
                    "Electric Bass (finger)",
                    "Electric Bass (pick)",
                    "Fretless Bass",
                    "Slap Bass 1",
                    "Slap Bass 2",
                    "Synth Bass 1",
                    "Synth Bass 2",
                    "Violin",
                    "Viola",
                    "Cello",
                    "Double bass",
                    "Tremelo Strings",
                    "Pizzicato Strings",
                    "Orchestral Harp",
                    "Timpani",
                    "String Ensemble 1",
                    "String Ensemble 2",
                    "Synth Strings 1",
                    "Synth Strings 2",
                    "Voice Aahs",
                    "Voice Oohs",
                    "Synth Voice",
                    "Orchestra Hit",
                    "Trumpet",
                    "Trombone",
                    "Tuba",
                    "Muted Trumpet",
                    "French Horn",
                    "Brass Section",
                    "Synth Brass 1",
                    "Synth Brass 2",
                    "Soprano Sax",
                    "Alto Sax",
                    "Tenor Sax",
                    "Baritone Sax",
                    "Oboe",
                    "English Horn",
                    "Bassoon",
                    "Clarinet",
                    "Piccolo",
                    "Flute",
                    "Recorder",
                    "Pan Flute",
                    "Brown Bottle",
                    "Shakuhachi",
                    "Whistle",
                    "Ocarina",
                    "Lead 1 (square)",
                    "Lead 2 (sawtooth)",
                    "Lead 3 (calliope)",
                    "Lead 4 (chiff)",
                    "Lead 5 (charang)",
                    "Lead 6 (voice)",
                    "Lead 7 (fifths)",
                    "Lead 8 (bass + lead)",
                    "Pad 1 (fantasia)",
                    "Pad 2 (warm)",
                    "Pad 3 (polysynth)",
                    "Pad 4 (choir)",
                    "Pad 5 (bowed)",
                    "Pad 6 (metallic)",
                    "Pad 7 (halo)",
                    "Pad 8 (sweep)",
                    "FX 1 (rain)",
                    "FX 2 (soundtrack)",
                    "FX 3 (crystal)",
                    "FX 4 (atmosphere)",
                    "FX 5 (brightness)",
                    "FX 6 (goblins)",
                    "FX 7 (echoes)",
                    "FX 8 (sci-fi)",
                    "Sitar",
                    "Banjo",
                    "Shamisen",
                    "Koto",
                    "Kalimba",
                    "Bagpipe",
                    "Fiddle",
                    "Shanai",
                    "Tinkle Bell",
                    "Agogo",
                    "Steel Drums",
                    "Woodblock",
                    "Taiko Drum",
                    "Melodic Tom",
                    "Synth Drum",
                    "Reverse Cymbal",
                    "Guitar Fret Noise",
                    "Breath Noise",
                    "Seashore",
                    "Bird Tweet",
                    "Telephone Ring",
                    "Helicopter",
                    "Applause",
                    "Gunshot"
            };
            programName = gmInstrumentNames[index];
        }

        void loadProgram(uint32_t index) override {
            // FIXME: it should not be supported like this...
            sf2.programChangeHack(index);
        }

        void initParameter(uint32_t index, DISTRHO::Parameter &parameter) override {
            DISTRHO::Plugin::initParameter(index, parameter);
            // DPF assigns "CC"s for CAf and pitch bend, but their names are left awkward. We fix them here.
            switch (index % 130) {
                case 128:
                    parameter.name = String{std::format("Channel {} Channel Pressure", index / 130 + 1).data()};
                    break;
                case 129:
                    parameter.name = String{std::format("Channel {} Pitch Bend", index / 130 + 1).data()};
                    break;
            }
        }

        void setParameterValue(uint32_t index, float value) override {
            // FIXME: implement
        }

        String getState(const char* statePropertyName) const override {
            printf("getState invoked: %s\n", statePropertyName);
            // FIXME: implement
            return String();
        }

        void setState(const char* statePropertyName, const char* value) override {
            printf("setState invoked: %s : %s\n", statePropertyName, value);
            // FIXME: implement
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
