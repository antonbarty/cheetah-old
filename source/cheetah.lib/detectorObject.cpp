/*
 *  pixelDetector.cpp
 *  cheetah
 *
 *  Created by Anton Barty on 7/2/11.
 *  Copyright 2011 CFEL. All rights reserved.
 *
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>
#include <limits>
#include <hdf5.h>
#include <fenv.h>
#include <stdlib.h>

#include "data2d.h"
#include "detectorObject.h"
#include "cheetahGlobal.h"
#include "cheetahEvent.h"

/*
 *	Set default values to something reasonable for CSPAD at CXI 
 */
cPixelDetectorCommon::cPixelDetectorCommon() {
    
    // Defaults to CXI cspad configuration
    strcpy(detectorType, "cspad");
    strcpy(detectorName, "CxiDs1");
    //detectorType = Pds::DetInfo::Cspad;
    //detectorPdsDetInfo = Pds::DetInfo::CxiDs1;
    
    // Calibration files
    strcpy(detectorConfigFile, "No_file_specified");
    strcpy(geometryFile, "No_file_specified");
    strcpy(badpixelFile, "No_file_specified");
    strcpy(darkcalFile, "No_file_specified");
    strcpy(wireMaskFile, "No_file_specified");
    strcpy(gaincalFile, "No_file_specified");
    
    // Default ASIC layout (cspad)
    asic_nx = CSPAD_ASIC_NX;
    asic_ny = CSPAD_ASIC_NY;
    asic_nn = CSPAD_ASIC_NX * CSPAD_ASIC_NY;
    nasics_x = CSPAD_nASICS_X;
    nasics_y = CSPAD_nASICS_Y;
    pix_nx = asic_nx*nasics_x;
    pix_ny = asic_ny*nasics_y;
    pix_nn = pix_nx*pix_ny;
    pixelSize = 110e-6;

    
    // Detector Z position
    //strcpy(detectorZpvname, "CXI:DS1:MMS:06.RBV");
    strcpy(detectorZpvname,"");
    //defaultCameraLengthMm = std::numeric_limits<float>::quiet_NaN();
    defaultCameraLengthMm = 0;
    fixedCameraLengthMm = 0;
    detposprev = 0;
    cameraLengthOffset = 500.0 + 79.0;
    cameraLengthScale = 1e-3;
    
    beamCenterPixX = 0;
    beamCenterPixY = 0;
    
    // Bad pixel mask    
    useBadPixelMask = 0;
    
    // Saturated pixels
    maskSaturatedPixels = 0;
    pixelSaturationADC = 15564;  // 95% of 2^14 ??

    // Static dark calibration (electronic offsets)
    useDarkcalSubtraction = 0;

    // Common mode subtraction from each ASIC
    cmModule = 0;
    cmFloor = 0.1;
    cspadSubtractUnbondedPixels = 0;
    cspadSubtractBehindWires = 0;

    // Gain calibration correction
    useGaincal = 0;
    invertGain = 0;
	
    // Subtraction of running background (persistent photon background) 
    useSubtractPersistentBackground = 0;
    bgMemory = 50;
    startFrames = 0;
    scaleBackground = 0;
    useBackgroundBufferMutex = 0;
    bgMedian = 0.5;
    bgRecalc = bgMemory;
    bgIncludeHits = 0;
    bgNoBeamReset = 0;
    bgFiducialGlitchReset = 0;
	
    // Local background subtraction
    useLocalBackgroundSubtraction = 0;
    localBackgroundRadius = 3;
	
    // Kill persistently hot pixels
    useAutoHotpixel = 0;
    hotpixFreq = 0.9;
    hotpixADC = 1000;
    hotpixMemory = 50;
    
    // correction for PNCCD read out artifacts 
    usePnccdOffsetCorrection = 0;

    // Saving options
    saveDetectorCorrectedOnly = 0;
    saveDetectorRaw = 0;


}

void cPixelDetectorCommon::configure(void) {
	
	/*
	 * Configure basic detector parameters
	 */
	printf("Configuring for detector: %s\n",detectorName);
	
	if(strcmp(detectorName, "CxiDs1") == 0 || strcmp(detectorName, "CxiDs2") == 0 ||
	   strcmp(detectorName, "CxiDsd") == 0 || strcmp(detectorName, "XppGon") == 0) {
		strcpy(detectorType, "cspad");
		asic_nx = CSPAD_ASIC_NX;
		asic_ny = CSPAD_ASIC_NY;
		asic_nn = CSPAD_ASIC_NX * CSPAD_ASIC_NY;
		nasics_x = CSPAD_nASICS_X;
		nasics_y = CSPAD_nASICS_Y;
		pix_nx = CSPAD_ASIC_NX*CSPAD_nASICS_X;
		pix_ny = CSPAD_ASIC_NY*CSPAD_nASICS_Y;
		pix_nn = pix_nx * pix_ny;
		pixelSize = 110e-6;
	} 
	else if(strcmp(detectorName, "pnCCD") == 0 ) {
		strcpy(detectorType, "pnccd");
		asic_nx = PNCCD_ASIC_NX;
		asic_ny = PNCCD_ASIC_NY;
		asic_nn = PNCCD_ASIC_NX * PNCCD_ASIC_NY;
		nasics_x = PNCCD_nASICS_X;
		nasics_y = PNCCD_nASICS_Y;
		pix_nx = PNCCD_ASIC_NX * PNCCD_nASICS_X;
		pix_ny = PNCCD_ASIC_NY * PNCCD_nASICS_Y;
		pix_nn = pix_nx * pix_ny;
		pixelSize = 75e-6;
		
	}
	else {
		printf("Error: unknown detector name %s\n", detectorName);
		printf("cPixelDetectorCommon::configure()\n");
		printf("Quitting\n");
		exit(1);
	}
	
	printf("\tDetector size: %lix%li\n",pix_nx,pix_ny);
	printf("\tASIC geometry: %lix%li\n",nasics_x,nasics_y);
	printf("\tASIC size: %lix%li\n",asic_nx,asic_ny);
	printf("\tPixel size: %g (m)\n",pixelSize);
}


/*
 *	Process tags for both configuration file and command line options
 */
int cPixelDetectorCommon::parseConfigTag(char *tag, char *value) {
	
	int fail = 0;

	/*
	 *	Convert to lowercase
	 */
	for(int i=0; i<strlen(tag); i++) 
		tag[i] = tolower(tag[i]);

	if (!strcmp(tag, "detectorname")) {
		strcpy(detectorName, value);
	}
	else if (!strcmp(tag, "detectortype")) {
		strcpy(detectorType, value);
	}
	else if (!strcmp(tag, "geometry")) {
		strcpy(geometryFile, value);
	}
	else if (!strcmp(tag, "darkcal")) {
		strcpy(darkcalFile, value);
		useDarkcalSubtraction = 1;
	}
	else if (!strcmp(tag, "gaincal")) {
		strcpy(gaincalFile, value);
		useGaincal = 1;
	}
	else if (!strcmp(tag, "badpixelmap")) {
		strcpy(badpixelFile, value);
		useBadPixelMask = 1;
	}
	else if (!strcmp(tag, "baddatamap")) {
		strcpy(baddataFile, value);
		useBadDataMask = 1;
	}
	else if (!strcmp(tag, "wiremask")) {
		strcpy(wireMaskFile, value);
	}
	else if (!strcmp(tag, "pixelsize")) {
		pixelSize = atof(value);
	}
	else if (!strcmp(tag, "savedetectorcorrectedonly")) {
		saveDetectorCorrectedOnly = atoi(value);
	}
	else if (!strcmp(tag, "savedetectorraw")) {
		saveDetectorRaw = atoi(value);
	}
	else if (!strcmp(tag, "beamcenterx")) {
		beamCenterPixX  = atof(value);
	}
	else if (!strcmp(tag, "beamcentery")) {
		beamCenterPixY  = atof(value);
	} 
	else if (!strcmp(tag, "detectorzpvname")) {
		strcpy(detectorZpvname, value);
	}
    else if (!strcmp(tag, "defaultcameralengthmm")) {
		defaultCameraLengthMm = atof(value);
	}
	else if (!strcmp(tag, "fixedcameralengthmm")) {
		fixedCameraLengthMm = atof(value);
	}
	else if (!strcmp(tag, "cameralengthoffset")) {
		cameraLengthOffset = atof(value);
	}
	else if (!strcmp(tag, "cameralengthscale")) {
		cameraLengthScale  = atof(value);
	}
	else if (!strcmp(tag, "masksaturatedpixels")) {
		maskSaturatedPixels = atoi(value);
	}
	else if (!strcmp(tag, "bgmemory")) {
		bgMemory = atoi(value);
	}
	else if (!strcmp(tag, "useautohotpixel")) {
		//useAutoHotpixel = atoi(value);
		// Eventually delete this, but not during beamtime!
	}
	else if (!strcmp(tag, "hotpixfreq")) {
		hotpixFreq = atof(value);
		useAutoHotpixel = 1;
	}
	else if (!strcmp(tag, "hotpixadc")) {
		hotpixADC = atoi(value);
		useAutoHotpixel = 1;
	}
	else if (!strcmp(tag, "hotpixmemory")) {
		hotpixMemory = atoi(value);
	}
	else if (!strcmp(tag, "cmmodule")) {
		cmModule = atoi(value);
	}
	else if (!strcmp(tag, "cmfloor")) {
		cmFloor = atof(value);
	}
    
	// Local background subtraction
	else if (!strcmp(tag, "uselocalbackgroundsubtraction")) {
		useLocalBackgroundSubtraction = atoi(value);
	}
	else if (!strcmp(tag, "localbackgroundradius")) {
		localBackgroundRadius = atoi(value);
	}
    
	else if (!strcmp(tag, "pixelsaturationadc")) {
		pixelSaturationADC = atoi(value);
	}
	else if (!strcmp(tag, "useselfdarkcal")) {
		printf("The keyword useSelfDarkcal has been changed.  It is\n"
               "now known as useSubtractPersistentBackground.\n"
               "Modify your ini file and try again...\n");
		fail = 1;
	}
	else if (!strcmp(tag, "usesubtractpersistentbackground")) {
		useSubtractPersistentBackground = atoi(value);
	}
	else if (!strcmp(tag, "usebackgroundbuffermutex")) {
		useBackgroundBufferMutex = atoi(value);
	}
	else if (!strcmp(tag, "subtractbehindwires")) {
		cspadSubtractBehindWires = atoi(value);
	}
	else if (!strcmp(tag, "invertgain")) {
		invertGain = atoi(value);
	}
	else if (!strcmp(tag, "subtractunbondedpixels")) {
		cspadSubtractUnbondedPixels = atoi(value);
	}
	else if (!strcmp(tag, "usepnccdoffsetcorrection")) {
	  //usePnccdOffsetCorrection = atoi(value);
	  usePnccdOffsetCorrection = 1;
	}
	else if (!strcmp(tag, "bgrecalc")) {
		bgRecalc = atoi(value);
	}
	else if (!strcmp(tag, "bgmedian")) {
		bgMedian = atof(value);
	}
	else if (!strcmp(tag, "bgincludehits")) {
		bgIncludeHits = atoi(value);
	}
	else if (!strcmp(tag, "bgnobeamreset")) {
		bgNoBeamReset = atoi(value);
	}
	else if (!strcmp(tag, "bgfiducialglitchreset")) {
		bgFiducialGlitchReset = atoi(value);
	}	
	else if (!strcmp(tag, "scalebackground")) {
		scaleBackground = atoi(value);
	}
	else if (!strcmp(tag, "scaledarkcal")) {
		printf("The keyword scaleDarkcal does the same thing as scaleBackground.\n"
               "Use scaleBackground instead.\n"
               "Modify your ini file and try again...\n");
		fail = 1;
	}
	else if (!strcmp(tag, "startframes")) {
		startFrames = atoi(value);
	} 
	// Unknown tags
	else {
		fail = 1;
	}

	return fail;

}


/*
 *  Allocate arrays for memory, etc
 */
void cPixelDetectorCommon::allocatePowderMemory(cGlobal *global) {
    
    // Constants
    nPowderClasses = global->nPowderClasses;
    radialStackSize = global->radialStackSize;
    
    
    // Background buffers and the like
    selfdark = (float*) calloc(pix_nn, sizeof(float));
    bg_buffer = (int16_t*) calloc(bgMemory*pix_nn, sizeof(int16_t)); 
    hotpix_buffer = (int16_t*) calloc(hotpixMemory*pix_nn, sizeof(int16_t)); 
    hotpixelmask = (int16_t*) calloc(pix_nn, sizeof(int16_t));
    wiremask = (int16_t*) calloc(pix_nn, sizeof(int16_t));
    for(long j=0; j<pix_nn; j++) {
        selfdark[j] = 0;
        hotpixelmask[j] = 1;
        wiremask[j] = 1;
    }
    
    // Powder sums and mutexes
    for(long i=0; i<nPowderClasses; i++) {
		nPowderFrames[i] = 0;
		powderRaw[i] = (double*) calloc(pix_nn, sizeof(double));
		powderRawSquared[i] = (double*) calloc(pix_nn, sizeof(double));
		powderAssembled[i] = (double*) calloc(image_nn, sizeof(double));
        
		pthread_mutex_init(&powderRaw_mutex[i], NULL);
		pthread_mutex_init(&powderRawSquared_mutex[i], NULL);
		pthread_mutex_init(&powderAssembled_mutex[i], NULL);
        pthread_mutex_init(&radialStack_mutex[i], NULL);
		
		for(long j=0; j<pix_nn; j++) {
			powderRaw[i][j] = 0;
			powderRawSquared[i][j] = 0;
		}
		for(long j=0; j<image_nn; j++) {
			powderAssembled[i][j] = 0;
		}
    }    
	
    
    // Radial stacks
    for(long i=0; i<nPowderClasses; i++) {
        radialStackCounter[i] = 0;
        radialAverageStack[i] = (float *) calloc(radial_nn*global->radialStackSize, sizeof(float));
        
        for(long j=0; j<radial_nn*global->radialStackSize; j++) {
            radialAverageStack[i][j] = 0;
        }
	}
    
}




/*
 *	Read in detector pixel layout
 */
void cPixelDetectorCommon::readDetectorGeometry(char* filename) {

	// Pixel size (measurements in geometry file are in m)
	//module_rows = asic_nx;
	//module_cols = asic_ny;	

	
	// Set filename here 
	printf("Reading detector geometry:\n");
	printf("\t%s\n",filename);
	
	
	// Check whether pixel map file exists!
	FILE* fp = fopen(filename, "r");
	if (fp) 	// file exists
		fclose(fp);
	else {		// file doesn't exist
		printf("Error: Detector geometry file does not exist: %s\n",filename);
		exit(1);
	}
	
	
	// Read pixel locations from file
	cData2d		detector_x;
	cData2d		detector_y;
	cData2d		detector_z;
	detector_x.readHDF5(filename, (char *) "x");
	detector_y.readHDF5(filename, (char *) "y");
	detector_z.readHDF5(filename, (char *) "z");
	
	// Sanity check that all detector arrays are the same size (!)
	if (detector_x.nn != detector_y.nn || detector_x.nn != detector_z.nn) {
		printf("readDetectorGeometry: array size mismatch\n");
		printf("Dimensions of x,y,z data do not match\n");
		printf("%li != %li || %li != %li\n", detector_x.nn, detector_y.nn, detector_x.nn, detector_z.nn);
		exit(1);
	}
	

	// Sanity check that size matches what we expect for cspad (!)
	if (detector_x.nx != pix_nx || detector_x.ny != pix_ny) {
		printf("readDetectorGeometry: array size mismatch\n");
		printf("%ldx%ld != %lix%li\n", pix_nx, pix_ny, detector_x.nx, detector_x.ny);
		exit(1);
	}
	
	
	// Create local arrays for detector pixel locations
	long	nx = pix_nx;
	long	ny = pix_ny;
	long	nn = nx*ny;
	long 	i;
	//pix_nx = nx;
	//pix_ny = ny;
	//pix_nn = nn;
	pix_x = (float *) calloc(nn, sizeof(float));
	pix_y = (float *) calloc(nn, sizeof(float));
	pix_z = (float *) calloc(nn, sizeof(float));
	pix_kx = (float *) calloc(nn, sizeof(float));
	pix_ky = (float *) calloc(nn, sizeof(float));
	pix_kz = (float *) calloc(nn, sizeof(float));
	pix_kr = (float *) calloc(nn, sizeof(float));
	pix_res = (float *) calloc(nn, sizeof(float));
	//hitfinderResMask = (int *) calloc(nn, sizeof(int)); // is there a better place for this?
	//for (i=0;i<nn;i++) hitfinderResMask[i]=1;
	printf("\tPixel map is %li x %li pixel array\n",nx,ny);
	
	
	// Copy values from 2D array
	for(long i=0;i<nn;i++){
		pix_x[i] = (float) detector_x.data[i];
		pix_y[i] = (float) detector_y.data[i];
		pix_z[i] = (float) detector_z.data[i];
	}
	
		
bounds:
	
	// Find bounds of image array
	float	xmax = -1e9;
	float	xmin =  1e9;
	float	ymax = -1e9;
	float	ymin =  1e9;
	for(long i=0;i<nn;i++){
		if (pix_x[i] > xmax) xmax = pix_x[i];
		if (pix_x[i] < xmin) xmin = pix_x[i];
		if (pix_y[i] > ymax) ymax = pix_y[i];
		if (pix_y[i] < ymin) ymin = pix_y[i];
	}
	printf("\tGeometry range:\n");
	printf("\tx range %f to %f\n",xmin,xmax);
	printf("\ty range %f to %f\n",ymin,ymax);
	
	// If physical detector size is less than 1 x 1, detector coordinates
	// must have been specified in physical distance (m) and not in pixels.
	if (fabs(xmax-xmin)<1 && fabs(ymax-ymin)<1 ) {
		printf("\tData range <1m in both x and y directions.\n");
		printf("\tConverting detector coordinates from meters to pixels\n");
        printf("\tPixel size: %g m\n",pixelSize);
		for(i=0;i<nn;i++){
			pix_x[i] /= pixelSize;
			pix_y[i] /= pixelSize;
			pix_z[i] /= pixelSize;
		}
		goto bounds;
	}

	fesetround(1);
	xmax = lrint(xmax);
	xmin = lrint(xmin);
	ymax = lrint(ymax);
	ymin = lrint(ymin);
	printf("\tImage bounds:\n");
	printf("\tx range %.0f to %.0f\n",xmin,xmax);
	printf("\ty range %.0f to %.0f\n",ymin,ymax);
	
	
	// How big must we make the output image?
	float max = xmax;
	if(ymax > max) max = ymax;
	if(fabs(xmin) > max) max = fabs(xmin);
	if(fabs(ymin) > max) max = fabs(ymin);
	image_nx = 2*(unsigned)max;
	image_nn = image_nx*image_nx;
	printf("\tImage output array will be %li x %li\n",image_nx,image_nx);
	
	// Apply image center shift
	for(i=0;i<nn;i++){
		pix_x[i] -= beamCenterPixX;
		pix_y[i] -= beamCenterPixY;
	}
	

	// Compute radial distances
	pix_r = (float *) calloc(nn, sizeof(float));
	radial_max = 0.0;
	for(long i=0;i<nn;i++){
		pix_r[i] = sqrt(pix_x[i]*pix_x[i]+pix_y[i]*pix_y[i]);
		if(pix_r[i] > radial_max)
			radial_max = pix_r[i];
	}	
	radial_nn = (long int) ceil(radial_max)+1;
}



/*
 *  Update K-space variables
 *  (called whenever detector has moved)
 */
void cPixelDetectorCommon::updateKspace(cGlobal *global, float wavelengthA) {
    double   x, y, z, r;
    double   kx,ky,kz,kr;
    double   res,minres,maxres;
	double	 sin_theta;
	
	minres = 100000;
	maxres = 0.0;

    printf("Recalculating K-space coordinates\n");

    for (long i=0; i<pix_nn; i++ ) {
        x = pix_x[i]*pixelSize;
        y = pix_y[i]*pixelSize;
        z = pix_z[i]*pixelSize + detectorZ*cameraLengthScale;
        r = sqrt(x*x + y*y + z*z);
		
        kx = (x/r)/wavelengthA;
        ky = (y/r)/wavelengthA;
        kz = (z/r - 1)/wavelengthA;                 // assuming incident beam is along +z direction
        kr = sqrt(kx*kx + ky*ky + kz*kz);
        //res = 1.0/kr;
		sin_theta = sqrt(x*x+y*y)/r;
		res = wavelengthA/(sin_theta);
        
        pix_kx[i] = kx;
        pix_ky[i] = ky;
        pix_kz[i] = kz;
        pix_kr[i] = kr;
        pix_res[i] = res;
        
		if ( res < minres )
			minres = res;
		if ( res > maxres )
			maxres = res;
        
		
        // Generate resolution limit mask
		// (resolution in PIXELS)
		if (pix_r[i] < global->hitfinderMaxRes && pix_r[i] > global->hitfinderMinRes ) 
			global->hitfinderResMask[i] = 1;
		else
			global->hitfinderResMask[i] = 0;
    }

	printf("\tCurrent resolution (i.e. d-spacing) range is %.2f - %.2f A\n", minres, maxres);

}


/*
 *	Read in darkcal file
 */
void cPixelDetectorCommon::readDarkcal(char *filename){	
	
	// Create memory space and pad with zeros
	darkcal = (float*) calloc(pix_nn, sizeof(float));
    for(long i=0; i<pix_nn; i++)
        darkcal[i] = 0;

	// Do we need a darkcal file?	
	if (useDarkcalSubtraction == 0){
		return;
	}
	
	// Check if a darkcal file has been specified
	if ( strcmp(filename,"") == 0 ){
		printf("Darkcal file path was not specified.\n");
		exit(1);
	}	

	printf("Reading darkcal configuration:\n");
	printf("\t%s\n",filename);

	// Check whether file exists!
	FILE* fp = fopen(filename, "r");
	if (fp) 	// file exists
		fclose(fp);
	else {		// file doesn't exist
		printf("\tDarkcal file does not exist: %s\n",filename);
		printf("\tAborting...\n");
		exit(1);
	}
	
	
	// Read darkcal data from file
	cData2d		temp2d;
	temp2d.readHDF5(filename);
	
	// Correct geometry?
	if(temp2d.nx != pix_nx || temp2d.ny != pix_ny) {
		printf("\tGeometry mismatch: %lix%li != %lix%li\n",temp2d.nx, temp2d.ny, pix_nx, pix_ny);
		printf("\tAborting...\n");
		exit(1);
	} 
	
	// Copy into darkcal array
	for(long i=0;i<pix_nn;i++)
		darkcal[i] = temp2d.data[i];
	
}


/*
 *	Read in gaincal file
 */
void cPixelDetectorCommon::readGaincal(char *filename){
	

	// Create memory space and set default gain to 1 everywhere
	gaincal = (float*) calloc(pix_nn, sizeof(float));
	for(long i=0;i<pix_nn;i++)
		gaincal[i] = 1;

	// Do we even need a gaincal file?
	if ( useGaincal == 0 ){
		return;
	}

	// Check if a gain calibration file has been specified
	if ( strcmp(filename,"") == 0 ){
		printf("Gain calibration file path was not specified.\n");
		printf("Aborting...\n");
		exit(1);
	}	

	printf("Reading detector gain calibration:\n");
	printf("\t%s\n",filename);

		
	// Check whether gain calibration file exists!
	FILE* fp = fopen(filename, "r");
	if (fp) 	// file exists
		fclose(fp);
	else {		// file doesn't exist
		printf("\tGain calibration file does not exist: %s\n",filename);
		printf("\tAborting...\n");
		exit(1);
	}
	
	
	// Read darkcal data from file
	cData2d		temp2d;
	temp2d.readHDF5(filename);
	

	// Correct geometry?
	if(temp2d.nx != pix_nx || temp2d.ny != pix_ny) {
		printf("\tGeometry mismatch: %lix%li != %lix%li\n",temp2d.nx, temp2d.ny, pix_nx, pix_ny);
		printf("\tAborting...\n");
		exit(1);
	} 
	
	
	// Copy into gaincal array
	for(long i=0;i<pix_nn;i++)
		gaincal[i] = (float) temp2d.data[i];


	// Invert the gain so we have an array that all we need to do is simple multiplication
	// Pixels with zero gain become dead pixels
	if(invertGain) {
		for(long i=0;i<pix_nn;i++) {
			if(gaincal[i] != 0)
				gaincal[i] = 1.0/gaincal[i];
			else 
				gaincal[i] = 0;
		}
	}
	
}


/*
 *	Read in peaksearch mask
 */
void cPixelDetectorCommon::readPeakmask(cGlobal *global, char *filename){
	
	// Create memory space and default to searching for peaks everywhere
	peakmask = (int16_t*) calloc(pix_nn, sizeof(int16_t));
	for(long i=0;i<pix_nn;i++)
		peakmask[i] = 1;
	
	// Do we even need a peakmask file?
	if ( global->hitfinderUsePeakmask == 0 ){
		return;
	}

	// Check if a peakmask file has been specified
	if ( strcmp(filename,"") == 0 ){
		printf("Peakmask file path was not specified.\n");
		printf("Aborting...\n");
		exit(1);
	}	

	printf("Reading peak search mask:\n");
	printf("\t%s\n",filename);
	
	// Check whether file exists!
	FILE* fp = fopen(filename, "r");
	if (fp) 	// file exists
		fclose(fp);
	else {		// file doesn't exist
		printf("\tPeak search mask does not exist: %s\n",filename);
		printf("\tAborting...\n");
		exit(1);
	}
	
	
	// Read darkcal data from file
	cData2d		temp2d;
	temp2d.readHDF5(filename);
	
	
	// Correct geometry?
	if(temp2d.nx != pix_nx || temp2d.ny != pix_ny) {
		printf("\tGeometry mismatch: %lix%li != %lix%li\n",temp2d.nx, temp2d.ny, pix_nx, pix_ny);
		printf("\tAborting...\n");
		exit(1);
	} 
	
	
	// Copy into darkcal array
	for(long i=0;i<pix_nn;i++)
		peakmask[i] = (int16_t) temp2d.data[i];
}


/*
 *	Read in bad pixel mask
 *  (Pixels will be set to zero before any analysis and when data is exported)
 */
void cPixelDetectorCommon::readBadpixelMask(char *filename){
	
	
	// Create memory space and default to searching for peaks everywhere
	badpixelmask = (int16_t*) calloc(pix_nn, sizeof(int16_t));
	for(long i=0;i<pix_nn;i++)
		badpixelmask[i] = 1;
	
	
	// Do we need a bad pixel map?
	if ( useBadPixelMask == 0 ){
		return;
	}

	// Check if a bad pixel mask file has been specified
	if ( strcmp(filename,"") == 0 ){
		printf("Bad pixel mask file path was not specified.\n");
		printf("Aborting...\n");
		exit(1);
	}	

	printf("Reading bad pixel mask:\n");
	printf("\t%s\n",filename);

	// Check whether file exists!
	FILE* fp = fopen(filename, "r");
	if (fp) 	// file exists
		fclose(fp);
	else {		// file doesn't exist
		printf("\tBad pixel mask does not exist: %s\n",filename);
		printf("\tAborting...\n");
		exit(1);
	}
	
	
	// Read darkcal data from file
	cData2d		temp2d;
	temp2d.readHDF5(filename);
	
	
	// Correct geometry?
	if(temp2d.nx != pix_nx || temp2d.ny != pix_ny) {
		printf("\tGeometry mismatch: %lix%li != %lix%li\n",temp2d.nx, temp2d.ny, pix_nx, pix_ny);
		printf("\tAborting...\n");
		exit(1);
	} 
	
	
	// Copy back into array
	for(long i=0;i<pix_nn;i++)
		badpixelmask[i] = (int16_t) temp2d.data[i];
}

/*
 *	Read in bad data mask
 *  (Pixels will be ignored in analysis, but passed through when data is exported)
 */
void cPixelDetectorCommon::readBaddataMask(char *filename){
	
	
	// Create memory space and default to searching for peaks everywhere
	baddatamask = (int16_t*) calloc(pix_nn, sizeof(int16_t));
	for(long i=0;i<pix_nn;i++)
		baddatamask[i] = 1;
	
	
	// Do we need a bad pixel map?
	if ( useBadDataMask == 0 ){
		return;
	}
    
	// Check if a bad pixel mask file has been specified
	if ( strcmp(filename,"") == 0 ){
		printf("Bad data mask file path was not specified.\n");
		printf("Aborting...\n");
		exit(1);
	}	
    
	printf("Reading bad data mask:\n");
	printf("\t%s\n",filename);
    
	// Check whether file exists!
	FILE* fp = fopen(filename, "r");
	if (fp) 	// file exists
		fclose(fp);
	else {		// file doesn't exist
		printf("\tBad data mask does not exist: %s\n",filename);
		printf("\tAborting...\n");
		exit(1);
	}
	
	
	// Read darkcal data from file
	cData2d		temp2d;
	temp2d.readHDF5(filename);
	
	
	// Correct geometry?
	if(temp2d.nx != pix_nx || temp2d.ny != pix_ny) {
		printf("\tGeometry mismatch: %lix%li != %lix%li\n",temp2d.nx, temp2d.ny, pix_nx, pix_ny);
		printf("\tAborting...\n");
		exit(1);
	} 
	
	
	// Copy back into array
	for(long i=0;i<pix_nn;i++)
		baddatamask[i] = (int16_t) temp2d.data[i];
}


/*
 *	Read in wire mask
 */
void cPixelDetectorCommon::readWireMask(char *filename){
	

	// Create memory space and default to searching for peaks everywhere
	wiremask = (int16_t*) calloc(pix_nn, sizeof(int16_t));
	for(long i=0;i<pix_nn;i++)
		wiremask[i] = 1;
	
	// Do we need this file?
	if ( cspadSubtractBehindWires == 0 ){
		return;
	}
	
	printf("Reading wire mask:\n");
	printf("\t%s\n",filename);

	// Check if a wire mask file has been specified.
	// We need to exit if this is expected but does not
	// exist.
	if ( strcmp(filename,"") == 0 ){
		printf("Wire mask file path was not specified.\n");
		printf("Aborting...\n");
		exit(1);
	}	

	// Check whether file exists!
	FILE* fp = fopen(filename, "r");
	if (fp) 	// file exists
		fclose(fp);
	else {		// file doesn't exist
		printf("\tWire mask does not exist: %s\n",filename);
		printf("\tAborting...\n");
		exit(1);
	}
	
	
	// Read darkcal data from file
	cData2d		temp2d;
	temp2d.readHDF5(filename);
	
	
	// Correct geometry?
	if(temp2d.nx != pix_nx || temp2d.ny != pix_ny) {
		printf("\tGeometry mismatch: %lix%li != %lix%li\n",temp2d.nx, temp2d.ny, pix_nx, pix_ny);
		printf("\tAborting...\n");
		exit(1);
	} 
	
	
	// Copy into darkcal array
	for(long i=0;i<pix_nn;i++)
		wiremask[i] = (int16_t) temp2d.data[i];
}

cPixelDetectorEvent::cPixelDetectorEvent() {

	detectorZ=0;

}
