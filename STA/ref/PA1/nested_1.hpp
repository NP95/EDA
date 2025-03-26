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
#include<tuple>
#include<cmath>
#include "cktfile_1.hpp"
#include "libertyfile_1.hpp"

ofstream final_file("ckt_traversal.txt");


void print_fanin(int node,vector<gatelist>gl[])
    {
        for (auto& cc: gl[node])
        {
                if(cc.gname!="INPUT"){
                print_fanin(cc.worst_fanin_node,gl);
                }
                //cout<<cc.gname<<"-n"<<node<<","<<" "; 
                if(cc.po!=1)
                    final_file<<cc.gname<<"-n"<<node<<","<<" ";    
                else
                    final_file<<cc.gname<<"-n"<<node;
            }

        //}
        
    }
//using logic from https://www.geeksforgeeks.org/how-to-return-multiple-values-from-a-function-in-c-or-cpp/
tuple<double,double> findsmallgreat(double arr[],double target)
{
    double max=arr[6],min=arr[0];
    
for(int i=0;i<6;i++)
{
    if(target>=arr[i] && target<arr[i+1])
    {
        min=arr[i];
        max=arr[i+1];
    }
}
return make_tuple(min,max);

}

//I got the following snippet from https://www.geeksforgeeks.org/how-to-find-the-entry-with-largest-value-in-a-c-map/
//tuple<int, double, double> findEntryWithLargestValue(
//    map<int, delay_tab> sampleMap)
tuple<int, double, double> findEntryWithLargestValue(
    vector<gatelist>sampleMap[])
{
  
    // Reference variable to help find
    // the entry with the highest value
    tuple<int, double, double> entryWithMaxValue
        = make_tuple(0, 0.0,0.0);
  
    vector<gatelist>::iterator currentEntry;
    for (int i=1;i<300000;i++){    
        for(auto& klm: sampleMap[i]){
        if (klm.arrival_time
            > get<1>(entryWithMaxValue)){
  
            entryWithMaxValue
                = make_tuple(i,klm.arrival_time,klm.output_slew);
                    
        }
        }

    }
  
    return entryWithMaxValue;
}


void delay_calc(vector<gatelist>gl[],map<string, finalfile>m)
{
    list<int>ts = top_call(gl);
    for(auto& al:ts)
    {
        for(auto& bgl: gl[al])
        {
            if(bgl.gname=="INPUT") //for input nodes computed initialized arrival time and slew
            {
                bgl.output_slew = 0.002;
                bgl.arrival_time=0.0;
                continue;    
            }
            else
            {
                //finding capacitance of each node
            if (bgl.po==1)
            {
                bgl.tot_cap = 4*m["INV"].capacitance;//4 times capacitance for primary outputs
                if(bgl.fo.size()!=0)
                {
                    for(auto& volks: bgl.fo)
                    {
                        for(auto& wagen: gl[volks])
                            bgl.tot_cap+=m[wagen.gname].capacitance; //capacitance of fanout gates
                    }
                }
                
            }
            else
            {
            for(auto& cfo: bgl.fo) //looping through fanout of each node
            {
                for(auto& nco: gl[cfo]) //going into each fanout node & extracting its capacitance
                {
                    
                    bgl.tot_cap+=m[nco.gname].capacitance;//capacitance of non-primary output nodes
                }
                
            }
            }
        }
        }
        
    }
    
    for(auto& al:ts)
        {
            
            for(auto& bgl: gl[al])
            {
                
                double fc1 = 0.0,fc2=-1.0; int idxc1=0,idxc2=0; int og=0;
                double sl1=0.0,sl2=-1.0; int idxsl1=0,idxsl2=0; int ag=0;
                double final_delay_val=0.0, final_slew_val=0.0;
                int flagsl=-2,flagc=-2;
                int worst_node =0;
                double max_delay=0.0,worst_slew=0.0,max_arrival_time=0.0;
                map<int, double>dle;
                map<int, double>sle;
                tuple<double,int,int,int> slew_tup;
                map<int,tuple<double,int,int,int> >slew_table;
                tuple<double,int,int,int>cap_tup;
                tuple<int,double> max_slw;
                tuple<int,double> max_del;
                if(bgl.gname!="INPUT") 
                    {
                    if( find(begin(m[bgl.gname].cap_index), end(m[bgl.gname].cap_index), bgl.tot_cap) != end(m[bgl.gname].cap_index) )
                        {
                        idxc1 = distance(m[bgl.gname].cap_index, find(m[bgl.gname].cap_index, m[bgl.gname].cap_index + 7, bgl.tot_cap));
                        idxc2 = idxc1+1;
                        flagc=0;
                        
                        cap_tup = make_tuple(bgl.tot_cap,idxc1,idxc2,flagc);
                        
                        }
                    
                    else if(bgl.tot_cap<=m[bgl.gname].cap_index[0])
                        {
                        idxc1=0; idxc2=1; //og=1;
                        flagc=-1;
                        cap_tup = make_tuple(bgl.tot_cap,idxc1,idxc2,flagc); 
                        }
                        
                    
                    else if(bgl.tot_cap>=m[bgl.gname].cap_index[6])
                        {
                        idxc1=5; idxc2=6; 
                        flagc=-1;
                        cap_tup = make_tuple(bgl.tot_cap,idxc1,idxc2,flagc);
                        
                        }
                    else
                    {
                        
                        tie(fc1,fc2) = findsmallgreat(m[bgl.gname].cap_index, bgl.tot_cap);
                        flagc=1;
                        
                        idxc1 = distance(m[bgl.gname].cap_index, find(m[bgl.gname].cap_index, m[bgl.gname].cap_index + 7, fc1));
                        idxc2=idxc1+1;
                        
                        cap_tup = make_tuple(bgl.tot_cap,idxc1,idxc2,flagc);
                        
                    }
                    /////for slew values for each fan-in/////////////////
                    
                    for(auto& cfo: bgl.fanin) //looping through fanin of each node
                    {   
                        
                    if ( find(begin(m[bgl.gname].slew_index), end(m[bgl.gname].slew_index), gl[cfo][0].output_slew) != end(m[bgl.gname].slew_index) )
                    {
                        
                        idxsl1 = distance(m[bgl.gname].slew_index, find(m[bgl.gname].slew_index, m[bgl.gname].slew_index + 7, gl[cfo][0].output_slew));
                        idxsl2 = idxsl1+1;
                        flagsl=0;
                        
                        slew_tup = make_tuple(gl[cfo][0].output_slew,idxsl1,idxsl2,flagsl);
                    }
                    
                        else if(gl[cfo][0].output_slew<=m[bgl.gname].slew_index[0])
                        {
                           idxsl1=0; idxsl2=1; flagsl=-1;
                            slew_tup = make_tuple(gl[cfo][0].output_slew,idxsl1,idxsl2,flagsl);
                        }
                    else if(gl[cfo][0].output_slew>=m[bgl.gname].slew_index[6])
                    {
                        idxsl1=5; idxsl2=6; flagsl=-1;
                        slew_tup = make_tuple(gl[cfo][0].output_slew,idxsl1,idxsl2,flagsl);
                    }
                    else
                    {
                        
                        tie(sl1,sl2) = findsmallgreat(m[bgl.gname].slew_index, gl[cfo][0].output_slew);
                        idxsl1 = distance(m[bgl.gname].slew_index, find(m[bgl.gname].slew_index, m[bgl.gname].slew_index + 7, sl1));
                        idxsl2 = idxsl1+1;
                        flagsl=1;
                        slew_tup = make_tuple(gl[cfo][0].output_slew,idxsl1,idxsl2,flagsl);
                    }
                        double cpv=0.0,val_11d=0.0,val_12d=0.0,tpv=0.0,val_21d=0.0,val_22d=0.0,fdv=0.0,fsv=0.0;
                        double cap_1=0.0,cap_2=0.0,tau_1=0.0,tau_2=0.0,val_11t=0.0,val_12t=0.0,val_21t=0.0,val_22t=0.0;
                        
                        cpv = get<0>(cap_tup);
                        
                        tpv = get<0>(slew_tup);
                        
                        cap_1 = m[bgl.gname].cap_index[get<1>(cap_tup)];
                        cap_2 = m[bgl.gname].cap_index[get<2>(cap_tup)];
                        
                        tau_1 = m[bgl.gname].slew_index[get<1>(slew_tup)];
                        tau_2 = m[bgl.gname].slew_index[get<2>(slew_tup)];
                        

                        if (bgl.fanin.size()==2 || bgl.fanin.size()==1)
                        {
                        val_11d = m[bgl.gname].timing[get<1>(slew_tup)][get<1>(cap_tup)];
                        val_12d = m[bgl.gname].timing[get<1>(slew_tup)][get<2>(cap_tup)];
                        val_21d = m[bgl.gname].timing[get<2>(slew_tup)][get<1>(cap_tup)];
                        val_22d = m[bgl.gname].timing[get<2>(slew_tup)][get<2>(cap_tup)];
                        
                        val_11t = m[bgl.gname].timing[7+get<1>(slew_tup)][get<1>(cap_tup)];
                        val_12t = m[bgl.gname].timing[7+get<1>(slew_tup)][get<2>(cap_tup)];
                        val_21t = m[bgl.gname].timing[7+get<2>(slew_tup)][get<1>(cap_tup)];
                        val_22t = m[bgl.gname].timing[7+get<2>(slew_tup)][get<2>(cap_tup)];
                        }
                        else{
                            double factor = bgl.fanin.size();
                            val_11d = (m[bgl.gname].timing[get<1>(slew_tup)][get<1>(cap_tup)])*(factor/2);
                            val_12d = (m[bgl.gname].timing[get<1>(slew_tup)][get<2>(cap_tup)])*(factor/2);
                            val_21d = (m[bgl.gname].timing[get<2>(slew_tup)][get<1>(cap_tup)])*(factor/2);
                            val_22d = (m[bgl.gname].timing[get<2>(slew_tup)][get<2>(cap_tup)])*(factor/2);
                            
                            val_11t = (m[bgl.gname].timing[7+get<1>(slew_tup)][get<1>(cap_tup)])*(factor/2);
                            val_12t = (m[bgl.gname].timing[7+get<1>(slew_tup)][get<2>(cap_tup)])*(factor/2);
                            val_21t = (m[bgl.gname].timing[7+get<2>(slew_tup)][get<1>(cap_tup)])*(factor/2);
                            val_22t = (m[bgl.gname].timing[7+get<2>(slew_tup)][get<2>(cap_tup)])*(factor/2);
                        }

                        //2-D interpolation value
                        fdv = ((val_11d*(cap_2-cpv)*(tau_2-tpv))+(val_12d*(cpv-cap_1)*(tau_2-tpv))+(val_21d*(cap_2-cpv)*(tpv-tau_1))
                        +(val_22d*(cpv-cap_1)*(tpv-tau_1)))/(abs(cap_2-cap_1)*abs(tau_2-tau_1));

                        fsv = ((val_11t*(cap_2-cpv)*(tau_2-tpv))+(val_12t*(cpv-cap_1)*(tau_2-tpv))+(val_21t*(cap_2-cpv)*(tpv-tau_1))
                        +(val_22t*(cpv-cap_1)*(tpv-tau_1)))/(abs(cap_2-cap_1)*abs(tau_2-tau_1));

                        bgl.dv.insert(pair<int,double>(cfo,fdv));

                       
                        if (gl[cfo][0].arrival_time+fdv>max_arrival_time)
                        {
                            worst_node = cfo;
                            max_delay = fdv;
                            max_arrival_time=gl[cfo][0].arrival_time+fdv;
                            worst_slew=fsv;
                        }
                    }

                    
                bgl.delay=max_delay;
                bgl.output_slew=worst_slew;
                bgl.worst_fanin_node=worst_node;
                bgl.arrival_time=max_arrival_time;
                }                
            }
        }


    

    

    //will start backward traversal now////////////////////
    double worst_timing = 0.0,wsl=0.0;
    int wn=0;
    
    tie(wn,worst_timing,wsl) = findEntryWithLargestValue(gl);
    
    double reqd_arriv_time = worst_timing*1.1;
    
    
    ts.reverse();
    for(auto& rev: ts)
    {
        for(auto& qr: gl[rev])
        {
            if(qr.po==1)
            {
                
                qr.rat=reqd_arriv_time;
                qr.slack=qr.rat - qr.arrival_time;
            }
           
                int best_node; double best_arrival;
                
                for(auto& rpr: qr.fanin){
                    if(gl[rpr][0].rat==1000000000)
                    {
                        gl[rpr][0].rat = qr.rat - qr.dv[rpr];
                        gl[rpr][0].slack = gl[rpr][0].rat - gl[rpr][0].arrival_time;
                    }
                    else
                    {
                        if(gl[rpr][0].rat>qr.rat - qr.dv[rpr])
                        {
                            gl[rpr][0].rat = qr.rat - qr.dv[rpr];
                            gl[rpr][0].slack = gl[rpr][0].rat - gl[rpr][0].arrival_time;
                        }
                    }
                    
                }
        }
    }

    

    
         
     //cout<<"Circuit delay: "<<(worst_timing)*1000<<"ps"<<endl;
     final_file<<"Circuit delay: "<<(worst_timing)*1000<<"ps\n";
     //cout<<"\nGate slacks:"<<endl;
     final_file<<"\nGate slacks:\n";
        for (int i=1; i<300000;i++) {
            if(gl[i].size()==0)
                continue;
            else
            {
                for(auto& art: gl[i]){
                    //cout<<art.gname<<"-n"<<i<<": "<<(art.slack)*1000<<"ps"<<endl;
                    final_file<<art.gname<<"-n"<<i<<": "<<(art.slack)*1000<<"ps\n";
                }
                    
            }
            
                }
    //cout<<"\nCritical Path\n";
    final_file<<"\nCritical Path:\n";
      print_fanin(wn,gl);
      //cout<<endl;
      final_file.close();
    

}//ending function brace