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
#include "opencv2/imgproc/imgproc.hpp"

#include "../include/masks.h"

using namespace cv;
using namespace std;

// Global variables
int serial; // serial handler

// Enum types
enum Command { goAHead, stop, turnLeft, turnRight, goLeft, goRight, squat, rise };
enum Obstacle { none, wall, degree, portal };

// Functions
void initMasks();
void sendCommand(Command);
Obstacle obstacleDetection(VideoCapture* cap, bool* dynamicDebug);
void testSerial(char**);

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

	//--- Debug Mode ------------------------------------------------------------------//

	bool dynamicDebug = false;

	if( argc > 2){

		if( argc > 3 ){

			if( argv[3] == std::string("serial") ){

				testSerial(argv);

			}

		}
    
    	if( argv[2] == std::string("debug") ){

    		dynamicDebug = true;

    	}
    
	}

	//--- Serial connection (Raspberry Pi <-> Arduino) ---------------------//

	serial = serialOpen(argv[1], 9600);	// Open serial
	if (serial < 0){	// test if serial was corrected opened

		cout << "Cant open serial. :(" << endl;
		return -1;

	}

	sleep(1);	// wait for serial configuration to finish
	serialPutchar(serial, '0'); // send some data to Arduino
	cout << "Sent 0 to Arduino" << endl; // verbose action
	sleep(1); // wait for Arduino response

	while(serialDataAvail(serial) == 0){ // wait for Arduino response
		continue;
	}

	cout << "Arduino sent " << serialGetchar(serial) - 48 << endl; // Show received data from Arduino
	cout << "Serial configured. " << endl;

	//--- Video Config -----------------------------------------------------//

	VideoCapture cap(0); //capture the video from web cam
    if ( !cap.isOpened() )  // if not success, exit program
    {
         cout << "Cannot open the web cam" << endl;
         return -1;
    }

    cout << "Video configured. " << endl;

    //--- Masks definition -------------------------------------------------//
    initMasks();

    cout << "Masks defined. " << endl;

    //--- Discovery initiation ---------------------------------------------//
    bool obstaclePresent = false;
    Obstacle obstacle = none;

//--- discovery ------------------------------------------------------------------//

	while(1){

		sendCommand(goAHead);
		cout << "Sent go ahead command. " << endl;

//--- obstacle detection ---------------------------------------------------------//

		cout << "Starting obstacleDetection function. " << endl;
	    while(!obstaclePresent){

	    	obstacle = obstacleDetection(&cap, &dynamicDebug);
	    	if (obstacle != none) obstaclePresent = true;

	    }

//--- obstacle overcomming -------------------------------------------------------//

	    cout << "Treating found obstacle. " << endl;
	    while(obstaclePresent){

    		cout << "Sent stop command. " << endl;
    		sendCommand(stop);
			

	    	switch(obstacle){

	    		case wall:

	    			cout << "Sent go right command. " << endl;
	    			sendCommand(goRight);	    			

	    		break;

	    		case degree:

	    			// go ahead
	    			// step in
	    			// go ahead
	    			// go down

	    		break;
	    		
	    		case portal:

	    			
	    			cout << "Sent squat command. " << endl;
	    			sendCommand(squat);

	    			// squat
	    			// go ahead
	    			// rise

	    		break;
	    		default:
	    		break;


	    	}//switch(obstacle)

	    	obstacle = obstacleDetection(&cap, &dynamicDebug);
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

	unsigned char msg;
	int dataAvailable = 0, arduinoResponse;

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
		case rise:
			msg = '8';
		break;
		default:
		break;
	}

	// sends command through serial port
	serialFlush();
	sleep(1);	
	serialPutchar(serial, msg);

	cout << "Sent " << msg << " to Arduino" << endl;
	sleep(1);

	while(dataAvailable == 0){ // wait for Arduino response
		dataAvailable = serialDataAvail(serial);
		sleep(1); // wait for Arduino response
	}

	arduinoResponse = serialGetchar(serial);
	cout << "Arduino sent " << arduinoResponse - 48 << endl; // Show received data from Arduino

	serialFlush(serial);		

}

/*
*	@name: 			obstacleDetection
*	@brief: 		...
*	@parameters:	cap - 
*/
Obstacle obstacleDetection(VideoCapture* cap, bool* dynamicDebug){
    
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
		//bitwise_and(imgOriginal, imgOriginal, bitWisedImage, imgThresholded);

	//--- Determine if chased color is present --------------------------//

		// Data structure to treat pixel
		Vec3d pix;
	    double hue;
		double sat;
		double val;
		long long pixelCounter;	//Simple counter for chased pixels	

		pixelCounter = 0;

		for( int i = 0; i < imgThresholded.rows; i++){
			for( int j = 0; j < imgThresholded.cols; j++){

				pix = imgThresholded.at<typeof(pix)>(i,j);
				hue = pix.val[0];
				sat = pix.val[1];
				val = pix.val[2];

				if( (hue != 0) || (sat != 0) || (val != 0)){

					pixelCounter++;
					
				}
			}				
		}

		if(*dynamicDebug){

			if(obstaclesMasks == 0){

				imshow("Wall", imgThresholded); 	//show the thresholded image
					
			}else if(obstaclesMasks == 1){

				imshow("Degree", imgThresholded); 	//show the thresholded image
					
			}else if(obstaclesMasks == 2){

				imshow("Portal", imgThresholded); 	//show the thresholded image
					
			}

		}	

		// Determine acceptable area
		if (pixelCounter > (imgThresholded.rows * imgThresholded.cols)/6){

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

	//--- Closes image ----------------------------------------------------//

	if (waitKey(30) == 27) //wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop
  	{
        cout << "esc key is pressed by user" << endl;
        *dynamicDebug = false; 
   	}
	
	return none;
}

void testSerial(char** argv){

	bool passed = false;
	unsigned char sentData;
	int dataAvailable = 0, arduinoResponse;

	serial = serialOpen(argv[1], 9600);	// Open serial
	sleep(1);	// wait for serial configuration to finish
	serialFlush(serial);

	if (serial < 0){	// test if serial was corrected opened

		cout << "Cant open serial. :(" << endl;

	}else{

		cout << "Serial configured. " << endl;

		sentData = '0';
		while(!passed){

			dataAvailable = 0;

			serialPutchar(serial, '0'); // send some data to Arduino
			cout << "Sent 0 to Arduino" << endl; // verbose action
			sleep(1); // wait for Arduino response

			while(dataAvailable == 0){ // wait for Arduino response
				dataAvailable = serialDataAvail(serial);
				sleep(1); // wait for Arduino response
			}
			cout << dataAvailable << " available data on serial." << endl;	

			arduinoResponse = serialGetchar(serial);
			cout << "Arduino sent " << arduinoResponse - 48 << endl; // Show received data from Arduino

			serialFlush(serial);

			if( arduinoResponse == sentData ){

				passed = true;

			}else{

				cout << "Serial test: Failed." << endl;

			}

		}

		cout << "Serial test: OK." << endl;
		serialClose(serial);

	}

	
}

// void dynamicDebug(){//VideoCapture* cap){

// 	initMasks();

// 	VideoCapture cap(0); //capture the video from web cam

//     if ( !cap.isOpened() )  // if not success, exit program
//     {
//          cout << "Cannot open the web cam" << endl;
//     }

//     while (true)
//     {
//         Mat imgOriginal;

//         bool bSuccess = cap.read(imgOriginal); // read a new frame from video

//          if (!bSuccess) //if not success, break loop
//         {
//              cout << "Cannot read a frame from video stream" << endl;
//              break;
//         }

// 		Mat imgHSV;

// 		cvtColor(imgOriginal, imgHSV, COLOR_BGR2HSV); //Convert the captured frame from BGR to HSV
	
// 		Mat imgThresholded;

// 		for( int obstaclesMasks = 0; obstaclesMasks < 3; obstaclesMasks++){

// 			inRange(
// 				imgHSV, 
// 				Scalar(	obstacles_mask[obstaclesMasks].lowH,	obstacles_mask[obstaclesMasks].lowS, 	obstacles_mask[obstaclesMasks].lowV), 
// 				Scalar(	obstacles_mask[obstaclesMasks].highH,	obstacles_mask[obstaclesMasks].highS,	obstacles_mask[obstaclesMasks].highV), 
// 				imgThresholded
// 			);
	      
// 			//morphological opening (removes small objects from the foreground)
// 			erode (imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
// 			dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5))); 

// 			//morphological closing (removes small holes from the foreground)
// 			dilate(imgThresholded, 	imgThresholded,	getStructuringElement(MORPH_ELLIPSE, Size(5, 5))); 
// 			erode (imgThresholded, 	imgThresholded,	getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

// 			if(obstaclesMasks == 0){

// 				imshow("Wall", imgThresholded); 	//show the thresholded image
					
// 			}else if(obstaclesMasks == 1){

// 				imshow("Degree", imgThresholded); 	//show the thresholded image
					
// 			}else if(obstaclesMasks == 2){

// 				imshow("Portal", imgThresholded); 	//show the thresholded image
					
// 			}
			


// 		}  
	   
//     	if (waitKey(30) == 27) //wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop
//       	{
//             cout << "esc key is pressed by user" << endl;
//             break; 
//        	}

//     }
// }

void initMasks(){

	// wall obstacle; Blue;
	obstacles_mask[0].lowH 	= 100;
	obstacles_mask[0].lowS 	= 100;
	obstacles_mask[0].lowV 	= 70;
	obstacles_mask[0].highH = 150;
	obstacles_mask[0].highS = 255;
	obstacles_mask[0].highV = 255;

	// degree obstacle; Yellow;
	obstacles_mask[1].lowH 	= 0;
	obstacles_mask[1].lowS 	= 200;
	obstacles_mask[1].lowV 	= 55;
	obstacles_mask[1].highH = 179;
	obstacles_mask[1].highS = 255;
	obstacles_mask[1].highV = 255;

	// portal obstacle; Green;
	obstacles_mask[2].lowH 	= 0;
	obstacles_mask[2].lowS 	= 100;
	obstacles_mask[2].lowV 	= 60; 
	obstacles_mask[2].highH = 90;
	obstacles_mask[2].highS = 205;
	obstacles_mask[2].highV = 180;
	
}
