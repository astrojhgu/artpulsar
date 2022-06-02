#!/usr/bin/env python3

import matplotlib.pylab as plt
import numpy as np
import numpy.fft as fft
import sys

if len(sys.argv)<4:
    print("Usage:{0} <iq_dump file> <fc in Hz> <rate in Hz> [fft len]".format(sys.argv[0]))
    sys.exit(-1)




raw_data=np.fromfile(sys.argv[1], dtype='int8')
iq_data=raw_data[::2]+1j*raw_data[1::2]
fft_len=sys.argv[4] if len(sys.argv)>4 else 1024
fc=float(sys.argv[2])
rate=float(sys.argv[3])

fmin=fc-rate/2
fmax=fc+rate/2
dt=1/rate

truncated_data_len=(len(iq_data)//fft_len)*fft_len
truncated_data=iq_data[:truncated_data_len].reshape((-1, fft_len))

print("truncated data len={0}".format(truncated_data_len))

fb=fft.fftshift(fft.fft(truncated_data, axis=1), axes=1).T

plt.imshow(np.abs(fb[::-1,:])**2, extent=[0, fft_len*dt*fb.shape[1]*1000, fmin/1e6, fmax/1e6], aspect='auto')
plt.xlabel('time (ms)')
plt.ylabel('freq (MHz)')
plt.show()


