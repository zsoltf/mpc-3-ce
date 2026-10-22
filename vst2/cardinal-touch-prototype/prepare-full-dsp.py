#!/usr/bin/env python3
"""Prepare pinned, measured DSP changes without changing patch formats or rates."""
import argparse
import hashlib
from pathlib import Path

CHANGES = [
    (
        'plugins/AudibleInstruments/src/Ripples/ripples.hpp',
        '316a16f37a628a193fca68b31cb51a8844fc9f503021d868d376a1d1541b6d4b',
        'b939ee71b50138c864782ae367d61f0eb14efed1260f07477c8da3c7fec6241a',
        [('protected:\n', 'protected:\n    friend class RipplesVoices;\n')],
    ),
    (
        'plugins/AudibleInstruments/src/Ripples.cpp',
        'efceae691639f47f14a5c320a037edb57b0f2dfdae8c9195c78527c0eb660e9d',
        'de8c0d3975b55d7dc464537833d772a385e13ec08efff45f27223eba9659ae53',
        [
            ('#include "Ripples/ripples.hpp"',
             '#include "Ripples/voice-engine.hpp"'),
            ("""ripples::RipplesEngine engines[16];""",
             """ripples::RipplesVoices engines[4];"""),
            ("""for (int c = 0; c < 16; c++) {""",
             """for (int c = 0; c < 4; c++) {"""),
            ("""		for (int c = 0; c < channels; c++) {
			frame.res_cv = inputs[RES_INPUT].getPolyVoltage(c);
			frame.freq_cv = inputs[FREQ_INPUT].getPolyVoltage(c);
			frame.fm_cv = inputs[FM_INPUT].getPolyVoltage(c);
			frame.input = inputs[IN_INPUT].getVoltage(c);
			frame.gain_cv = inputs[GAIN_INPUT].getPolyVoltage(c);

			engines[c].process(frame);

			outputs[BP2_OUTPUT].setVoltage(frame.bp2, c);
			outputs[LP2_OUTPUT].setVoltage(frame.lp2, c);
			outputs[LP4_OUTPUT].setVoltage(frame.lp4, c);
			outputs[LP4VCA_OUTPUT].setVoltage(frame.lp4vca, c);
		}""",
             """		const int fullGroups = channels / 4;
        if (fullGroups) processFullGroups(frame, fullGroups);
        if (channels % 4) {
            ripples::RipplesEngine* scalar = engines[fullGroups].selectScalar();
            // Original one-frame input/process/output loop; no partial frame array.
            for (int c = fullGroups * 4; c < channels; ++c) {
                frame.res_cv = inputs[RES_INPUT].getPolyVoltage(c);
                frame.freq_cv = inputs[FREQ_INPUT].getPolyVoltage(c);
                frame.fm_cv = inputs[FM_INPUT].getPolyVoltage(c);
                frame.input = inputs[IN_INPUT].getVoltage(c);
                frame.gain_cv = inputs[GAIN_INPUT].getPolyVoltage(c);
                scalar[c - fullGroups * 4].process(frame);
                outputs[BP2_OUTPUT].setVoltage(frame.bp2, c);
                outputs[LP2_OUTPUT].setVoltage(frame.lp2, c);
                outputs[LP4_OUTPUT].setVoltage(frame.lp4, c);
                outputs[LP4VCA_OUTPUT].setVoltage(frame.lp4vca, c);
            }
        }"""),
            ("""	void process(const ProcessArgs& args) override {""",
             """	__attribute__((noinline)) void processFullGroups(ripples::RipplesEngine::Frame& frame, int groups) {
        for (int group = 0; group < groups; ++group) {
            ripples::RipplesEngine::Frame voices[4];
            for (int lane = 0; lane < 4; ++lane) {
                const int c = group * 4 + lane;
                voices[lane] = frame;
                voices[lane].res_cv = inputs[RES_INPUT].getPolyVoltage(c);
                voices[lane].freq_cv = inputs[FREQ_INPUT].getPolyVoltage(c);
                voices[lane].fm_cv = inputs[FM_INPUT].getPolyVoltage(c);
                voices[lane].input = inputs[IN_INPUT].getVoltage(c);
                voices[lane].gain_cv = inputs[GAIN_INPUT].getPolyVoltage(c);
            }
            engines[group].process(voices, 4);
            for (int lane = 0; lane < 4; ++lane) {
                const int c = group * 4 + lane;
                outputs[BP2_OUTPUT].setVoltage(voices[lane].bp2, c);
                outputs[LP2_OUTPUT].setVoltage(voices[lane].lp2, c);
                outputs[LP4_OUTPUT].setVoltage(voices[lane].lp4, c);
                outputs[LP4VCA_OUTPUT].setVoltage(voices[lane].lp4vca, c);
            }
        }
    }

	void process(const ProcessArgs& args) override {"""),
        ],
    ),
    (
        'plugins/Cardinal/src/HostMIDI.cpp',
        '25c982898f6b71f5ed75d40cafdf34140bddab340d64d5788f4fa05e9e1ef5e4',
        'aeb615f6121713fe2f3b410653fd875adc5de1d23d80a6cb68ad6cbce70e0e63',
        [
            ("""        int channels;
""",
             """        static constexpr int kMaxMusicalVoices = 4;
        int channels;
"""),
            ("""        int rotateIndex;
""",
             """        int rotateIndex;
        // MIDI member channels remain independent of the bounded musical voice lanes.
        int8_t mpeSourceChannels[kMaxMusicalVoices];
        bool mpeHeld[kMaxMusicalVoices];
        uint8_t mpePressures[16];

        int expressionChannel(int voice) const {
            return mpeSourceChannels[voice] >= 0 ? mpeSourceChannels[voice] : voice;
        }

        int assignMpeChannel(int sourceChannel) {
            for (int c = 0; c < channels; ++c) {
                if (mpeSourceChannels[c] == sourceChannel)
                    return c;
            }
            // Same free-slot/steal order as Rotate; a source channel owns one MPE note.
            for (int i = 0; i < channels; ++i) {
                rotateIndex = (rotateIndex + 1) % channels;
                if (!gates[rotateIndex]) {
                    mpeSourceChannels[rotateIndex] = sourceChannel;
                    return rotateIndex;
                }
            }
            rotateIndex = (rotateIndex + 1) % channels;
            mpeSourceChannels[rotateIndex] = sourceChannel;
            return rotateIndex;
        }
"""),
            ("""                aftertouches[c] = 0;
""",
             """                aftertouches[c] = 0;
                mpePressures[c] = 0;
                if (c < kMaxMusicalVoices) {
                    mpeSourceChannels[c] = -1;
                    mpeHeld[c] = false;
                }
"""),
            ("""            const int wheelChannels = (polyMode == MPE_MODE) ? 16 : 1;
            float pwValues[16] = {};""",
             """            const int sourceChannels = (polyMode == MPE_MODE) ? 16 : 1;
            const int wheelChannels = (polyMode == MPE_MODE) ? channels : 1;
            float pwValues[16] = {};
            float modValues[16] = {};"""),
            ("""            for (int c = 0; c < wheelChannels; c++) {""",
             """            for (int c = 0; c < sourceChannels; c++) {"""),
            ("""                outputs[PITCHBEND_OUTPUT].setVoltage(pw * 5.f, c);
""",
             """"""),
            ("""                outputs[MODWHEEL_OUTPUT].setVoltage(mod * 10.f, c);
            }""",
             """                modValues[c] = mod;
            }
            for (int c = 0; c < wheelChannels; ++c) {
                const int source = polyMode == MPE_MODE ? expressionChannel(c) : 0;
                outputs[PITCHBEND_OUTPUT].setVoltage(pwValues[source] * 5.f, c);
                outputs[MODWHEEL_OUTPUT].setVoltage(modValues[source] * 10.f, c);
            }"""),
            ("""                float pw = pwValues[(polyMode == MPE_MODE) ? c : 0];""",
             """                float pw = pwValues[(polyMode == MPE_MODE) ? expressionChannel(c) : 0];"""),
            ("""                case 0x8: {
                    releaseNote(msg.getNote());""",
             """                case 0x8: {
                    releaseNote(msg.getNote(), msg.getChannel());"""),
            ("""                        releaseNote(msg.getNote());""",
             """                        releaseNote(msg.getNote(), msg.getChannel());"""),
            ("""                    // TODO Should we handle the MPE case differently?
                    for (int c = 0; c < 16; c++) {
                        if (notes[c] == msg.getNote())""",
             """                    for (int c = 0; c < 16; c++) {
                        if (polyMode == MPE_MODE && (c >= channels || mpeSourceChannels[c] != msg.getChannel()))
                            continue;
                        if (notes[c] == msg.getNote())"""),
            ("""                        aftertouches[msg.getChannel()] = msg.getNote();""",
             """                        mpePressures[msg.getChannel()] = msg.getNote();
                        for (int c = 0; c < channels; ++c) {
                            if (mpeSourceChannels[c] == msg.getChannel())
                                aftertouches[c] = msg.getNote();
                        }"""),
            ("""            // Remove existing similar note
            auto it = std::find(heldNotes.begin(), heldNotes.end(), note);
            if (it != heldNotes.end())
                heldNotes.erase(it);
            // Push note
            heldNotes.push_back(note);
            // Determine actual channel
            if (polyMode == MPE_MODE) {
                // Channel is already decided for us
            }
            else {
                *channel = assignChannel(note);
            }""",
             """            if (polyMode == MPE_MODE) {
                const int sourceChannel = *channel;
                *channel = assignMpeChannel(sourceChannel);
                mpeHeld[*channel] = true;
                aftertouches[*channel] = mpePressures[sourceChannel];
            }
            else {
                // Preserve the existing non-MPE note priority and allocation.
                auto it = std::find(heldNotes.begin(), heldNotes.end(), note);
                if (it != heldNotes.end())
                    heldNotes.erase(it);
                heldNotes.push_back(note);
                *channel = assignChannel(note);
            }"""),
            ("""        void releaseNote(uint8_t note) {
""",
             """        void releaseNote(uint8_t note, int sourceChannel) {
            if (polyMode == MPE_MODE) {
                for (int c = 0; c < channels; ++c) {
                    if (mpeSourceChannels[c] != sourceChannel || notes[c] != note)
                        continue;
                    mpeHeld[c] = false;
                    if (!pedal) {
                        gates[c] = false;
                        gatesForceGap[c] = gateForceGaps;
                    }
                }
                return;
            }
"""),
            ("""            pedal = false;
            // Set last note if monophonic""",
             """            pedal = false;
            if (polyMode == MPE_MODE) {
                for (int c = 0; c < channels; ++c) {
                    if (!mpeHeld[c])
                        gates[c] = false;
                }
                return;
            }
            // Set last note if monophonic"""),
            ("""        void setChannels(const int channels)
        {""",
             """        void setChannels(const int64_t requestedChannels)
        {
            const int channels = requestedChannels < 1 ? 1 :
                requestedChannels > kMaxMusicalVoices ? kMaxMusicalVoices : static_cast<int>(requestedChannels);"""),
            ("""            midiInput.polyMode = (MidiInput::PolyMode) json_integer_value(polyModeJ);""",
             """            midiInput.setPolyMode((MidiInput::PolyMode) json_integer_value(polyModeJ));"""),
            ("""            for (int c = 1; c <= 16; c++) {""",
             """            for (int c = 1; c <= HostMIDI::MidiInput::kMaxMusicalVoices; c++) {"""),
            ("""        menu->addChild(createIndexPtrSubmenuItem("Polyphony mode", {
            "Rotate",
            "Reuse",
            "Reset",
            "MPE",
        }, &module->midiInput.polyMode));""",
             """        menu->addChild(createSubmenuItem("Polyphony mode", "", [=](Menu* menu) {
            const char* labels[] = {"Rotate", "Reuse", "Reset", "MPE (4 voices maximum)"};
            for (int mode = 0; mode < HostMIDI::MidiInput::NUM_POLY_MODES; ++mode) {
                menu->addChild(createCheckMenuItem(labels[mode], "",
                    [=]() {return module->midiInput.polyMode == mode;},
                    [=]() {module->midiInput.setPolyMode((HostMIDI::MidiInput::PolyMode) mode);}
                ));
            }
        }));"""),
        ],
    ),
    (
        'plugins/AudibleInstruments/src/Ripples/sos.hpp',
        '810b1ef07b96f332f6cffc23254c5f28b7c24117616265479a7d15c8143c6d86',
        '3cfa2de8d191bc0e9c75daac96eb925cd91f1c18a207f06e33f74b5eee992331',
        [
            ("""        Reset();
    }

    void Init(int num_sections, const SOSCoefficients* sections)
    {
        num_sections_ = num_sections;
        Reset();
        SetCoefficients(sections);
    }
""",
             """        Reset();
    }

    template<int Count>
    void Init(const SOSCoefficients (&sections)[Count])
    {
        static_assert(Count > 0 && Count <= max_num_sections, "SOS coefficient extent");
        num_sections_ = Count;
        Reset();
        SetCoefficients(sections);
    }
"""),
            ("""        {
            x_[n][0] = 0.f;
            x_[n][1] = 0.f;
            x_[n][2] = 0.f;
        }

        x_[num_sections_][0] = 0.f;
        x_[num_sections_][1] = 0.f;
        x_[num_sections_][2] = 0.f;
    }

    void SetCoefficients(const SOSCoefficients* sections)
    {
        for (int n = 0; n < num_sections_; n++)
        {
            sections_[n].b[0] = sections[n].b[0];
            sections_[n].b[1] = sections[n].b[1];
            sections_[n].b[2] = sections[n].b[2];
""",
             """        {
            x_[n][0] = 0.f;
            x_[n][1] = 0.f;
        }

        x_[num_sections_][0] = 0.f;
        x_[num_sections_][1] = 0.f;
    }

    template<int Count>
    void SetCoefficients(const SOSCoefficients (&sections)[Count])
    {
        static_assert(Count > 0 && Count <= max_num_sections, "SOS coefficient extent");
        unitTail_ = true;
        for (int n = 0; n < Count; n++)
        {
            if (n > 0 && (sections[n].b[0] != 1.f || sections[n].b[2] != 1.f)) unitTail_ = false;
            sections_[n].b[0] = sections[n].b[0];
            sections_[n].b[1] = sections[n].b[1];
            sections_[n].b[2] = sections[n].b[2];
"""),
            ("""
    T Process(T in)
    {
        for (int n = 0; n < num_sections_; n++)
        {
            // Shift x state
            x_[n][2] = x_[n][1];
            x_[n][1] = x_[n][0];
            x_[n][0] = in;

            T out = 0.f;

            // Add x state
            out += sections_[n].b[0] * x_[n][0];
            out += sections_[n].b[1] * x_[n][1];
            out += sections_[n].b[2] * x_[n][2];

            // Subtract y state
            out -= sections_[n].a[0] * x_[n+1][0];
""",
             """
    T Process(T in)
    {
        if (!unitTail_) return ProcessGeneric(in);
        if (num_sections_ == 7) return ProcessKnown<7>(in);
        if (num_sections_ == 4) return ProcessKnown<4>(in);
        return ProcessGeneric(in);
    }

    template<int Count>
    T ProcessKnown(T in)
    {
        for (int n = 0; n < Count; n++)
        {
            const T previous = x_[n][0];
            const T previous2 = x_[n][1];
            x_[n][1] = previous;
            x_[n][0] = in;

            T out = 0.f;

            // Add x state
            out += (n == 0 ? sections_[n].b[0] * x_[n][0] : x_[n][0]);
            out += sections_[n].b[1] * previous;
            out += (n == 0 ? sections_[n].b[2] * previous2 : previous2);

            // Subtract y state
            out -= sections_[n].a[0] * x_[n+1][0];
"""),
            ("""        }

        // Shift final section x state
        x_[num_sections_][2] = x_[num_sections_][1];
        x_[num_sections_][1] = x_[num_sections_][0];
        x_[num_sections_][0] = in;

""",
             """        }

        // Shift final section x state
        x_[Count][1] = x_[Count][0];
        x_[Count][0] = in;

        return in;
    }

    T ProcessGeneric(T in)
    {
        for (int n = 0; n < num_sections_; n++)
        {
            const T previous = x_[n][0];
            const T previous2 = x_[n][1];
            x_[n][1] = previous;
            x_[n][0] = in;

            T out = 0.f;

            // Add x state
            out += sections_[n].b[0] * x_[n][0];
            out += sections_[n].b[1] * previous;
            out += sections_[n].b[2] * previous2;

            // Subtract y state
            out -= sections_[n].a[0] * x_[n+1][0];
            out -= sections_[n].a[1] * x_[n+1][1];
            in = out;
        }

        // Shift final section x state
        x_[num_sections_][1] = x_[num_sections_][0];
        x_[num_sections_][0] = in;

"""),
            ("""    }

protected:
    int num_sections_;
    SOSCoefficients sections_[max_num_sections];
    T x_[max_num_sections + 1][3];
};

}
""",
             """    }

protected:
    bool unitTail_ = false;
    int num_sections_;
    SOSCoefficients sections_[max_num_sections];
    T x_[max_num_sections + 1][2];
};

}
"""),
            ('protected:\n', 'protected:\n    friend class RipplesVoices;\n'),
        ],
    ),
    (
        'plugins/AudibleInstruments/src/Ripples/aafilter.hpp',
        '1bbb8c07e3cfb67de405c2bc4b280df7b97e449cd7cf7844dce58b1948fa8dd6',
        '140406a9b5041b688733689dc06d3045bb82c48a5603360f57359e9c6cbbec6f',
        [
            ("""                a = ''.join(['{:.8e},'.format(c).ljust(17) for c in sec[4:]])
                cog.outl('        { {' + b + '}, {' + a + '} },')
            cog.outl('    };')
            cog.outl('    up_filter_.Init({}, {});'.format(num_sections, name))
            cog.outl('    down_filter_.Init({}, {});'.format(num_sections, name))
            cog.outl('    oversampling_factor_ = {};'.format(factor))
            cog.outl('}')
        cog.outl('else {{ InitFilter({}); }}'.format(*cascades[0]))
""",
             """                a = ''.join(['{:.8e},'.format(c).ljust(17) for c in sec[4:]])
                cog.outl('        { {' + b + '}, {' + a + '} },')
            cog.outl('    };')
            cog.outl('    up_filter_.Init({});'.format(name))
            cog.outl('    down_filter_.Init({});'.format(name))
            cog.outl('    oversampling_factor_ = {};'.format(factor))
            cog.outl('}')
        cog.outl('else {{ InitFilter({}); }}'.format(*cascades[0]))
"""),
            ("""            {
                { {1.83197956e-02,  3.66063440e-02,  1.83197956e-02,  }, {-1.60702602e+00, 6.80271956e-01,  } },
            };
            up_filter_.Init(1, kFilter768000x1);
            down_filter_.Init(1, kFilter768000x1);
            oversampling_factor_ = 1;
        }
        else if (705600 <= sample_rate)
""",
             """            {
                { {1.83197956e-02,  3.66063440e-02,  1.83197956e-02,  }, {-1.60702602e+00, 6.80271956e-01,  } },
            };
            up_filter_.Init(kFilter768000x1);
            down_filter_.Init(kFilter768000x1);
            oversampling_factor_ = 1;
        }
        else if (705600 <= sample_rate)
"""),
            ("""            {
                { {2.13438638e-02,  4.26550556e-02,  2.13438638e-02,  }, {-1.57253460e+00, 6.57877382e-01,  } },
            };
            up_filter_.Init(1, kFilter705600x1);
            down_filter_.Init(1, kFilter705600x1);
            oversampling_factor_ = 1;
        }
        else if (384000 <= sample_rate)
""",
             """            {
                { {2.13438638e-02,  4.26550556e-02,  2.13438638e-02,  }, {-1.57253460e+00, 6.57877382e-01,  } },
            };
            up_filter_.Init(kFilter705600x1);
            down_filter_.Init(kFilter705600x1);
            oversampling_factor_ = 1;
        }
        else if (384000 <= sample_rate)
"""),
            ("""            {
                { {6.09620331e-02,  1.21896769e-01,  6.09620331e-02,  }, {-1.22760212e+00, 4.71422957e-01,  } },
            };
            up_filter_.Init(1, kFilter384000x1);
            down_filter_.Init(1, kFilter384000x1);
            oversampling_factor_ = 1;
        }
        else if (352800 <= sample_rate)
""",
             """            {
                { {6.09620331e-02,  1.21896769e-01,  6.09620331e-02,  }, {-1.22760212e+00, 4.71422957e-01,  } },
            };
            up_filter_.Init(kFilter384000x1);
            down_filter_.Init(kFilter384000x1);
            oversampling_factor_ = 1;
        }
        else if (352800 <= sample_rate)
"""),
            ("""            {
                { {6.99874107e-02,  1.39948456e-01,  6.99874107e-02,  }, {-1.16347041e+00, 4.43393682e-01,  } },
            };
            up_filter_.Init(1, kFilter352800x1);
            down_filter_.Init(1, kFilter352800x1);
            oversampling_factor_ = 1;
        }
        else if (192000 <= sample_rate)
""",
             """            {
                { {6.99874107e-02,  1.39948456e-01,  6.99874107e-02,  }, {-1.16347041e+00, 4.43393682e-01,  } },
            };
            up_filter_.Init(kFilter352800x1);
            down_filter_.Init(kFilter352800x1);
            oversampling_factor_ = 1;
        }
        else if (192000 <= sample_rate)
"""),
            ("""            {
                { {1.74603587e-01,  3.49188678e-01,  1.74603587e-01,  }, {-5.65216145e-01, 2.63611998e-01,  } },
            };
            up_filter_.Init(1, kFilter192000x1);
            down_filter_.Init(1, kFilter192000x1);
            oversampling_factor_ = 1;
        }
        else if (176400 <= sample_rate)
""",
             """            {
                { {1.74603587e-01,  3.49188678e-01,  1.74603587e-01,  }, {-5.65216145e-01, 2.63611998e-01,  } },
            };
            up_filter_.Init(kFilter192000x1);
            down_filter_.Init(kFilter192000x1);
            oversampling_factor_ = 1;
        }
        else if (176400 <= sample_rate)
"""),
            ("""            {
                { {1.95938020e-01,  3.91858763e-01,  1.95938020e-01,  }, {-4.62313019e-01, 2.46047822e-01,  } },
            };
            up_filter_.Init(1, kFilter176400x1);
            down_filter_.Init(1, kFilter176400x1);
            oversampling_factor_ = 1;
        }
        else if (96000 <= sample_rate)
""",
             """            {
                { {1.95938020e-01,  3.91858763e-01,  1.95938020e-01,  }, {-4.62313019e-01, 2.46047822e-01,  } },
            };
            up_filter_.Init(kFilter176400x1);
            down_filter_.Init(kFilter176400x1);
            oversampling_factor_ = 1;
        }
        else if (96000 <= sample_rate)
"""),
            ("""                { {1.00000000e+00,  -7.04269454e-01, 1.00000000e+00,  }, {-1.49925562e+00, 8.20191196e-01,  } },
                { {1.00000000e+00,  -9.36222412e-01, 1.00000000e+00,  }, {-1.51854586e+00, 9.39911675e-01,  } },
            };
            up_filter_.Init(4, kFilter96000x2);
            down_filter_.Init(4, kFilter96000x2);
            oversampling_factor_ = 2;
        }
        else if (88200 <= sample_rate)
""",
             """                { {1.00000000e+00,  -7.04269454e-01, 1.00000000e+00,  }, {-1.49925562e+00, 8.20191196e-01,  } },
                { {1.00000000e+00,  -9.36222412e-01, 1.00000000e+00,  }, {-1.51854586e+00, 9.39911675e-01,  } },
            };
            up_filter_.Init(kFilter96000x2);
            down_filter_.Init(kFilter96000x2);
            oversampling_factor_ = 2;
        }
        else if (88200 <= sample_rate)
"""),
            ("""                { {1.00000000e+00,  -5.38705333e-01, 1.00000000e+00,  }, {-1.43146550e+00, 8.07687680e-01,  } },
                { {1.00000000e+00,  -7.87002288e-01, 1.00000000e+00,  }, {-1.44140131e+00, 9.35689662e-01,  } },
            };
            up_filter_.Init(4, kFilter88200x2);
            down_filter_.Init(4, kFilter88200x2);
            oversampling_factor_ = 2;
        }
        else if (48000 <= sample_rate)
""",
             """                { {1.00000000e+00,  -5.38705333e-01, 1.00000000e+00,  }, {-1.43146550e+00, 8.07687680e-01,  } },
                { {1.00000000e+00,  -7.87002288e-01, 1.00000000e+00,  }, {-1.44140131e+00, 9.35689662e-01,  } },
            };
            up_filter_.Init(kFilter88200x2);
            down_filter_.Init(kFilter88200x2);
            oversampling_factor_ = 2;
        }
        else if (48000 <= sample_rate)
"""),
            ("""                { {1.00000000e+00,  -1.04177534e+00, 1.00000000e+00,  }, {-1.26951947e+00, 9.34679234e-01,  } },
                { {1.00000000e+00,  -1.09276235e+00, 1.00000000e+00,  }, {-1.26454687e+00, 9.80322986e-01,  } },
            };
            up_filter_.Init(6, kFilter48000x3);
            down_filter_.Init(6, kFilter48000x3);
            oversampling_factor_ = 3;
        }
        else if (44100 <= sample_rate)
""",
             """                { {1.00000000e+00,  -1.04177534e+00, 1.00000000e+00,  }, {-1.26951947e+00, 9.34679234e-01,  } },
                { {1.00000000e+00,  -1.09276235e+00, 1.00000000e+00,  }, {-1.26454687e+00, 9.80322986e-01,  } },
            };
            up_filter_.Init(kFilter48000x3);
            down_filter_.Init(kFilter48000x3);
            oversampling_factor_ = 3;
        }
        else if (44100 <= sample_rate)
"""),
            ("""                { {1.00000000e+00,  -1.03102512e+00, 1.00000000e+00,  }, {-1.15782377e+00, 9.63657309e-01,  } },
                { {1.00000000e+00,  -1.05757483e+00, 1.00000000e+00,  }, {-1.15253824e+00, 9.89272846e-01,  } },
            };
            up_filter_.Init(7, kFilter44100x3);
            down_filter_.Init(7, kFilter44100x3);
            oversampling_factor_ = 3;
        }
        else if (24000 <= sample_rate)
""",
             """                { {1.00000000e+00,  -1.03102512e+00, 1.00000000e+00,  }, {-1.15782377e+00, 9.63657309e-01,  } },
                { {1.00000000e+00,  -1.05757483e+00, 1.00000000e+00,  }, {-1.15253824e+00, 9.89272846e-01,  } },
            };
            up_filter_.Init(kFilter44100x3);
            down_filter_.Init(kFilter44100x3);
            oversampling_factor_ = 3;
        }
        else if (24000 <= sample_rate)
"""),
            ("""                { {1.00000000e+00,  3.23593812e-01,  1.00000000e+00,  }, {-9.84074266e-01, 7.48267989e-01,  } },
                { {1.00000000e+00,  4.69137219e-02,  1.00000000e+00,  }, {-9.17508757e-01, 9.16260523e-01,  } },
            };
            up_filter_.Init(4, kFilter24000x5);
            down_filter_.Init(4, kFilter24000x5);
            oversampling_factor_ = 5;
        }
        else if (22050 <= sample_rate)
""",
             """                { {1.00000000e+00,  3.23593812e-01,  1.00000000e+00,  }, {-9.84074266e-01, 7.48267989e-01,  } },
                { {1.00000000e+00,  4.69137219e-02,  1.00000000e+00,  }, {-9.17508757e-01, 9.16260523e-01,  } },
            };
            up_filter_.Init(kFilter24000x5);
            down_filter_.Init(kFilter24000x5);
            oversampling_factor_ = 5;
        }
        else if (22050 <= sample_rate)
"""),
            ("""                { {1.00000000e+00,  9.46030879e-02,  1.00000000e+00,  }, {-1.12297856e+00, 7.63193697e-01,  } },
                { {1.00000000e+00,  -1.84341946e-01, 1.00000000e+00,  }, {-1.08165394e+00, 9.20980215e-01,  } },
            };
            up_filter_.Init(4, kFilter22050x6);
            down_filter_.Init(4, kFilter22050x6);
            oversampling_factor_ = 6;
        }
        else if (12000 <= sample_rate)
""",
             """                { {1.00000000e+00,  9.46030879e-02,  1.00000000e+00,  }, {-1.12297856e+00, 7.63193697e-01,  } },
                { {1.00000000e+00,  -1.84341946e-01, 1.00000000e+00,  }, {-1.08165394e+00, 9.20980215e-01,  } },
            };
            up_filter_.Init(kFilter22050x6);
            down_filter_.Init(kFilter22050x6);
            oversampling_factor_ = 6;
        }
        else if (12000 <= sample_rate)
"""),
            ("""                { {1.00000000e+00,  1.42136933e+00,  1.00000000e+00,  }, {-9.55595652e-01, 5.55195466e-01,  } },
                { {1.00000000e+00,  1.05842861e+00,  1.00000000e+00,  }, {-8.35474882e-01, 8.34840828e-01,  } },
            };
            up_filter_.Init(3, kFilter12000x10);
            down_filter_.Init(3, kFilter12000x10);
            oversampling_factor_ = 10;
        }
        else if (11025 <= sample_rate)
""",
             """                { {1.00000000e+00,  1.42136933e+00,  1.00000000e+00,  }, {-9.55595652e-01, 5.55195466e-01,  } },
                { {1.00000000e+00,  1.05842861e+00,  1.00000000e+00,  }, {-8.35474882e-01, 8.34840828e-01,  } },
            };
            up_filter_.Init(kFilter12000x10);
            down_filter_.Init(kFilter12000x10);
            oversampling_factor_ = 10;
        }
        else if (11025 <= sample_rate)
"""),
            ("""                { {1.00000000e+00,  1.40863044e+00,  1.00000000e+00,  }, {-9.69538649e-01, 5.57917370e-01,  } },
                { {1.00000000e+00,  1.03994151e+00,  1.00000000e+00,  }, {-8.54328717e-01, 8.35728285e-01,  } },
            };
            up_filter_.Init(3, kFilter11025x11);
            down_filter_.Init(3, kFilter11025x11);
            oversampling_factor_ = 11;
        }
        else if (8000 <= sample_rate)
""",
             """                { {1.00000000e+00,  1.40863044e+00,  1.00000000e+00,  }, {-9.69538649e-01, 5.57917370e-01,  } },
                { {1.00000000e+00,  1.03994151e+00,  1.00000000e+00,  }, {-8.54328717e-01, 8.35728285e-01,  } },
            };
            up_filter_.Init(kFilter11025x11);
            down_filter_.Init(kFilter11025x11);
            oversampling_factor_ = 11;
        }
        else if (8000 <= sample_rate)
"""),
            ("""                { {1.00000000e+00,  1.42136933e+00,  1.00000000e+00,  }, {-9.55595652e-01, 5.55195466e-01,  } },
                { {1.00000000e+00,  1.05842861e+00,  1.00000000e+00,  }, {-8.35474882e-01, 8.34840828e-01,  } },
            };
            up_filter_.Init(3, kFilter8000x15);
            down_filter_.Init(3, kFilter8000x15);
            oversampling_factor_ = 15;
        }
        else { InitFilter(8000); }
""",
             """                { {1.00000000e+00,  1.42136933e+00,  1.00000000e+00,  }, {-9.55595652e-01, 5.55195466e-01,  } },
                { {1.00000000e+00,  1.05842861e+00,  1.00000000e+00,  }, {-8.35474882e-01, 8.34840828e-01,  } },
            };
            up_filter_.Init(kFilter8000x15);
            down_filter_.Init(kFilter8000x15);
            oversampling_factor_ = 15;
        }
        else { InitFilter(8000); }
"""),
            ('protected:\n', 'protected:\n    friend class RipplesVoices;\n'),
        ],
    ),
    (
        'plugins/ValleyAudio/src/Plateau/Dattorro.hpp',
        'c7f737ecafb9ff6a98a9fcb5799a120b7c37dc88558ae1eeb0c9fa6772972e2e',
        '9b453a7495530012d321f4376b53ea7a19cb0f290943074e9e2312f2d0f69984',
        [
            ("""    double maxTimeScale = 1.0;
    double timeScale = 1.0;

    double modDepth = 0.0;
    double decayParam = 0.0;
    double decay = 0.0;
""",
             """    double maxTimeScale = 1.0;
    double timeScale = 1.0;

    bool modShapeValid = false;
    double lastModShape = 0.0;
    double modDepth = 0.0;
    double decayParam = 0.0;
    double decay = 0.0;
"""),
            ("""    double getRightOutput() const;

private:
    double preDelayTime = 0.0;
    static constexpr long kInApf1Time = 141;
    static constexpr long kInApf2Time = 107;
""",
             """    double getRightOutput() const;

private:
    bool InputLowPitchValid = false;
    double lastInputLowPitch = 0.0;
    bool InputHighPitchValid = false;
    double lastInputHighPitch = 0.0;
    bool TankLowPitchValid = false;
    double lastTankLowPitch = 0.0;
    bool TankHighPitchValid = false;
    double lastTankHighPitch = 0.0;
    double preDelayTime = 0.0;
    static constexpr long kInApf1Time = 141;
    static constexpr long kInApf2Time = 107;
"""),
            ("""    double timeScale = 1.0;
""",
             """    double timeScale = 1.0;
    bool delayTimeScaleValid = false;
"""),
            ("""    double preDelayTime = 0.0;
""",
             """    double preDelayTime = 0.0;
    bool preDelayTimeValid = false;
"""),
        ],
    ),
    (
        'plugins/ValleyAudio/src/Plateau/Dattorro.cpp',
        '9376f08104ca8c373983e57184af2232a1ca0cbdacc8207f9b6b7ec1f1918f8b',
        '8debb7095c9af014f8ef5eb5b7d2d6fc59a3527f278aa8b0aa0fce6767a1dd5c',
        [
            ("""}

void Dattorro1997Tank::setModShape(const double shape) {
    lfo1.setRevPoint(shape);
    lfo2.setRevPoint(shape);
    lfo3.setRevPoint(shape);
""",
             """}

void Dattorro1997Tank::setModShape(const double shape) {
    if (modShapeValid && lastModShape == shape) return;
    lastModShape = shape;
    modShapeValid = true;
    lfo1.setRevPoint(shape);
    lfo2.setRevPoint(shape);
    lfo3.setRevPoint(shape);
"""),
            ("""}

void Dattorro::setInputFilterLowCutoffPitch(double pitch) {
    inputLowCut = 440.0 * std::pow(2.0, pitch - 5.0);
}

void Dattorro::setInputFilterHighCutoffPitch(double pitch) {
    inputHighCut = 440.0 * std::pow(2.0, pitch - 5.0);
}

""",
             """}

void Dattorro::setInputFilterLowCutoffPitch(double pitch) {
    if (InputLowPitchValid && lastInputLowPitch == pitch) return;
    lastInputLowPitch = pitch;
    InputLowPitchValid = true;
    inputLowCut = 440.0 * std::pow(2.0, pitch - 5.0);
}

void Dattorro::setInputFilterHighCutoffPitch(double pitch) {
    if (InputHighPitchValid && lastInputHighPitch == pitch) return;
    lastInputHighPitch = pitch;
    InputHighPitchValid = true;
    inputHighCut = 440.0 * std::pow(2.0, pitch - 5.0);
}

"""),
            ("""}

void Dattorro::setTankFilterHighCutFrequency(const double pitch) {
    auto frequency = 440.0 * std::pow(2.0, pitch - 5.0);
    tank.setHighCutFrequency(frequency);
}

void Dattorro::setTankFilterLowCutFrequency(const double pitch) {
    auto frequency = 440.0 * std::pow(2.0, pitch - 5.0);
    tank.setLowCutFrequency(frequency);
}
""",
             """}

void Dattorro::setTankFilterHighCutFrequency(const double pitch) {
    if (TankHighPitchValid && lastTankHighPitch == pitch) return;
    lastTankHighPitch = pitch;
    TankHighPitchValid = true;
    auto frequency = 440.0 * std::pow(2.0, pitch - 5.0);
    tank.setHighCutFrequency(frequency);
}

void Dattorro::setTankFilterLowCutFrequency(const double pitch) {
    if (TankLowPitchValid && lastTankLowPitch == pitch) return;
    lastTankLowPitch = pitch;
    TankLowPitchValid = true;
    auto frequency = 440.0 * std::pow(2.0, pitch - 5.0);
    tank.setLowCutFrequency(frequency);
}
"""),
            ("""#include <algorithm>
""",
             """#include <algorithm>
#include <cstring>

// Compare representations so fast-math cannot merge signed zero or cache NaNs.
static inline bool sameFiniteDelayTime(double a, double b) {
    uint64_t aBits, bBits;
    std::memcpy(&aBits, &a, sizeof(aBits));
    std::memcpy(&bBits, &b, sizeof(bBits));
    return aBits == bBits &&
           (aBits & UINT64_C(0x7ff0000000000000)) != UINT64_C(0x7ff0000000000000);
}
"""),
            ("""    rescaleTapTimes();
    setTimeScale(timeScale);""",
             """    rescaleTapTimes();
    delayTimeScaleValid = false;
    setTimeScale(timeScale);"""),
            ("""    timeScale = newTimeScale;
    timeScale = timeScale < minTimeScale ? minTimeScale : timeScale;

    rescaleApfAndDelayTimes();""",
             """    const double effectiveTimeScale = newTimeScale < minTimeScale ? minTimeScale : newTimeScale;
    if (delayTimeScaleValid && sameFiniteDelayTime(effectiveTimeScale, timeScale)) return;
    timeScale = effectiveTimeScale;
    rescaleApfAndDelayTimes();
    delayTimeScaleValid = true;"""),
            ("""void Dattorro1997Tank::initialiseDelaysAndApfs() {
""",
             """void Dattorro1997Tank::initialiseDelaysAndApfs() {
    // These replacements discard configured delay times; the next setter reapplies them.
    delayTimeScaleValid = false;
"""),
            ("""void Dattorro::setPreDelay(double t) {
    preDelayTime = t;
    preDelay.setDelayTime(preDelayTime * sampleRate);
}""",
             """void Dattorro::setPreDelay(double t) {
    if (preDelayTimeValid && sameFiniteDelayTime(t, preDelayTime)) return;
    preDelayTime = t;
    preDelay.setDelayTime(preDelayTime * sampleRate);
    preDelayTimeValid = true;
}"""),
            ("""    setPreDelay(preDelayTime);
""",
             """    preDelayTimeValid = false;
    setPreDelay(preDelayTime);
"""),
        ],
    ),
    (
        'plugins/ValleyAudio/src/dsp/filters/VecOnePoleFilters.hpp',
        '13429c6bd92aabcc23c89bea603ce8e3c7b57623d8fe5db9d2fae7c38d4bd13b',
        'd480e174f984a4e23a540afd11ec088a8aa3d56274921588ece09c6ef714a242',
        [
            ("""    void setSampleRate(float sampleRate);
    float getMaxCutoffFreq() const;

    float _sampleRate;
    float _1_sampleRate;
    float _cutoffFreq;
""",
             """    void setSampleRate(float sampleRate);
    float getMaxCutoffFreq() const;

    float _altRateSine;
    float _sampleRate;
    float _1_sampleRate;
    float _cutoffFreq;
"""),
        ],
    ),
    (
        'plugins/ValleyAudio/src/dsp/filters/VecOnePoleFilters.cpp',
        '38a7f1c9b2a7da5fccc1609e3872ca5ac54ed08aad53333588d478890cf2bb04',
        '36afd7dafdd2ecdbcf4afdec96464926c8b9560fbf785c0bbdcd1d29207fb31a',
        [
            ("""void VecOnePoleLPFilter::setSampleRate(float sampleRate) {
    _sampleRate = sampleRate;
    _1_sampleRate = 1.f / sampleRate;
    _maxCutoffFreq = sampleRate / 2.f - 2.f;
    setCutoffFreq(_cutoffFreq);
}
""",
             """void VecOnePoleLPFilter::setSampleRate(float sampleRate) {
    _sampleRate = sampleRate;
    _1_sampleRate = 1.f / sampleRate;
    _altRateSine = sin(_2M_PI * _1_sampleRate);
    _maxCutoffFreq = sampleRate / 2.f - 2.f;
    setCutoffFreq(_cutoffFreq);
}
"""),
            ("""
void VecOnePoleLPFilter::setCutoffFreqAlt(const __m128& cutoffFreq) {
    _fc = _mm_clamp_ps(cutoffFreq, _mm_set1_ps(1.f), _mm_set1_ps(_maxCutoffFreq));
    _fc = _mm_mul_ps(_fc, _mm_set1_ps(sin(_2M_PI * _1_sampleRate)));
    _a = valley::_mm_sine_ps(_fc);
    _b = _mm_sub_ps(_mm_set1_ps(1.f), _a);

""",
             """
void VecOnePoleLPFilter::setCutoffFreqAlt(const __m128& cutoffFreq) {
    _fc = _mm_clamp_ps(cutoffFreq, _mm_set1_ps(1.f), _mm_set1_ps(_maxCutoffFreq));
    _fc = _mm_mul_ps(_fc, _mm_set1_ps(_altRateSine));
    _a = valley::_mm_sine_ps(_fc);
    _b = _mm_sub_ps(_mm_set1_ps(1.f), _a);

"""),
        ],
    ),
    (
        'plugins/ValleyAudio/src/dsp/modulation/VecSegment.hpp',
        '31570d67efc2a17a5c09d52984d469785f0741a470976fdc3b3e42346af028da',
        'e173f51efa08ff7160f199c8f6940edda0cb46722a14b34c39643d12c4d99ac7',
        [
            ("""
    __m128 process() {
        output = seg;
        seg = _mm_mul_ps(seg, valley::_mm_power_ps(rate, __1_timeScale));
        return output;
    }

""",
             """
    __m128 process() {
        output = seg;
        if (!powerValid || _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(
                _mm_castps_si128(rate), _mm_castps_si128(previousRate)))) != 15) {
            powerRate = valley::_mm_power_ps(rate, __1_timeScale);
            previousRate = rate;
            powerValid = true;
        }
        seg = _mm_mul_ps(seg, powerRate);
        return output;
    }

"""),
            ("""
    void setTimeScale(const __m128& __tScale) {
        __1_timeScale = _mm_div_ps(ones, __tScale);
    }

    __m128 hasFinished() {
""",
             """
    void setTimeScale(const __m128& __tScale) {
        __1_timeScale = _mm_div_ps(ones, __tScale);
        powerValid = false;
    }

    __m128 hasFinished() {
"""),
            ("""    }
private:
    __m128 __1_timeScale;
};
""",
             """    }
private:
    __m128 __1_timeScale;
    __m128 previousRate, powerRate;
    bool powerValid = false;
};
"""),
        ],
    ),
    (
        'plugins/Bidoo/src/dep/osc/wtOsc.h',
        'b9393051b45135f77d2da34d8bfb4718b2d34281938532aac49af8baa992e25e',
        'ed28b5753def361ef110b7b675b3b5c81624e56c204c834066983ba609544975',
        [
            ("""			}\r
		}\r
\r
		outValue = out(deltaTime, phase, index);\r
\r
    T deltaOut = outValue - lastOutValue;\r
    T outCrossing = -lastOutValue / deltaOut;\r
""",
             """			}\r
		}\r
\r
		alignas(16) T evaluated = out(deltaTime, phase, index);\r
		outValue = evaluated;\r
\r
    T deltaOut = outValue - lastOutValue;\r
    T outCrossing = -lastOutValue / deltaOut;\r
"""),
        ],
    ),
    (
        'plugins/MockbaModular/src/MockbaModular.hpp',
        '3e37a95fe00719ab91c36cd107c3c8727d7477f8ea406974b74bbd3a58187aef',
        '33b5231f3ea70d904a1b98409875f9a57e520f2569e179073c4dd75aee376792',
        [
            ("""		phase += deltaPhase;
		phase -= simd::floor(phase);
		// Process oscillator step
		outValue = oscStep(phase, shape, wave);
	}

	// Override this to define the oscillator waveform - sinewave by default
""",
             """		phase += deltaPhase;
		phase -= simd::floor(phase);
		// Process oscillator step
		alignas(16) float_4 evaluated = oscStep(phase, shape, wave);
		outValue = evaluated;
	}

	// Override this to define the oscillator waveform - sinewave by default
"""),
        ],
    ),
    (
        'plugins/ChowDSP/src/ChowChorus/BBDDelayLine.h',
        '1cd82dc5d6e1470426336739fd25056c1371d8bc73c76a1d3b053b79acd4e43b',
        '82c1a602c8c13f960f7219f5ad52305faf8bc12652cf8c2340aad1f3362600b8',
        [
            ("""#include <algorithm>
#include <array>
#include <memory>
#include <numeric>
#include <vector>

""",
             """#include <algorithm>
#include <array>
#include <memory>
#include <cstring>
#include <numeric>
#include <vector>

"""),
            ("""    std::unique_ptr<InputFilterBank> inputFilter;
    std::unique_ptr<OutputFilterBank> outputFilter;
    float H0 = 1.0f;

    std::array<std::complex<float>, FilterSpec::N_filt> xIn;
    std::array<std::complex<float>, FilterSpec::N_filt> xOut;
""",
             """    std::unique_ptr<InputFilterBank> inputFilter;
    std::unique_ptr<OutputFilterBank> outputFilter;
    float H0 = 1.0f;
    float cachedFilterFrequency = 0.0f;
    bool filterFrequencyValid = false;

    std::array<std::complex<float>, FilterSpec::N_filt> xIn;
    std::array<std::complex<float>, FilterSpec::N_filt> xOut;
"""),
        ],
    ),
    (
        'plugins/ChowDSP/src/ChowChorus/BBDDelayLine.cpp',
        'a99986ff9407ff1a467a14a01497495ced5f0953ec4ceaa108efaa484472199e',
        'fad646b05718e21d49658b1ce97ce302a3a14c6f12c90d908b3ba7c145b36d9c',
        [
            ("""
template <size_t STAGES> void BBDDelayLine<STAGES>::prepare(double sampleRate)
{
    evenOn = true;
    FS = (float)sampleRate;
    Ts = 1.0f / FS;
""",
             """
template <size_t STAGES> void BBDDelayLine<STAGES>::prepare(double sampleRate)
{
    filterFrequencyValid = false;
    evenOn = true;
    FS = (float)sampleRate;
    Ts = 1.0f / FS;
"""),
            ("""
template <size_t STAGES> void BBDDelayLine<STAGES>::setFilterFreq(float freqHz)
{
    inputFilter->set_freq(freqHz);
    inputFilter->set_time(tn);

    outputFilter->set_freq(freqHz);
    outputFilter->set_time(tn);
}

template class BBDDelayLine<64>;
""",
             """
template <size_t STAGES> void BBDDelayLine<STAGES>::setFilterFreq(float freqHz)
{
    const bool changed = !filterFrequencyValid ||
        std::memcmp(&freqHz, &cachedFilterFrequency, sizeof(freqHz)) != 0;
    if (changed) inputFilter->set_freq(freqHz);
    inputFilter->set_time(tn);

    if (changed) outputFilter->set_freq(freqHz);
    outputFilter->set_time(tn);
    cachedFilterFrequency = freqHz;
    filterFrequencyValid = true;
}

template class BBDDelayLine<64>;
"""),
        ],
    ),
    (
        'plugins/ChowDSP/src/ChowChorus/BBDFilterBank.h',
        '07f2e9f9784295d8650641405268590ecd77d449a492b04608968fefe26c939c',
        'ce63516e7aecec69b24948ba3fa9158ad67bdce76429468ef84cb245d0c8151c',
        [
            ("""
inline float vSum(__m128 x)
{
    __m128 a = _mm_add_ps(x, _mm_movehl_ps(x, x));
    a = _mm_add_ss(a, _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 1)));
    float f;
    _mm_store_ss(&f, a);

    return f;
}

struct InputFilterBank
""",
             """
inline float vSum(__m128 x)
{
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE)
    const float32x4_t lanes = simde__m128_to_private(x).neon_f32;
    const float32x2_t pair = vadd_f32(vget_low_f32(lanes), vget_high_f32(lanes));
    return vget_lane_f32(vpadd_f32(pair, pair), 0);
#else
    __m128 a = _mm_add_ps(x, _mm_movehl_ps(x, x));
    a = _mm_add_ss(a, _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 1)));
    float f;
    _mm_store_ss(&f, a);

    return f;
#endif
}

struct InputFilterBank
"""),
        ],
    ),
    (
        'plugins/voxglitch/src/GrainFx/GrainFxCore.hpp',
        'ab053afd9a778c75e23f7ef71220b93ddc5530518803f725d1dbb3b3739224eb',
        '899f6184ef896f4b5b4070a48ec8a46937020aa8732d58d53aa7b6128a5f1a73',
        [
            ("""\r
                grain_array[i].step();\r
\r
                grain_array_tmp[grain_array_tmp_length] = grain_array[i];\r
                grain_array_tmp_length++;\r
            }\r
        }\r
\r
        std::swap(grain_array, grain_array_tmp);\r
        grain_array_length = grain_array_tmp_length;\r
\r
        return {left_mix_output, right_mix_output};\r
""",
             """\r
                grain_array[i].step();\r
\r
                if (grain_array_tmp_length != i)\r
                    grain_array[grain_array_tmp_length] = grain_array[i];\r
                grain_array_tmp_length++;\r
            }\r
        }\r
\r
        grain_array_length = grain_array_tmp_length;\r
\r
        return {left_mix_output, right_mix_output};\r
"""),
        ],
    ),
    (
        'plugins/voxglitch/src/GrainEngineMK2/GrainManager.hpp',
        'ea49ef184962fc5782c8b8eb251888faa845aba4fcc9a2b263d2c117d0d98b82',
        '91550677b273fa1156e566d14986fc78d1a2cee006857946f690d875bda8edeb',
        [
            ("""\r
                grain_array[i].step();\r
\r
                grain_array_tmp[grain_array_tmp_length] = grain_array[i];\r
                grain_array_tmp_length++;\r
            }\r
        }\r
\r
        std::swap(grain_array, grain_array_tmp);\r
        grain_array_length = grain_array_tmp_length;\r
\r
\r
""",
             """\r
                grain_array[i].step();\r
\r
                if (grain_array_tmp_length != i)\r
                    grain_array[grain_array_tmp_length] = grain_array[i];\r
                grain_array_tmp_length++;\r
            }\r
        }\r
\r
        grain_array_length = grain_array_tmp_length;\r
\r
\r
"""),
        ],
    ),
    (
        'plugins/PinkTrombone/src/PinkTrombone/Biquad.cpp',
        '91013ab641093996912c7acd56f0923c5b360a50f9c94019b3718c63f5bc573b',
        'cac10bd0450011e21b06be549073e4244ba57f8aa9da8a2734eb43004d38f17c',
        [
            ("""
#include "Biquad.hpp"
#include <math.h>
#include "../plugin.hpp"

Biquad::Biquad(sample_t sampleRate) : frequency(200),
""",
             """
#include "Biquad.hpp"
#include <math.h>
#include <cstring>
#include "../plugin.hpp"

Biquad::Biquad(sample_t sampleRate) : frequency(200),
"""),
            ("""
void Biquad::setFrequency(sample_t f)
{
	this->frequency = f;
	this->updateCoefficients();
}
""",
             """
void Biquad::setFrequency(sample_t f)
{
	if (std::memcmp(&this->frequency, &f, sizeof(f)) == 0)
		return;
	this->frequency = f;
	this->updateCoefficients();
}
"""),
            ("""
void Biquad::setQ(sample_t q)
{
	this->q = q;
	this->updateCoefficients();
}
""",
             """
void Biquad::setQ(sample_t q)
{
	if (std::memcmp(&this->q, &q, sizeof(q)) == 0)
		return;
	this->q = q;
	this->updateCoefficients();
}
"""),
        ],
    ),
    (
        'plugins/ML_modules/src/TrigSwitch3_2.cpp',
        'c90b257f6dc5e858a06da23eaeaf45eb21601fb887e31a47e18c92864c814331',
        '30aa5c476d247b3c1e591212b29beb8ee035349296578b58efb8380a684a066d',
        [
            ("""	};

	void process(const ProcessArgs &args) override;

	int position=0;

""",
             """	};

	void process(const ProcessArgs &args) override;
	__attribute__((noinline)) void clearOutputs();

	int position=0;

"""),
            ("""		position = 0;
		for(int i=0; i<8; i++) {
			lights[i].value = 0.0;
			memset(out1, 0, sizeof(out1));
			memset(out2, 0, sizeof(out2));
			memset(out3, 0, sizeof(out3));
		};

	};

""",
             """		position = 0;
		for(int i=0; i<8; i++) {
			lights[i].value = 0.0;
		};
		memset(out1, 0, sizeof(out1));
		memset(out2, 0, sizeof(out2));
		memset(out3, 0, sizeof(out3));

	};

"""),
            ("""};


void TrigSwitch3_2::process(const ProcessArgs &args) {

	int channels1 = inputs[CV1_INPUT].getChannels();
""",
             """};


void TrigSwitch3_2::clearOutputs() {
		memset(out1, 0, sizeof(out1));
		memset(out2, 0, sizeof(out2));
		memset(out3, 0, sizeof(out3));
}


void TrigSwitch3_2::process(const ProcessArgs &args) {

	int channels1 = inputs[CV1_INPUT].getChannels();
"""),
            ("""	for(int i=0; i<8; i++) outputs[OUT3_OUTPUT+i].setChannels(channels3);

	if(outMode==ZERO) {\x20
		for(int i=0; i<8; i++) {
			memset(out1, 0, sizeof(out1));
			memset(out2, 0, sizeof(out2));
			memset(out3, 0, sizeof(out3));
		}
	}

	for(int i=0; i<8; i++) {
""",
             """	for(int i=0; i<8; i++) outputs[OUT3_OUTPUT+i].setChannels(channels3);

	if(outMode==ZERO) {\x20
		clearOutputs();
	}

	for(int i=0; i<8; i++) {
"""),
        ],
    ),
    (
        'plugins/Sapphire/src/nucleus_engine.hpp',
        '4f3b4709aa3b159e7b1199155f778c24aceeaf407e70309434dc0169742902cb',
        'ecefaa733820c03459d761c594d21f3ee42bd0ff4538e26fd666edbbec009ebb',
        [
            ("""            for (int i = 0; i+1 < n; ++i)
            {
                Particle& a = array.at(i);
                for (int j = i+1; j < n; ++j)
                {
                    Particle& b = array.at(j);
""",
             """            for (int i = 0; i+1 < n; ++i)
            {
                Particle& a = array.at(i);
                PhysicsVector av = EffectiveVelocity(a.vel, speedLimit);
                for (int j = i+1; j < n; ++j)
                {
                    Particle& b = array.at(j);
"""),
            ("""                        float dist3 = dist2 * dist;

                        // Use effective velocities, not raw velocities, to calculate magnetic cross product.
                        PhysicsVector av = EffectiveVelocity(a.vel, speedLimit);
                        PhysicsVector bv = EffectiveVelocity(b.vel, speedLimit);

                        // Calculate the magnetic component of the mutual force and include it in the vector sum.
""",
             """                        float dist3 = dist2 * dist;

                        // Use effective velocities, not raw velocities, to calculate magnetic cross product.
                        PhysicsVector bv = EffectiveVelocity(b.vel, speedLimit);

                        // Calculate the magnetic component of the mutual force and include it in the vector sum.
"""),
        ],
    ),
    (
        'plugins/ChowDSP/src/ChowTape/tape/ChowTape.cpp',
        'dc4ffa43ecfe7b8d5c8e74dc038049b05a725f15e4f58e3d00d0016b978c5bf0',
        '81e2e5fe5ec840b59c72de962845d7f79cd2f7d73429e76adef8bd54000c58e4',
        [
            ("""            osBuffer[k] = (float) hysteresis.process((double) osBuffer[k]);
        float y = oversample.downsample();

        // process DC blocker
        dcBlocker.setParameters(BiquadFilter::HIGHPASS, 30.0f / args.sampleRate, M_SQRT1_2, 1.0f);

        outputs[AUDIO_OUTPUT].setVoltage(y * 4.18f);
	}

""",
             """            osBuffer[k] = (float) hysteresis.process((double) osBuffer[k]);
        float y = oversample.downsample();

        outputs[AUDIO_OUTPUT].setVoltage(y * 4.18f);
	}

"""),
        ],
    ),
    (
        'plugins/Fundamental/src/VCO.cpp',
        '55f5b9395370a54d7db3a630ae599b4e56f322dd8417dc6aba98a154ee031f75',
        'ce366fdcd6e6476e758ce5451046a3e4edebde1b594b48403ccf253926548f19',
        [
            ("""#include "components.hpp"


""",
             """#include "vco_rounding.hpp"
#include "components.hpp"


"""),
            ("""		}
		phase += deltaPhase;
		// Wrap phase
		phase -= simd::floor(phase);

		// Jump sqr when crossing 0, or 1 if backwards
		T wrapPhase = (syncDirection == -1.f) & 1.f;
""",
             """		}
		phase += deltaPhase;
		// Wrap phase
		phase -= fundamental_vco::floor(phase);

		// Jump sqr when crossing 0, or 1 if backwards
		T wrapPhase = (syncDirection == -1.f) & 1.f;
"""),
            ("""		T v;
		if (analog) {
			T x = phase + 0.25f;
			x -= simd::trunc(x);
			T halfX = (x >= 0.5f);
			x *= 2;
			x -= simd::trunc(x);
			v = expCurve(x) * simd::ifelse(halfX, 1.f, -1.f);
		}
		else {
""",
             """		T v;
		if (analog) {
			T x = phase + 0.25f;
			x -= fundamental_vco::trunc(x);
			T halfX = (x >= 0.5f);
			x *= 2;
			x -= fundamental_vco::trunc(x);
			v = expCurve(x) * simd::ifelse(halfX, 1.f, -1.f);
		}
		else {
"""),
            ("""	T saw(T phase) {
		T v;
		T x = phase + 0.5f;
		x -= simd::trunc(x);
		if (analog) {
			v = -expCurve(x);
		}
""",
             """	T saw(T phase) {
		T v;
		T x = phase + 0.5f;
		x -= fundamental_vco::trunc(x);
		if (analog) {
			v = -expCurve(x);
		}
"""),
        ],
    ),
    (
        'plugins/Sapphire/src/elastika_engine.hpp',
        '05270dd8f1895df3b59397e7b2bd7ed7a71cfd5bfecefe44674e42b9223f9e6d',
        'ad9f8f99a5b5120a1fff3240542e83d3148a8b8620152e65f0703455d3d6ba45',
        [
            ("""#pragma once
#include <algorithm>
#include "elastika_mesh.hpp"

// Sapphire mesh physics engine, by Don Cross <cosinekitty@gmail.com>
""",
             """#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include "elastika_mesh.hpp"

// Sapphire mesh physics engine, by Don Cross <cosinekitty@gmail.com>
"""),
            ("""        float mix;
        AutomaticGainLimiter agc;
        bool enableAgc = false;

    public:
        ElastikaEngine()
""",
             """        float mix;
        AutomaticGainLimiter agc;
        bool enableAgc = false;
        uint32_t frictionBits = 0;
        uint32_t stiffnessBits = 0;
        uint32_t massBits = 0;
        unsigned validParameterCache = 0;

        bool parameterChanged(float value, uint32_t& previousBits, unsigned flag)
        {
            uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            if ((validParameterCache & flag) && bits == previousBits)
                return false;
            previousBits = bits;
            validParameterCache |= flag;
            return true;
        }

    public:
        ElastikaEngine()
"""),
            ("""
        void initialize()
        {
            outputVerifyCounter = 0;

            // Define how stereo inputs go into the mesh.
""",
             """
        void initialize()
        {
            validParameterCache = 0;
            outputVerifyCounter = 0;

            // Define how stereo inputs go into the mesh.
"""),
            ("""
        void setFriction(float slider = 0.5f)
        {
            float x = std::clamp(slider, 0.0f, 1.0f);
            halfLife = TenToPower(-4.5f*x + 1.3f);
        }
""",
             """
        void setFriction(float slider = 0.5f)
        {
            if (!parameterChanged(slider, frictionBits, 1))
                return;
            float x = std::clamp(slider, 0.0f, 1.0f);
            halfLife = TenToPower(-4.5f*x + 1.3f);
        }
"""),
            ("""
        void setStiffness(float slider = 0.5f)
        {
            float x = std::clamp(slider, 0.0f, 1.0f);
            float stiffness = TenToPower(3.4f*x - 0.1f);
            mesh.SetStiffness(stiffness);
""",
             """
        void setStiffness(float slider = 0.5f)
        {
            if (!parameterChanged(slider, stiffnessBits, 2))
                return;
            float x = std::clamp(slider, 0.0f, 1.0f);
            float stiffness = TenToPower(3.4f*x - 0.1f);
            mesh.SetStiffness(stiffness);
"""),
            ("""
        void setMass(float slider = 0.0f)
        {
            const float mass = TenToPower(std::clamp(slider, -1.0f, +1.0f) - 6);
            mesh.SetBallMass(mp.leftVarMassBallIndex, mass);
            mesh.SetBallMass(mp.rightVarMassBallIndex, mass);
""",
             """
        void setMass(float slider = 0.0f)
        {
            if (!parameterChanged(slider, massBits, 4))
                return;
            const float mass = TenToPower(std::clamp(slider, -1.0f, +1.0f) - 6);
            mesh.SetBallMass(mp.leftVarMassBallIndex, mass);
            mesh.SetBallMass(mp.rightVarMassBallIndex, mass);
"""),
        ],
    ),
    (
        'plugins/ValleyAudio/src/Terrorform/Terrorform.hpp',
        'ac6657d535b3ec2da0757545491f106db4caf7de4e890d60e95e5c08dfff93a0',
        'd0feb54d7f587a5e5b6b4e388895a52da89f4235c0bcfdf0d0627fe9f00e4aaa',
        [
            ("""
    float rootPitch;
    float* freqs;
    __m128 __freq;
    float pitchCV1;
    float pitchCV2;
""",
             """
    float rootPitch;
    float* freqs;
    float previousPitch[kMaxNumGroups * 4] = {};
    bool frequencyValid[kMaxNumGroups * 4] = {};
    __m128 __freq;
    float pitchCV1;
    float pitchCV2;
"""),
            ("""    static const int kMaxNumGroups = 4;""",
             """    static const int kMaxNumGroups = 4;
    static constexpr int kMaxMusicalVoices = 4;"""),
        ],
    ),
    (
        'plugins/ValleyAudio/src/Terrorform/Terrorform.cpp',
        '28401e693a9cfe85666a33e99d9da229630c3adc83dde6e7fa4855e33e474130',
        'd60cf0732e2ae35d699cd455924b1020f09a3fa5cb44469f558a7194e3ce052c',
        [
            ("""#include "Terrorform.hpp"

Terrorform::Terrorform() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
""",
             """#include "Terrorform.hpp"
#include <cstring>

Terrorform::Terrorform() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
"""),
            ("""    decayCV2Depth = params[LPG_DECAY_CV_2_PARAM].getValue();

    for(auto i = 0; i < numActiveChannels; ++i) {
        freqs[i] = pitch2freq(inputs[VOCT_1_INPUT].getPolyVoltage(i) * pitchCV1 +
                              inputs[VOCT_2_INPUT].getPolyVoltage(i) * pitchCV2 +
                              rootPitch);
        wavesCV[i] = inputs[WAVE_INPUT_1].getPolyVoltage(i) * waveCV1 * 0.1f;
        wavesCV[i] += inputs[WAVE_INPUT_2].getPolyVoltage(i) * waveCV2 * 0.1f;
        waves[i] = (rootWave + wavesCV[i]) * numWavesInTable;
""",
             """    decayCV2Depth = params[LPG_DECAY_CV_2_PARAM].getValue();

    for(auto i = 0; i < numActiveChannels; ++i) {
        const float pitch = inputs[VOCT_1_INPUT].getPolyVoltage(i) * pitchCV1 +
                              inputs[VOCT_2_INPUT].getPolyVoltage(i) * pitchCV2 +
                              rootPitch;
        if (!frequencyValid[i] || std::memcmp(&pitch, &previousPitch[i], sizeof(float)) != 0) {
            freqs[i] = pitch2freq(pitch);
            previousPitch[i] = pitch;
            frequencyValid[i] = true;
        }
        wavesCV[i] = inputs[WAVE_INPUT_1].getPolyVoltage(i) * waveCV1 * 0.1f;
        wavesCV[i] += inputs[WAVE_INPUT_2].getPolyVoltage(i) * waveCV2 * 0.1f;
        waves[i] = (rootWave + wavesCV[i]) * numWavesInTable;
"""),
            ("""        numVoices = json_integer_value(numVoicesJ);""",
             """        const json_int_t restoredVoices = json_integer_value(numVoicesJ);
        numVoices = restoredVoices < 0 ? 0 : restoredVoices > kMaxMusicalVoices ? kMaxMusicalVoices : static_cast<int>(restoredVoices);"""),
            ("""    numActiveChannels = numActiveChannels < 1 ? 1 : numActiveChannels;""",
             """    numActiveChannels = numActiveChannels < 1 ? 1 : numActiveChannels > kMaxMusicalVoices ? kMaxMusicalVoices : numActiveChannels;"""),
            ("""    module->numVoices = numVoices;""",
             """    module->numVoices = numVoices < 0 ? 0 : numVoices > Terrorform::kMaxMusicalVoices ? Terrorform::kMaxMusicalVoices : numVoices;"""),
            ("""    for (size_t i = 0; i < 17; ++i) {""",
             """    for (size_t i = 0; i <= Terrorform::kMaxMusicalVoices; ++i) {"""),
        ],
    ),
    (
        'plugins/cf/src/CUBE.cpp',
        'c458870d704b4c45c96c3a4f81bfec63268a5e5d96e1df21ee58fa9f658750fb',
        'd467e95636dbdef009239a7437f02824a2e910aa44bd31a858c1224b7436a7ac',
        [
            ("""\tif (inputs[X_INPUT].isConnected()) gainX=inputs[X_INPUT].getVoltage();
\tif (inputs[Y_INPUT].isConnected()) gainY=inputs[Y_INPUT].getVoltage();

       \tfor(int i=0; i<8; i++)
        \t{
\t\t\td = sqrt(yy[i]*yy[i] + zz[i]*zz[i]);
\t\t\ttheta = atan2(yy[i],zz[i])+frameX;
\t\t\tx[i] = xx[i];\x20
\t\t\ty[i] = d * sin(theta);\x20
\t\t\tz[i] = d * cos(theta);

\t\t\td = sqrt(x[i]*x[i] + z[i]*z[i]);
\t\t\ttheta = atan2(x[i],z[i])+frameY;
""",
             """\tif (inputs[X_INPUT].isConnected()) gainX=inputs[X_INPUT].getVoltage();
\tif (inputs[Y_INPUT].isConnected()) gainY=inputs[Y_INPUT].getVoltage();

       \tfloat firstRotationZ = 0.0f;
       \tfor(int i=0; i<8; i++)
        \t{
\t\t\tx[i] = xx[i];
\t\t\tif ((i & 1) == 0) {
\t\t\t\td = sqrt(yy[i]*yy[i] + zz[i]*zz[i]);
\t\t\t\ttheta = atan2(yy[i],zz[i])+frameX;
\t\t\t\ty[i] = d * sin(theta);
\t\t\t\tz[i] = d * cos(theta);
\t\t\t\tfirstRotationZ = z[i];
\t\t\t} else {
\t\t\t\ty[i] = y[i-1];
\t\t\t\tz[i] = firstRotationZ;
\t\t\t}

\t\t\td = sqrt(x[i]*x[i] + z[i]*z[i]);
\t\t\ttheta = atan2(x[i],z[i])+frameY;
"""),
        ],
    ),
    (
        'plugins/mscHack/src/Dronez.cpp',
        '1b3a724015179f1f6205cba562803abce227b7184f4badb34d01b638bf28c5d9',
        '63e69de18d069582872a176233c3a087ab8f6306923e518e6daab6e4507b24cd',
        [
            ("""\t\tIn= 0.0f;\r
\r
\t\t// get wave audio\r
\t\tfor( wv = 0; wv < nMORPH_WAVES; wv++ )\r
\t\t{\r
\t\t\tm_wav[ ch ][ wv ].m_Clock.syncInc = 32.7032f * clamp( powf( 2.0f, clamp( inputs[ IN_VOCT ].getNormalVoltage( 3.0f ) + m_osc[ ch ].semi, 0.0f, VOCT_MAX ) ), 0.0f, 4186.01f );\r
\r
\t\t\tif( wv == 0 )\r
\t\t\t{\r
""",
             """\t\tIn= 0.0f;\r
\r
\t\t// get wave audio\r
\t\tconst float baseSyncInc = 32.7032f * clamp( powf( 2.0f, clamp( inputs[ IN_VOCT ].getNormalVoltage( 3.0f ) + m_osc[ ch ].semi, 0.0f, VOCT_MAX ) ), 0.0f, 4186.01f );\r
\t\tfor( wv = 0; wv < nMORPH_WAVES; wv++ )\r
\t\t{\r
\t\t\tm_wav[ ch ][ wv ].m_Clock.syncInc = baseSyncInc;\r
\r
\t\t\tif( wv == 0 )\r
\t\t\t{\r
"""),
        ],
    ),
    (
        'plugins/ValleyAudio/src/Dexter/Dexter.cpp',
        'fbb2a9c7647836a311c9aabd175d7f46ef962b20f0c852a22c9e5a60f0d1bf46',
        '70cc0302d77b4755573adad458dfadfdd712a85755b65ec405eb85cbb0d6189f',
        [
            ("""//

#include "Dexter.hpp"

Dexter::Dexter() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
""",
             """//

#include "Dexter.hpp"
#include <cstring>

Dexter::Dexter() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
"""),
            ("""        modMatrix[op].process();
    }

    for (size_t op = 0; op < DexterCore::kNumOperators; ++op) {
        opPitch[op] = opCoarseKnob[op] + opFineKnob[op];
        opPitch[op] += inputs[opCVInputs[op][OP_PITCH_CV_1]].getVoltage() * opPitchCV1Knob[op];
""",
             """        modMatrix[op].process();
    }

    __m128 previousLowFreq = _mm_setzero_ps();
    __m128 previousHighFreq = _mm_setzero_ps();
    for (size_t op = 0; op < DexterCore::kNumOperators; ++op) {
        opPitch[op] = opCoarseKnob[op] + opFineKnob[op];
        opPitch[op] += inputs[opCVInputs[op][OP_PITCH_CV_1]].getVoltage() * opPitchCV1Knob[op];
"""),
            ("""
        __opLowPitch[op] = _mm_loadu_ps(pOpPitches);
        __opHighPitch[op] = _mm_loadu_ps(pOpPitches + 4);
        __opLowFreq[op] = valley::_mm_pitch_freq_ps(__opLowPitch[op]);
        __opHighFreq[op] = valley::_mm_pitch_freq_ps(__opHighPitch[op]);
        __opLowFreq[op] = _mm_mul_ps(__opLowFreq[op], _mm_set1_ps(multiples[multiple]));
        __opHighFreq[op] = _mm_mul_ps(__opHighFreq[op], _mm_set1_ps(multiples[multiple]));

""",
             """
        __opLowPitch[op] = _mm_loadu_ps(pOpPitches);
        __opHighPitch[op] = _mm_loadu_ps(pOpPitches + 4);
        if (op > 0 && std::memcmp(&opPitch[op], &opPitch[op - 1], sizeof(float)) == 0) {
            __opLowFreq[op] = previousLowFreq;
            __opHighFreq[op] = previousHighFreq;
        }
        else {
            __opLowFreq[op] = valley::_mm_pitch_freq_ps(__opLowPitch[op]);
            __opHighFreq[op] = valley::_mm_pitch_freq_ps(__opHighPitch[op]);
            previousLowFreq = __opLowFreq[op];
            previousHighFreq = __opHighFreq[op];
        }
        __opLowFreq[op] = _mm_mul_ps(__opLowFreq[op], _mm_set1_ps(multiples[multiple]));
        __opHighFreq[op] = _mm_mul_ps(__opHighFreq[op], _mm_set1_ps(multiples[multiple]));

"""),
        ],
    ),
    (
        'plugins/dBiz/src/DAOSC.cpp',
        'def71fbc5c343a0e0c85c508575d728dc07656ac4c390cd9e5495db176f237db',
        '213aed5bcdf878c6e6be4eba4919ed80ce2a6cc34567fd119f74e746f3986dfb',
        [
            ("""\r
\t//////////////////////////////////////////////////////////////////////////////////\r
\r
\t\tfor (int c = 0; c < channelsB + channelsA; c += 8)\r
\t\t{\r
\t\t\toutputs[A_OUTPUT].setVoltageSimd(5 * sumDA, c);\r
\t\t\toutputs[B_OUTPUT].setVoltageSimd(5 * sumDB, c);\r
""",
             """\r
\t//////////////////////////////////////////////////////////////////////////////////\r
\r
\t\tconst int sumChannels = std::min(channelsB + channelsA, engine::PORT_MAX_CHANNELS);\r
\t\tfor (int c = 0; c < sumChannels; c += 8)\r
\t\t{\r
\t\t\toutputs[A_OUTPUT].setVoltageSimd(5 * sumDA, c);\r
\t\t\toutputs[B_OUTPUT].setVoltageSimd(5 * sumDB, c);\r
"""),
            ("""\r
\toutputs[A_OUTPUT].setChannels(channelsA);\r
\toutputs[B_OUTPUT].setChannels(channelsB);\r
\toutputs[SUM_OUTPUT].setChannels(channelsB+channelsA);\r
\r
}\r
};\r
""",
             """\r
\toutputs[A_OUTPUT].setChannels(channelsA);\r
\toutputs[B_OUTPUT].setChannels(channelsB);\r
\toutputs[SUM_OUTPUT].setChannels(sumChannels);\r
\r
}\r
};\r
"""),
        ],
    ),
]

def prepare(source):
    pending = []
    for relative, before_hash, after_hash, replacements in CHANGES:
        path = source / relative
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest == after_hash:
            continue
        if digest != before_hash:
            raise SystemExit("Pinned DSP source mismatch: " + relative)
        text = data.decode("utf-8")
        for before, after in replacements:
            if text.count(before) != 1:
                raise SystemExit("Pinned DSP seam mismatch: " + relative)
            text = text.replace(before, after, 1)
        result = text.encode("utf-8")
        if hashlib.sha256(result).hexdigest() != after_hash:
            raise SystemExit("Incomplete DSP preparation: " + relative)
        pending.append((path, result))
    # Fundamental alone consumes the qualified ARMv7 phase-rounding helper.
    # Do not replace Rack's SIMD implementation or affect other oscillators.
    path = source / "plugins/Fundamental/src/vco_rounding.hpp"
    result = (Path(__file__).resolve().parent / "fundamental-vco-rounding.hpp").read_bytes()
    if not path.exists() or path.read_bytes() != result:
        pending.append((path, result))
    path = source / "plugins/AudibleInstruments/src/Ripples/voice-engine.hpp"
    result = (Path(__file__).resolve().parent / "ripples-voice-engine.hpp").read_bytes()
    if not path.exists() or path.read_bytes() != result:
        pending.append((path, result))
    for path, result in pending:
        path.write_bytes(result)
    print("Prepared measured catalog DSP and aligned oscillator returns:",
          len(pending), "files changed")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    prepare(parser.parse_args().source)
