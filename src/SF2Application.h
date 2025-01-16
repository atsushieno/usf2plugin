#pragma once

#include "distrho/DistrhoPlugin.hpp"
#include "tsf.h"
#include <string>
#include <filesystem>
#include "GeneralUser-GS.h"

namespace usf2 {

    class SF2Entry {
        std::filesystem::path sf2_path{};
        tsf* sf2{};

    public:
        SF2Entry() = default;

        ~SF2Entry() {
            if (sf2)
                tsf_close(sf2);
        }

        std::filesystem::path& path() { return sf2_path; }
        tsf* soundfont() { return sf2; }

        bool load(std::filesystem::path& sf2File) {
            sf2_path = sf2File;
            sf2 = tsf_load_filename(sf2File.string().c_str());
            return sf2 != nullptr;
        }
    };

    class SF2Application {
        float sample_rate{44100};
        float global_gain{1.0};
        std::vector<std::unique_ptr<SF2Entry>> sf2_entries{16};
        std::vector<tsf*> tsfs{16};
        tsf* channels[16];

    public:
        static tsf* gugs() {
            static int32_t pos;
            static tsf* sf = tsf_load_memory(GeneralUser_GS_GeneralUser_GS_sf2(), GS_GeneralUser_GS_sf2_len);
            return sf;
        }

        SF2Application() = default;

        void initialize() {
            sampleRate(sample_rate); // initialize
            mapChannelToSF2(0, -1);
        }

        float sampleRate() { return sample_rate; }
        void sampleRate(float newSampleRate) {
            sample_rate = newSampleRate;

            tsf_set_output(gugs(), TSFOutputMode::TSF_STEREO_UNWEAVED, sample_rate, global_gain);
            for (auto sf2 : tsfs)
                if (sf2)
                    tsf_set_output(sf2, TSFOutputMode::TSF_STEREO_UNWEAVED, sample_rate, global_gain);
        }

        std::vector<tsf*>& soundfonts() { return tsfs; }

        uint32_t loadSF2(std::filesystem::path& file) {
            for (int32_t i = 0, n = sf2_entries.size(); i < n; i++)
                if (sf2_entries[i]->path() == file)
                    return i;

            auto entry = std::make_unique<SF2Entry>();
            if (!entry->load(file))
                return -1;
            tsfs.emplace_back(entry->soundfont());
            sf2_entries.emplace_back(std::move(entry));
            return tsfs.size() - 1;
        }

        void mapChannelToSF2(uint8_t channel, int32_t sf2Index) {
            auto tsf = sf2Index >= 0 ? soundfonts()[sf2Index] : gugs();
            channels[channel] = tsf;
            // FIXME: this 48 is only for testing
            tsf_channel_set_presetindex(tsf, channel, 48);
        }

        void process(float* outputL, float* outputR, uint32_t size, const MidiEvent* midiEvents, uint32_t midiEventCount) {

            // process MIDI events
            for (uint32_t i = 0; i < midiEventCount; i++) {
                auto &e = midiEvents[i];
                auto ch = e.data[0] & 0xF;
                switch (e.data[0] & 0xF0) {
                    case 0x80:
                        tsf_note_off(channels[ch], 48, e.data[1]);
                        break;
                    case 0x90:
                        tsf_note_on(channels[ch], 48, e.data[1], e.data[2] / 127.0);
                        break;
                    case 0xB0:
                        tsf_channel_midi_control(channels[ch], ch, e.data[1], e.data[2]);
                        break;
                    case 0xC0:
                        // FIXME: drums
                        tsf_channel_set_presetindex(channels[ch], ch, e.data[1]);
                        break;
                    case 0xE0:
                        tsf_channel_set_pitchwheel(channels[ch], ch, e.data[1] * 0x80 + e.data[2]);
                        break;
                }
            }

            // FIXME: channels
            tsf_render_float_separate(channels[0], outputL, outputR, size);
        }
    };
}