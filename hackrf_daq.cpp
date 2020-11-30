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


namespace po = boost::program_options;
using namespace std;

constexpr double SAMP_RATE=10e6;
double FREQ_CENTRE_MHZ=150.0;
size_t cnt=0;
ofstream ofs;

int config_hackrf( hackrf_device * & dev, const int16_t & gain)
{
    unsigned int lna_gain=40; // default value
    unsigned int vga_gain=40; // default value
    if (gain!=-9999)
        vga_gain = (gain/2)*2;

    int result = hackrf_init();
    
	if( result != HACKRF_SUCCESS ) {
		printf("config_hackrf hackrf_init() failed: %s (%d)\n", hackrf_error_name((hackrf_error)result), result);
        return(result);
	}

	result = hackrf_open(&dev);
    
	if( result != HACKRF_SUCCESS ) {
		printf("config_hackrf hackrf_open() failed: %s (%d)\n", hackrf_error_name((hackrf_error)result), result);
        return(result);
	}

    double sampling_rate = SAMP_RATE;
    // Sampling frequency
    result = hackrf_set_sample_rate_manual(dev, sampling_rate, 1);
    
	if( result != HACKRF_SUCCESS ) {
		printf("config_hackrf hackrf_sample_rate_set() failed: %s (%d)\n", hackrf_error_name((hackrf_error)result), result);
        return(result);
	}

    // Need to make more study in the future. temperily set it 0.
    result = hackrf_set_baseband_filter_bandwidth(dev, 5000000);
	if( result != HACKRF_SUCCESS ) {
		printf("config_hackrf hackrf_baseband_filter_bandwidth_set() failed: %s (%d)\n", hackrf_error_name((hackrf_error)result), result);
		return(result);
	}

    result = hackrf_set_vga_gain(dev, vga_gain);
	result |= hackrf_set_lna_gain(dev, lna_gain);

    if( result != HACKRF_SUCCESS ) {
		printf("config_hackrf hackrf_set_vga_gain hackrf_set_lna_gain failed: %s (%d)\n", hackrf_error_name((hackrf_error)result), result);
		return(result);
	}

    // Center frequency
    result = hackrf_set_freq(dev, FREQ_CENTRE_MHZ*1e6);
    if( result != HACKRF_SUCCESS ) {
        printf("config_hackrf hackrf_set_freq() failed: %s (%d)\n", hackrf_error_name((hackrf_error)result), result);
        return(result);
    }
    
    return(result);
}

int rx_callback(hackrf_transfer* transfer) {
    ofs.write((char*)transfer->buffer, transfer->valid_length);
    cnt+=1;
    if (cnt%(1000)==0){
        std::cerr<<"cb called "<<cnt<<" times"<<std::endl;
    }
	return(0);
}


int rx(hackrf_device *dev)
{
    int result = 0;

    result = hackrf_start_rx(dev, rx_callback, NULL);
    // printf("2\n");
	// while( (hackrf_is_streaming(device) == HACKRF_TRUE) &&(do_exit == false) )
    cout<<"result="<<result<<std::endl;
    cout<<hackrf_is_streaming(dev)<<std::endl;
	while( (hackrf_is_streaming(dev) == HACKRF_TRUE) )
	{
        std::this_thread::sleep_for(1s);
        //sleep(1);
        cerr<<".";
    }
    cout<<"stopped"<<std::endl;

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

	// hackrf_exit();
	fprintf(stderr, "hackrf_exit() done\n");


    return result;
}



int main(int argc, char* argv[]){
    std::string ofname;
    int16_t gain=0;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "help message")
        ("freq", po::value<double>(&FREQ_CENTRE_MHZ)->default_value(150), "freq in MHz")
        ("gain", po::value<int16_t>(&gain)->default_value(0), "gain")
        ("out", po::value<std::string>(&ofname)->default_value("/dev/stdout"), "outfile name")
        ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);


    // print the help message
    if (vm.count("help")) {
        std::cout << boost::format("acquiring pulsar signal %s") % desc << std::endl;
        return ~0;
    }

    ofs.open(ofname.c_str());

    std::cerr<<"freq= "<<FREQ_CENTRE_MHZ<<std::endl;


    hackrf_device *hackrf_dev = nullptr;
    int result = 0;
    result=config_hackrf(hackrf_dev, gain);
    
    if (result ==0)
    {
        cout << "OK!\n";
        rx(hackrf_dev);
    }
    else
    {
        cout << "HACKRF device not FOUND!\n";
    }    
}
