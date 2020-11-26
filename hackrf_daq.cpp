#include <libhackrf/hackrf.h>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>
using namespace std;

constexpr double SAMP_RATE=10e6;
constexpr double FREQ_CENTRE=150e6;


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
    result = hackrf_set_freq(dev, FREQ_CENTRE);
    if( result != HACKRF_SUCCESS ) {
        printf("config_hackrf hackrf_set_freq() failed: %s (%d)\n", hackrf_error_name((hackrf_error)result), result);
        return(result);
    }
    
    return(result);
}

int rx_callback(hackrf_transfer* transfer) {
    static ofstream ofs("a.bin");
    ofs.write((char*)transfer->buffer, transfer->valid_length);
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



int main(){

    



    hackrf_device *hackrf_dev = nullptr;
    int result = 0;
    result=config_hackrf(hackrf_dev, 0);
    
    if (result ==0)
    {
        cout << "OK!\n";
    }
    else
    {
        cout << "HACKRF device not FOUND!\n";
    }

    rx(hackrf_dev);
}
