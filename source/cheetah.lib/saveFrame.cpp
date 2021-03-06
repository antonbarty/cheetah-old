/*
 *  saveFrame.cpp
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
#include <sys/stat.h>


#include "detectorObject.h"
#include "cheetahGlobal.h"
#include "cheetahEvent.h"
#include "cheetahmodules.h"
#include "median.h"





/*
 *	Create a standardised name for this event
 */
void nameEvent(cEventData *event, cGlobal *global){
	/*
	 *	Create filename based on date, time and fiducial for this image
	 */
	char buffer1[80];
	char buffer2[80];	
	time_t eventTime = event->seconds;
	
	struct tm *timestatic, timelocal;
	timestatic=localtime_r( &eventTime, &timelocal );	
	strftime(buffer1,80,"%Y_%b%d",&timelocal);
	strftime(buffer2,80,"%H%M%S",&timelocal);
	sprintf(event->eventname,"LCLS_%s_r%04u_%s_%x.h5",buffer1,global->runNumber,buffer2,event->fiducial);
}



/*
 *	Update the subdirectory name
 */
void makeSubdir(cEventData *event, cGlobal *global) {

	long filesPerDirectory = 1000;
	
	pthread_mutex_lock(&global->subdir_mutex);
	
	if (global->subdirFileCount == filesPerDirectory || global->subdirFileCount == -1) {
		char subdir[80];
		global->subdirNumber += 1;
		sprintf(subdir, "data%li", global->subdirNumber);
		mkdir(subdir, 0777);
		strcpy(global->subdirName, subdir);
		global->subdirFileCount = 0;
	};

	global->subdirFileCount += 1;
	strcpy(event->eventSubdir, global->subdirName);
	
	pthread_mutex_unlock(&global->subdir_mutex);

}




/*
 *	Write out processed data to our 'standard' HDF5 format
 */
void writeHDF5(cEventData *info, cGlobal *global){
	/*
	 *	Create filename based on date, time and fiducial for this image
	 *	and put it in the current working sub-directory
	 */
	char outfile[1024];
	makeSubdir(info, global);
	sprintf(outfile, "%s/%s", global->subdirName, info->eventname);

	
	/*
	 *	Update text file log
	 */
	pthread_mutex_lock(&global->framefp_mutex);
	fprintf(global->cleanedfp, "r%04u/%s/%s, %li, %i, %g, %g, %g, %g, %g\n",global->runNumber, info->eventSubdir, info->eventname, info->frameNumber, info->nPeaks, info->peakNpix, info->peakTotal, info->peakResolution, info->peakResolutionA, info->peakDensity);
	pthread_mutex_unlock(&global->framefp_mutex);
	
	
	/* 
 	 *  HDF5 variables
	 */
	hid_t		hdf_fileID;
	hid_t		dataspace_id;
	hid_t		dataset_id;
	hid_t		datatype;
	hsize_t 	size[2],max_size[2];
	herr_t		hdf_error;
	hid_t   	gid, gidHitfinder;
	//char 		fieldname[100]; 
	char        fieldID[1023];

	
	/*
	 *	Create the HDF5 file
	 */
	hdf_fileID = H5Fcreate(outfile,  H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
	
	
	
	
	/*
	 *	Save image data into '/data' part of HDF5 file
	 */
	gid = H5Gcreate(hdf_fileID, "data", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if ( gid < 0 ) {
		ERROR("%li: Couldn't create group\n", info->threadNum);
		H5Fclose(hdf_fileID);
		return;
	}
	
	// Assembled image
	if(global->saveAssembled) {
		char fieldID[1023];
		DETECTOR_LOOP {
			size[0] = global->detector[detID].image_nx;	// size[0] = height
			size[1] = global->detector[detID].image_nx;	// size[1] = width
			max_size[0] = global->detector[detID].image_nx;
			max_size[1] = global->detector[detID].image_nx;
			dataspace_id = H5Screate_simple(2, size, max_size);
			sprintf(fieldID, "assembleddata%li", detID);
			dataset_id = H5Dcreate(gid, fieldID, H5T_STD_I16LE, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
			if ( dataset_id < 0 ) {
				ERROR("%li: Couldn't create dataset\n", info->threadNum);
				H5Fclose(hdf_fileID);
				return;
			}
			hdf_error = H5Dwrite(dataset_id, H5T_STD_I16LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, info->detector[detID].image);
			if ( hdf_error < 0 ) {
				ERROR("%li: Couldn't write data\n", info->threadNum);
				H5Dclose(dataspace_id);
				H5Fclose(hdf_fileID);
				return;
			}
			H5Dclose(dataset_id);
			H5Sclose(dataspace_id);
		}	
	}
	// Save raw data
	if(global->saveRaw) {
		char fieldID[1023];
		DETECTOR_LOOP {
			size[0] = global->detector[detID].pix_ny;	// size[0] = height
			size[1] = global->detector[detID].pix_nx;	// size[1] = width
			max_size[0] = global->detector[detID].pix_ny;
			max_size[1] = global->detector[detID].pix_nx;
			dataspace_id = H5Screate_simple(2, size, max_size);
			sprintf(fieldID, "rawdata%li", detID);
			dataset_id = H5Dcreate(gid, fieldID, H5T_STD_I16LE, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
			if ( dataset_id < 0 ) {
				ERROR("%li: Couldn't create dataset\n", info->threadNum);
				H5Fclose(hdf_fileID);
				return;
			}
			hdf_error = H5Dwrite(dataset_id, H5T_STD_I16LE, H5S_ALL, H5S_ALL, H5P_DEFAULT, info->detector[detID].corrected_data_int16);
			if ( hdf_error < 0 ) {
				ERROR("%li: Couldn't write data\n", info->threadNum);
				H5Dclose(dataspace_id);
				H5Fclose(hdf_fileID);
				return;
			}
			H5Dclose(dataset_id);
			H5Sclose(dataspace_id);
		}
	}
		
	// Create symbolic link from /data/data to whatever is deemed the 'main' data set 
	if(global->saveAssembled) {
		hdf_error = H5Lcreate_soft( "/data/assembleddata0", hdf_fileID, "/data/data",0,0);
		hdf_error = H5Lcreate_soft( "/data/assembleddata0", hdf_fileID, "/data/assembleddata",0,0);
		hdf_error = H5Lcreate_soft( "/data/rawdata0", hdf_fileID, "/data/rawdata",0,0);
	}
	else {
		hdf_error = H5Lcreate_soft( "/data/rawdata0", hdf_fileID, "/data/data",0,0);
		hdf_error = H5Lcreate_soft( "/data/rawdata0", hdf_fileID, "/data/rawdata",0,0);
	}
	
	
	/*
	 *	Save radial average (always, it's not much space)
	 */
    DETECTOR_LOOP {
        size[0] = global->detector[detID].radial_nn;
        dataspace_id = H5Screate_simple(1, size, NULL);
        
        sprintf(fieldID, "radialAverage%li", detID);
        dataset_id = H5Dcreate(gid, fieldID, H5T_NATIVE_FLOAT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        H5Dwrite(dataset_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, info->detector[detID].radialAverage);
        H5Dclose(dataset_id);
        
        sprintf(fieldID, "radialAverageCounter%li", detID);
        dataset_id = H5Dcreate(gid, fieldID, H5T_NATIVE_FLOAT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        H5Dwrite(dataset_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, info->detector[detID].radialAverageCounter);
        H5Dclose(dataset_id);
        
        H5Sclose(dataspace_id);
    }
	
    

	
	/*
	 *	Save TOF data (Aqiris)
	 */
	if(info->TOFPresent==1) {
		size[0] = 2;	
		size[1] = global->AcqNumSamples;	
		max_size[0] = 2;
		max_size[1] = global->AcqNumSamples;
		
		double tempData[2][global->AcqNumSamples];
		memcpy(&tempData[0][0], info->TOFTime, global->AcqNumSamples);
		memcpy(&tempData[1][0], info->TOFVoltage, global->AcqNumSamples);
		
		dataspace_id = H5Screate_simple(2, size, max_size);
		dataset_id = H5Dcreate(gid, "tof", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
		if ( dataset_id < 0 ) {
			
		}
		hdf_error = H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, tempData);
		if ( hdf_error < 0 ) {
			
		}
		H5Dclose(dataset_id);
		H5Sclose(dataspace_id);
	}	
	
	/*
	 *	Save microscope images (Pulnix CCD) as data/pulnixCCD
	 */
	if(info->pulnixFail == 0) {
		size[0] = info->pulnixHeight;	
		size[1] = info->pulnixWidth;	

		dataspace_id = H5Screate_simple(2, size, size);
		dataset_id = H5Dcreate(gid, "pulnixCCD", H5T_NATIVE_USHORT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
		H5Dwrite(dataset_id, H5T_NATIVE_USHORT, H5S_ALL, H5S_ALL, H5P_DEFAULT, info->pulnixImage);
		H5Dclose(dataset_id);
		H5Sclose(dataspace_id);
	}
    
    /*
	 *	Save energy spectrum images (Opal2k CCD) as data/energySpectrumCCD
	 */
	if(info->specFail == 0) {
		size[0] = info->specHeight;
		size[1] = info->specWidth;
        
		dataspace_id = H5Screate_simple(2, size, size);
		dataset_id = H5Dcreate(gid, "energySpectrumCCD", H5T_NATIVE_USHORT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
		H5Dwrite(dataset_id, H5T_NATIVE_USHORT, H5S_ALL, H5S_ALL, H5P_DEFAULT, info->specImage);
		H5Dclose(dataset_id);
		H5Sclose(dataspace_id);
	}
	
    /*
     *  Save energy spectrum data to HDF file as data/energySpectrum1D
     */
    if(info->energySpectrumExist ==1) {
        size[0] = global->espectrumLength;
        
        dataspace_id = H5Screate_simple(1, size, NULL);
        dataset_id = H5Dcreate(gid, "energySpectrum1D", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, info->energySpectrum1D);
        H5Dclose(dataset_id);
        H5Sclose(dataspace_id);
    }
	
	
	// Done with the /data group
	H5Gclose(gid);
	
	
	
	/*
	 * save processing info
	 */
	
	// Create sub-groups
	gid = H5Gcreate(hdf_fileID, "processing", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if ( gid < 0 ) {
		ERROR("%li: Couldn't create group\n", info->threadNum);
		H5Fclose(hdf_fileID);
		return;
	}
	gidHitfinder = H5Gcreate(gid, "hitfinder", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if ( gid < 0 ) {
		ERROR("%li: Couldn't create group\n", info->threadNum);
		H5Fclose(hdf_fileID);
		return;
	}
	
	if(global->savePeakInfo && global->hitfinder && info->nPeaks > 0 ) {
		size[0] = info->nPeaks;			// size[0] = height
		size[1] = 4;					// size[1] = width
		max_size[0] = info->nPeaks;
		max_size[1] = 4;
		double *peak_info = (double *) calloc(4*info->nPeaks, sizeof(double));
		
		// Save peak info in Assembled layout
		for (int i=0; i<info->nPeaks;i++){
			peak_info[i*4] = info->peak_com_x_assembled[i];
			peak_info[i*4+1] = info->peak_com_y_assembled[i];
			peak_info[i*4+2] = info->peak_intensity[i];
			peak_info[i*4+3] = info->peak_npix[i];
		}
		
		dataspace_id = H5Screate_simple(2, size, max_size);
		dataset_id = H5Dcreate(gidHitfinder, "peakinfo-assembled", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
		if ( dataset_id < 0 ) {
			ERROR("%li: Couldn't create dataset\n", info->threadNum);
			H5Fclose(hdf_fileID);
			return;
		}
		hdf_error = H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, peak_info);
		if ( hdf_error < 0 ) {
			ERROR("%li: Couldn't write data\n", info->threadNum);
			H5Dclose(dataspace_id);
			H5Fclose(hdf_fileID);
			return;
		}
		H5Dclose(dataset_id);
		H5Sclose(dataspace_id);
		
		
		// Save peak info in Raw layout
		for (int i=0; i<info->nPeaks;i++){
			peak_info[i*4] = info->peak_com_x[i];
			peak_info[i*4+1] = info->peak_com_y[i];
			peak_info[i*4+2] = info->peak_intensity[i];
			peak_info[i*4+3] = info->peak_npix[i];
		}
		
		dataspace_id = H5Screate_simple(2, size, max_size);
		dataset_id = H5Dcreate(gidHitfinder, "peakinfo-raw", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
		if ( dataset_id < 0 ) {
			ERROR("%li: Couldn't create dataset\n", info->threadNum);
			H5Fclose(hdf_fileID);
			return;
		}
		hdf_error = H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, peak_info);
		if ( hdf_error < 0 ) {
			ERROR("%li: Couldn't write data\n", info->threadNum);
			H5Dclose(dataspace_id);
			H5Fclose(hdf_fileID);
			return;
		}
		H5Dclose(dataset_id);
		H5Sclose(dataspace_id);
		
		
		// Create symbolic link from /processing/hitfinder/peakinfo to whatever is deemed the 'main' data set 
		if(global->saveAssembled) {
			hdf_error = H5Lcreate_soft( "/processing/hitfinder/peakinfo-assembled", hdf_fileID, "/processing/hitfinder/peakinfo",0,0);
		}
		else {
			hdf_error = H5Lcreate_soft( "/processing/hitfinder/peakinfo-raw", hdf_fileID, "/processing/hitfinder/peakinfo",0,0);
		}
		
		
		
		/*
		 * Save pixelmaps
		 * Here's the plan so far:
		 *    Pixelmaps are saved as an 8-bit unsigned int array in the hdf5 file
		 *    All bits set to 0 by default.  A pixel is flagged by setting the bit to 1.
		 *    Bit 0: if equal to 1, this is a "bad pixel".
		 *    Bit 1: if equal to 1, this is a "hot pixel".
		 *    Bit 2: if equal to 1, this is a "saturated pixel".
		 *    Bit 3: unused
		 *    Bit 4: unused
		 *    Bit 5: unused
		 *    Bit 6: unused
		 *    Bit 7: if equal to 1, this pixel is bad for miscellaneous reasions (e.g. ice rings).  
		 */	
		
		long i;
		char * pixelmasks = (char *) calloc(global->detector[0].pix_nn,sizeof(char));
		for (i=0; i<global->detector[0].pix_nn; i++) {
			pixelmasks[i] = 0; // default: all bits are equal to 1
			if ( global->detector[0].badpixelmask[i] == 0 )
				pixelmasks[i] |= (1 << 0);
			if ( global->detector[0].hotpixelmask[i] == 0 ) // Should use a mutex lock here...
				pixelmasks[i] |= (1 << 1);
			if ( info->detector[0].saturatedPixelMask[i] == 0 )
				pixelmasks[i] |= (1 << 2);		
		}
		
		size[0] = global->detector[0].pix_ny;	// size[0] = height
		size[1] = global->detector[0].pix_nx;	// size[1] = width
		max_size[0] = global->detector[0].pix_ny;
		max_size[1] = global->detector[0].pix_nx;
		dataspace_id = H5Screate_simple(2, size, max_size);
		dataset_id = H5Dcreate(gid, "pixelmasks", H5T_NATIVE_CHAR, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
		if ( dataset_id < 0 ) {
			ERROR("%li: Couldn't create dataset\n", info->threadNum);
			H5Fclose(hdf_fileID);
			return;
		}
		hdf_error = H5Dwrite(dataset_id, H5T_NATIVE_CHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, pixelmasks);
		if ( hdf_error < 0 ) {
			ERROR("%li: Couldn't write data\n", info->threadNum);
			H5Dclose(dataspace_id);
			H5Fclose(hdf_fileID);
			return;
		}
		H5Dclose(dataset_id);
		H5Sclose(dataspace_id);
		
		
		
		free(pixelmasks);
		free(peak_info);
	}
	
    /*
     *  Save energy spectrum tilt angle value to HDF file as processing/energySpectrum-tilt
     */
    if(info->energySpectrumExist) {
        size[0] = 1;			// size[0] = length
        dataspace_id = H5Screate_simple(1, size, NULL);
        dataset_id = H5Dcreate(gid, "energySpectrum-tilt", H5T_NATIVE_INT, dataspace_id, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
        if ( dataset_id < 0 ) {
            ERROR("%li: Couldn't create dataset\n", info->threadNum);
            H5Fclose(hdf_fileID);
            return;
        }
        hdf_error = H5Dwrite(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &global->espectiltang);
        if ( hdf_error < 0 ) {
            ERROR("%li: Couldn't write data\n", info->threadNum);
            H5Dclose(dataspace_id);
            H5Fclose(hdf_fileID);
            return;
        }
        H5Dclose(dataset_id);
        H5Sclose(dataspace_id);
	}
    
    // Done with /processing group
	H5Gclose(gid);
	H5Gclose(gidHitfinder);
	
	/*
	 *	Write LCLS event information
	 */
	gid = H5Gcreate1(hdf_fileID,"LCLS",0);
	size[0] = 1;
	dataspace_id = H5Screate_simple( 1, size, NULL );
	//dataspace_id = H5Screate(H5S_SCALAR);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/machineTime", H5T_NATIVE_INT32, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_UINT32, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->seconds );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/fiducial", H5T_NATIVE_INT32, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_INT32, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->fiducial );
	H5Dclose(dataset_id);
	
	// Electron beam data
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/ebeamCharge", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->fEbeamCharge );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/ebeamL3Energy", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->fEbeamL3Energy );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/ebeamPkCurrBC2", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->fEbeamPkCurrBC2 );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/ebeamLTUPosX", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->fEbeamLTUPosX );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/ebeamLTUPosY", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->fEbeamLTUPosY );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/ebeamLTUAngX", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->fEbeamLTUAngX );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/ebeamLTUAngY", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->fEbeamLTUAngY );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/phaseCavityTime1", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->phaseCavityTime1 );
	H5Dclose(dataset_id);
	
	// Phase cavity information
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/phaseCavityTime2", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->phaseCavityTime2 );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/phaseCavityCharge1", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->phaseCavityCharge1 );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/phaseCavityCharge2", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->phaseCavityCharge2 );
	H5Dclose(dataset_id);
	
	// Calculated photon energy
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/photon_energy_eV", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->photonEnergyeV);
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/photon_wavelength_A", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->wavelengthA);
	H5Dclose(dataset_id);
	
	
	// Gas detector values
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/f_11_ENRC", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->gmd11 );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/f_12_ENRC", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->gmd12 );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/f_21_ENRC", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->gmd21 );
	H5Dclose(dataset_id);
	
	dataset_id = H5Dcreate1(hdf_fileID, "/LCLS/f_22_ENRC", H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->gmd22 );	
	H5Dclose(dataset_id);
	
	
	// LaserOn event code
	int LaserOnVal = (info->laserEventCodeOn)?1:0;
	//printf("LaserOnVal %d \n", LaserOnVal);
	dataset_id = H5Dcreate1(hdf_fileID, "LCLS/evr41", H5T_NATIVE_INT, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, H5T_NATIVE_INT32, H5S_ALL, H5S_ALL, H5P_DEFAULT, &LaserOnVal);
	H5Dclose(dataset_id);


	// Misc EPICS PVs
	for (int i=0; i < global->nEpicsPvFloatValues; i++ ) {	
		sprintf(fieldID, "/LCLS/%s", &global->epicsPvFloatAddresses[i][0]);
		dataset_id = H5Dcreate1(hdf_fileID, fieldID, H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
		H5Dwrite(dataset_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->epicsPvFloatValues[i] );	
		H5Dclose(dataset_id);
	}

    // Detector motor positions
    DETECTOR_LOOP {
        sprintf(fieldID, "/LCLS/detector%li-Position", detID);
        dataset_id = H5Dcreate1(hdf_fileID, fieldID, H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
        H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &global->detector[detID].detectorZ );	
        H5Dclose(dataset_id);
        
        sprintf(fieldID, "/LCLS/detector%li-EncoderValue", detID);
        dataset_id = H5Dcreate1(hdf_fileID, fieldID, H5T_NATIVE_DOUBLE, dataspace_id, H5P_DEFAULT);
        H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &global->detector[detID].detectorEncoderValue);	

        //H5Dwrite(dataset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, &global->detector[detID].detectorEncoderValue );	
        H5Dclose(dataset_id);
    }    

	
	// Finished with scalar dataset ID
	H5Sclose(dataspace_id);
	
	
	// cspad temperature
	//size[0] = 4;
	//dataspace_id = H5Screate_simple(1, size, NULL);
	//dataset_id = H5Dcreate1(hdf_fileID, "LCLS/cspadQuadTemperature", H5T_NATIVE_FLOAT, dataspace_id, H5P_DEFAULT);
	//H5Dwrite(dataset_id, H5T_NATIVE_FLOAT, H5S_ALL, H5S_ALL, H5P_DEFAULT, &info->detector[0].quad_temperature[0]);
	//H5Dclose(dataset_id);
	//H5Sclose(dataspace_id);
	
	
	
	// Time in human readable format
	// Writing strings in HDF5 is a little tricky --> this could be improved!
	char ctime_buffer[1024];
	char* timestr;
	time_t eventTime = info->seconds;
	timestr = ctime_r(&eventTime, ctime_buffer);
	dataspace_id = H5Screate(H5S_SCALAR);
	datatype = H5Tcopy(H5T_C_S1);  
	H5Tset_size(datatype,strlen(timestr)+1);
	dataset_id = H5Dcreate1(hdf_fileID, "LCLS/eventTimeString", datatype, dataspace_id, H5P_DEFAULT);
	H5Dwrite(dataset_id, datatype, H5S_ALL, H5S_ALL, H5P_DEFAULT, timestr );
	H5Dclose(dataset_id);
	H5Sclose(dataspace_id);
	hdf_error = H5Lcreate_soft( "/LCLS/eventTimeString", hdf_fileID, "/LCLS/eventTime",0,0);
	
	
	
	// Close group and flush buffers
	H5Gclose(gid);
	H5Fflush(hdf_fileID,H5F_SCOPE_LOCAL);
	
	
	/*
	 *	Clean up stale HDF5 links
	 *		(thanks Tom/Filipe)
	 */
	int n_ids;
	hid_t ids[256];
	n_ids = H5Fget_obj_ids(hdf_fileID, H5F_OBJ_ALL, 256, ids);
	for ( int i=0; i<n_ids; i++ ) {
		hid_t id;
		H5I_type_t type;
		id = ids[i];
		type = H5Iget_type(id);
		if ( type == H5I_GROUP ) H5Gclose(id);
		if ( type == H5I_DATASET ) H5Dclose(id);
		if ( type == H5I_DATATYPE ) H5Tclose(id);
		if ( type == H5I_DATASPACE ) H5Sclose(id);
		if ( type == H5I_ATTR ) H5Aclose(id);
	}
	
	H5Fclose(hdf_fileID); 
}


void writePeakFile(cEventData *eventData, cGlobal *global){
	
	// No peaks --> go home
	if(eventData->nPeaks <= 0) {
		return;
	}
	
	// Dump peak info to file
	pthread_mutex_lock(&global->peaksfp_mutex);
	fprintf(global->peaksfp, "%s\n", eventData->eventname);
	fprintf(global->peaksfp, "photonEnergy_eV=%f\n", eventData->photonEnergyeV);
	fprintf(global->peaksfp, "wavelength_A=%f\n", eventData->wavelengthA);
	fprintf(global->peaksfp, "pulseEnergy_mJ=%f\n", (float)(eventData->gmd21+eventData->gmd21)/2);
	fprintf(global->peaksfp, "npeaks=%i\n", eventData->nPeaks);
	fprintf(global->peaksfp, "peakResolution=%g\n", eventData->peakResolution);
	fprintf(global->peaksfp, "peakDensity=%g\n", eventData->peakDensity);
	fprintf(global->peaksfp, "peakNpix=%g\n", eventData->peakNpix);
	fprintf(global->peaksfp, "peakTotal=%g\n", eventData->peakTotal);
	
	for(long i=0; i<eventData->nPeaks; i++) {
		fprintf(global->peaksfp, "%f, %f, %f, %f, %g, %g\n", eventData->peak_com_x_assembled[i], eventData->peak_com_y_assembled[i], eventData->peak_com_x[i], eventData->peak_com_y[i], eventData->peak_npix[i], eventData->peak_intensity[i]);
	}
	pthread_mutex_unlock(&global->peaksfp_mutex);
	
	
}



/*
 *	Write test data to a simple HDF5 file
 */
void writeSimpleHDF5(const char *filename, const void *data, int width, int height, int type) 
{
	hid_t fh, gh, sh, dh;	/* File, group, dataspace and data handles */
	herr_t r;
	hsize_t size[2];
	hsize_t max_size[2];
	
	fh = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
	if ( fh < 0 ) {
		ERROR("Couldn't create file: %s\n", filename);
	}
	
	gh = H5Gcreate(fh, "data", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if ( gh < 0 ) {
		ERROR("Couldn't create group\n");
		H5Fclose(fh);
	}
	
	size[0] = height;
	size[1] = width;
	max_size[0] = height;
	max_size[1] = width;
	sh = H5Screate_simple(2, size, max_size);
	
	dh = H5Dcreate(gh, "data", type, sh,
	               H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if ( dh < 0 ) {
		ERROR("Couldn't create dataset\n");
		H5Fclose(fh);
	}
	
	/* Muppet check */
	H5Sget_simple_extent_dims(sh, size, max_size);
	
	r = H5Dwrite(dh, type, H5S_ALL,
	             H5S_ALL, H5P_DEFAULT, data);
	if ( r < 0 ) {
		ERROR("Couldn't write data\n");
		H5Dclose(dh);
		H5Fclose(fh);
	}
	
	H5Gclose(gh);
	H5Dclose(dh);
	
	
	/*
	 *	Clean up stale HDF5 links
	 *		(thanks Tom/Filipe)
	 */
	int n_ids;
	hid_t ids[256];
	n_ids = H5Fget_obj_ids(fh, H5F_OBJ_ALL, 256, ids);
	for ( int i=0; i<n_ids; i++ ) {
		hid_t id;
		H5I_type_t type;
		id = ids[i];
		type = H5Iget_type(id);
		if ( type == H5I_GROUP ) H5Gclose(id);
		if ( type == H5I_DATASET ) H5Dclose(id);
		if ( type == H5I_DATATYPE ) H5Tclose(id);
		if ( type == H5I_DATASPACE ) H5Sclose(id);
		if ( type == H5I_ATTR ) H5Aclose(id);
	}
	
	
	H5Fclose(fh);
}

void writeSpectrumInfoHDF5(const char *filename, const void *data1, int length1, int type1, const void *data2, int length2, int type2) {
	
    hid_t fh, gh, sh, dh;	/* File, group, dataspace and data handles */
	herr_t r;
	hsize_t size[1];
	hsize_t max_size[1];
	
	fh = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
	if ( fh < 0 ) {
		ERROR("Couldn't create file: %s\n", filename);
	}
	
	// create group energySpectrum
    gh = H5Gcreate(fh, "energySpectrum", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if ( gh < 0 ) {
		ERROR("Couldn't create group\n");
		H5Fclose(fh);
	}
	
	size[0] = length1;
	sh = H5Screate_simple(1, size, NULL);
	
	// save run integrated energy spectrum in HDF5 as energySpectrum/runIntegratedEnergySpectrum
    dh = H5Dcreate(gh, "runIntegratedEnergySpectrum", type1, sh,
	               H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if ( dh < 0 ) {
		ERROR("Couldn't create dataset\n");
		H5Fclose(fh);
	}
	
    /* Muppet check */
	H5Sget_simple_extent_dims(sh, size, max_size);
	
	r = H5Dwrite(dh, type1, H5S_ALL, H5S_ALL, H5P_DEFAULT, data1);
	if ( r < 0 ) {
		ERROR("Couldn't write data\n");
		H5Dclose(dh);
		H5Fclose(fh);
	}
    
    H5Dclose(dh);
    H5Sclose(sh);
    
    // save index of max integrated value in HDF5 as energySpectrum/runIntegratedEnergySpectrum_maxindex
    size[0] = length2;
	sh = H5Screate_simple(1, size, NULL);
    dh = H5Dcreate(gh, "runIntegratedEnergySpectrum_maxindex", type2, sh,
	               H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
	if ( dh < 0 ) {
		ERROR("Couldn't create dataset\n");
		H5Fclose(fh);
	}
    r = H5Dwrite(dh, type2, H5S_ALL, H5S_ALL, H5P_DEFAULT, data2);
	if ( r < 0 ) {
		ERROR("Couldn't write data\n");
		H5Dclose(dh);
		H5Fclose(fh);
	}
    
	//H5Gclose(gh);
	H5Dclose(dh);
    H5Sclose(sh);
    H5Gclose(gh);
    
	/*
	 *	Clean up stale HDF5 links
	 *		(thanks Tom/Filipe)
	 */
	int n_ids;
	hid_t ids[256];
	n_ids = H5Fget_obj_ids(fh, H5F_OBJ_ALL, 256, ids);
	for ( int i=0; i<n_ids; i++ ) {
		hid_t id;
		H5I_type_t type;
		id = ids[i];
		type = H5Iget_type(id);
		if ( type == H5I_GROUP ) H5Gclose(id);
		if ( type == H5I_DATASET ) H5Dclose(id);
		if ( type == H5I_DATATYPE ) H5Tclose(id);
		if ( type == H5I_DATASPACE ) H5Sclose(id);
		if ( type == H5I_ATTR ) H5Aclose(id);
	}
	
	
	H5Fclose(fh);
}

