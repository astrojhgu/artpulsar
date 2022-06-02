TARGETS=tx_pulsar #hackrf_daq hackrf_daq_fb single_tone
all: $(TARGETS)

LIBS=-luhd -lboost_program_options -lboost_system -pthread -ldl -lcufftw -g

pulsar.o: pulsar.cpp pulsar.hpp
	g++ -c -o $@ $< -O3 -fopenmp


tx_pulsar: tx_pulsar.cpp pulsar.o
	g++ $< -o $@ -O3 pulsar.o $(LIBS) -fopenmp

single_tone: single_tone.cpp pulsar.o
	g++ $< -o $@ -O3 pulsar.o $(LIBS) -fopenmp

hackrf_daq: hackrf_daq.cpp
	g++ $< -o $@ -O3 -lhackrf $(LIBS) -fopenmp

hackrf_daq_fb: hackrf_daq_fb.cpp fft_channelizer.hpp
	g++ $< -o $@ -O3 -lhackrf -lfftw3f -lfftw3 -lboost_program_options -lboost_system -pthread -fopenmp -g

clean:
	rm -rf lib $(TARGETS) *.o
