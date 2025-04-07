Circuit.cpp

stores information about the circuit, including the list of nodes and the database about the different gates.
Includes function to import circuit from the circuit file

CircuitNode.cpp

store information about a given node, which is a logic gate in this sense. It stores info about the gates delays, arrival time, slack, and more

GateDatabase.cpp

stores information about the types of gates in the circuit, includes functions to import gate data from library file.

main.cpp

contains functions nessecary for PA1, forward traversal, backward traversal, critical path finder and the final output function as well as helper functions

Makefile

allows for easy compilation of the project, simply compile the progrram by running 'make' in the terminal. The executable will be 'sta'

Link to GitHub incase the project is missing files
https://github.com/Hymes-Git/EE5301

In addition if were supposed to output the results to cout AND the ckt_traversal.txt file, the "outputCircuitTraversal" function call in main can have the 0 changed to a 1 to use cout