#include "../Source/VocoderEngine.h"
#include <cstdio>
#include <cmath>
using namespace holyvox;

static bool finite (const std::vector<float>& v, float& rms, float& peak)
{ double s=0; peak=0; for (float x: v){ if(!std::isfinite(x)) return false; s+=x*x; peak=std::max(peak,std::fabs(x)); } rms=std::sqrt(s/v.size()); return true; }

int main()
{
    const double sr = 48000; const int N = (int)(sr*3);
    // synthetic "voice": 220 Hz saw (A3 = midi 57) with a vowel-like resonance via simple 2-pole formant filter
    std::vector<float> in(N); double ph=0, z1=0,z2=0;
    for (int i=0;i<N;++i){ ph+=220.0/sr; if(ph>=1)ph-=1; double s=(2*ph-1)*0.4; double y=s+1.6*z1-0.8*z2; z2=z1; z1=y; in[i]=(float)(y*0.15);}  
    int fails = 0;
    auto run = [&](const char* name, EngineParams p, bool midi, int expectNote){
        Engine e; e.prepare(sr);
        if (midi) { e.noteOn(60); }
        std::vector<float> L(N), R(N);
        const int blk=512;
        for (int i=0;i<N;i+=blk){ int n=std::min(blk,N-i); e.process(&in[i],&in[i],&L[i],&R[i],n,p); }
        float rms,peak; bool ok=finite(L,rms,peak)&&finite(R,rms,peak);
        std::vector<float> tail(L.begin()+N/2, L.end()); float r2,p2; finite(tail,r2,p2);
        bool noteOk = expectNote<0 || e.getCurrentNote()==expectNote;
        printf("%-22s finite=%d rms(tail)=%.4f peak=%.3f note=%d voiced=%d det=%.2f %s\n", name, ok, r2, p2, e.getCurrentNote(), e.isVoiced(), e.getDetectedMidi(), (ok&&r2>0.001f&&noteOk)?"PASS":"FAIL");
        if(!(ok&&r2>0.001f&&noteOk)) ++fails;
    };
    EngineParams p; 
    run("auto/chromatic", p, false, 57);
    p.scale=1; p.key=0; run("auto/Cmajor", p, false, 57);
    p.humanize=true; run("humanize", p, false, 57);
    p.humanize=false; p.arpOn=true; run("arp", p, false, -1);
    p.arpOn=false; p.exc=true; p.drive=1; p.mods=1; p.v1Formant=12; p.v2Formant=-12; p.tone=1; run("extreme", p, false, -1);
    EngineParams m; m.noteSourceMidi=true; run("midi", m, true, 60);
    // MIDI mode with no note held -> silent synth
    { Engine e; e.prepare(sr); std::vector<float> L(N),R(N); EngineParams q; q.noteSourceMidi=true; q.voiceMix=0; q.sibilance=0;
      for(int i=0;i<N;i+=512){int n=std::min(512,N-i); e.process(&in[i],&in[i],&L[i],&R[i],n,q);} float r,pk; finite(L,r,pk);
      printf("%-22s rms=%.6f %s\n","midi no-note silent",r, r<1e-4?"PASS":"FAIL"); if(!(r<1e-4)) ++fails; }
    // other sample rates
    for (double s2 : {44100.0, 96000.0}) { Engine e; e.prepare(s2); int M=(int)(s2*1.5); std::vector<float> a(M),L(M),R(M);
      double ph2=0; for(int i=0;i<M;++i){ph2+=196.0/s2; if(ph2>=1)ph2-=1; a[i]=(float)((2*ph2-1)*0.2);} EngineParams q;
      for(int i=0;i<M;i+=256){int n=std::min(256,M-i); e.process(&a[i],nullptr,&L[i],&R[i],n,q);} float r,pk; bool ok=finite(L,r,pk);
      printf("sr=%.0f finite=%d peak=%.3f note=%d (196Hz=G3=55) %s\n", s2, ok, pk, e.getCurrentNote(), (ok&&e.getCurrentNote()==55)?"PASS":"FAIL"); if(!(ok&&e.getCurrentNote()==55)) ++fails; }
    printf("\n%s (%d failures)\n", fails?"SOME TESTS FAILED":"ALL TESTS PASSED", fails);
    return fails;
}
