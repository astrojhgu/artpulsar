//
// Copyright 2011-2012,2014 Ettus Research LLC
// Copyright 2018 Ettus Research, a National Instruments Company
//
// SPDX-License-Identifier: GPL-3.0-or-later
//

#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/utils/thread.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <complex>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>

#include "bufq.hpp"

namespace po = boost::program_options;
using std::complex;

extern "C"{
    void init_pulsar(size_t nch, double fs, double dm,double period_s, double sigma, size_t ch_min);
    void fill_buf(std::complex<int16_t>* ptr, size_t len);
    size_t signal_length();
}

static bool stop_signal_called = false;
void sig_int_handler(int)
{
    stop_signal_called = true;
}


void send_signal(
    uhd::tx_streamer::sptr tx_stream, 
    double period_ms, 
    double dm, 
    size_t npp, 
    double fmax_Hz, 
    size_t ch_max, size_t ch_min, double sigma=0.05)
{
    init_pulsar(ch_max, fmax_Hz*2, dm, period_ms/1000.0, sigma, ch_min);
    size_t nperiods=npp;
    std::cerr<<"npp="<<nperiods<<std::endl;
    std::cerr<<"dm="<<dm<<std::endl;
    std::cerr<<"period="<<period_ms<<" ms"<<std::endl;
    std::cerr<<"chmax="<<ch_max<<std::endl;
    std::cerr<<"chmin="<<ch_min<<std::endl;
    std::cerr<<"fmax="<<fmax_Hz<<std::endl;
    std::cerr<<"fmin="<<fmax_Hz/ch_max*ch_min<<std::endl;
    size_t sl=signal_length();
    uhd::tx_metadata_t md;
    md.start_of_burst = false;
    md.end_of_burst   = false;

    BufQ<std::vector<std::complex<int16_t>>> queue{
        std::make_shared<std::vector<std::complex<int16_t>>>(nperiods*sl),
        std::make_shared<std::vector<std::complex<int16_t>>>(nperiods*sl),
        std::make_shared<std::vector<std::complex<int16_t>>>(nperiods*sl),
        std::make_shared<std::vector<std::complex<int16_t>>>(nperiods*sl)};

    std::thread th([&](){
        while(!stop_signal_called){
            queue.write([&](auto x){
                //sleep for a while to emulate
                //expensive computings.
                fill_buf(x.get()->data(), nperiods*sl);
            });
        }
    });


    // loop until the entire file has been read
    std::shared_ptr<std::vector<std::complex<int16_t>>> ptr=queue.fetch();
    while (not md.end_of_burst and not stop_signal_called) {
        if (!queue.filled_q.empty()){
            std::cout<<"new data comming"<<std::endl;
            ptr=queue.fetch();
        }else{
            std::cout<<"reusing old data"<<std::endl;
        }
        
        const size_t samples_sent=tx_stream->send(&ptr->front(), ptr->size(), md);
        
        if (samples_sent != ptr->size()) {
            UHD_LOG_ERROR("TX-STREAM",
                "The tx_stream timed out sending " << ptr->size() << " samples ("
                                                   << samples_sent << " sent).");
            return;
        }

        
	    std::cerr<<".";
    }
    th.join();   
}

int UHD_SAFE_MAIN(int argc, char* argv[])
{
    // variables to be set by po
    std::string args, type, ant, subdev, ref, wirefmt, channel;
    
    double dm=0, period_ms=50.0;
    
    double rate, freq, gain, bw, lo_offset;

    size_t nperiod_per_shoot=10;
    size_t nch=32768;
    double sigma=0.05;
    // setup the program options
    po::options_description desc("Allowed options");
    // clang-format off
    desc.add_options()
        ("help", "help message")
        ("args", po::value<std::string>(&args)->default_value(""), "multi uhd device address args")
        ("dm", po::value<double>(&dm)->default_value(0.0), "dm value")
        ("period", po::value<double>(&period_ms)->default_value(50.0), "period in ms")
        ("sigma", po::value<double>(&sigma)->default_value(0.05), "sigma<0.5")
        ("npp", po::value<size_t>(&nperiod_per_shoot)->default_value(10), "nperiod per shoot")
        ("nch", po::value<size_t>(&nch)->default_value(32768), "num of ch")
        ("rate", po::value<double>(&rate), "rate of outgoing samples")
        ("freq", po::value<double>(&freq), "RF center frequency in Hz")
        ("lo-offset", po::value<double>(&lo_offset)->default_value(0.0),
            "Offset for frontend LO in Hz (optional)")
        ("gain", po::value<double>(&gain), "gain for the RF chain")
        ("ant", po::value<std::string>(&ant), "antenna selection")
        ("subdev", po::value<std::string>(&subdev), "subdevice specification")
        ("bw", po::value<double>(&bw), "analog frontend filter bandwidth in Hz")
        ("ref", po::value<std::string>(&ref)->default_value("internal"), "reference source (internal, external, mimo)")
        //("wirefmt", po::value<std::string>(&wirefmt)->default_value("sc16"), "wire format (sc8 or sc16)")
        ("channel", po::value<std::string>(&channel)->default_value("0"), "which channel to use")
        ("int-n", "tune USRP with integer-n tuning")
    ;
    // clang-format on
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // print the help message
    if (vm.count("help")) {
        std::cout << boost::format("transmitting pulsar signal %s") % desc << std::endl;
        return ~0;
    }

    // create a usrp device
    std::cout << std::endl;
    std::cout << boost::format("Creating the usrp device with: %s...") % args
              << std::endl;
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args);

    // Lock mboard clocks
    if (vm.count("ref")) {
        usrp->set_clock_source(ref);
    }

    // always select the subdevice first, the channel mapping affects the other settings
    if (vm.count("subdev"))
        usrp->set_tx_subdev_spec(subdev);

    std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;

    // set the sample rate
    if (not vm.count("rate")) {
        std::cerr << "Please specify the sample rate with --rate" << std::endl;
        return ~0;
    }
    std::cout << boost::format("Setting TX Rate: %f Msps...") % (rate / 1e6) << std::endl;
    usrp->set_tx_rate(rate);
    std::cout << boost::format("Actual TX Rate: %f Msps...") % (usrp->get_tx_rate() / 1e6)
              << std::endl
              << std::endl;

    // set the center frequency
    if (not vm.count("freq")) {
        std::cerr << "Please specify the center frequency with --freq" << std::endl;
        return ~0;
    }
    std::cout << boost::format("Setting TX Freq: %f MHz...") % (freq / 1e6) << std::endl;
    std::cout << boost::format("Setting TX LO Offset: %f MHz...") % (lo_offset / 1e6)
              << std::endl;
    uhd::tune_request_t tune_request;
    tune_request = uhd::tune_request_t(freq, lo_offset);
    if (vm.count("int-n"))
        tune_request.args = uhd::device_addr_t("mode_n=integer");
    usrp->set_tx_freq(tune_request);
    std::cout << boost::format("Actual TX Freq: %f MHz...") % (usrp->get_tx_freq() / 1e6)
              << std::endl
              << std::endl;

    // set the rf gain
    if (vm.count("gain")) {
        std::cout << boost::format("Setting TX Gain: %f dB...") % gain << std::endl;
        usrp->set_tx_gain(gain);
        std::cout << boost::format("Actual TX Gain: %f dB...") % usrp->get_tx_gain()
                  << std::endl
                  << std::endl;
    }

    // set the analog frontend filter bandwidth
    if (vm.count("bw")) {
        std::cout << boost::format("Setting TX Bandwidth: %f MHz...") % (bw / 1e6)
                  << std::endl;
        usrp->set_tx_bandwidth(bw);
        std::cout << boost::format("Actual TX Bandwidth: %f MHz...")
                         % (usrp->get_tx_bandwidth() / 1e6)
                  << std::endl
                  << std::endl;
    }

    // set the antenna
    if (vm.count("ant"))
        usrp->set_tx_antenna(ant);

    // allow for some setup time:
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Check Ref and LO Lock detect
    std::vector<std::string> sensor_names;
    sensor_names = usrp->get_tx_sensor_names(0);
    if (std::find(sensor_names.begin(), sensor_names.end(), "lo_locked")
        != sensor_names.end()) {
        uhd::sensor_value_t lo_locked = usrp->get_tx_sensor("lo_locked", 0);
        std::cout << boost::format("Checking TX: %s ...") % lo_locked.to_pp_string()
                  << std::endl;
        UHD_ASSERT_THROW(lo_locked.to_bool());
    }
    sensor_names = usrp->get_mboard_sensor_names(0);
    if ((ref == "mimo")
        and (std::find(sensor_names.begin(), sensor_names.end(), "mimo_locked")
             != sensor_names.end())) {
        uhd::sensor_value_t mimo_locked = usrp->get_mboard_sensor("mimo_locked", 0);
        std::cout << boost::format("Checking TX: %s ...") % mimo_locked.to_pp_string()
                  << std::endl;
        UHD_ASSERT_THROW(mimo_locked.to_bool());
    }
    if ((ref == "external")
        and (std::find(sensor_names.begin(), sensor_names.end(), "ref_locked")
             != sensor_names.end())) {
        uhd::sensor_value_t ref_locked = usrp->get_mboard_sensor("ref_locked", 0);
        std::cout << boost::format("Checking TX: %s ...") % ref_locked.to_pp_string()
                  << std::endl;
        UHD_ASSERT_THROW(ref_locked.to_bool());
    }

    // set sigint if user wants to receive
    
    std::signal(SIGINT, &sig_int_handler);
    std::cout << "Press Ctrl + C to stop streaming..." << std::endl;

    // create a transmit streamer
    std::string cpu_format;
    std::vector<size_t> channel_nums;
    cpu_format = "sc16";
    wirefmt="sc16";
    uhd::stream_args_t stream_args(cpu_format, wirefmt);
    channel_nums.push_back(boost::lexical_cast<size_t>(channel));
    stream_args.channels             = channel_nums;
    uhd::tx_streamer::sptr tx_stream = usrp->get_tx_stream(stream_args);

    // send from file
    
    double fmax=freq+rate/2;
    double fmin=freq-rate/2;
    double df=rate/nch;
    size_t ch_max=fmax/df;
    //size_t ch_min=fmin/df;
    size_t ch_min=ch_max-nch;
    

    send_signal(tx_stream, period_ms, dm, nperiod_per_shoot, fmax, ch_max, ch_min, sigma);
    
    // finished
    std::cout << std::endl << "Done!" << std::endl << std::endl;

    return EXIT_SUCCESS;
}
