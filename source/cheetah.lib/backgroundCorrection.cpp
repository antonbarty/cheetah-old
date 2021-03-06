/*
 *  backgroundCorrection.cpp
 *  cheetah
 *
 *  Created by Anton Barty on 23/11/11.
 *  Copyright 2011 CFEL. All rights reserved.
 *
 */


#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <hdf5.h>
#include <stdlib.h>

#include "detectorObject.h"
#include "cheetahGlobal.h"
#include "cheetahEvent.h"
#include "cheetahmodules.h"
#include "median.h"





/*
 *	Subtract persistent background 
 *	(and perform recalculation when required)
 */
void subtractPersistentBackground(cEventData *eventData, cGlobal *global){

	DETECTOR_LOOP {
        if(global->detector[detID].useSubtractPersistentBackground){
			
			/*
			 *	Subtract persistent background
			 */
			int		scaleBg = global->detector[detID].scaleBackground;
			long	pix_nn = global->detector[detID].pix_nn;
			float	*frameData = eventData->detector[detID].corrected_data;
			float	*background = global->detector[detID].selfdark;

			subtractPersistentBackground(frameData, background, scaleBg, pix_nn);
			
			
			/*
			 *	Recalculate background from time to time
			 */
			int16_t *frameBuffer = global->detector[detID].bg_buffer; 
			int		lockThreads = global->detector[detID].useBackgroundBufferMutex;
			long	bufferDepth = global->detector[detID].bgMemory;
			long	bgCounter = global->detector[detID].bgCounter;
			long	bgRecalc = global->detector[detID].bgRecalc;
			long	lastUpdate = global->detector[detID].last_bg_update;
			float	medianPoint = global->detector[detID].bgMedian;
			long	threshold = lrint(bufferDepth*medianPoint);

			// Lock threads for the first few cycles
			if(bgCounter < 3*bgRecalc)
				lockThreads = 1;
			
            pthread_mutex_lock(&global->bgbuffer_mutex);

            if( ((bgCounter % bgRecalc) == 0)  && bgCounter != lastUpdate ) {
				if(lockThreads)
					pthread_mutex_lock(&global->selfdark_mutex);
				printf("Finding %lith smallest element of buffer depth %li\n",threshold,bufferDepth);	
				global->detector[detID].last_bg_update = bgCounter;				
				pthread_mutex_unlock(&global->bgbuffer_mutex);
				calculatePersistentBackground(background, frameBuffer, threshold, bufferDepth, pix_nn);
				if(lockThreads)
					pthread_mutex_unlock(&global->selfdark_mutex);
            }
			else
				pthread_mutex_unlock(&global->bgbuffer_mutex);
			
			
			
			/*
			 *	Remember GMD values  (why is this here?)
			 */
			float	gmd;
			pthread_mutex_lock(&global->selfdark_mutex);
			gmd = (eventData->gmd21+eventData->gmd22)/2;
			global->avgGMD = ( gmd + (global->detector[0].bgMemory-1)*global->avgGMD) / global->detector[0].bgMemory;
			pthread_mutex_unlock(&global->selfdark_mutex);

        }
    }	
}

/*
 *	Update background buffer
 *	Requires eventData->detector[detID].corrected_data_int16
 */
void updateBackgroundBuffer(cEventData *eventData, cGlobal *global, int hit) {
	
	DETECTOR_LOOP {
        if (global->detector[detID].useSubtractPersistentBackground)
						
            if (hit==0 || global->detector[detID].bgIncludeHits) {
				
				int		lockThreads = global->detector[detID].useBackgroundBufferMutex;
				long	bgCounter = global->detector[detID].bgCounter;
				long	bufferDepth = global->detector[detID].bgMemory;
				long	pix_nn = global->detector[detID].pix_nn;
				int16_t	*frameBuffer = global->detector[detID].bg_buffer;
				int16_t	*data16 = eventData->detector[detID].corrected_data_int16;
				

				if(lockThreads)
					pthread_mutex_lock(&global->bgbuffer_mutex);
				
				updateBackgroundBuffer(data16, frameBuffer, bgCounter, bufferDepth, pix_nn);
				global->detector[detID].bgCounter += 1;
				
				if(lockThreads)
					pthread_mutex_unlock(&global->bgbuffer_mutex);
				
			}		
    }
}

// Do we really need a separarte function for this?
void updateBackgroundBuffer(int16_t *data16, int16_t *frameBuffer, long bgCounter, long bufferDepth, long pix_nn){
	
	long frameID = bgCounter%bufferDepth;	
	memcpy(frameBuffer+pix_nn*frameID, data16, pix_nn*sizeof(int16_t));
	
}



/*
 *	Subtract pre-calculated persistent background value
 */
void subtractPersistentBackground(float *data, float *background, int scaleBg, long pix_nn) {
	
	float	top = 0;
	float	s1 = 0;
	float	s2 = 0;
	float	v1, v2;
	float	factor;
	
	
	/*
	 *	Find appropriate scaling factor to match background with current image
	 *	Use with care: this assumes background vector is orthogonal to the image vector (which is often not true)
	 */
	factor = 1;
	if(scaleBg) {
		for(long i=0; i<pix_nn; i++){
			v1 = background[i];
			v2 = data[i];
			
			// Simple inner product gives cos(theta), which is always less than zero
			// Want ( (a.b)/|b| ) * (b/|b|)
			top += v1*v2;
			s1 += v1*v1;
			s2 += v2*v2;
		}
		factor = top/s1;
	}

	// Do the weighted subtraction
	for(long i=0; i<pix_nn; i++) {
		data[i] -= (factor*background[i]);	
	}	
}


/*
 *	Calculate persistent background from stack of remembered frames
 */
void calculatePersistentBackground(float *background, int16_t *frameBuffer, long threshold, long bufferDepth, long pix_nn) {

	// Buffer for median calculation
	int16_t	*buffer = (int16_t*) calloc(bufferDepth, sizeof(int16_t));
		
	// Loop over all pixels 
	for(long i=0; i<pix_nn; i++) {
		
		// Create a local array for sorting
		for(long j=0; j< bufferDepth; j++) {
			buffer[j] = frameBuffer[j*pix_nn+i];
		}
		
		// Find median value of the temporary array
		background[i] = kth_smallest(buffer, bufferDepth, threshold);
	}
	
	free (buffer);
}







/*
 *	Local background subtraction
 */
void subtractLocalBackground(cEventData *eventData, cGlobal *global){
	
	DETECTOR_LOOP {
        if(global->detector[detID].useLocalBackgroundSubtraction) {
			long		asic_nx = global->detector[detID].asic_nx;
			long		asic_ny = global->detector[detID].asic_ny;
			long		nasics_x = global->detector[detID].nasics_x;
			long		nasics_y = global->detector[detID].nasics_y;
			long		radius = global->detector[detID].localBackgroundRadius;
			float		*data = eventData->detector[detID].corrected_data;
			
			subtractLocalBackground(data, radius, asic_nx, asic_ny, nasics_x, nasics_y);
		}
	}
	
}


void subtractLocalBackground(float *data, long radius, long asic_nx, long asic_ny, long nasics_x, long nasics_y) {
	
	// Tank for silly radius values
	if(radius <= 0 || radius >= asic_ny/2 )
		return;
	
	// Raw data array size can be calculated from asic modules
	long	pix_nx = asic_nx*nasics_x;
	long	pix_ny = asic_ny*nasics_y;
	long	pix_nn = pix_nx*pix_ny;
	
	
	// Subunit size
	long nn = (2*radius+1);
	nn=nn*nn;
	
	float	*buffer = (float*) calloc(nn, sizeof(float));
	float	*localBg = (float*) calloc(pix_nn, sizeof(float)); 
	
	
	/*
	 *	Determine local background
	 *	(median over window width either side of current pixel)
	 */
	long		e,ee;
	long		counter;
	
	
	// Loop over ASIC modules 
	for(long mi=0; mi<nasics_x; mi++){
		for(long mj=0; mj<nasics_y; mj++){
			
			// Loop over pixels within a module
			for(long j=0; j<asic_ny; j++){
				for(long i=0; i<asic_nx; i++){
					
					counter = 0;
					e = (j+mj*asic_ny)*pix_nx;
					e += i+mi*asic_nx;
					
					// Loop over median window
					for(long jj=-radius; jj<=radius; jj++){
						for(long ii=-radius; ii<=radius; ii++){
							
							// Quick array bounds check
							if((i+ii) < 0)
								continue;
							if((i+ii) >= asic_nx)
								continue;
							if((j+jj) < 0)
								continue;
							if((j+jj) >= asic_ny)
								continue;
							
							ee = (j+jj+mj*asic_ny)*pix_nx;
							ee += i+ii+mi*asic_nx;
							
							if(ee < 0 || ee >= pix_nn){
								printf("Error: Array bounds error: e = %li > %li\n",e,pix_nn);
								continue;
							}
							
							buffer[counter] = data[ee];
							counter++;
						}
					}
					
					// No elements -> trap an error
					if(counter == 0) {
						printf("Error: Local background counter == 0\n");
						localBg[e] = 0;
						continue;
					}
					if(counter > nn) {
						printf("Error: counter == %li > %li\n",counter,nn);
						continue;
					}
					
					// Find median value
					localBg[e] = kth_smallest(buffer, counter, counter/2);
				}
			}
		}
	}
	
	
	/*
	 *	Do the background subtraction
	 */
	for(long i=0;i<pix_nn;i++) {
		data[i] -= localBg[i];	
	}	
	
	
	// Cleanup
	free(localBg);
	free(buffer);
}




/*
 * Make a saturated pixel mask
 */
void checkSaturatedPixels(uint16_t *raw_data, int16_t *saturatedPixelMask, long pix_nn, long pixelSaturationADC) {
	
	for(long i=0; i<pix_nn; i++) { 
		if ( raw_data[i] >= pixelSaturationADC) 
			saturatedPixelMask[i] = 0;
		else
			saturatedPixelMask[i] = 1;
	}
}


void checkSaturatedPixels(cEventData *eventData, cGlobal *global){

    DETECTOR_LOOP {
        if ( global->detector[detID].maskSaturatedPixels == 1 ) {
			
			uint16_t	*raw_data = eventData->detector[detID].raw_data;
			int16_t		*saturatedPixelMask = eventData->detector[detID].saturatedPixelMask;
			long		nn = global->detector[detID].pix_nn;
			long		pixelSaturationADC = global->detector[detID].pixelSaturationADC;
			
			checkSaturatedPixels(raw_data, saturatedPixelMask, nn, pixelSaturationADC);
		}
	}	
}



