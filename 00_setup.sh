# download and extract intel pin
pushd .
cd ..
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz
tar xf pin-3.28-98749-g6643ecee5-gcc-linux.tar.gz
popd

# build CodeCoverage(this repository code)
./build.sh

# build code coverage target example
pushd .
cd examples/c_function_call
make
popd

# run code coverage tool

