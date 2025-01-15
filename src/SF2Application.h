#pragma once

#include "sf2.hpp"
#include <string>
#include <sstream>
#include "GeneralUser-GS.h"

namespace usf2 {

    typedef std::basic_istream<char> usf2stream;

    void setupRIFFStream(RIFF::stream& stream) {
        stream.func_read_ptr = [](void* src, void* dest, size_t size) -> size_t {
            static_cast<usf2stream*>(src)->read((char*)dest, size);
            return static_cast<usf2stream*>(src)->gcount();
        };
        stream.func_skip_ptr = [](void* src, size_t size) -> size_t {
            static_cast<usf2stream*>(src)->ignore(size);
            return static_cast<usf2stream*>(src)->gcount();
        };
        stream.func_getpos_ptr = [](void* src) -> size_t {
            return static_cast<usf2stream*>(src)->tellg();
        };
    }

    class SF2Entry {
        std::filesystem::path sf2_path{};
        std::unique_ptr<std::ifstream> file{};
        RIFF::stream stream{};
        RIFF::RIFF riff{};
        std::unique_ptr<SF2::SoundFont2> sf2{};

    public:
        SF2Entry() = default;

        ~SF2Entry() {
            file->close();
        }

        std::filesystem::path& path() { return sf2_path; }
        SF2::SoundFont2* soundfont() { return sf2.get(); }

        bool load(std::filesystem::path& sf2File) {
            sf2_path = sf2File;
            stream.src = nullptr;
            setupRIFFStream(stream);
            stream.func_read_ptr = [](void* src, void* dest, size_t size) -> size_t {
                static_cast<std::istream*>(src)->read((char*)dest, size);
                return static_cast<std::istream*>(src)->gcount();
            };
            stream.func_skip_ptr = [](void* src, size_t size) -> size_t {
                static_cast<std::istream*>(src)->ignore(size);
                return static_cast<std::istream*>(src)->gcount();
            };
            stream.func_getpos_ptr = [](void* src) -> size_t {
                return static_cast<std::istream*>(src)->tellg();
            };
            stream.func_setpos_ptr = [](void* src, size_t pos) {
                static_cast<std::istream*>(src)->clear();
                static_cast<std::ifstream*>(src)->seekg(pos, static_cast<std::ifstream*>(src)->beg);
            };

            file = std::make_unique<std::ifstream>(sf2File.string(), std::ios::binary);
            if (!file)
                return false;
            stream.src = file.get();
            riff.parse(stream, false);

            sf2 = std::make_unique<SF2::SoundFont2>(&riff, &stream);

            return true;
        }
    };

    SF2::SoundFont2 loadGUGS(RIFF::stream& stream, RIFF::RIFF& riff, int32_t* pos) {
        stream.func_read_ptr = [](void* src, void* dest, size_t size) -> size_t {
            auto pos = *(int32_t*) src;
            memcpy(dest, GeneralUser_GS_GeneralUser_GS_sf2() + pos, size);
            *(int32_t*) src += size;
            return GS_GeneralUser_GS_sf2_len - pos;
        };
        stream.func_skip_ptr = [](void* src, size_t size) -> size_t {
            auto pos = *(int32_t*) src;
            *(int32_t*) src += size;
            return GS_GeneralUser_GS_sf2_len - pos;
        };
        stream.func_getpos_ptr = [](void* src) -> size_t {
            return static_cast<size_t>(*(int32_t*) src);
        };
        stream.func_setpos_ptr = [](void* src, size_t pos) {
            *(int32_t*) src = (int32_t) pos;
        };
        stream.src = pos;
        riff.parse(stream, false);
        SF2::SoundFont2 sf{&riff, &stream};
        return std::move(sf);
    }

    class SF2Application {
        float sample_rate{44100};
        std::vector<std::unique_ptr<SF2Entry>> sf2_entries{16};
        std::vector<SF2::SoundFont2*> sf2s{16};
        SF2::SoundFont2::Channel channels[16];

    public:
        static SF2::SoundFont2* gugs() {
            static RIFF::stream stream{};
            static int32_t pos;
            static RIFF::RIFF riff{};
            static SF2::SoundFont2 sf = loadGUGS(stream, riff, &pos);
            return &sf;
        }

        SF2Application() = default;

        void initialize() {
            for (auto & ch : channels) {
                ch.sf = gugs(); // It is used as the default SF2 when no channel-sf2 mapping is defined.
                ch.SetPreset(48, 0);
            }
        }

        float sampleRate() { return sample_rate; }
        void sampleRate(float newSampleRate) {
            sample_rate = newSampleRate;
        }

        std::vector<SF2::SoundFont2*>& soundfonts() { return sf2s; }

        uint32_t loadSF2(std::filesystem::path& file) {
            for (int32_t i = 0, n = sf2_entries.size(); i < n; i++)
                if (sf2_entries[i]->path() == file)
                    return i;

            auto entry = std::make_unique<SF2Entry>();
            if (!entry->load(file))
                return -1;
            sf2s.emplace_back(entry->soundfont());
            sf2_entries.emplace_back(std::move(entry));
            return sf2s.size() - 1;
        }

        void mapChannelToSF2(uint8_t channel, int32_t sf2Index) {
            channels[channel].sf = sf2Index >= 0 ? soundfonts()[sf2Index] : gugs();
        }

        void process(float* output_L, float* output_R, uint32_t size,
                     const MidiEvent* midiEvents, uint32_t midiEventCount) {

            // process MIDI events
            for (uint32_t i = 0; i < midiEventCount; i++) {
                auto &e = midiEvents[i];
                auto ch = e.data[0] & 0xF;
                switch (e.data[0] & 0xF0) {
                    case 0x80:
                        channels[ch].NoteOff(e.data[1]);
                        break;
                    case 0x90:
                        channels[ch].NoteOn(e.data[1], e.data[2], sample_rate);
                        break;
                    case 0xC0:
                        channels[ch].SetPreset(e.data[1]);
                        break;
                }
            }

            channels[0].Render(output_L, output_R, size, sample_rate);
        }
    };
}