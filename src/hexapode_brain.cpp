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

// Global variable
int serial;

// Enum types
enum Command { goAHead, stop, turnLeft, turnRight, goLeft, goRight };
enum Obstacle { none, wall, degree, portal };

// Functions
void sendCommand(Command);
Obstacle obstacleDetection(VideoCapture* cap);
void initMasks();

/*
*	@name: 			main
*	@brief: 		Main code
*	@parameters:	
*					argc - ...
*					argv - ...
*/
int main( int argc, char** argv )
{

//--- init --------------------------------------------------------------------//


	//--- Serial connection (Raspberry Pi <-> Arduino) ---------------------//	
	serial = open(argv[1], O_RDWR);
	if (serial == -1) {
	  perror(argv[1]);
	  return 1;
	}

	usleep(1000);	// start bit; sleep for 1ms to let it settle

	//--- Video Config -----------------------------------------------------//
	VideoCapture cap(0); //capture the video from web cam

    if ( !cap.isOpened() )  // if not success, exit program
    {
         cout << "Cannot open the web cam" << endl;
         return -1;
    }

    //--- Video Config -----------------------------------------------------//
    initMasks();

    //--- Discovery initiation ----------------------------------------------//
    bool obstaclePresent = false;
    Obstacle obstacle = none;

//--- discovery ----------------------------------------------------------------//

	while(1){

		sendCommand(goAHead);	

//--- obstacle detecction ------------------------------------------------------//
	    while(!obstaclePresent){

	    	obstacle = obstacleDetection(&cap);
	    	if (obstacle != none) obstaclePresent = true;

	    }

//--- obstacle overcomming -----------------------------------------------------//

	    while(obstaclePresent){

	    	switch(obstacle){

	    		case wall:

	    			sendCommand(goRight);
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
	
	
}

void initMasks(){

	// wall obstacle
	obstacles_mask[0].lowH 	= 84;
	obstacles_mask[0].lowS 	= 161;
	obstacles_mask[0].lowV 	= 38; 
	obstacles_mask[0].highH = 193;
	obstacles_mask[0].highS = 48;
	obstacles_mask[0].highV = 101;

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

    

    //--- Trackbars ---------------------------------------------------------------//

 //    namedWindow("Control",CV_WINDOW_AUTOSIZE); //create a window called "Control"

	// int iLowH = 0;
	// int iHighH = 179;

	// int iLowS = 0; 
	// int iHighS = 255;

	// int iLowV = 0;
	// int iHighV = 255;

	// //Create trackbars in "Control" window
	// cvCreateTrackbar("LowH", "Control", &iLowH, 179); //Hue (0 - 179)
	// cvCreateTrackbar("HighH", "Control", &iHighH, 179);

	// cvCreateTrackbar("LowS", "Control", &iLowS, 255); //Saturation (0 - 255)
	// cvCreateTrackbar("HighS", "Control", &iHighS, 255);

	// cvCreateTrackbar("LowV", "Control", &iLowV, 255);//Value (0 - 255)
	// cvCreateTrackbar("HighV", "Control", &iHighV, 255);

	//--- Video Processing -------------------------------------------------------//

  //   while (true)
  //   {

  //   	//--- Video Capturing ----------------------------------------------------//

  //       Mat imgOriginal;

  //       bool bSuccess = cap.read(imgOriginal); // read a new frame from video

  //        if (!bSuccess) //if not success, break loop
  //       {
  //            cout << "Cannot read a frame from video stream" << endl;
  //            break;
  //       }



  //       //--- HSV conversion -----------------------------------------------------//

		// Mat imgHSV;

		// cvtColor(imgOriginal, imgHSV, COLOR_BGR2HSV); //Convert the captured frame from BGR to HSV



		// //--- Define and Apply mask ----------------------------------------------//
	
		// Mat imgThresholded;

		// Mat bitWisedImage, mask;

		// //Define mask
		// int mask_iLowH 	= mask_nubank_card_iLowH;
		// int mask_iHighH = mask_nubank_card_iHighH;
		// int mask_iLowS 	= mask_nubank_card_iLowS; 
		// int mask_iHighS = mask_nubank_card_iHighS;
		// int mask_iLowV 	= mask_nubank_card_iLowV;
		// int mask_iHighV = mask_nubank_card_iHighV;

		// //Creates and applies mask. 
		// //Bounds defines chased color. 
		// //The last parameter is a binary image.
		// inRange(imgHSV, Scalar(mask_iLowH, mask_iLowS, mask_iLowV), Scalar(mask_iHighH, mask_iHighS, mask_iHighV), imgThresholded);

		// // inRange(imgHSV, Scalar(iLowH, iLowS, iLowV), Scalar(iHighH, iHighS, iHighV), imgThresholded); //Threshold the image


  //     	//--- Morphological changes ----------------------------------------------//

		// //morphological opening (removes small objects from the foreground)
		// erode (imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));
		// dilate(imgThresholded, imgThresholded, getStructuringElement(MORPH_ELLIPSE, Size(5, 5))); 

		// //morphological closing (removes small holes from the foreground)
		// dilate(imgThresholded, 	imgThresholded,	getStructuringElement(MORPH_ELLIPSE, Size(5, 5))); 
		// erode (imgThresholded, 	imgThresholded,	getStructuringElement(MORPH_ELLIPSE, Size(5, 5)));

		

		// //--- Creates binary image and filters original image --------------------//

		// //Apply mask to the original image, before HSV conversion.
		// bitwise_and(imgOriginal, imgOriginal, bitWisedImage, imgThresholded);

		

		// //--- Determine if chased color is present -------------------------------//

		// // Data structure to treat pixel
		// Vec3d pix;
  //       double hue;
		// double sat;
		// double val;

		// //Simple counter for chased pixels
		// long long pixelCounter;
		// pixelCounter = 0;

		// for( int i = 0; i < bitWisedImage.rows; i++){
		// 	for( int j = 0; j < bitWisedImage.cols; j++){

		// 		pix = bitWisedImage.at<typeof(pix)>(i,j);
		// 		hue = pix.val[0];
		// 		sat = pix.val[1];
		// 		val = pix.val[2];

		// 		if( (hue != 0) || (sat != 0) || (val != 0)){

		// 			pixelCounter++;
					
		// 		}
		// 	}				
		// }

		// if (pixelCounter > (bitWisedImage.rows * bitWisedImage.cols)/6){

		// 	cout << "Nubank purple found." << endl;

		// }



		// //--- Show Filtered image ------------------------------------------------//

		// imshow("Bitwised Image", 	bitWisedImage); 	//show the Filtered image



		// //--- Finishes image ----------------------------------------------------//

		// if (waitKey(30) == 27) //wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop
  //     	{
  //           cout << "esc key is pressed by user" << endl;
  //           break; 
  //      	}










		//---                T R A S H             -------------------------------//

		// Mat bitWisedImage, mask;

		// int mask_iLowH 	= mask_nubank_card_iLowH;
		// int mask_iHighH = mask_nubank_card_iHighH;
		// int mask_iLowS 	= mask_nubank_card_iLowS; 
		// int mask_iHighS = mask_nubank_card_iHighS;
		// int mask_iLowV 	= mask_nubank_card_iLowV;
		// int mask_iHighV = mask_nubank_card_iHighV;

		// int mask_iLowH 	= mask_highBrightRubikCubeYellow_iLowH;
		// int mask_iHighH = mask_highBrightRubikCubeYellow_iHighH;
		// int mask_iLowS 	= mask_highBrightRubikCubeYellow_iLowS; 
		// int mask_iHighS = mask_highBrightRubikCubeYellow_iHighS;
		// int mask_iLowV 	= mask_highBrightRubikCubeYellow_iLowV;
		// int mask_iHighV = mask_highBrightRubikCubeYellow_iHighV;

		// int mask_iLowH 	= iLowH;
		// int mask_iHighH = iHighH;
		// int mask_iLowS 	= iLowS; 
		// int mask_iHighS = iHighS;
		// int mask_iLowV 	= iLowV;
		// int mask_iHighV = iHighV;

		//Creates mask. Bounds defines chased color.
		// inRange(imgHSV, Scalar(mask_iLowH, mask_iLowS, mask_iLowV), Scalar(mask_iHighH, mask_iHighS, mask_iHighV), mask);
		// bitwise_and(imgOriginal, imgOriginal, bitWisedImage, mask);

		// imshow("Thresholded Image", imgThresholded); 	//show the thresholded image
		// imshow("Original", 			imgOriginal); 		//show the original image
		// imshow("HSV Image", 		imgHSV); 			//show the HSV image
		// imshow("Bitwised Image", 	bitWisedImage); 	//show the Filtered image

		// Vec3d pix;
  //       double hue;
		// double sat;
		// double val;

		// for( int i = 0; i < bitWisedImage.rows; i++){
		// 	for( int j = 0; j < bitWisedImage.cols; j++){

		// 		pix = bitWisedImage.at<typeof(pix)>(i,j);
		// 		hue = pix.val[0];
		// 		sat = pix.val[1];
		// 		val = pix.val[2];

		// 		if( (hue != 0) || (sat != 0) || (val != 0)){

		// 			cout << "Hue: " << hue << ". Sat: " << sat << ". Val: " << val << "." << endl;
		// 		}
		// 	}				
		// }
	   
   //  	if (waitKey(30) == 27) //wait for 'esc' key press for 30ms. If 'esc' key is pressed, break loop
   //    	{
   //          cout << "esc key is pressed by user" << endl;

   // 			//          for( int i = 0; i < bitWisedImage.rows/4; i++){
			// // 	for( int j = 0; j < bitWisedImage.cols/4; j++){

			// // 		pix = bitWisedImage.at<typeof(pix)>(i,j);
			// // 		hue = pix.val[0];
			// // 		sat = pix.val[1];
			// // 		val = pix.val[2];

			// // 		cout << "Hue: " << hue << ". Sat: " << sat << ". Val: " << val << "." << endl;
					
			// // 	}				
			// // }

			// // cout << "\nCanais: " << bitWisedImage.channels() << "\nLinhas: " << bitWisedImage.rows << "\nColunas: " << bitWisedImage.cols << endl;

   //          // Show last pixel info
			// // pix	= bitWisedImage.at<typeof(pix)>(bitWisedImage.rows -1, bitWisedImage.cols -1);
			// // hue = pix.val[0];
			// // sat = pix.val[1];
			// // val = pix.val[2];

			// // cout << "\nHue: " << hue << "\nSat: " << sat << "\nVal: " << val << endl;

   //          break; 
   //     	}

//     }

//    return 0;
// }