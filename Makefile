TARGETS=tx_pulsar hackrf_daq
all: $(TARGETS)

LIBS=-luhd -lboost_program_options -pthread -ldl -lcufftw

pulsar.o: pulsar.cpp pulsar.hpp
	g++ -c -o $@ $< -O3 -fopenmp


tx_pulsar: tx_pulsar.cpp pulsar.o
	g++ $< -o $@ -O3 pulsar.o $(LIBS) -fopenmp

hackrf_daq: hackrf_daq.cpp
	g++ $< -o $@ -O3 -lhackrf $(LIBS) -fopenmp

clean:
	rm -rf lib $(TARGETS)
