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
using namespace std;

struct finalfile{
double timing[14][7]={0};
char cname[20];
double capacitance;
double slew_index[7]={0};
double cap_index[7]={0};
}u1[100];

int x=0,tt=-1,ii=0,jj=0;
int qq=0,zz=0,kk=-1;

map<string, finalfile> parseFileCFormat(char *fName)
{
    FILE *fp,*fp1;
    //fp1=fopen("baner170.txt","w+");
    char lineBuf[1024];
    char *cellName = NULL;
    
    //cout << "Parsing input file " << fName << " using C syntax." << endl;
    
    fp = fopen(fName, "r");
    if (fp == NULL) {
        cout << "Error opening file " << fName << endl;
       // return -1;
    }
    int check1=0;
    int check=0;
    int check3=0;
    int check4=0;
    
    int cellcount=0;
    
    
    while (!feof(fp)) 
    {
        fgets(lineBuf, 1023, fp); // get one whole line
        char *bufPtr = strstr(lineBuf, "cell"); // can you find "cell" in lineBuf?
        //   bufPtr will be a char pointer to a char in lineBuf at the start of cell
        char *bufPtr2 = strstr(lineBuf,"values");
        char *bufPtr3 = strstr(lineBuf,"output_slew");
        char *bufPtr4 = strstr(lineBuf,"index_");
        char *bufPtr5 = strstr(lineBuf,"capacitance\t\t: ");
        char *bufPtr6 = strstr(lineBuf,"index_1");
        char *bufPtr7 = strstr(lineBuf,"index_2");
        
        if(bufPtr2!=NULL)
            check=1;
        if(bufPtr3!=NULL)
            check1=1;
        
        if (bufPtr6!=NULL && check4==1)
        {
            
            const char *PATTERN5 = "\"";
                const char *PATTERN6 = "\"";

                char *target2 = NULL;
                char *start2, *end2;
      //I got the following snippet of code from stackoverflow. Link to that is given below.
    //https://stackoverflow.com/questions/24696113/how-to-find-text-between-two-strings-in-c
    // I have made the requisite changes as per the requirement
            if ( start2 = strstr( lineBuf, PATTERN5 ) )
                {
                    start2 += strlen( PATTERN5 );
                    if ( end2 = strstr( start2, PATTERN6 ) )
                    {
                        target2 = ( char * )malloc( end2 - start2 + 1 );
                        memcpy( target2, start2, end2 - start2 );
                        target2[end2 - start2] = '\0';
                    }
                }
            
            if ( target2 ) {
                ii=0;
               char *p = strtok(target2,",");
                while (p != NULL)
                    {
                        double k = strtod(p,NULL);
                        u1[kk].slew_index[ii++]= k;
                        p = strtok (NULL, ",");
                        
                    }
            }
                free( target2 );
        }

        if (bufPtr7!=NULL && check4==1)
        {
            
            const char *PATTERN7 = "\"";
                const char *PATTERN8 = "\"";

                char *target3 = NULL;
                char *start3, *end3;
      //I got the following snippet of code from stackoverflow. Link to that is given below.
    //https://stackoverflow.com/questions/24696113/how-to-find-text-between-two-strings-in-c
    // I have made the requisite changes as per the requirement
            if ( start3 = strstr( lineBuf, PATTERN7 ) )
                {
                    start3 += strlen( PATTERN8 );
                    if ( end3 = strstr( start3, PATTERN7 ) )
                    {
                        target3 = ( char * )malloc( end3 - start3 + 1 );
                        memcpy( target3, start3, end3 - start3 );
                        target3[end3 - start3] = '\0';
                    }
                }
            
            if ( target3 ) {
                jj=0;
               char *p = strtok(target3,",");
                while (p != NULL)
                    {
                        double k = strtod(p,NULL);
                        u1[kk].cap_index[jj++]= k;
                        p = strtok (NULL, ",");
                        
                    }
            }
                free( target3 );
        }


        if (bufPtr5!=NULL)
        {
            //cout<<lineBuf<<endl;
            const char *PATTERN3 = "capacitance\t\t: ";
                const char *PATTERN4 = ";";

                char *target1 = NULL;
                char *start1, *end1;
      //I got the following snippet of code from stackoverflow. Link to that is given below.
    //https://stackoverflow.com/questions/24696113/how-to-find-text-between-two-strings-in-c
    // I have made the requisite changes as per the requirement
            if ( start1 = strstr( lineBuf, PATTERN3 ) )
                {
                    start1 += strlen( PATTERN3 );
                    if ( end1 = strstr( start1, PATTERN4 ) )
                    {
                        target1 = ( char * )malloc( end1 - start1 + 1 );
                        memcpy( target1, start1, end1 - start1 );
                        target1[end1 - start1] = '\0';
                    }
                }
                if(target1){
                    //cout<<"cap value = "<<target1<<endl;
                    double cap = strtod(target1,NULL);
                    u1[kk].capacitance=cap;
                }
                free(target1);
            
        }
        
        if ((check==1 && check3==1 && (bufPtr4==NULL)))
        {
            
            const char *PATTERN1 = "\"";
                const char *PATTERN2 = "\"";

                char *target = NULL;
                char *start, *end;
      //I got the following snippet of code from stackoverflow. Link to that is given below.
    //https://stackoverflow.com/questions/24696113/how-to-find-text-between-two-strings-in-c
    // I have made the requisite changes as per the requirement
            if ( start = strstr( lineBuf, PATTERN1 ) )
                {
                    start += strlen( PATTERN1 );
                    if ( end = strstr( start, PATTERN2 ) )
                    {
                        target = ( char * )malloc( end - start + 1 );
                        memcpy( target, start, end - start );
                        target[end - start] = '\0';
                    }
                }
            
            if ( target ) {
                tt=tt+1;
                x=0;
               char *p = strtok(target,",");
                while (p != NULL)
                    {
                        double k = strtod(p,NULL);
                        u1[kk].timing[tt][x++]= k;
                        p = strtok (NULL, ",");
                        
                    }
            }
                free( target );
        }
        if (bufPtr == NULL)
            continue;        // the first word was not "cell". Go on to reading the next line
        
        while (*bufPtr != '(' && *bufPtr != NULL)
            bufPtr++;
        if (*bufPtr == NULL) {
            //cout << " ERROR! Found cell, but didn't find '('" << endl;
          //  return -1;
        }

        // we found '('. Now we are at the start of the cell name
        cellName = strtok(bufPtr, "() ");
        // if we wanted to read another token, we would have used:
        //     cellName2 = strtok_s(NULL, "() ", &nextToken);
        char *cna = strstr(cellName,"Timing");
        if (cna==NULL)
        {
        //cout << cellName << endl;
            cellcount++;
            ++kk;
            check4=1;
            while (*cellName != NULL)
            {
                u1[kk].cname[zz++]=*cellName;
                cellName++;
            }
            zz=0;
        }
        check1=0;
        check=0;
        check3=1;
        
        tt=-1;
            
    }
    
    //cout<<"Total Number of Cells ="<<cellcount<<endl;
    //writing to file begins
    int y=0;
    /*fprintf(fp1,"%d",cellcount);
    fputs("\n",fp1);
    for (int fc=0;fc<cellcount;fc++)
    {
        fputs(u1[fc].cname,fp1);
        fputs("\n",fp1);
        for(y=0;y<14;y++)
        {
            for (int z=0;z<7;z++)
            {
                fprintf(fp1,"%lf",u1[fc].timing[y][z]);
                fputs(" ",fp1);
            }
            fputs("\n",fp1);
        }
        fputs("\n index values now\n",fp1);
        for (int t=0;t<7;t++)
        {
            fprintf(fp1,"%lf",u1[fc].cap_index[t]);
            fputs("\n",fp1);
        }
    }
   
    fclose(fp);
    fclose(fp1);*/

    map<string, finalfile>ff;

    for (int fk=0;fk<cellcount;fk++)
    {
        ff.insert(pair<string, finalfile>(u1[fk].cname,u1[fk]));
    }

   /*map<string, vector <vector<double> > > ff;
    for (int fk=0;fk<cellcount;fk++)
    {
        vector <vector<double> > ss;
        
        for (int y=0; y<14;y++)
        {
            vector<double> temp;
            //std::vector<double> vec {1,2,3,4,5};
            for (int z=0;z<7;z++)
            {
            temp.push_back(u1[fk].timing[y][z]);
            }
            ss.push_back(temp);
        }
        ff.insert(pair<string, vector<vector <double> > >(u1[fk].cname,ss));
    }*/

    
    //vector<vector<double> >ss1(7, vector<double>(7,0.0));
    /*double ss2[14][7]={ { 0.0 } };
    finalfile dff={
        {0} , "DFF",0.0,{0},{0}};
    u1[cellcount]=dff;
    ff.insert(pair<string, finalfile>(u1[cellcount].cname,u1[cellcount]));*/
    
    
    return ff;
}
