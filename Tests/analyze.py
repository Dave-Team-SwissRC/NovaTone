import numpy as np, sys
SR=48000
def load(n):
    a=np.fromfile(f'/tmp/nova/{n}.f32',dtype=np.float32).reshape(-1,2); return a
def f0_est(x,lo,hi,sr=SR):
    # FFT-Peak mit Parabel-Interpolation, Fenster 0.3 s ab 0.1 s
    seg=x[int(.1*sr):int(.1*sr)+int(.5*sr)]*np.hanning(int(.5*sr))
    N=1<<18; S=np.abs(np.fft.rfft(seg,N)); fr=np.fft.rfftfreq(N,1/sr)
    m=(fr>lo)&(fr<hi); i=np.argmax(S*m)
    a,b,c=np.log(S[i-1]+1e-12),np.log(S[i]+1e-12),np.log(S[i+1]+1e-12)
    d=0.5*(a-c)/(a-2*b+c); return fr[i]+d*(fr[1]-fr[0])
def centroid(x,t0=0.02,t1=0.3):
    seg=x[int(t0*SR):int(t1*SR)]; seg=seg*np.hanning(len(seg)); S=np.abs(np.fft.rfft(seg)); fr=np.fft.rfftfreq(len(seg),1/SR)
    return (S*fr).sum()/S.sum()
names=["Pick Bass","Finger Bass","Synth Bass Fast","Synth Bass Soft","Warm Analog","Digital","Atmospheric","Wide Synth","Concert Grand","Vintage Piano","Muted Grand","Classic Piano"]
print("--- Tonhoehen-Genauigkeit (Abweichung in Cent), Grundton-Suche +-1 Halbton")
worst=0
for i,nm in enumerate(names):
    row=[]
    for n,key in ((36,'n36'),(60,'n60'),(84,'n84')):
        x=load(f'inst{i}_{key}_v120')[:,0]
        f=440*2**((n-69)/12)
        # Suche das staerkste Maximum nahe f (Grundton) ; bei Synth mit Sub ggf. 0.5f -> daher +-1 Halbton
        fe=f0_est(x,f*2**(-1/12),f*2**(1/12))
        c=1200*np.log2(fe/f); row.append(c)
        worst=max(worst,abs(c))
    print(f"{nm:18s} "+"  ".join(f"{c:+6.1f}" for c in row))
print("max |Abweichung| = %.1f ct"%worst)
print("--- Helligkeit (Spektralschwerpunkt Hz) leise->laut @ C4")
for i in (0,1,8,9,10,11):
    lo=centroid(load(f'inst{i}_n60_v40')[:,0]); hi=centroid(load(f'inst{i}_n60_v120')[:,0])
    print(f"{names[i]:18s} vel40: {lo:7.0f}   vel120: {hi:7.0f}   Verhaeltnis {hi/lo:.2f}")
print("--- Knackser: groesster Sprung zwischen Samples (relativ zum Peak)")
for i,nm in enumerate(names):
    for key in ('n36','n60','n84'):
        x=load(f'inst{i}_{key}_v120')[:,0]; d=np.abs(np.diff(x)).max()/np.abs(x).max()
        if d>0.35: print(f"  {nm} {key}: Sprung {d:.2f}")
print("(nur Werte >0.35 gelistet)")
# Legato/Glide: Frequenzverlauf
x=load('legato_glide')[:,0]
def inst_f(x,t,w=0.04):
    s=x[int(t*SR):int((t+w)*SR)]*np.hanning(int(w*SR)); N=1<<16; S=np.abs(np.fft.rfft(s,N)); fr=np.fft.rfftfreq(N,1/SR); m=(fr>40)&(fr<400); return fr[np.argmax(S*m)]
print("--- Legato 40->52 @0.5s, Glide 0.3s: Frequenz ueber Zeit (Soll E2=82.4 -> E3=164.8)")
print("  t[s]: "+"  ".join(f"{t:.2f}:{inst_f(x,t):6.1f}" for t in (0.2,0.45,0.55,0.65,0.8,0.95,1.1,1.25)))
print("  Hinweis: Instrument 3 hat Sub-Oszillator (halbe Frequenz) -> 82.4/164.8 ist die Grundwelle")
# Bend
x=load('bend_up_octave')[:,0]
print("--- Pitchbend voll hoch bei Range 12 st (C4 261.6 -> C5 523.3): ", f"vor {inst_f(x,0.1,0.1):.1f}  nach {inst_f(x,0.6,0.1):.1f}")
# Oktav-Layer
x=load('octave_up2')[:,0]; ref=load('pick_a2_ref')[:,0]
f=f0_est(x,50,2000); print("--- Pick A2(110) + 2 Oktaven hoeher gemischt: staerkste Frequenz", f"{f:.1f}", "; Referenz", f"{f0_est(ref,50,200):.1f}")
# Sample
x=load('sample_c3_then_c4')[:,0]
print("--- Sample (Root C3=130.8): C3 gespielt ->", f"{inst_f(x,0.1,0.1):.1f}", "; C4 gespielt ->", f"{inst_f(x,1.4,0.1):.1f}", "(Soll 130.8 / 261.6)")
