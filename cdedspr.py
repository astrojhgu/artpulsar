#!/usr/bin/env python3
import matplotlib
matplotlib.use('Agg')
import numpy.fft as fft
import numpy as np
import matplotlib.pylab as plt
from matplotlib.gridspec import GridSpec

import sys

nch=4096
s1=32
s2=256
bw=10e6
nperiods=2
def calc_delay_s(nu_mhz, dm):
    return 1 / 2.41e-4 * dm / nu_mhz**2

def truncate(data, nch):
    return data[:nch*(len(data)//nch)]

def read_data(f, count):
    #data=np.fromfile(fname, dtype=np.int8, count=count*2)
    buf=f.read(count*2)
    if len(buf)==count*2:
        data=np.frombuffer(buf, dtype=np.int8)
        print(len(data))
        return data[::2]+1j*data[1::2]
    else:
        return None

def dedisp(data, dm, fc, fs):
    dt=1/fs
    freqs=fft.fftfreq(len(data), dt)+fc
    pf=np.exp(-1j*np.array([calc_delay_s(f/1e6, dm) for f in freqs])*freqs*2*np.pi)
    return fft.ifft(fft.fft(data)*pf)

def dedisp_chirp(data, dm, fc, fs):
    dt=1/fs
    f1_MHz=fft.fftfreq(len(data), dt)/1e6
    f0_MHz=fc/1e6
    phase=dm/2.41e-10*f1_MHz*f1_MHz/(f0_MHz*f0_MHz*(f0_MHz+f1_MHz))
    pf=np.exp(-2.0*np.pi*1j*phase)
    return fft.ifft(fft.fft(data)*pf)

def before_fold(data, period, nperiods):
    data=abs(data[:len(data)//(period*nperiods)*(period*nperiods)])**2
    return data.reshape(-1, period*nperiods)

def fold(data, period, nperiods):
    bf=before_fold(data, period, nperiods)
    return np.sum(bf, axis=0)
    
def lowpass(data):
    fd=fft.fft(data)
    fd[np.abs(fft.fftfreq(len(data)))>0.002]=0
    return fft.ifft(fd).real
    
def waterfall(data, nch):
    data=truncate(data, nch).reshape(-1, nch)
    fdata=fft.fft(data, axis=1)
    fdata[:,0:3]=0
    fdata[:,-2:]=0
    fdata=fft.fftshift(fdata, axes=1)
    return np.abs(fdata)**2

def remove_dc(fdata):
    nch=fdata.shape[1]
    fdata[:, nch//2-3:nch//2+4]=np.nan

def fold_waterfall(data, period, nperiods):
    nch=data.shape[1]
    data=data[:data.shape[0]//(period//nch*nperiods)*(period//nch*nperiods),:]
    result=np.zeros(((period//nch+1)*nperiods, nch))
    #for i in range(data.shape[0]//(period//nch*nperiods)):
    #    result+=data[i*result.shape[0]:(i+1)*result.shape[0], :]
    for i in range(data.shape[0]):
        j=int(i%(period/nch*nperiods))
        result[j,:]+=abs(data[i,:])**2
    
    return result

def slide_average(x, n): 
    result=[0.0]*len(x) 
    for i in range(len(x)): 
        w=0
        for j in range(-n//2, n//2+1): 
            if i+j<0 or i+j>=len(x): 
                continue 
            w+=1
            result[i]+=x[i+j]
        result[i]/=w
    return result


def mask_channel(fwf, nch):
    x=np.sum(fwf, axis=0)
    y1=np.array(slide_average(x, s1))
    y2=np.array(slide_average(x, s2))
    mask=y1>(1.1*y2)
    fwf[:, mask]=np.nan


def process_data(data, nch, period, nperiods, dm, fc, bw):
    dddata=dedisp_chirp(data, dm, fc, bw)
    folded=fold(dddata, period, nperiods)
    bf=before_fold(dddata, period, nperiods)
    fdata=waterfall(dddata, nch)
    remove_dc(fdata)
    fdata=fold_waterfall(fdata, period, nperiods)
    return folded, bf, fdata
        


if __name__=='__main__':
    dm=float(sys.argv[2])

    fc=float(sys.argv[3])*1e6

    bw=float(sys.argv[4])*1e6

    count=int(float(sys.argv[5]))

    period=int(float(sys.argv[6]))

    dt=1/bw*1000 #ms

    f=open(sys.argv[1], 'rb')

    s=f.seek(0,2)
    f.seek(0,0)

    nsamples=s//2

    ntimeslots=nsamples//period//2
    print(ntimeslots)

    bf_img=np.zeros((int(np.minimum(1000, ntimeslots)), period*nperiods))
    bf_count=np.zeros((int(np.minimum(1000, ntimeslots)), period*nperiods), dtype=int)
    

    data=read_data(f, count//period*period)
    folded, fdata=None, None
    #folded, bf, fdata=process_data(data, nch, period, nperiods, dm, fc, bw)
    
    nbytes=0
    islot=0
    while True:
    #for i in range(10):
        data=read_data(f, count//period*period)
        if data is None:
            break

        nbytes+=2*len(data)
        print("{0} bytes read".format(nbytes))
        

        if folded is None:
            folded, bf1, fdata=process_data(data, nch, period, nperiods, dm, fc, bw)
        else:
            folded1, bf1, fdata1=process_data(data, nch, period, 2, dm, fc, bw)
            folded+=folded1
            fdata+=fdata1
        for j in range(bf1.shape[0]):
            k=int(round(islot/ntimeslots*bf_img.shape[0]))
            print(islot, k)
            bf_img[k, :]+=bf1[j,:]
            bf_count[k, :]+=1
            islot+=1
        
    bf_img[bf_count==0]=np.nan
    bf_count[bf_count==0]=1
    bf_img/=bf_count
    
    gs = GridSpec(3, 1)
    gs.update(hspace=0., wspace=0)

    folded=lowpass(folded)

    for i in range(0, bf_img.shape[0]):
        bf_img[i, :]=lowpass(bf_img[i, :])

    print(folded.shape)
    ax = plt.subplot(gs[0, 0])
    ax.plot([dt*i for i in range(len(folded))],folded, 'k')
    ax.set_xlim(0.0, dt*(len(folded)-1))
    ax.set_ylabel("Amplitude", fontsize=15)
    ax.set_yticks([])
    ax.xaxis.tick_top()
    ax.xaxis.set_label_position("top")
    ax.set_xlabel("Time (ms)", fontsize=15)
    for i in ax.get_xticklabels()+ax.get_yticklabels():
        i.set_fontsize(15)
    for i in ax.get_yticklabels():
        i.set_visible(False)

    ax = plt.subplot(gs[1, 0])
    ax.imshow(bf_img[::-1, :], aspect='auto', 
        extent=[0, 2, 0, ntimeslots*dt*nperiods*period/1000.0],
        cmap=plt.cm.gray_r)
    #plt.show()
    ax.set_ylabel("Time (s)", fontsize=15)
    for i in ax.get_xticklabels()+ax.get_yticklabels():
        i.set_fontsize(15)

    for i in ax.get_xticklabels()+[ax.get_yticklabels()[0]]:
        i.set_visible(False)

    mask_channel(fdata, nch)
    #waterfall_fig=np.log10(fdata.T[::-1,:])*10
    waterfall_fig=np.sqrt(fdata.T[::-1,:])
    
    ax = plt.subplot(gs[2, 0])
    ax.imshow(waterfall_fig[:,:],  aspect='auto',
        cmap=plt.cm.gray_r, 
        extent=[0, 2.0, (fc-bw/2)/1e6, (fc+bw/2)/1e6])
    #plt.savefig('waterfall.pdf')
    for i in ax.get_xticklabels()+ax.get_yticklabels():
        i.set_fontsize(15)
    ax.set_ylabel("$\\nu$ (MHz)", fontsize=15)
    ax.set_xlabel("phase/$360^\\circ$", fontsize=15)
    plt.tight_layout()
    plt.savefig('result.pdf')
    #plt.show()
