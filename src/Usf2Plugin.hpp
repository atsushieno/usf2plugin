#pragma once

#include "distrho/DistrhoPlugin.hpp"
#include "SF2Application.h"
#include "Usf2EditControllerWebServer.h"
#include "memory/choc_Base64.h"

namespace usf2 {

    class Usf2Plugin : public DISTRHO::Plugin {
        mutable SF2Application sf2{};
        Usf2WebUIServer web_server{};

    public:
        // Parameter layout: stride 256 per channel.
        //   local  0–127 : CC 0–127
        //   local    128 : Channel Pressure
        //   local    129 : Program Change
        //   local    130 : Pitch Bend  (raw 14-bit, 0–16383, center 8192)
        //   local 131–255: hidden padding (keeps channel blocks power-of-2 aligned)
        // Total: 15×256 + 131 = 3971
        static constexpr uint32_t kParamStride    = 256;
        static constexpr uint32_t kParamPerCh     = 131; // real params per channel
        static constexpr uint32_t kParamTotal     = 15 * kParamStride + kParamPerCh; // 3971

        static uint8_t  paramChannel(uint32_t index) { return (uint8_t)(index / kParamStride); }
        static uint32_t paramLocal  (uint32_t index) { return index % kParamStride; }

        Usf2Plugin() : Plugin(/*parameterCount*/kParamTotal, /*programCount*/128, /*stateCount*/1) {
            auto bp = getBundlePath();
            auto bundlePath = bp ? std::string{bp} : "";
#if __APPLE__
            bundlePath = bp ? std::format("{}/Contents/Resources", bundlePath) : "";
#endif
            sf2.initialize(bundlePath);
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

        void initParameter(uint32_t index, DISTRHO::Parameter& parameter) override {
            uint8_t  ch    = paramChannel(index);
            uint32_t local = paramLocal(index);

            if (local >= kParamPerCh) {
                // Padding slots — hidden from host UI.
                parameter.hints = kParameterIsHidden;
                parameter.name  = String("-");
                parameter.ranges.min = 0.0f;
                parameter.ranges.max = 1.0f;
                parameter.ranges.def = 0.0f;
                return;
            }

            parameter.hints = kParameterIsAutomatable;
            parameter.ranges.min = 0.0f;

            if (local < 129) { // CC 0–127 and Channel Pressure
                parameter.name  = local < 128
                    ? String(std::format("Ch{} CC{}", ch + 1, local).c_str())
                    : String(std::format("Ch{} Pressure", ch + 1).c_str());
                parameter.ranges.max = 127.0f;
                parameter.ranges.def = 0.0f;
            } else if (local == 129) {
                parameter.name  = String(std::format("Ch{} Program", ch + 1).c_str());
                parameter.ranges.max = 127.0f;
                parameter.ranges.def = 0.0f;
            } else { // local == 130
                parameter.name  = String(std::format("Ch{} PitchBend", ch + 1).c_str());
                parameter.ranges.max = 16383.0f;
                parameter.ranges.def = 8192.0f; // center
            }
        }

        void setParameterValue(uint32_t index, float value) override {
            uint8_t  ch    = paramChannel(index);
            uint32_t local = paramLocal(index);

            if (local >= kParamPerCh) return;

            if (local < 129) {
                auto u7 = (uint8_t)std::clamp(std::lround(value), 0L, 127L);
                if (local < 128) sf2.setCC(ch, (uint8_t)local, u7);
                else              sf2.setPressure(ch, u7);
            } else if (local == 129) {
                sf2.setProgram(ch, (uint8_t)std::clamp(std::lround(value), 0L, 127L));
            } else {
                sf2.setPitchBend(ch, (uint16_t)std::clamp(std::lround(value), 0L, 16383L));
            }
        }

        void initState(uint32_t index, State& state) override {
            if (index == 0) {
                state.key          = "smf";
                state.label        = "MIDI State (SMF)";
                state.defaultValue = "";
            }
        }

        String getState(const char* key) const override {
            if (std::string_view{key} == "smf") {
                auto smf     = sf2.serializeToSMF();
                auto encoded = choc::base64::encodeToString(smf.data(), smf.size());
                return String(encoded.c_str());
            }
            return String();
        }

        void setState(const char* key, const char* value) override {
            if (std::string_view{key} == "smf" && value && *value) {
                std::vector<uint8_t> smf;
                if (choc::base64::decodeToContainer(smf, std::string_view{value}))
                    sf2.deserializeFromSMF(smf);
            }
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
