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
#include<stack>
#include <algorithm>

using namespace std;

struct ParenCommaEq_is_space : std::ctype<char>{
    ParenCommaEq_is_space():std::ctype<char>(get_table()) {}
    static mask const* get_table(){
        static mask rc[table_size];
        rc['(']=std::ctype_base::space;
        rc[')']=std::ctype_base::space;
        rc[',']=std::ctype_base::space;
        rc['=']=std::ctype_base::space;
        rc[' ']=std::ctype_base::space;
        rc['\t']=std::ctype_base::space;
        return &rc[0];
        }};


//I got the following function from https://www.delftstack.com/howto/cpp/how-to-determine-if-a-string-is-number-cpp/
bool isNumber(const string& str)
{
    for (char const &c : str) {
        if (std::isdigit(c) == 0) return false;
    }
    return true;
}


struct gatelist{
    string gname;
    //int fanout;
    vector<int> fanin;
    vector<int> fo;
    int po=0;
    int dfo=0;
    double tot_cap=0.0;
    double arrival_time=0.0;
    double output_slew=0.0;
    double delay;
    double rat=1000000000;
    double slack;
    int worst_fanin_node;
    map<int,double> dv;
}gg;

gatelist EmptyStruct;


void insert_fanout(vector<gatelist>gl[])
{
    for(int it=0;it<300000;it++){
        if(gl[it].size()!=0){
            for(auto& a: gl[it])
            {
                if(a.gname!="INPUT"||a.gname!="OUTPUT")
                {
                for(auto& b:a.fanin)
                {
                    if(gl[b].size()!=0){
                        for(auto& c:gl[b])
                    c.fo.push_back(it);
                    }
                    
                }
                
            }
            }
            
        }
        else
        continue;
    }

   

}



void insert_data(vector<gatelist>gl[],int node_name,gatelist gg)
{
    
    if (gl[node_name].size()==0)
        gl[node_name].push_back(gg);
    else
    {
        for(auto& c:gl[node_name])
        {
            if(c.gname=="OUTPUT")
            {
                c.gname=gg.gname;
                c.po=1;
                c.fanin=gg.fanin;
                c.fo=gg.fo;
                if (gg.dfo==1)
                    c.dfo=1;
            }
            else{
                c.po=1;
                if (gg.dfo=1)
                    c.dfo=1;
            }
        }
    }
    
}


void cktfileparsing(char *fname2,vector<gatelist>gl[])
{
    char lineBuf[1024];
    //cout << "Parsing input file " << fname2 << " using C++ syntax." << endl;
    ifstream ifs(fname2);
    if (ifs.is_open() == 0)
    { // or we could say if (!ifs)
        cout << "Error opening file " << fname2 << endl;
        //return -1;
    }
    while (ifs.good()) 
    {
        ifs.getline(lineBuf, 1023);// read one linestring 
        string lineStr(lineBuf); // convert to C++ string
        if (lineStr.empty())// is empty line?
            continue;
        transform(lineStr.begin(),lineStr.end(),lineStr.begin(),::toupper);
        istringstream iss(lineStr); 
        iss.imbue(locale(cin.getloc(),new ParenCommaEq_is_space));
        string firstWord;
        iss >> firstWord;
        if ((isNumber(firstWord)) && stoi(firstWord)!=0)
        {
          int fanout_1 = stoi(firstWord);
          string sec;
          iss >> sec;
          if(sec=="DFF"){
              gg.gname="INPUT";
              string fourth;
              iss>>fourth;
              insert_data(gl,fanout_1,gg);
              gg=EmptyStruct;
              gg.gname="OUTPUT";
              gg.dfo=1;
              insert_data(gl,stoi(fourth),gg);
              gg=EmptyStruct;
          }
          else{
          gg.gname=sec;
          string third;
          while(iss >> third)
          {
              if (isNumber(third)){
                gg.fanin.push_back(stoi(third));
                
              }
                
          }
        insert_data(gl,fanout_1,gg);
        
        gg = EmptyStruct;
        }
        }
        
        else {
            if (firstWord=="INPUT" || firstWord=="OUTPUT"){
                gg.gname = firstWord;
                string sec;
                iss>>sec;
                int fanout_1=stoi(sec);
                insert_data(gl,fanout_1,gg);
                gg = EmptyStruct;
            }
        }
        
    }
}

//using idea from https://www.geeksforgeeks.org/topological-sorting/
void topological_sort(vector<gatelist> graph[], vector<bool>& visited, list<int>& result, int node)
{
    //cout<<"Inside Top sort"<<endl;
    
    visited[node] = true;
    for(auto& a: graph[node])
            {
                if(a.gname=="OUTPUT")
                continue;
                for(auto& b: a.fanin)
                {
        if (!visited[b])
        {
           topological_sort(graph, visited, result, b);
           
        }
    }
    
    result.push_front(node);
    
}
}

list<int> top_call(vector<gatelist>gl[])
{
    vector<bool> visited(300000, false);
    list<int> result ;
    for (long i=1; i < 300000; ++i)
        if (!visited[i])
        {
           topological_sort(gl, visited, result, i);
        }
    result.reverse();
    return result;
}
