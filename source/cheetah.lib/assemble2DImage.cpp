//
//  assembleImage.cpp
//  cheetah
//
//  Created by Anton Barty on 14/08/12.
//  Copyright (c) 2012 CFEL. All rights reserved.
//


#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <hdf5.h>
#include <stdlib.h>

#include "cheetah.h"
#include "cheetahmodules.h"
#include "median.h"



void assemble2Dimage(cEventData *eventData, cGlobal *global) {

    DETECTOR_LOOP {
        long		pix_nn = global->detector[detID].pix_nn;
        long		image_nx = global->detector[detID].image_nx;
        long		image_nn = global->detector[detID].image_nn;
        float		*pix_x = global->detector[detID].pix_x;
        float		*pix_y = global->detector[detID].pix_y;
        float		*corrected_data = eventData->detector[detID].corrected_data;
        int16_t		*image = eventData->detector[detID].image;
        
        assemble2Dimage(image, corrected_data, pix_x, pix_y, pix_nn, image_nx, image_nn);
        
    }
}



/*
 *	Interpolate raw (corrected) cspad data into a physical 2D image
 *	using pre-defined pixel mapping (as loaded from .h5 file)
 */
void assemble2Dimage(int16_t *image, float *corrected_data, float *pix_x, float *pix_y, long pix_nn, long image_nx, long image_nn) {
	
    	
	// Allocate temporary arrays for pixel interpolation (needs to be floating point)
	float	*data = (float*) calloc(image_nn,sizeof(float));
	float	*weight = (float*) calloc(image_nn,sizeof(float));
	for(long i=0; i<image_nn; i++){
		data[i] = 0;
		weight[i]= 0;
	}
	
	
	// Loop through all pixels and interpolate onto regular grid
	float	x, y;
	float	pixel_value, w;
	long	ix, iy;
	float	fx, fy;
	long	image_index;
    
	for(long i=0;i<pix_nn;i++){
		// Pixel location with (0,0) at array element (0,0) in bottom left corner
		x = pix_x[i] + image_nx/2;
		y = pix_y[i] + image_nx/2;
		pixel_value = corrected_data[i];
		
		// Split coordinate into integer and fractional parts
		ix = (long) floor(x);
		iy = (long) floor(y);
		fx = x - ix;
		fy = y - iy;
		
		// Interpolate intensity over adjacent 4 pixels using fractional overlap as the weighting factor
		// (0,0)
		if(ix>=0 && iy>=0 && ix<image_nx && iy<image_nx) {
			w = (1-fx)*(1-fy);
			image_index = ix + image_nx*iy;
			data[image_index] += w*pixel_value;
			weight[image_index] += w;
		}
		// (+1,0)
		if((ix+1)>=0 && iy>=0 && (ix+1)<image_nx && iy<image_nx) {
			w = (fx)*(1-fy);
			image_index = (ix+1) + image_nx*iy;
			data[image_index] += w*pixel_value;
			weight[image_index] += w;
		}
		// (0,+1)
		if(ix>=0 && (iy+1)>=0 && ix<image_nx && (iy+1)<image_nx) {
			w = (1-fx)*(fy);
			image_index = ix + image_nx*(iy+1);
			data[image_index] += w*pixel_value;
			weight[image_index] += w;
		}
		// (+1,+1)
		if((ix+1)>=0 && (iy+1)>=0 && (ix+1)<image_nx && (iy+1)<image_nx) {
			w = (fx)*(fy);
			image_index = (ix+1) + image_nx*(iy+1);
			data[image_index] += w*pixel_value;
			weight[image_index] += w;
		}
	}
	
	
	// Reweight pixel interpolation
	for(long i=0; i<image_nn; i++){
		if(weight[i] < 0.05)
			data[i] = 0;
		else
			data[i] /= weight[i];
	}
    
	
    
	// Check for int16 overflow
	for(long i=0;i<image_nn;i++){
		if(lrint(data[i]) > 32767) 
			data[i]=32767;
		if(lrint(data[i]) < -32767) 
			data[i]=-32767;
	}
	
	
	// Copy interpolated image across into int_16 image array
	for(long i=0;i<image_nn;i++){
		image[i] = (int16_t) lrint(data[i]);
	}
	
	
	// Free temporary arrays
	free(data);
	free(weight);
	
}
