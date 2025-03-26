File Descriptions:

libertyfile_1.hpp: Contains the parsing of the NLDM file and stores the relevant data in a map for each gate type.

cktfile_1.hpp: Contains the parsing of the circuit in which a graph of the circuit is created.

nested_1.hpp: Contains the forward and backward of the topologically sorted graph of the circuit and calculates the slack at each node.

soham_5618068_ee5301_PA1.cpp: The main file which encapsulates all the above header files and passes the arguments passed to it to the required functions.


make main: This command is to be used for creating the executable file "sta.out".

make run: This command runs the executable "sta.out" with the arguments NLDM_lib_max2Inp and b19_1.isc. For other circuit files, the argument b19_1.isc has to be changed to the relevant circuit file name. 

####The scripts have been tested on case-kh4250-02.cselabs.umn.edu machine#####