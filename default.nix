with import <nixpkgs> {};
stdenv.mkDerivation {
    name = "mpi_rust"; # Probably put a more meaningful name here
    buildInputs = [
        cudaPackages.libcufft.all
        cudaPackages.cuda_nvcc
        cudaPackages.cuda_cudart
        uhd.dev
        boost.dev
        hackrf
        fftwFloat.dev
        fftw.dev
    ];
    hardeningDisable = [ "all" ];
    #buildInputs = [gcc-unwrapped gcc-unwrapped.out gcc-unwrapped.lib];
    
}
