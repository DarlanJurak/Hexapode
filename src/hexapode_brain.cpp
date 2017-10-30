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
#include <wiringSerial.h>				// serial

#include <opencv2/opencv.hpp>			//
#include <opencv2/core/core.hpp>		//
#include <opencv2/highgui/highgui.hpp>	//

#include "../include/masks.h"

using namespace cv;
using namespace std;

// Global variables
int serial; // serial handler

// Enum types
enum Command { goAHead, stop, turnLeft, turnRight, goLeft, goRight, squat };
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

	serial = serialOpen(argv[1], 9600);
	if (serial < 0){

		cout << "Cant open serial. :(" << endl;
		return -1;

	}

	sleep(1);
	serialPutchar(serial, '5');
	cout << "Sent 5 to Arduino" << endl;
	sleep(1);

	while(serialDataAvail(serial) == 0){
		continue;
	}

	cout << "Arduino sent " << serialGetchar(serial) - 48 << endl;
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
		cout << " Sent go ahead command. " << endl;

//--- obstacle detection ---------------------------------------------------------//

		cout << " Starting obstacleDetection function. " << endl;
	    while(!obstaclePresent){

	    	obstacle = obstacleDetection(&cap);
	    	if (obstacle != none) obstaclePresent = true;

	    }

//--- obstacle overcomming -------------------------------------------------------//

	    cout << " Treating found obstacle. " << endl;
	    while(obstaclePresent){

    		sendCommand(stop);
			cout << " Sent stop command. " << endl;
			// @improve: wait for arduino response of finished movement
			sleep(2);// wait finished hexapode movement

	    	switch(obstacle){

	    		case wall:

	    			sendCommand(goRight);
	    			cout << " Sent go right command. " << endl;
	    			// @improve: wait for arduino response of finished movement
	    			sleep(2);// wait finished hexapode movement
	    			

	    		break;

	    		case degree:

	    			// go ahead
	    			// step in
	    			// go ahead
	    			// go down

	    		break;
	    		
	    		case portal:

	    			sendCommand(squat);
	    			cout << " Sent squat command. " << endl;
	    			// @improve: wait for arduino response of finished movement
	    			sleep(2);// wait finished hexapode movement

	    			// squat
	    			// go ahead
	    			// rise

	    		break;
	    		default:
	    		break;


	    	}//switch(obstacle)

	    	obstacle = obstacleDetection(&cap);
			if 	(obstacle != none) 	obstaclePresent = true;	
			else 					obstaclePresent = false;

	    }//while(obstacle)

	}//while(1) 
}//main

/*
*	@name: 			sendCommand
*	@brief: 		Send command through serial comunicaiton ()
*	@parameters:	cmd - Command type. (See globally declared Command type).
*/
void sendCommand(Command cmd){

	char msg;

	switch(cmd){
		case goAHead:
			msg = '1';
		break;
		case stop:
			msg = '2';
		break;
		case turnLeft:
			msg = '3';
		break;
		case turnRight:
			msg = '4';
		break;
		case goLeft:
			msg = '5';
		break;
		case goRight:
			msg = '6';
		break;
		case squat:
			msg = '7';
		break;
		default:
		break;
	}

	// sends command through serial port	
	serialPutchar(serial, msg);

	cout << "Sent " << msg << " to Arduino" << endl;
	sleep(1);

	while(serialDataAvail(serial) == 0){
		continue;
	}

	cout << "Arduino sent " << serialGetchar(serial) - 48 << endl;

}

/*
*	@name: 			obstacleDetection
*	@brief: 		...
*	@parameters:	cap - 
*/
Obstacle obstacleDetection(VideoCapture* cap){

	// Matrices for video processing
	Mat imgOriginal;
	Mat imgHSV;
	Mat imgThresholded;
	Mat bitWisedImage, mask;

//--- Video Capturing ---------------------------------------------//

	bool bSuccess;	// read a new frame from video
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
		Vec3d pix;
	    double hue;
		double sat;
		double val;
		long long pixelCounter;	//Simple counter for chased pixels	

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

	// wall obstacle; Blue;
	obstacles_mask[0].lowH 	= 119;
	obstacles_mask[0].lowS 	= 162;
	obstacles_mask[0].lowV 	= 110;
	obstacles_mask[0].highH = 130;
	obstacles_mask[0].highS = 255;
	obstacles_mask[0].highV = 255;

	// degree obstacle; Red;
	obstacles_mask[1].lowH 	= 0;
	obstacles_mask[1].lowS 	= 0;
	obstacles_mask[1].lowV 	= 0;
	obstacles_mask[1].highH = 15;
	obstacles_mask[1].highS = 255;
	obstacles_mask[1].highV = 255;

	// portal obstacle; Green;
	obstacles_mask[2].lowH 	= 60;
	obstacles_mask[2].lowS 	= 0;
	obstacles_mask[2].lowV 	= 0; 
	obstacles_mask[2].highH = 75;
	obstacles_mask[2].highS = 255;
	obstacles_mask[2].highV = 255;
	
}
