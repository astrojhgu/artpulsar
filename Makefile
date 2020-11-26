all: tx_pulsar hackrf_daq

LIBS=-luhd -lboost_program_options -pthread -L lib -lpulsar_signal -ldl


lib/libpulsar_signal.a: ../pulsar_signal/src/lib.rs
	cargo build --manifest-path ../pulsar_signal/Cargo.toml --release --out-dir ./lib -Z unstable-options

tx_pulsar: tx_pulsar.cpp lib/libpulsar_signal.a
	g++ $< -o $@ -O3 $(LIBS)

hackrf_daq: hackrf_daq.cpp
	g++ $< -o $@ -O3 -lhackrf $(LIBS)
