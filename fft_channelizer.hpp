#ifndef FFT_CH
#define FFT_CH

#include <fftw3.h>
#include <vector>
#include <complex>
#include <iostream>

class channelizer{
private:
    size_t nch;
    fftwf_plan pf;
public:
    channelizer(size_t nch)
    :pf(fftwf_plan_dft_1d(nch, nullptr, nullptr, FFTW_FORWARD, FFTW_ESTIMATE)),
    nch(nch)
    {
    }

    std::vector<std::complex<float>> exec(char* buffer, size_t size){
        assert((size/2)%nch==0);
        std::vector<std::complex<float>> buffer_f(size/2);
        for (int i=0;i<size/2;i+=1){
            buffer_f[i]=std::complex<float>((float)buffer[i*2], -(float)buffer[i*2+1]);
        }
        for (int i=0;i<(size/2)/nch;++i){
            fftwf_execute_dft(pf, (fftwf_complex*)&buffer_f[i*nch], (fftwf_complex*)&buffer_f[i*nch]);
        }
        return buffer_f;
    }

    template<typename T>
    void fftshift(T* data, size_t len){
        size_t s2=len/2;
        for (int i=0;i<s2;++i){
            std::swap(data[i], data[s2+i]);
        }
        for(int i=-2;i<3;++i){
            data[s2+i]=0.0;
        }
    }

    std::vector<float> spec(char* buffer, size_t size){
        std::vector<std::complex<float>> buf=this->exec(buffer, size);
        std::vector<float> result(buf.size());
        for (int i=0;i<buf.size();++i){
            result[i]=std::norm(buf[i]);
        }
        for (int i=0;i<buf.size()/nch;++i){
            fftshift(result.data()+i*nch, nch);
        }
        assert(result.size()==size/2);
        return result;
    }
};


#endif
