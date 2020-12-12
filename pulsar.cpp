#include <vector>
#include <complex>
#include "pulsar.hpp"
#include <fftw3.h>
#include <cassert>
#include <functional>
#include <random>
#include <iostream>
using namespace std;

constexpr double PI=3.14159265358979323846;

double calc_delay_s(double f_MHz, double dm){
    return 1.0 / 2.410331e-4 * dm / (f_MHz*f_MHz);
}

std::vector<double> fftfreq(int n){
    
    std::vector<double> result(n);
    for (int i=0;i<=(n-1)/2;++i){
        result[i]=(double)i/(double)n;
    }
    for (int i=-n/2;i<=-1;++i){
        result[i+n]=(double)i/(double)n;
    }
    return result;
}


void delay_signal(vector<complex<double>>& signal, 
const vector<complex<double>>& phase_factor, fftw_plan pf, fftw_plan pb){
    size_t n=signal.size();
    assert(phase_factor.size()==n);
    fftw_execute_dft(pf, (fftw_complex*)signal.data(), (fftw_complex*)signal.data());
    for (size_t i=0;i<n;++i){
        signal[i]*=phase_factor[i]/(double)n;
    }
    fftw_execute_dft(pb, (fftw_complex*)signal.data(), (fftw_complex*)signal.data());
}

template<typename RNGT>
void fill_signal(std::vector<complex<double>>& signal, size_t period_n, 
const std::function<double(double)>& profile, RNGT& gen){
    auto nsignal=signal.size()/period_n;
    assert(nsignal*period_n==signal.size());
    std::normal_distribution<> normal;
    for(int i=0;i<nsignal;++i){
        for (int j=0;j<period_n;++j){
            double phase=(double)j/(double)period_n-0.5;
            signal[i*period_n+j]=profile(phase)*complex<double>(normal(gen), normal(gen))/2.0;
        }
    }
}

complex<double> chirp(double dm, double f0_MHz, double f1_MHz){
    double phase=dm/2.41e-10*f1_MHz*f1_MHz/(f0_MHz*f0_MHz*(f0_MHz+f1_MHz));
    return exp(2.0*PI*complex<double>(0.0, 1.0)*phase);
}

double default_profile(double p){
    return exp(-p*p/(2.0*0.05*0.05));
}

std::tuple<function<void(vector<complex<double>>&)>, size_t> get_pulsar(
                                                            double fmin_MHz, 
                                                            double fmax_MHz, 
                                                            size_t period_n, 
                                                            double dm, 
                                                            size_t nperiods, 
                                                            const std::function<double(double)> profile){
    double dt=1.0/(fmax_MHz-fmin_MHz)/1e6;
    double dt_ms=dt*1000.0;
    std::cout<<"dt="<<dt<<std::endl;
    
    //size_t period_n=round(period_ms/dt_ms);
    
    size_t signal_length=period_n*nperiods;
    fftw_plan pf=fftw_plan_dft_1d(signal_length, nullptr, nullptr, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_plan pb=fftw_plan_dft_1d(signal_length, nullptr, nullptr, FFTW_BACKWARD, FFTW_ESTIMATE);

    double bw_Hz=(fmax_MHz-fmin_MHz)*1e6;
    double fc_Hz=(fmax_MHz+fmin_MHz)*1e6/2.0;

    auto freq=fftfreq(signal_length);
    vector<complex<double>> phase_factor(signal_length);
    for(size_t i=0;i<signal_length;++i){
        auto freq1=fc_Hz+freq[i]*bw_Hz;
        //double delay=calc_delay_s(freq1/1e6, dm);
        //double dphi=delay*freq1*2.0*3.14159265358979323846;
        //phase_factor[i]=exp(complex<double>(0.0, 1.0)*dphi);
        phase_factor[i]=chirp(dm, fc_Hz/1e6, freq[i]*bw_Hz/1e6);
    }
    
    

    auto ff=[=](vector<complex<double>>& buf){
        std::random_device rd{};
        std::mt19937 gen{rd()};
        assert(buf.size()==signal_length);
        fill_signal(buf, period_n, profile, gen);
        delay_signal(buf, phase_factor, pf, pb);
    };

    return std::make_tuple(ff, signal_length);
}
