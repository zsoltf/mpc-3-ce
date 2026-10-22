// Mutable Instruments Ripples emulation for VCV Rack
// Copyright (C) 2020 Tyler Coy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Permanent voice-lane Ripples state, using the original rate table and RK2 model.
#pragma once
#include "ripples.hpp"
namespace ripples {
using VoiceVector = simd::float_4;
struct VoiceAA {
    int factor=1, count=0;
    SOSCoefficients coefficients[7];
    bool unitTail=false;
    VoiceVector histories[2][8][2][4] = {};
    template<int N> void configure(const SOSCoefficients (&c)[N]) {
        count=N;
        unitTail=true;
        for(int i=0;i<N;++i) {
            coefficients[i]=c[i];
            if(i>0 && (c[i].b[0]!=1.f || c[i].b[2]!=1.f)) unitTail=false;
        }
        for(auto& bank:histories) for(auto& section:bank) for(auto& delay:section) for(auto& v:delay) v=0.f;
    }
    void InitFilter(float sample_rate) {
        if (false) {}
        /*[[[cog
        for cascade in reversed(cascades):
            (fs, factor, order, wc, sos) = cascade
            num_sections = len(sos)
            name = '{:s}{:d}x{:d}'.format(array_name, fs, factor)
            cost = fs * factor * num_sections

            cog.outl('else if ({} <= sample_rate)'.format(fs))
            cog.outl('{')
            cog.outl('    const SOSCoefficients {:s}[{:d}] ='
                ' // n = {:d}, wc = {:f}, cost = {:d}'
                .format(name, num_sections, order, wc, cost))
            cog.outl('    {')
            for sec in sos:
                b = ''.join(['{:.8e},'.format(c).ljust(17) for c in sec[:3]])
                a = ''.join(['{:.8e},'.format(c).ljust(17) for c in sec[4:]])
                cog.outl('        { {' + b + '}, {' + a + '} },')
            cog.outl('    };')
            cog.outl('    configure({});'.format(name))
            cog.outl('    factor = {};'.format(factor))
            cog.outl('}')
        cog.outl('else {{ InitFilter({}); }}'.format(*cascades[0]))
        ]]]*/
        else if (768000 <= sample_rate)
        {
            const SOSCoefficients kFilter768000x1[1] = // n = 2, wc = 0.052083, cost = 768000
            {
                { {1.83197956e-02,  3.66063440e-02,  1.83197956e-02,  }, {-1.60702602e+00, 6.80271956e-01,  } },
            };
            configure(kFilter768000x1);
            factor = 1;
        }
        else if (705600 <= sample_rate)
        {
            const SOSCoefficients kFilter705600x1[1] = // n = 2, wc = 0.056689, cost = 705600
            {
                { {2.13438638e-02,  4.26550556e-02,  2.13438638e-02,  }, {-1.57253460e+00, 6.57877382e-01,  } },
            };
            configure(kFilter705600x1);
            factor = 1;
        }
        else if (384000 <= sample_rate)
        {
            const SOSCoefficients kFilter384000x1[1] = // n = 2, wc = 0.104167, cost = 384000
            {
                { {6.09620331e-02,  1.21896769e-01,  6.09620331e-02,  }, {-1.22760212e+00, 4.71422957e-01,  } },
            };
            configure(kFilter384000x1);
            factor = 1;
        }
        else if (352800 <= sample_rate)
        {
            const SOSCoefficients kFilter352800x1[1] = // n = 2, wc = 0.113379, cost = 352800
            {
                { {6.99874107e-02,  1.39948456e-01,  6.99874107e-02,  }, {-1.16347041e+00, 4.43393682e-01,  } },
            };
            configure(kFilter352800x1);
            factor = 1;
        }
        else if (192000 <= sample_rate)
        {
            const SOSCoefficients kFilter192000x1[1] = // n = 2, wc = 0.208333, cost = 192000
            {
                { {1.74603587e-01,  3.49188678e-01,  1.74603587e-01,  }, {-5.65216145e-01, 2.63611998e-01,  } },
            };
            configure(kFilter192000x1);
            factor = 1;
        }
        else if (176400 <= sample_rate)
        {
            const SOSCoefficients kFilter176400x1[1] = // n = 2, wc = 0.226757, cost = 176400
            {
                { {1.95938020e-01,  3.91858763e-01,  1.95938020e-01,  }, {-4.62313019e-01, 2.46047822e-01,  } },
            };
            configure(kFilter176400x1);
            factor = 1;
        }
        else if (96000 <= sample_rate)
        {
            const SOSCoefficients kFilter96000x2[4] = // n = 8, wc = 0.208333, cost = 768000
            {
                { {1.61637850e-04,  2.48564833e-04,  1.61637850e-04,  }, {-1.55379599e+00, 6.19242969e-01,  } },
                { {1.00000000e+00,  -3.56106191e-03, 1.00000000e+00,  }, {-1.52397985e+00, 7.01779035e-01,  } },
                { {1.00000000e+00,  -7.04269454e-01, 1.00000000e+00,  }, {-1.49925562e+00, 8.20191196e-01,  } },
                { {1.00000000e+00,  -9.36222412e-01, 1.00000000e+00,  }, {-1.51854586e+00, 9.39911675e-01,  } },
            };
            configure(kFilter96000x2);
            factor = 2;
        }
        else if (88200 <= sample_rate)
        {
            const SOSCoefficients kFilter88200x2[4] = // n = 8, wc = 0.226757, cost = 705600
            {
                { {2.14361684e-04,  3.44618768e-04,  2.14361684e-04,  }, {-1.51452462e+00, 5.91486912e-01,  } },
                { {1.00000000e+00,  1.79381294e-01,  1.00000000e+00,  }, {-1.47183116e+00, 6.80568376e-01,  } },
                { {1.00000000e+00,  -5.38705333e-01, 1.00000000e+00,  }, {-1.43146550e+00, 8.07687680e-01,  } },
                { {1.00000000e+00,  -7.87002288e-01, 1.00000000e+00,  }, {-1.44140131e+00, 9.35689662e-01,  } },
            };
            configure(kFilter88200x2);
            factor = 2;
        }
        else if (48000 <= sample_rate)
        {
            const SOSCoefficients kFilter48000x3[6] = // n = 12, wc = 0.277778, cost = 864000
            {
                { {1.96007199e-04,  3.15285921e-04,  1.96007199e-04,  }, {-1.49750952e+00, 5.79487424e-01,  } },
                { {1.00000000e+00,  1.64502383e-01,  1.00000000e+00,  }, {-1.43900370e+00, 6.63196513e-01,  } },
                { {1.00000000e+00,  -5.92180251e-01, 1.00000000e+00,  }, {-1.36241892e+00, 7.75058824e-01,  } },
                { {1.00000000e+00,  -9.07488127e-01, 1.00000000e+00,  }, {-1.30223398e+00, 8.69165582e-01,  } },
                { {1.00000000e+00,  -1.04177534e+00, 1.00000000e+00,  }, {-1.26951947e+00, 9.34679234e-01,  } },
                { {1.00000000e+00,  -1.09276235e+00, 1.00000000e+00,  }, {-1.26454687e+00, 9.80322986e-01,  } },
            };
            configure(kFilter48000x3);
            factor = 3;
        }
        else if (44100 <= sample_rate)
        {
            const SOSCoefficients kFilter44100x3[7] = // n = 14, wc = 0.302343, cost = 926100
            {
                { {2.33467524e-04,  3.85146244e-04,  2.33467524e-04,  }, {-1.46779940e+00, 5.59300587e-01,  } },
                { {1.00000000e+00,  2.84344987e-01,  1.00000000e+00,  }, {-1.39743012e+00, 6.47280334e-01,  } },
                { {1.00000000e+00,  -4.81735913e-01, 1.00000000e+00,  }, {-1.30466696e+00, 7.63828718e-01,  } },
                { {1.00000000e+00,  -8.14458422e-01, 1.00000000e+00,  }, {-1.22921466e+00, 8.60153843e-01,  } },
                { {1.00000000e+00,  -9.63424410e-01, 1.00000000e+00,  }, {-1.18164620e+00, 9.24279595e-01,  } },
                { {1.00000000e+00,  -1.03102512e+00, 1.00000000e+00,  }, {-1.15782377e+00, 9.63657309e-01,  } },
                { {1.00000000e+00,  -1.05757483e+00, 1.00000000e+00,  }, {-1.15253824e+00, 9.89272846e-01,  } },
            };
            configure(kFilter44100x3);
            factor = 3;
        }
        else if (24000 <= sample_rate)
        {
            const SOSCoefficients kFilter24000x5[4] = // n = 8, wc = 0.333333, cost = 480000
            {
                { {9.93374792e-04,  1.81504524e-03,  9.93374792e-04,  }, {-1.28123502e+00, 4.43830055e-01,  } },
                { {1.00000000e+00,  9.69736619e-01,  1.00000000e+00,  }, {-1.14056361e+00, 5.73274737e-01,  } },
                { {1.00000000e+00,  3.23593812e-01,  1.00000000e+00,  }, {-9.84074266e-01, 7.48267989e-01,  } },
                { {1.00000000e+00,  4.69137219e-02,  1.00000000e+00,  }, {-9.17508757e-01, 9.16260523e-01,  } },
            };
            configure(kFilter24000x5);
            factor = 5;
        }
        else if (22050 <= sample_rate)
        {
            const SOSCoefficients kFilter22050x6[4] = // n = 8, wc = 0.302343, cost = 529200
            {
                { {6.47358611e-04,  1.15520581e-03,  6.47358611e-04,  }, {-1.35050917e+00, 4.84676642e-01,  } },
                { {1.00000000e+00,  7.82770646e-01,  1.00000000e+00,  }, {-1.24212580e+00, 6.01760550e-01,  } },
                { {1.00000000e+00,  9.46030879e-02,  1.00000000e+00,  }, {-1.12297856e+00, 7.63193697e-01,  } },
                { {1.00000000e+00,  -1.84341946e-01, 1.00000000e+00,  }, {-1.08165394e+00, 9.20980215e-01,  } },
            };
            configure(kFilter22050x6);
            factor = 6;
        }
        else if (12000 <= sample_rate)
        {
            const SOSCoefficients kFilter12000x10[3] = // n = 6, wc = 0.333333, cost = 360000
            {
                { {3.42306291e-03,  6.53522273e-03,  3.42306291e-03,  }, {-1.13209947e+00, 3.65774415e-01,  } },
                { {1.00000000e+00,  1.42136933e+00,  1.00000000e+00,  }, {-9.55595652e-01, 5.55195466e-01,  } },
                { {1.00000000e+00,  1.05842861e+00,  1.00000000e+00,  }, {-8.35474882e-01, 8.34840828e-01,  } },
            };
            configure(kFilter12000x10);
            factor = 10;
        }
        else if (11025 <= sample_rate)
        {
            const SOSCoefficients kFilter11025x11[3] = // n = 6, wc = 0.329829, cost = 363825
            {
                { {3.26702718e-03,  6.22983576e-03,  3.26702718e-03,  }, {-1.14130758e+00, 3.70354990e-01,  } },
                { {1.00000000e+00,  1.40863044e+00,  1.00000000e+00,  }, {-9.69538649e-01, 5.57917370e-01,  } },
                { {1.00000000e+00,  1.03994151e+00,  1.00000000e+00,  }, {-8.54328717e-01, 8.35728285e-01,  } },
            };
            configure(kFilter11025x11);
            factor = 11;
        }
        else if (8000 <= sample_rate)
        {
            const SOSCoefficients kFilter8000x15[3] = // n = 6, wc = 0.333333, cost = 360000
            {
                { {3.42306291e-03,  6.53522273e-03,  3.42306291e-03,  }, {-1.13209947e+00, 3.65774415e-01,  } },
                { {1.00000000e+00,  1.42136933e+00,  1.00000000e+00,  }, {-9.55595652e-01, 5.55195466e-01,  } },
                { {1.00000000e+00,  1.05842861e+00,  1.00000000e+00,  }, {-8.35474882e-01, 8.34840828e-01,  } },
            };
            configure(kFilter8000x15);
            factor = 15;
        }
        else { InitFilter(8000); }
    }
    // Carry feedback into the next section without reloading the same histories.
    // Named vectors avoid aggregate-array copies in the ARM compiler.
    template<bool Full> __attribute__((noinline)) void process(int bank, VoiceVector* __restrict signal, VoiceVector active) {
        auto& x=histories[bank];
        VoiceVector w0=signal[0], p0=x[0][0][0], q0=x[0][1][0];
        VoiceVector w1=signal[1], p1=x[0][0][1], q1=x[0][1][1];
        VoiceVector w2=signal[2], p2=x[0][0][2], q2=x[0][1][2];
        VoiceVector w3=signal[3], p3=x[0][0][3], q3=x[0][1][3];
        for(int n=0;n<count;++n) {
            x[n][1][0]=Full?p0:simd::ifelse(active,p0,q0);
            x[n][0][0]=Full?w0:simd::ifelse(active,w0,p0);
            x[n][1][1]=Full?p1:simd::ifelse(active,p1,q1);
            x[n][0][1]=Full?w1:simd::ifelse(active,w1,p1);
            x[n][1][2]=Full?p2:simd::ifelse(active,p2,q2);
            x[n][0][2]=Full?w2:simd::ifelse(active,w2,p2);
            x[n][1][3]=Full?p3:simd::ifelse(active,p3,q3);
            x[n][0][3]=Full?w3:simd::ifelse(active,w3,p3);
            // Preserve the original SOS unity-tail shortcut without unrolling sections.
            const bool unity=unitTail && n>0;
            if(!unity) {
                const VoiceVector coefficient=coefficients[n].b[0];
                w0=VoiceVector(0.f)+coefficient*w0;
                w1=VoiceVector(0.f)+coefficient*w1;
                w2=VoiceVector(0.f)+coefficient*w2;
                w3=VoiceVector(0.f)+coefficient*w3;
            }
            else {
                w0=VoiceVector(0.f)+w0;
                w1=VoiceVector(0.f)+w1;
                w2=VoiceVector(0.f)+w2;
                w3=VoiceVector(0.f)+w3;
            }
            {
                const VoiceVector coefficient=coefficients[n].b[1];
                w0+=coefficient*p0;
                w1+=coefficient*p1;
                w2+=coefficient*p2;
                w3+=coefficient*p3;
            }
            if(unity) {
                w0+=q0;
                w1+=q1;
                w2+=q2;
                w3+=q3;
            }
            else {
                const VoiceVector coefficient=coefficients[n].b[2];
                w0+=coefficient*q0;
                w1+=coefficient*q1;
                w2+=coefficient*q2;
                w3+=coefficient*q3;
            }
            p0=x[n+1][0][0]; q0=x[n+1][1][0];
            p1=x[n+1][0][1]; q1=x[n+1][1][1];
            p2=x[n+1][0][2]; q2=x[n+1][1][2];
            p3=x[n+1][0][3]; q3=x[n+1][1][3];
            {
                const VoiceVector coefficient=coefficients[n].a[0];
                w0-=coefficient*p0;
                w1-=coefficient*p1;
                w2-=coefficient*p2;
                w3-=coefficient*p3;
            }
            {
                const VoiceVector coefficient=coefficients[n].a[1];
                w0-=coefficient*q0;
                w1-=coefficient*q1;
                w2-=coefficient*q2;
                w3-=coefficient*q3;
            }
        }
        x[count][1][0]=Full?p0:simd::ifelse(active,p0,q0);
        x[count][0][0]=Full?w0:simd::ifelse(active,w0,p0);
        signal[0]=w0;
        x[count][1][1]=Full?p1:simd::ifelse(active,p1,q1);
        x[count][0][1]=Full?w1:simd::ifelse(active,w1,p1);
        signal[1]=w1;
        x[count][1][2]=Full?p2:simd::ifelse(active,p2,q2);
        x[count][0][2]=Full?w2:simd::ifelse(active,w2,p2);
        signal[2]=w2;
        x[count][1][3]=Full?p3:simd::ifelse(active,p3,q3);
        x[count][0][3]=Full?w3:simd::ifelse(active,w3,p3);
        signal[3]=w3;
    }
};
class RipplesVoices {
public:
    using Frame=RipplesEngine::Frame;
    VoiceAA aa;
    VoiceVector cell[4] = {};
    dsp::TRCFilter<VoiceVector> rc[4], vca;
    float sampleTime;
    // Only the selected representation advances. Partial groups retain the
    // original one-vector-per-voice processor; full groups use voice SIMD.
    RipplesEngine scalar[4];
    bool vectorActive = false;
    RipplesVoices() {setSampleRate(1.f);}
    void setSampleRate(float rate) {
        for(auto& engine:scalar)engine.setSampleRate(rate);
        sampleTime=1.f/rate;
        for(auto& c:cell)c=0.f;
        aa.InitFilter(rate);
        float oversampleRate=rate*aa.factor;
        float cuts[4]={1.f/(2.f*M_PI*kFeedforwardR*kFeedforwardC),1.f/(2.f*M_PI*kFreqAmpR*kFreqAmpC),1.f/(2.f*M_PI*kResAmpR*kResAmpC),1.f/(2.f*M_PI*kGainAmpR*kGainAmpC)};
        for(int s=0;s<4;++s)rc[s].setCutoffFreq(VoiceVector(cuts[s])/oversampleRate);
        dsp::TRCFilter<float> scalarVca;scalarVca.setCutoffFreq((1.f/(2.f*M_PI*kVCAInputR*kVCAInputC))/oversampleRate);vca.c=scalarVca.c;
    }
    static float current(float rfb,float vc,float rcv,float vp=0.f,float rp=1e12f) {
        float vnom=-(vc*rfb/rcv+vp*rfb/rp);
        float vout=std::max(vnom,kVtoICollectorVSat);
        float nrc=rp*rfb,nrp=rcv*rfb,nrfb=rcv*rp;
        float vneg=(vc*nrc+vp*nrp+vout*nrfb)/(nrc+nrp+nrfb);
        return std::max((vneg-vout)/rfb,0.f);
    }
    static VoiceVector divideRefined(VoiceVector numerator, VoiceVector denominator) {
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && !defined(SIMDE_ARM_NEON_A64V8_NATIVE)
        auto n=simde__m128_to_private(numerator.v), d=simde__m128_to_private(denominator.v);
        float32x4_t r=vrecpeq_f32(d.neon_f32);
        r=vmulq_f32(r,vrecpsq_f32(r,d.neon_f32));
        r=vmulq_f32(r,vrecpsq_f32(r,d.neon_f32));
        simde__m128_private result;result.neon_f32=vmulq_f32(n.neon_f32,r);
        return simde__m128_from_private(result);
#else
        return numerator/denominator;
#endif
    }
    static VoiceVector ota(VoiceVector vp,VoiceVector vn,VoiceVector bias) {
        constexpr float vt=8.617333262145e-5f*(40.f+273.15f);
        const float limit=2.f*std::sqrt(3.f);
        VoiceVector z=simd::clamp((vp-vn)/(2.f*vt),-limit,limit);
        VoiceVector z2=z*z,q=12.f+z2;
        // SIMD division uses NEON reciprocal estimate and two Newton refinements.
        return bias*divideRefined(12.f*z*q,36.f*z2+q*q);
    }
    template<bool Full> void rcProcess(dsp::TRCFilter<VoiceVector>& f,VoiceVector x,VoiceVector active) {
        VoiceVector oldX=f.xstate[0],oldY=f.ystate[0];f.process(x);
        if(!Full){f.xstate[0]=simd::ifelse(active,f.xstate[0],oldX);f.ystate[0]=simd::ifelse(active,f.ystate[0],oldY);}
    }
    void derivative(const VoiceVector (&v)[4],VoiceVector (&d)[4],VoiceVector audio,VoiceVector feed,VoiceVector bias,VoiceVector rad) {
        VoiceVector in=audio*kFilterInputGain+kFilterCellR*ota(feed*kFeedforwardGain,v[3]*kFeedbackGain,bias);
        VoiceVector sums[4]={in+v[0],v[0]+v[1],v[1]+v[2],v[2]+v[3]};
        for(int s=0;s<4;++s)d[s]=(rad*sums[s])*(1.f+sums[s]*kFilterCellSelfModulation);
    }
    static float divideScalar(float numerator,float denominator) {
#if defined(__arm__) && defined(__ARM_FP)
        float result;
        asm("vdiv.f32 %0, %1, %2" : "=t"(result) : "t"(numerator), "t"(denominator));
        return result;
#else
        return numerator/denominator;
#endif
    }
    template<bool Full> void processVca(VoiceVector input,VoiceVector active,int channels) {
        const VoiceVector numerator=input+vca.xstate[0]-vca.ystate[0]*(1.f-vca.c);
        const VoiceVector denominator=1.f+vca.c;
        VoiceVector next=vca.ystate[0];
        // This slow high-pass retains its history across rate changes. Exact
        // division avoids accumulated reciprocal rounding at its near-unit pole.
        for(int c=0;c<channels;++c)next[c]=divideScalar(numerator[c],denominator[c]);
        vca.xstate[0]=Full?input:simd::ifelse(active,input,vca.xstate[0]);
        vca.ystate[0]=next;
    }
    template<bool Full> __attribute__((noinline)) void run(Frame* frames,int channels) {
        VoiceVector active=VoiceVector(0.f,1.f,2.f,3.f)<float(channels);
        VoiceVector input[4]={0.f,0.f,0.f,0.f};
        for(int c=0;c<channels;++c) {
            auto& f=frames[c];float oct=0.f;
            oct+=(f.freq_knob-1.f)*kFreqKnobVoltage;oct+=f.freq_cv;oct+=f.fm_cv*f.fm_knob;oct=std::min(oct,0.f);
            input[0][c]=f.input+1e-6*(random::uniform()-.5f);
            input[1][c]=oct;
            input[2][c]=current(kResAmpR,f.res_cv,kResInputR,f.res_knob*kResKnobV,kResKnobR);
            input[3][c]=current(kGainAmpR,f.gain_cv_present?f.gain_cv:kGainNormalV,kGainInputR+(f.gain_cv_present?0.f:kGainNormalR));
        }
        for(auto& v:input)v*=aa.factor;
        const float dt=sampleTime/aa.factor;
        VoiceVector output[4];
        for(int step=0;step<aa.factor;++step) {
            VoiceVector signal[4];for(int s=0;s<4;++s)signal[s]=step==0?input[s]:VoiceVector(0.f);
            aa.process<Full>(0,signal,active);
            for(int s=0;s<4;++s)rcProcess<Full>(rc[s],signal[s],active);
            VoiceVector rad(0.f);for(int c=0;c<channels;++c)rad[c]=-std::exp2f(rc[1].lowpass()[c])/kFilterCellRC;
            VoiceVector first[4],mid[4],second[4];
            derivative(cell,first,signal[0],rc[0].highpass(),rc[2].lowpass(),rad);
            for(int s=0;s<4;++s)mid[s]=cell[s]+first[s]*dt/2.f;
            derivative(mid,second,signal[0],rc[0].highpass(),rc[2].lowpass(),rad);
            for(int s=0;s<4;++s){VoiceVector next=simd::clamp(cell[s]+dt*second[s],-kOpampSatV,kOpampSatV);cell[s]=Full?next:simd::ifelse(active,next,cell[s]);}
            processVca<Full>(cell[3],active,channels);
            output[0]=(cell[0]+cell[1])*kBP2Gain;output[1]=cell[1]*kLP2Gain;output[2]=cell[3]*kLP4Gain;
            output[3]=-kVCAOutputR*ota(VoiceVector(0.f),vca.highpass()*kVCAInputGain,rc[3].lowpass());
            aa.process<Full>(1,output,active);
        }
        for(int c=0;c<channels;++c){frames[c].bp2=output[0][c];frames[c].lp2=output[1][c];frames[c].lp4=output[2][c];frames[c].lp4vca=output[3][c];}
    }
    __attribute__((noinline)) void copyToScalar() {
        for(int lane=0;lane<4;++lane) {
            auto& engine=scalar[lane];
            for(int signal=0;signal<4;++signal) {
                engine.cell_voltage_[signal]=cell[signal][lane];
                engine.rc_filters_.xstate[0][signal]=rc[signal].xstate[0][lane];
                engine.rc_filters_.ystate[0][signal]=rc[signal].ystate[0][lane];
                for(int section=0;section<=aa.count;++section)for(int delay=0;delay<2;++delay) {
                    engine.aa_filter_.up_filter_.x_[section][delay][signal]=aa.histories[0][section][delay][signal][lane];
                    engine.aa_filter_.down_filter_.x_[section][delay][signal]=aa.histories[1][section][delay][signal][lane];
                }
            }
            engine.vca_hpf_.xstate[0]=vca.xstate[0][lane];
            engine.vca_hpf_.ystate[0]=vca.ystate[0][lane];
        }
    }
    __attribute__((noinline)) void copyToVector() {
        for(int lane=0;lane<4;++lane) {
            const auto& engine=scalar[lane];
            for(int signal=0;signal<4;++signal) {
                cell[signal][lane]=engine.cell_voltage_[signal];
                rc[signal].xstate[0][lane]=engine.rc_filters_.xstate[0][signal];
                rc[signal].ystate[0][lane]=engine.rc_filters_.ystate[0][signal];
                for(int section=0;section<=aa.count;++section)for(int delay=0;delay<2;++delay) {
                    aa.histories[0][section][delay][signal][lane]=engine.aa_filter_.up_filter_.x_[section][delay][signal];
                    aa.histories[1][section][delay][signal][lane]=engine.aa_filter_.down_filter_.x_[section][delay][signal];
                }
            }
            vca.xstate[0][lane]=engine.vca_hpf_.xstate[0];
            vca.ystate[0][lane]=engine.vca_hpf_.ystate[0];
        }
    }
    __attribute__((noinline)) void runPartial(Frame* frames,int count) {
        for(int lane=0;lane<count;++lane)scalar[lane].process(frames[lane]);
    }
    RipplesEngine* selectScalar() {
        if(vectorActive){copyToScalar();vectorActive=false;}
        return scalar;
    }
    void process(Frame* frames,int count) {
        if(count==4) {
            if(!vectorActive){copyToVector();vectorActive=true;}
            run<true>(frames,count);
        } else {
            selectScalar();
            runPartial(frames,count);
        }
    }
};
}
