#pragma once

#include "distrho/DistrhoPlugin.hpp"
#include "tsf.h"
#include "containers/choc_SingleReaderSingleWriterFIFO.h"
#include <string>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <variant>

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

    // Commands sent from non-RT thread to RT thread via lock-free FIFO.
    struct SetChannelCommand   { uint8_t channel; tsf* soundfont; };
    struct SetPresetCommand    { uint8_t channel; int32_t preset; };
    struct UpdateOutputCommand { float sample_rate; float gain; };

    using SoundfontCommand = std::variant<SetChannelCommand, SetPresetCommand, UpdateOutputCommand>;

    class SF2Application {
        // Non-RT state — only accessed outside of process().
        float sample_rate{44100};
        float global_gain{1.0};
        std::vector<std::unique_ptr<SF2Entry>> sf2_entries{};
        std::vector<tsf*> tsfs{};
        std::string bundle_path{};
        tsf* sf{nullptr};

        // RT thread state — only accessed inside process().
        tsf* rt_channels[16]{};
        int32_t rt_presets[16]{};

        // Lock-free command queue: non-RT pushes, RT pops.
        choc::fifo::SingleReaderSingleWriterFIFO<SoundfontCommand> commands;

        void applyOutputMode(tsf* t) {
            if (t)
                tsf_set_output(t, TSFOutputMode::TSF_STEREO_UNWEAVED, sample_rate, global_gain);
        }

        // Only called from process() on the RT thread.
        void applyCommand(const SoundfontCommand& cmd) {
            std::visit([this](auto&& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, SetChannelCommand>) {
                    rt_channels[c.channel] = c.soundfont;
                    if (c.soundfont)
                        tsf_channel_set_presetindex(c.soundfont, c.channel, rt_presets[c.channel]);
                } else if constexpr (std::is_same_v<T, SetPresetCommand>) {
                    rt_presets[c.channel] = c.preset;
                    if (rt_channels[c.channel])
                        tsf_channel_set_presetindex(rt_channels[c.channel], c.channel, c.preset);
                } else if constexpr (std::is_same_v<T, UpdateOutputCommand>) {
                    // Apply to each unique active soundfont without allocating.
                    tsf* seen[16] = {};
                    int seen_count = 0;
                    for (auto t : rt_channels) {
                        if (!t) continue;
                        bool already = false;
                        for (int i = 0; i < seen_count; i++)
                            if (seen[i] == t) { already = true; break; }
                        if (!already)
                            seen[seen_count++] = t;
                    }
                    for (int i = 0; i < seen_count; i++)
                        tsf_set_output(seen[i], TSFOutputMode::TSF_STEREO_UNWEAVED, c.sample_rate, c.gain);
                }
            }, cmd);
        }

    public:
        SF2Application() = default;

        // Eagerly loads the default SF2. Must be called from non-RT context.
        tsf* gugs() {
            if (!sf && !bundle_path.empty()) {
                auto path = std::format("{}/{}", bundle_path, "GeneralUser-GS.sf2");
                auto fs = std::ifstream(path);
                std::ostringstream ss;
                ss << fs.rdbuf();
                auto s = ss.str();
                sf = tsf_load_memory(s.data(), s.size());
                applyOutputMode(sf);
            }
            return sf;
        }

        // Called once from plugin constructor (non-RT).
        void initialize(std::string& bundlePath) {
            bundle_path = bundlePath;
            commands.reset(64);
            gugs(); // blocking file I/O — safe here, not on RT thread
            for (int ch = 0; ch < 16; ch++)
                commands.push(SetChannelCommand{(uint8_t)ch, sf});
        }

        float sampleRate() { return sample_rate; }

        // Called from non-RT context (sampleRateChanged, initialize).
        void sampleRate(float newSampleRate) {
            sample_rate = newSampleRate;
            commands.push(UpdateOutputCommand{sample_rate, global_gain});
        }

        std::vector<tsf*>& soundfonts() { return tsfs; }

        // Called from non-RT thread when user loads an SF2 file.
        uint32_t loadSF2(std::filesystem::path& file) {
            for (int32_t i = 0, n = sf2_entries.size(); i < n; i++)
                if (sf2_entries[i] && sf2_entries[i]->path() == file)
                    return i;

            auto entry = std::make_unique<SF2Entry>();
            if (!entry->load(file))
                return -1;
            applyOutputMode(entry->soundfont());
            tsfs.emplace_back(entry->soundfont());
            sf2_entries.emplace_back(std::move(entry));
            return tsfs.size() - 1;
        }

        // Called from non-RT thread (UI callback).
        void mapChannelToSF2(uint8_t channel, int32_t sf2Index) {
            auto t = sf2Index >= 0 ? soundfonts()[sf2Index] : sf;
            commands.push(SetChannelCommand{channel, t});
        }

        // FIXME: it should not be supported like this...
        void programChangeHack(int32_t index) {
            commands.push(SetPresetCommand{0, index});
        }

        // Called from RT thread only.
        void process(float* outputL, float* outputR, uint32_t size,
                     const MidiEvent* midiEvents, uint32_t midiEventCount) {
            // Drain the command queue before processing — no locks, no allocation.
            SoundfontCommand cmd;
            while (commands.pop(cmd))
                applyCommand(cmd);

            for (uint32_t i = 0; i < midiEventCount; i++) {
                auto& e = midiEvents[i];
                auto ch = e.data[0] & 0xF;
                auto t = rt_channels[ch];
                if (!t) continue;
                switch (e.data[0] & 0xF0) {
                    case 0x80:
                        tsf_note_off(t, rt_presets[ch], e.data[1]);
                        break;
                    case 0x90:
                        tsf_note_on(t, rt_presets[ch], e.data[1], e.data[2] / 127.0);
                        break;
                    case 0xB0:
                        tsf_channel_midi_control(t, ch, e.data[1], e.data[2]);
                        break;
                    case 0xC0:
                        // FIXME: drums
                        tsf_channel_set_bank_preset(t, ch, e.data[1], 0);
                        break;
                    case 0xE0:
                        tsf_channel_set_pitchwheel(t, ch, e.data[1] * 0x80 + e.data[2]);
                        break;
                }
            }

            // FIXME: channels — currently only renders channel 0's soundfont
            auto render_tsf = rt_channels[0];
            if (render_tsf) {
                tsf_render_float_separate(render_tsf, outputL, outputR, size);
            } else {
                memset(outputL, 0, size * sizeof(float));
                memset(outputR, 0, size * sizeof(float));
            }
        }
    };
}
