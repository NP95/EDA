# EDA

Implementation of some EDA tools to workout my C++ skills

1. STA Tool with Instrumentation - I had implemented the tool as part of my VLSI CAD assignment in Grad school, I wanted to implement a multi-threaded version however since my original implementation was using STL
containers which are not thread safe, I would have to rearchitect from scratch for multi threading. Instead I implemented a lightweight instrumentation library to debug the STA tool for intermediate steps, taking inspiration from Singleton pattern and the Facade Pattern. Detailed README.md is available in ref/instrumented library.

2. Fidducia Mattheyes Partitioning - Professor T.W Huang currently at Wisconsin, in his coursework has this assignment, since this had a binary file to check the results I implemented this. Detailed README is available in the root directory of the project.
