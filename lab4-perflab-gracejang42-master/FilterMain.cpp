#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include "Filter.h"

using namespace std;

#include "rdtsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);

    if ( ok ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *) outputFilename.c_str(), output);
    }
    delete input;
    delete output;
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

}

class Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  } else {
    cerr << "Bad input in readFilter:" << filename << endl;
    exit(-1);
  }
}


double
applyFilter(class Filter *filter, cs1300bmp *input, cs1300bmp *output)
{
      int a[3][3]; // this is a hardcoded replacement for filter -> get(i,j) 
      a[0][0] = filter -> get(0,0);
      a[0][1] = filter -> get(0,1);
      a[0][2] = filter -> get(0,2);
      a[1][0] = filter -> get(1,0);
      a[1][1] = filter -> get(1,1);
      a[1][2] = filter -> get(1,2);
      a[2][0] = filter -> get(2,0);
      a[2][1] = filter -> get(2,1);
      a[2][2] = filter -> get(2,2);

  long long cycStart, cycStop;

  cycStart = rdtscll();

  output -> width = input -> width;
  output -> height = input -> height;
  
  short int d = filter -> getDivisor();
  int w = (input -> width)-1;//local variable for width
  int h = (input -> height)-1;//local variable for height

    //notes: 2 X 2 loop unrooling attempt
    //original for loops: (short int row = 1; row < h; row = row + 1) (short int col = 1; col < w; col = col + 1)
    //original variables int w = (input -> width)-1; int h = (input -> height)-1;
    

  for(short int plane = 0; plane < 3; plane++) // swapping loop order
  {
    for(short int row = 1; row < h; row = row + 1) //changing to row++ slows it down.. a lot 
    {
      for(short int col = 1; col < w; col = col + 1) // was formerly short int plane = 0; plane < 3; plane++
      {

    //output -> color[plane][row][col] = 0;

    int temp = 0; 
	//for (int j=0; j<9; j++) //(int j = 0; j < filter -> getSize(); j++).. int j=0; j<3; j++
   //{
	  //for (int i = 0; i < filter -> getSize(); i++) // int i=0; i<3; i++	
      //{
        //temp = temp + (input -> color[plane][row + i - 1][col + j - 1] * filter -> get(i, j) ); 
        // or, try the following for better efficiency 
    
          
        temp = temp + ((input -> color[plane][row - 1][col - 1] * a[0][0]) + (input -> color[plane][row][col - 1] * a[1][0]));
        temp = temp + ((input -> color[plane][row + 1][col - 1] * a[2][0]) + (input -> color[plane][row - 1][col] * a[0][1]));
        temp = temp + ((input -> color[plane][row][col] * a[1][1]) + (input -> color[plane][row + 1][col] * a[2][1]));
        temp = temp + ((input -> color[plane][row - 1][col + 1] * a[0][2]) + (input -> color[plane][row][col + 1] * a[1][2]));
        temp = temp + (input -> color[plane][row + 1][col + 1] * a[2][2]);//optimizing arithmetic 
         
        //temp = temp + (input -> color[plane][row - 1][col - 1] * a[0][0]);//hardcoding the loop ... 
        //temp = temp + (input -> color[plane][row][col - 1] * a[1][0]);//x
        //temp = temp + (input -> color[plane][row + 1][col - 1] * a[2][0]);
        //temp = temp + (input -> color[plane][row - 1][col] * a[0][1]);//x
        //temp = temp + (input -> color[plane][row][col] * a[1][1]);
        //temp = temp + (input -> color[plane][row + 1][col] * a[2][1]);//x
        //temp = temp + (input -> color[plane][row - 1][col + 1] * a[0][2]);
        //temp = temp + (input -> color[plane][row][col + 1] * a[1][2]);//x
        //temp = temp + (input -> color[plane][row + 1][col + 1] * a[2][2]);
                       
	    //output -> color[plane][row][col] = output -> color[plane][row][col]
	     // + (input -> color[plane][row + i - 1][col + j - 1] * filter -> get(i, j) );
	  //}
	//}
	temp = temp/d;
                       
	//output -> color[plane][row][col] = 	
	  //output -> color[plane][row][col] / filter -> getDivisor();// output-> etc / d;
        
        if(temp < 0)
        {
            temp = 0;
        }
        
        if(temp > 255)
        {
            temp = 255;
        }

        output -> color[plane][row][col] = temp; 
        //if ( output -> color[plane][row][col]  < 0 ) 
        //{
          //output -> color[plane][row][col] = 0;
       // }

        //if ( output -> color[plane][row][col]  > 255 ) 
        //{ 
         // output -> color[plane][row][col] = 255;
        //}
      }
    }
  }

  cycStop = rdtscll();
  double diff = cycStop - cycStart;
  double diffPerPixel = diff / (output -> width * output -> height);
  fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (output -> width * output -> height));
  return diffPerPixel;
}
