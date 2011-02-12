/*
 *  setup.cpp
 *  cspad_cryst
 *
 *  Created by Anton Barty on 7/2/11.
 *  Copyright 2011 CFEL. All rights reserved.
 *
 */



/*
 *	Global variables
 */
class cGlobal {
	
public:
	// Configuration files
	char		configFile[1024];
	char		geometryFile[1024];
	
	// Run information
	unsigned	runNumber;
	
	
	// Thread management
	long			nThreads;
	long			nActiveThreads;
	long			threadCounter;
	pthread_t		*threadID;
	pthread_mutex_t	nActiveThreads_mutex;
	
	
	// Detector geometry
	long			pix_nx;
	long			pix_ny;
	long			pix_nn;
	float			*pix_x;
	float			*pix_y;
	float			*pix_z;
	float			pix_dx;
	unsigned		module_rows;
	unsigned		module_cols;
	long			image_nx;
	long			image_nn;
	
public:
	void defaultConfiguration(void);
	void parseConfigFile(char *);
	void parseCommandLineArguments(int, char**);
	void setupThreads(void);
	void readDetectorGeometry(char *);

private:
	void parseConfigTag(char*, char*);

	
};
