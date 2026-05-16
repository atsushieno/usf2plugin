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

    // ---------------------------------------------------------------------------
    // Commands sent from non-RT thread to RT thread via lock-free FIFO.
    // ---------------------------------------------------------------------------

    struct SetChannelCommand     { uint8_t channel; tsf* soundfont; };
    struct SetPresetCommand      { uint8_t channel; uint8_t program; };
    struct UpdateOutputCommand   { float sample_rate; float gain; };
    struct CCCommand             { uint8_t channel; uint8_t cc; uint8_t value; };
    struct PitchBendCommand      { uint8_t channel; uint8_t lsb; uint8_t msb; };
    struct ChannelPressureCommand{ uint8_t channel; uint8_t value; };

    using SoundfontCommand = std::variant<
        SetChannelCommand,
        SetPresetCommand,
        UpdateOutputCommand,
        CCCommand,
        PitchBendCommand,
        ChannelPressureCommand
    >;

    class SF2Application {
        // -----------------------------------------------------------------------
        // Non-RT state — only touched outside of process().
        // -----------------------------------------------------------------------
        float sample_rate{44100};
        float global_gain{1.0};
        std::vector<std::unique_ptr<SF2Entry>> sf2_entries{};
        std::vector<tsf*> tsfs{};
        std::string bundle_path{};
        tsf* sf{nullptr};

        // -----------------------------------------------------------------------
        // State snapshot — written by RT thread (relaxed atomics), read by
        // non-RT thread in serializeToSMF().
        // -----------------------------------------------------------------------
        std::atomic<uint8_t>  state_cc[16][128]{};
        std::atomic<uint8_t>  state_program[16]{};
        std::atomic<uint8_t>  state_pressure[16]{};
        // Stored as (msb << 8) | lsb; MIDI center = 0x4000 (lsb=0x00, msb=0x40).
        std::atomic<uint16_t> state_pitchbend[16]{};

        // -----------------------------------------------------------------------
        // RT thread state — only accessed inside process().
        // -----------------------------------------------------------------------
        tsf*    rt_channels[16]{};
        int32_t rt_presets[16]{};

        // Lock-free command queue: non-RT pushes, RT pops.
        choc::fifo::SingleReaderSingleWriterFIFO<SoundfontCommand> commands;

        // -----------------------------------------------------------------------
        // Helpers
        // -----------------------------------------------------------------------
        void applyOutputMode(tsf* t) const {
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
                    rt_presets[c.channel] = c.program;
                    state_program[c.channel].store(c.program, std::memory_order_relaxed);
                    if (rt_channels[c.channel])
                        tsf_channel_set_presetindex(rt_channels[c.channel], c.channel, c.program);

                } else if constexpr (std::is_same_v<T, UpdateOutputCommand>) {
                    tsf* seen[16] = {};
                    int  seen_count = 0;
                    for (auto t : rt_channels) {
                        if (!t) continue;
                        bool already = false;
                        for (int i = 0; i < seen_count; i++)
                            if (seen[i] == t) { already = true; break; }
                        if (!already)
                            seen[seen_count++] = t;
                    }
                    for (int i = 0; i < seen_count; i++)
                        tsf_set_output(seen[i], TSFOutputMode::TSF_STEREO_UNWEAVED,
                                       c.sample_rate, c.gain);

                } else if constexpr (std::is_same_v<T, CCCommand>) {
                    state_cc[c.channel][c.cc].store(c.value, std::memory_order_relaxed);
                    if (rt_channels[c.channel])
                        tsf_channel_midi_control(rt_channels[c.channel], c.channel, c.cc, c.value);

                } else if constexpr (std::is_same_v<T, PitchBendCommand>) {
                    state_pitchbend[c.channel].store(
                        (uint16_t)((c.msb << 8) | c.lsb), std::memory_order_relaxed);
                    if (rt_channels[c.channel])
                        tsf_channel_set_pitchwheel(rt_channels[c.channel], c.channel,
                                                   c.lsb * 0x80 + c.msb);

                } else if constexpr (std::is_same_v<T, ChannelPressureCommand>) {
                    // Track state; tsf has no dedicated channel pressure API.
                    state_pressure[c.channel].store(c.value, std::memory_order_relaxed);
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
            // 4096 slots: enough for 16 init commands + full state restore (≤ 2096) + margin.
            commands.reset(4096);

            // MIDI pitch bend center is 0x4000 ((msb=0x40)<<8 | lsb=0x00).
            for (int ch = 0; ch < 16; ch++)
                state_pitchbend[ch].store(0x4000, std::memory_order_relaxed);

            gugs(); // blocking file I/O — safe here, not on RT thread
            for (int ch = 0; ch < 16; ch++)
                commands.push(SetChannelCommand{(uint8_t)ch, sf});
        }

        float sampleRate() const { return sample_rate; }

        // Called from non-RT context (sampleRateChanged).
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
            setProgram(0, (uint8_t)index);
        }

        // Called from non-RT thread (setParameterValue / setState replay).
        void setCC(uint8_t channel, uint8_t cc, uint8_t value) {
            state_cc[channel][cc].store(value, std::memory_order_relaxed);
            commands.push(CCCommand{channel, cc, value});
        }

        void setPressure(uint8_t channel, uint8_t value) {
            state_pressure[channel].store(value, std::memory_order_relaxed);
            commands.push(ChannelPressureCommand{channel, value});
        }

        void setProgram(uint8_t channel, uint8_t program) {
            state_program[channel].store(program, std::memory_order_relaxed);
            commands.push(SetPresetCommand{channel, program});
        }

        // raw: 14-bit MIDI value 0–16383, center = 8192.
        void setPitchBend(uint8_t channel, uint16_t raw) {
            uint8_t lsb = raw & 0x7F;
            uint8_t msb = (raw >> 7) & 0x7F;
            state_pitchbend[channel].store((uint16_t)((msb << 8) | lsb),
                                           std::memory_order_relaxed);
            commands.push(PitchBendCommand{channel, lsb, msb});
        }

        // -----------------------------------------------------------------------
        // State serialization — called from non-RT thread.
        // -----------------------------------------------------------------------

        // Returns a Type-0 SMF encoding the current MIDI controller / program state.
        std::vector<uint8_t> serializeToSMF() const {
            std::vector<uint8_t> track;
            auto emit = [&](std::initializer_list<uint8_t> bytes) {
                track.insert(track.end(), bytes);
            };

            for (int ch = 0; ch < 16; ch++) {
                emit({0x00, (uint8_t)(0xC0 | ch),
                      state_program[ch].load(std::memory_order_relaxed)});

                uint16_t pb = state_pitchbend[ch].load(std::memory_order_relaxed);
                emit({0x00, (uint8_t)(0xE0 | ch),
                      (uint8_t)(pb & 0xFF), (uint8_t)(pb >> 8)});

                emit({0x00, (uint8_t)(0xD0 | ch),
                      state_pressure[ch].load(std::memory_order_relaxed)});

                for (int cc = 0; cc < 128; cc++)
                    emit({0x00, (uint8_t)(0xB0 | ch), (uint8_t)cc,
                          state_cc[ch][cc].load(std::memory_order_relaxed)});
            }

            emit({0x00, 0xFF, 0x2F, 0x00}); // End of Track

            std::vector<uint8_t> smf;
            auto push4cc = [&](const char* s) { smf.insert(smf.end(), s, s + 4); };
            auto push32  = [&](uint32_t v) {
                smf.push_back(v >> 24);
                smf.push_back((v >> 16) & 0xFF);
                smf.push_back((v >>  8) & 0xFF);
                smf.push_back( v        & 0xFF);
            };
            auto push16  = [&](uint16_t v) {
                smf.push_back(v >> 8);
                smf.push_back(v & 0xFF);
            };

            push4cc("MThd"); push32(6);
            push16(0); push16(1); push16(480); // format 0, 1 track, 480 ticks/beat

            push4cc("MTrk"); push32((uint32_t)track.size());
            smf.insert(smf.end(), track.begin(), track.end());

            return smf;
        }

        // Parses a Type-0 SMF and replays its events into the command queue.
        // Called from non-RT thread (setState).
        bool deserializeFromSMF(const std::vector<uint8_t>& smf) {
            if (smf.size() < 14) return false;
            if (smf[0] != 'M' || smf[1] != 'T' || smf[2] != 'h' || smf[3] != 'd')
                return false;

            size_t pos = 14; // skip MThd (4) + length (4) + format (2) + ntracks (2) + division (2)

            auto readVLQ = [&]() -> uint32_t {
                uint32_t val = 0;
                while (pos < smf.size()) {
                    uint8_t b = smf[pos++];
                    val = (val << 7) | (b & 0x7F);
                    if (!(b & 0x80)) break;
                }
                return val;
            };

            while (pos + 8 <= smf.size()) {
                bool is_track = smf[pos]=='M' && smf[pos+1]=='T'
                             && smf[pos+2]=='r' && smf[pos+3]=='k';
                uint32_t chunk_len = ((uint32_t)smf[pos+4] << 24)
                                   | ((uint32_t)smf[pos+5] << 16)
                                   | ((uint32_t)smf[pos+6] <<  8)
                                   |  (uint32_t)smf[pos+7];
                pos += 8;

                if (!is_track) { pos += chunk_len; continue; }

                size_t track_end = pos + chunk_len;
                uint8_t running_status = 0;

                while (pos < track_end) {
                    readVLQ(); // delta time (ignored — we restore state, not timing)

                    if (pos >= track_end) break;

                    uint8_t b = smf[pos];
                    uint8_t status;
                    if (b & 0x80) {
                        status = b;
                        running_status = (b != 0xFF && (b & 0xF0) != 0xF0) ? b : running_status;
                        pos++;
                    } else {
                        status = running_status; // running status — don't advance
                    }

                    if (status == 0xFF) {
                        if (pos + 1 >= track_end) break;
                        pos++; // meta type
                        uint32_t meta_len = readVLQ();
                        pos += meta_len;
                        continue;
                    }

                    if ((status & 0xF0) == 0xF0) {
                        // SysEx / realtime — skip to 0xF7
                        if (status == 0xF0)
                            while (pos < track_end && smf[pos++] != 0xF7) {}
                        continue;
                    }

                    uint8_t ch   = status & 0x0F;
                    uint8_t type = status & 0xF0;

                    switch (type) {
                        case 0xC0: { // Program Change
                            if (pos >= track_end) break;
                            uint8_t prog = smf[pos++];
                            state_program[ch].store(prog, std::memory_order_relaxed);
                            commands.push(SetPresetCommand{ch, prog});
                            break;
                        }
                        case 0xE0: { // Pitch Bend
                            if (pos + 1 >= track_end) break;
                            uint8_t lsb = smf[pos++];
                            uint8_t msb = smf[pos++];
                            state_pitchbend[ch].store(
                                (uint16_t)((msb << 8) | lsb), std::memory_order_relaxed);
                            commands.push(PitchBendCommand{ch, lsb, msb});
                            break;
                        }
                        case 0xD0: { // Channel Pressure
                            if (pos >= track_end) break;
                            uint8_t pressure = smf[pos++];
                            state_pressure[ch].store(pressure, std::memory_order_relaxed);
                            commands.push(ChannelPressureCommand{ch, pressure});
                            break;
                        }
                        case 0xB0: { // Control Change
                            if (pos + 1 >= track_end) break;
                            uint8_t cc  = smf[pos++];
                            uint8_t val = smf[pos++];
                            state_cc[ch][cc].store(val, std::memory_order_relaxed);
                            commands.push(CCCommand{ch, cc, val});
                            break;
                        }
                        case 0x80: pos += 2; break; // Note Off  — skip
                        case 0x90: pos += 2; break; // Note On   — skip
                        case 0xA0: pos += 2; break; // Poly AT   — skip
                        default:   pos += 1; break; // unknown   — best effort
                    }
                }

                break; // only need the first (and only) track
            }

            return true;
        }

        // -----------------------------------------------------------------------
        // Audio thread entry point.
        // -----------------------------------------------------------------------

        void process(float* outputL, float* outputR, uint32_t size,
                     const MidiEvent* midiEvents, uint32_t midiEventCount) {
            // Drain the command queue — lock-free, no allocation.
            SoundfontCommand cmd;
            while (commands.pop(cmd))
                applyCommand(cmd);

            for (uint32_t i = 0; i < midiEventCount; i++) {
                auto& e = midiEvents[i];
                auto ch = e.data[0] & 0x0F;
                auto t  = rt_channels[ch];

                switch (e.data[0] & 0xF0) {
                    case 0x80:
                        if (t) tsf_note_off(t, rt_presets[ch], e.data[1]);
                        break;
                    case 0x90:
                        if (t) tsf_note_on(t, rt_presets[ch], e.data[1], e.data[2] / 127.0f);
                        break;
                    case 0xB0:
                        state_cc[ch][e.data[1]].store(e.data[2], std::memory_order_relaxed);
                        if (t) tsf_channel_midi_control(t, ch, e.data[1], e.data[2]);
                        break;
                    case 0xC0:
                        rt_presets[ch] = e.data[1];
                        state_program[ch].store(e.data[1], std::memory_order_relaxed);
                        // FIXME: drums
                        if (t) tsf_channel_set_bank_preset(t, ch, e.data[1], 0);
                        break;
                    case 0xD0:
                        // Track state; tsf has no dedicated channel pressure API.
                        state_pressure[ch].store(e.data[1], std::memory_order_relaxed);
                        break;
                    case 0xE0: {
                        state_pitchbend[ch].store(
                            (uint16_t)((e.data[2] << 8) | e.data[1]), std::memory_order_relaxed);
                        if (t) tsf_channel_set_pitchwheel(t, ch,
                                                           e.data[1] * 0x80 + e.data[2]);
                        break;
                    }
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
