#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <libhackrf/hackrf.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>
#include "fft_channelizer.hpp"

namespace po = boost::program_options;
using namespace std;

double SAMP_RATE_MHz=10.0;
double FREQ_CENTRE_MHz=150.0;
size_t cnt=0;
ofstream ofs;
size_t nsamples=size_t(SAMP_RATE_MHz*1e6*60);
channelizer* chptr=nullptr;



int config_hackrf( hackrf_device * & dev, const int16_t & gain)
{
    unsigned int lna_gain=40; // default value
    unsigned int vga_gain=40; // default value
    if (gain!=-9999)
        vga_gain = (gain/2)*2;

    int result = hackrf_init();
    
	if( result != HACKRF_SUCCESS ) {
		//printf("config_hackrf hackrf_init() failed: %s (%d)\n", 
        //hackrf_error_name((hackrf_error)result), result);
        std::cerr<<"config_hackrf hackrf_init() failed:"<<hackrf_error_name((hackrf_error)result)<<" "<<result<<std::endl;
        return(result);
	}

	result = hackrf_open(&dev);
    //result = hackrf_reset(dev);
	if( result != HACKRF_SUCCESS ) {
		std::cerr<<"config_hackrf hackrf_init() failed:"<<hackrf_error_name((hackrf_error)result)<<" "<<result<<std::endl;
        return(result);
	}

    double sampling_rate = SAMP_RATE_MHz*1e6;
    // Sampling frequency
    result = hackrf_set_sample_rate_manual(dev, sampling_rate, 1);
    //result |= hackrf_set_hw_sync_mode(dev, 1);
	if( result != HACKRF_SUCCESS ) {
		std::cerr<<"config_hackrf hackrf_init() failed:"<<hackrf_error_name((hackrf_error)result)<<" "<<result<<std::endl;
        return(result);
	}

    // Need to make more study in the future. temperily set it 0.
    //result = hackrf_set_baseband_filter_bandwidth(dev, 5000000);
	if( result != HACKRF_SUCCESS ) {
		std::cerr<<"config_hackrf hackrf_init() failed:"<<hackrf_error_name((hackrf_error)result)<<" "<<result<<std::endl;
		return(result);
	}

    result = hackrf_set_vga_gain(dev, vga_gain);
	result |= hackrf_set_lna_gain(dev, lna_gain);
    if( result != HACKRF_SUCCESS ) {
		std::cerr<<"config_hackrf hackrf_init() failed:"<<hackrf_error_name((hackrf_error)result)<<" "<<result<<std::endl;
		return(result);
	}

    // Center frequency
    result = hackrf_set_freq(dev, FREQ_CENTRE_MHz*1e6);
    if( result != HACKRF_SUCCESS ) {
        std::cerr<<"config_hackrf hackrf_init() failed:"<<hackrf_error_name((hackrf_error)result)<<" "<<result<<std::endl;
        return(result);
    }
    
    return(result);
}





int rx_callback(hackrf_transfer* transfer) {
    auto buf=chptr->spec((char*)transfer->buffer, transfer->buffer_length);
    //std::cerr<<buf.size()<<std::endl;
    ofs.write((char*)buf.data(), buf.size()*sizeof(float));
    assert(ofs.good());
    cnt+=1;
    if (cnt*transfer->valid_length>2*nsamples){
        exit(0);
    }
    
	return(0);
}


int rx(hackrf_device *dev)
{
    int result = 0;
    //hackrf_reset(dev);

    result = hackrf_start_rx(dev, rx_callback, NULL);
    // printf("2\n");
	// while( (hackrf_is_streaming(device) == HACKRF_TRUE) &&(do_exit == false) )
    cerr<<"result="<<result<<std::endl;
    cerr<<hackrf_is_streaming(dev)<<std::endl;
	while( (hackrf_is_streaming(dev) == HACKRF_TRUE) )
	{
        std::this_thread::sleep_for(1s);
        //sleep(1);
        cerr<<".";
    }
    cerr<<"stopped"<<std::endl;

	result = hackrf_stop_rx(dev);

    if( result != HACKRF_SUCCESS ) {
		// fprintf(stderr, "hackrf_stop_rx() failed: %s (%d)\n", hackrf_error_name(result), result);
		fprintf(stderr, "hackrf_stop_rx() failed: (%d)\n", result);
	}else {
		fprintf(stderr, "hackrf_stop_rx() done\n");
	}
  printf("\n");

	result = hackrf_close(dev);
	if(result != HACKRF_SUCCESS) {
		// fprintf(stderr, "hackrf_close() failed: %s (%d)\n", hackrf_error_name(result), result);
		fprintf(stderr, "hackrf_close() failed: (%d)\n", result);
	} else {
		fprintf(stderr, "hackrf_close() done\n");
	}
    hackrf_close(dev);
	hackrf_exit();
	fprintf(stderr, "hackrf_exit() done\n");


    return result;
}



int main(int argc, char* argv[]){
    std::string ofname;
    int16_t gain=0;
    size_t nch=0;
    po::options_description desc("Allowed options");
    double secs=10;
    desc.add_options()
        ("help", "help message")
        ("freq", po::value<double>(&FREQ_CENTRE_MHz)->default_value(150), "freq in MHz")
        ("bw", po::value<double>(&SAMP_RATE_MHz)->default_value(2), "freq in MHz")
        ("secs", po::value<double>(&secs)->default_value(10), "daq secs of data")
        ("gain", po::value<int16_t>(&gain)->default_value(0), "gain")
        ("out", po::value<std::string>(&ofname)->default_value("/dev/stdout"), "outfile name")
        ("nch", po::value<size_t>(&nch)->default_value(512), "nch")
        ;

    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    std::cerr<<nch<<std::endl;
    
    chptr=new channelizer(nch);

    nsamples=secs*SAMP_RATE_MHz*1e6;


    // print the help message
    if (vm.count("help")) {
        std::cerr << boost::format("acquiring pulsar signal %s") % desc << std::endl;
        return ~0;
    }


    ofs.open(ofname.c_str(), std::ios_base::app|std::ios_base::binary);

    std::cerr<<"freq= "<<FREQ_CENTRE_MHz<<std::endl;
    std::cerr<<"bw= "<<SAMP_RATE_MHz<<std::endl;


    hackrf_device *hackrf_dev = nullptr;
    int result = 0;
    result=config_hackrf(hackrf_dev, gain);
    
    if (result ==0)
    {
        cerr << "OK!\n";
        rx(hackrf_dev);
    }
    else
    {
        cerr << "HACKRF device not FOUND!\n";
    }    
}
