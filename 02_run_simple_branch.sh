pushd .
cd examples/c_simple_branch
make
popd
../pin-3.27-98718-gbeaa5d51e-gcc-linux/pin -t ./obj-intel64/CodeCoverage.so -- examples/c_simple_branch/c_simple_branch 1 2 
