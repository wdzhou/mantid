#!/bin/bash
# Simple script to build and run the tests.
# Will run all tests in the directory if no arguments are supplied,
#      or alternatively just the test files given as arguments.
#
# This script is optimised for linuxs1 (i.e. it probably won't work anywhere else!)
#
# You will need to have the directories containing the Mantid and Third Party 
#      .so libraries in your LD_LIBRARY_PATH environment variable
#
# Author: Russell Taylor, 07/11/07
#

# Remove previously generated files to ensure that they're not inadvertently run
#   when something in the chain has failed.
rm -f *.log
rm -rf runner.*
rm -f *.properties

echo "Generating the source file from the test header files..."
# Chaining all tests together can have effects that you don't think of
#  - it's always a good idea to run your new/changed test on its own
if [ $# -eq 0 ]; then
	python ../../../Third_Party/src/cxxtest/cxxtestgen.py
else
	python ../../../Third_Party/src/cxxtest/cxxtestgen.py --error-printer -o runner.cpp $*
fi
echo

echo "Compiling the test executable..."
# -lboost_filesystem added for the SaveCSVTest test
g++ -O0 -g3 -o runner.exe runner.cpp -I ../inc -I ../../Kernel/inc -I ../../API/inc -I ../../DataObjects/inc \
            -I ../../Geometry/inc  -I ../../../Third_Party/src \
            -L../../Bin/Shared -L/opt/OpenCASCADE/lib64\
            -lMantidDataHandling -lMantidKernel -lMantidGeometry -lMantidAPI -lMantidDataObjects \
            -lPocoFoundation -lPocoUtil \
            -lboost_regex -lboost_signals -lboost_date_time \
            -lmuparser -lgsl -lgslcblas -lNeXus -lGL -lGLU \
            -lTKernel -lTKBO -lTKPrim -lTKMesh -lpython2.4
echo


echo "Running the tests..."
ln ../../Build/Tests/*.properties .
./runner.exe
echo

echo "Done."
