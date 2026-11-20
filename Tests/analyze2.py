import numpy as np
SR=48000
def load(n): return np.fromfile(f'/tmp/nova/{n}.f32',dtype=np.float32).reshape(-1,2)
def peakfreq(x,t,w,lo,hi):
    s=x[int(t*SR):int((t+w)*SR)]; s=s*np.hanning(len(s)); N=1<<18; S=np.abs(np.fft.rfft(s,N)); fr=np.fft.rfftfreq(N,1/SR); m=(fr>lo)&(fr<hi); return fr[np.argmax(S*m)]
def amp_at(x,f,t=0.05,w=0.4,tol=0.03):
    s=x[int(t*SR):int((t+w)*SR)]; s=s*np.hanning(len(s)); N=1<<18; S=np.abs(np.fft.rfft(s,N)); fr=np.fft.rfftfreq(N,1/SR); m=(fr>f*(1-tol))&(fr<f*(1+tol)); return S[m].max()
x=load('bend_up_octave')[:,0]
print("Pitchbend +12 st (Soll C4 261.6 -> C5 523.3): vor %.1f Hz, nach %.1f Hz"%(peakfreq(x,0.1,0.15,150,400),peakfreq(x,0.6,0.3,150,1200)))
x=load('octave_up2')[:,0]; ref=load('pick_a2_ref')[:,0]
print("Oktav-Layer (+2 Okt, Mix 0.5): Energie bei 110 Hz %.1f, bei 440 Hz %.1f (Referenz ohne Layer bei 440: %.1f)"%(amp_at(x,110),amp_at(x,440),amp_at(ref,440)))
x=load('legato_glide')[:,0]
print("Legato (lange Fenster): "+"  ".join(f"{t:.2f}s:{peakfreq(x,t,0.12,60,400):6.1f}" for t in (0.2,0.35,0.55,0.65,0.8,0.9,1.15,1.3)))
print("--- Note-Off/On Klicktest: max Sprung im Release-Fenster vs. Sprung davor (Faktor >3 = verdaechtig)")
bad=0
for i in range(12):
    for key in ('n36','n60','n84'):
        x=load(f'inst{i}_{key}_v120')[:,0]
        d=np.abs(np.diff(x)); pre=d[int(.90*SR):int(.99*SR)].max(); post=d[int(1.0*SR):int(1.01*SR)].max(); on=d[:int(.003*SR)].max()/np.abs(x).max()
        if pre>1e-6 and post>3*pre and post>0.02: print(f"  inst{i} {key}: Release-Sprung {post:.3f} vs {pre:.3f}"); bad+=1
        if on>0.05: print(f"  inst{i} {key}: Onset-Sprung {on:.3f}"); bad+=1
print("auffaellige Faelle:",bad)
