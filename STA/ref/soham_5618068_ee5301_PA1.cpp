#include <iostream>  // basic C++ input/output (e.g., cin, cout)
#include <fstream>    // needed to open files in C++
#include <sstream>   // needed if you are using sstream (C++)
#include <stdio.h>    // needed to open files in C
#include <string.h> // needed for C string processing (e.g., strstr)
#include <ctype.h>
#include<list>
#include<map>
#include<set>
#include<algorithm>
#include<vector>
#include<array>
#include<cstdarg>
#include <chrono>
#include <ctime> 
#include "nested_1.hpp"
#define MAX_NODES 300000

using namespace std;

int fpeek(FILE *stream);

int main(int argc, char *argv[])
{
    if (argc < 2) {
        cout << "I need two parameters, for this code to run." << endl;
        return -1;
    }
    auto start = std::chrono::system_clock::now();

    map<string, finalfile>m = parseFileCFormat(argv[1]);
    vector<gatelist> gl[300000];
    cktfileparsing(argv[2],gl);
    insert_fanout(gl);
    delay_calc(gl,m);
    auto end = std::chrono::system_clock::now();

    std::chrono::duration<double> elapsed_seconds = end-start;
    cout<<"##########Completed Processing############"<<endl;
    cout<<"Elapsed time="<<elapsed_seconds.count()<<"s\n";
    return 0;
}


// Kia got this function from stackoverflow https://stackoverflow.com/questions/2082743/c-equivalent-to-fstreams-peek
int fpeek(FILE *stream)
{
    int c;

    c = fgetc(stream);
    ungetc(c, stream);

    return c;
}










