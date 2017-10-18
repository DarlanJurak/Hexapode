/*
*	@author:		Darlan Alves Jurak
*	@created:		16 oct 2017
*	@brief:			Main code to control the hexapode.
*/

#include <iostream> 					// cout
#include <unistd.h> 					//
#include <stdio.h> 						//
#include <stdlib.h>						//
#include <typeinfo>						//
#include <termios.h>					//
#include <fcntl.h>						//
#include <sys/ioctl.h>					//
#include <sys/stat.h>					//
#include <sys/types.h>					//
#include <string.h>						// string

#include <opencv2/opencv.hpp>			//
#include <opencv2/core/core.hpp>		//
#include <opencv2/highgui/highgui.hpp>	//

#include "../include/masks.h"

using namespace cv;
using namespace std;

// Global variables
int serial; // serial handler

// Enum types
enum Command { goAHead, stop, turnLeft, turnRight, goLeft, goRight };
enum Obstacle { none, wall, degree, portal };

// Functions
void initMasks();
void sendCommand(Command);
Obstacle obstacleDetection(VideoCapture* cap);

/*
*	@name: 			main
*	@brief: 		Main code
*	@parameters:	
*					argc - ...
*					argv - ...
*/
int main( int argc, char** argv )
{

//--- init -----------------------------------------------------------------------//


	//--- Serial connection (Raspberry Pi <-> Arduino) ---------------------//

	serial = open(argv[1], O_RDWR);
	if (serial == -1) {
	  perror(argv[1]);
	  return 1;
	}
	usleep(1000);	// start bit; sleep for 1ms to let it settle

	cout << " Serial configured. " << endl;

	//--- Video Config -----------------------------------------------------//

	VideoCapture cap(0); //capture the video from web cam
    if ( !cap.isOpened() )  // if not success, exit program
    {
         cout << "Cannot open the web cam" << endl;
         return -1;
    }

    cout << " Video configured. " << endl;

    //--- Masks definition -------------------------------------------------//
    initMasks();

    cout << " Masks defined. " << endl;

    //--- Discovery initiation ---------------------------------------------//
    bool obstaclePresent = false;
    Obstacle obstacle = none;

//--- discovery ------------------------------------------------------------------//

	while(1){

		sendCommand(goAHead);
		cout << " Send go ahead command. " << endl;

//--- obstacle detection ---------------------------------------------------------//
		cout << " Starting obstacleDetection function. " << endl;
	    while(!obstaclePresent){

	    	obstacle = obstacleDetection(&cap);
	    	if (obstacle != none) obstaclePresent = true;

	    }

//--- obstacle overcomming -------------------------------------------------------//

	    cout << " Treating found obstacle. " << endl;
	    while(obstaclePresent){

	    	switch(obstacle){

	    		case wall:

	    			sendCommand(goRight);
	    			sleep(1);
	    			obstacle = obstacleDetection(&cap);
	    			if (obstacle != none){

	    				obstaclePresent = true;	

	    			}else{

	    				obstaclePresent = false;

	    			}

	    		break;
	    		case degree:
	    		break;
	    		case portal:
	    		break;
	    		default:
	    		break;


	    	}//switch(obstacle)

	    }//while(obstacle)

	}//while(1) 
}//main

/*
*	@name: 			sendCommand
*	@brief: 		Send command through serial comunicaiton ()
*	@parameters:	cmd - Command type. (See globally declared Command type).
*/
void sendCommand(Command cmd){

	char *msg;

	switch(cmd){
		case goAHead:
			msg = (char*)"1";
		break;
		case stop:
			msg = (char*)"2";
		break;
		case turnLeft:
			msg = (char*)"3";
		break;
		case turnRight:
			msg = (char*)"4";
		break;
		case goLeft:
			msg = (char*)"5";
		break;
		case goRight:
			msg = (char*)"6";
		break;
		default:
		break;
	}

	// sends command through serial port	
	write(serial, msg, strlen(msg));

}

/*
*	@name: 			obstacleDetection
*	@brief: 		...
*	@parameters:	cap - 
*/
Obstacle obstacleDetection(VideoCapture* cap){

	// Matrices for video processing
	static Mat imgOriginal;
	static Mat imgHSV;
	static Mat imgThresholded;
	static Mat bitWisedImage, mask;

//--- Video Capturing ---------------------------------------------//

	static bool bSuccess;	// read a new frame from video
	bSuccess = (*cap).read(imgOriginal);

    if (!bSuccess) //if not success, break loop
    {
         cout << "Cannot read a frame from video stream" << endl;
         return none;
    }

//--- HSV conversion -----------------------------------------------//

	//Convert the captured frame from BGR to HSV
	cvtColor(imgOriginal, imgHSV, COLOR_BGR2HSV); 

//--- Define and Apply mask ----------------------------------------//

	for( int obstaclesMasks = 0; obstaclesMasks < 3; obstaclesMasks++){

		//Bounds defines chased color. 
		//The last parameter is a binary image.
		inRange(
				imgHSV, 
				Scalar(	obstacles_mask[obstaclesMasks].lowH,	obstacles_mask[obstaclesMasks].lowS, 	obstacles_mask[obstaclesMasks].lowV), 
				Scalar(	obstacles_mask[obstaclesMasks].highH,	obstacles_mask[obstaclesMasks].highS,	obstacles_mask[obstaclesMasks].highV), 
				imgThresholded
		);

		//--- Morphological changes -----------------------------------------//

		//morphological opening (removes small objects from the foreground)
		erode (imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
		dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5))); 

		//morphological closing (removes small holes from the foreground)
		dilate(imgThresholded, 	imgThresholded,	getStructuringElement(MORPH_ELLIPSE, Size(5, 5))); 
		erode (imgThresholded, 	imgThresholded,	getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));			

	//--- Creates binary image and filters original image ---------------//

		//Apply mask to the original image, before HSV conversion.
		bitwise_and(imgOriginal, imgOriginal, bitWisedImage, imgThresholded);

	//--- Determine if chased color is present --------------------------//

		// Data structure to treat pixel
		static Vec3d pix;
	    static double hue;
		static double sat;
		static double val;
		static long long pixelCounter;	//Simple counter for chased pixels	

		pixelCounter = 0;

		for( int i = 0; i < bitWisedImage.rows; i++){
			for( int j = 0; j < bitWisedImage.cols; j++){

				pix = bitWisedImage.at<typeof(pix)>(i,j);
				hue = pix.val[0];
				sat = pix.val[1];
				val = pix.val[2];

				if( (hue != 0) || (sat != 0) || (val != 0)){

					pixelCounter++;
					
				}
			}				
		}

		// Determine acceptable area
		if (pixelCounter > (bitWisedImage.rows * bitWisedImage.cols)/6){

			switch(obstaclesMasks){

				case 0:
				cout << "wall" << endl;
				return wall;
				break;

				case 1:
				cout << "degree" << endl;
				return degree;
				break;

				case 2:
				cout << "portal" << endl;
				return portal;
				break;

				default:
				return none;
				break;
			}

		}

	}
	
	return none;
}

void initMasks(){

	// wall obstacle
	obstacles_mask[0].lowH 	= 127;
	obstacles_mask[0].lowS 	= 103;
	obstacles_mask[0].lowV 	= 146;
	obstacles_mask[0].highH = 151;
	obstacles_mask[0].highS = 200;
	obstacles_mask[0].highV = 255;

	// degree obstacle
	obstacles_mask[1].lowH 	= 84;
	obstacles_mask[1].lowS 	= 161;
	obstacles_mask[1].lowV 	= 38; 
	obstacles_mask[1].highH = 193;
	obstacles_mask[1].highS = 48;
	obstacles_mask[1].highV = 101;

	// portal obstacle
	obstacles_mask[2].lowH 	= 84;
	obstacles_mask[2].lowS 	= 161;
	obstacles_mask[2].lowV 	= 38; 
	obstacles_mask[2].highH = 193;
	obstacles_mask[2].highS = 48;
	obstacles_mask[2].highV = 101;
	
}
