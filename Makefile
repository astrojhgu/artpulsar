TARGETS=tx_pulsar hackrf_daq
all: $(TARGETS)

LIBS=-luhd -lboost_program_options -pthread -ldl -lfftw3

pulsar.o: pulsar.cpp pulsar.hpp
	g++ -c -o $@ $< -O3


tx_pulsar: tx_pulsar.cpp pulsar.o
	g++ $< -o $@ -O3 pulsar.o $(LIBS)

hackrf_daq: hackrf_daq.cpp
	g++ $< -o $@ -O3 -lhackrf $(LIBS)

clean:
	rm -rf lib $(TARGETS)
