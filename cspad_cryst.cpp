/* $Id: myana_cspad.cc,v 1.6 2010/10/21 22:09:43 weaver Exp $ */

#include "myana/myana.hh"
#include "myana/main.hh"
#include "myana/XtcRun.hh"
#include "release/pdsdata/cspad/ConfigV1.hh"
#include "release/pdsdata/cspad/ConfigV2.hh"
#include "release/pdsdata/cspad/ElementHeader.hh"
#include "release/pdsdata/cspad/ElementIterator.hh"
#include "cspad-gjw/CspadTemp.hh"
#include "cspad-gjw/CspadCorrector.hh"
#include "cspad-gjw/CspadGeometry.hh"

#include <stdio.h>
#include <string.h>
#include <hdf5.h>
#include <pthread.h>

#include "worker.h"
#include "setup.h"


static tGlobal		global;


// Quad class definition
class MyQuad {
	public:
	MyQuad(unsigned q) : _quad(q) {
		char buff[64];
		for(unsigned i=0; i<16; i++) {
	  		sprintf(buff,"Q%d_ASIC%d_values",q,i);
	  		sprintf(buff,"Q%d_ASIC%d_map",q,i);
		}
	}
	
  	void Fill(Pds::CsPad::ElementIterator& iter) {
    	unsigned section_id;
    	const Pds::CsPad::Section* s;
    	while((s=iter.next(section_id))) {
      		for(unsigned col=0; col<COLS; col++)
				for(unsigned row=0; row<ROWS; row++) {	
				}
    	}
  	}    
  
  	void write() {
    	for(unsigned i=0; i<16; i++) {
    	}
  	}

	private:
  		unsigned _quad;
  		CspadSection _s;
};

static MyQuad* quads[4];    
using namespace Pds;



/*
 *	Beam on or off??
 */
static bool beamOn()
{
	int nfifo = getEvrDataNumber();
	for(int i=0; i<nfifo; i++) {
		unsigned eventCode, fiducial, timestamp;
		if (getEvrData(i,eventCode,fiducial,timestamp)) 
			printf("Failed to fetch evr fifo data\n");
		else if (eventCode==140)
			return true;
	}
	return false;;
}


// This function is called once at the beginning of the analysis job,
// You can ask for detector "configuration" information here.
void beginjob() {
	printf("beginjob()\n");
	
	/*
	 * Get time information
	 */
	int fail = 0;
	int seconds, nanoSeconds;
	const char* time;

	getTime( seconds, nanoSeconds );	
	fail = getLocalTime( time );

	
	/*
	 *	New csPad corrector
	 */
	corrector = new CspadCorrector(Pds::DetInfo::XppGon,0,CspadCorrector::DarkFrameOffset);
				 
	for(unsigned i=0; i<4; i++)
		quads[i] = new MyQuad(i);
	
	
	/*
	 *	Stuff for worker thread management
	 */
	globalConfiguration(&global);
	setupThreads(&global);
	readDetectorGeometry(&global);
	
}


void fetchConfig()
{
	if (getCspadConfig( Pds::DetInfo::XppGon, configV1 )==0) {
		configVsn= 1;
		quadMask = configV1.quadMask();
		asicMask = configV1.asicMask();
		printf("CSPAD configuration: quadMask %x  asicMask %x  runDelay %d\n", quadMask,asicMask,configV1.runDelay());
		printf("\tintTime %d/%d/%d/%d\n", configV1.quads()[0].intTime(), configV1.quads()[1].intTime(), configV1.quads()[2].intTime(), configV1.quads()[3].intTime());
	}
	else if (getCspadConfig( Pds::DetInfo::XppGon, configV2 )==0) {
		configVsn= 2;
		quadMask = configV2.quadMask();
		asicMask = configV2.asicMask();
		printf("CSPAD configuration: quadMask %x  asicMask %x  runDelay %d\n", quadMask,asicMask,configV2.runDelay());
		printf("\tintTime %d/%d/%d/%d\n", configV2.quads()[0].intTime(), configV2.quads()[1].intTime(), configV2.quads()[2].intTime(), configV2.quads()[3].intTime());
	}
	else {
		configVsn= 0;
		printf("Failed to get CspadConfig\n");
	}
}


/*
 * 	This function is called once for each run.  You should check to see
 *  	if detector configuration information has changed.
 */
void beginrun() 
{
	printf("beginrun\n");
	
	fetchConfig();
	corrector->loadConstants(getRunNumber());
}

/*
 *	Calibration
 */
void begincalib()
{
	fetchConfig();
	printf("begincalib\n");
}



/*
 *	This function is called once per shot
 */
void event() {
	
	
	printf("New event\n");
	FILE* fp;
	char filename[1024];


	/*
	 * Get time information
	 */
	static int ievent = 0;
	int fail = 0;
	int seconds, nanoSeconds;
	getTime( seconds, nanoSeconds );

	const char* time;
	fail = getLocalTime( time );

	
	/*
	 *	Get fiducials
	 */
	unsigned fiducial;
	fail = getFiducials(fiducial);

	
	/* 
	 *	Is the beam on?
	 */
	bool beam = beamOn();
	printf("Beam %s : fiducial %x\n", beam ? "On":"Off", fiducial);
  
	/*
	 *	FEE Gas detector values 
	 */
	double gasdet[4];
	if (getFeeGasDet( gasdet )==0 && ievent<10)
		printf("gasdet %g/%g/%g/%g\n", gasdet[0], gasdet[1], gasdet[2], gasdet[3]);

	/*
	 *	Phase cavity data
	 */
	double ft1, ft2, c1, c2;
	if ( getPhaseCavity(ft1, ft2, c1, c2) == 0 ) 
		printf("phase cav: %+11.8f %+11.8f %+11.8f %+11.8f\n", ft1, ft2, c1, c2);
	else 
		printf("no phase cavity data.\n");

	
	
	/*
	 *	Create a new worker threadInfo structure in which to place all information
	 */
	tThreadInfo	*threadInfo;
	threadInfo = (tThreadInfo*) malloc(sizeof(threadInfo));
	//for(int jj=0; jj<4; jj++) threadInfo->quad_data[jj] = NULL;
		

	/*
	 *	Copy raw cspad image data into worker thread structure for processing
	 */
	Pds::CsPad::ElementIterator iter;
	fail=getCspadData(DetInfo::XppGon, iter);

	if (fail) {
		printf("getCspadData fail %d (%x)\n",fail,fiducial);
		threadInfo->cspad_fail = fail;
		return;
	}

	else {
		nevents++;
		const Pds::CsPad::ElementHeader* element;

		// loop over elements (quadrants)
		while(( element=iter.next() )) {  
			
			// Which quadrant is this?
			int quadrant = element->quad();
			
			// Have we jumped to a new fiducial (event??)
			if (fiducial != element->fiducials())
				printf("Fiducial jump: %x/%d:%x\n",fiducial,element->quad(),element->fiducials());
			
			// Get temperature on strong back 
			float	temperature = CspadTemp::instance().getTemp(element->sb_temp(2));
			printf("Temperature on quadrant %i: %3.1fC\n",quadrant, temperature);
			threadInfo->quad_temperature[quadrant] = temperature;
			

			// Allocate data space for this quadrant
			threadInfo->quad_data[quadrant] = (uint16_t*) calloc(ROWS*COLS*16, sizeof(uint16_t));

			
			// Read 2x1 "sections" into data array in DAQ format, i.e., 2x8 array of asics (two bytes / pixel)
			const Pds::CsPad::Section* s;
			unsigned section_id;
			uint16_t data[COLS*ROWS*16];
			while(( s=iter.next(section_id) )) {  
				//memcpy(&threadInfo->quad_data[quadrant][section_id*2*ROWS*COLS],s->pixel[0],2*ROWS*COLS*sizeof(uint16_t));
				memcpy(&data[section_id*2*ROWS*COLS],s->pixel[0],2*ROWS*COLS*sizeof(uint16_t));
			}

			// Copy image data into threadInfo structure
			memcpy(threadInfo->quad_data[quadrant], data, 16*ROWS*COLS*sizeof(uint16_t));
		}
	}
	++ievent;

	/*
	 *	Copy all interesting information into worker thread structure if we got this far.
	 *	(ie: presume that myana itself is NOT thread safe and any event info may get overwritten)
	 */
	threadInfo->seconds = seconds;
	threadInfo->nanoSeconds = nanoSeconds;
	strcpy(threadInfo->timeString, time);
	threadInfo->fiducial = fiducial;
	threadInfo->beamOn = beam;
	threadInfo->gmd11 = gasdet[0];
	threadInfo->gmd12 = gasdet[1];
	threadInfo->gmd21 = gasdet[2];
	threadInfo->gmd22 = gasdet[3];
	threadInfo->ft1 = ft1;
	threadInfo->ft2 = ft1;
	threadInfo->c1 = c1;
	threadInfo->c2 = c2;
	threadInfo->pGlobal = &global;
	threadInfo->nActiveThreads_mutex = &global.nActiveThreads_mutex;

	
	
	
	/*
	 *	Spawn worker thread to process this frame
	 */

	// Avoid fork-bombing the system: wait until we have a spare thread in the thread pool
	while(global.nActiveThreads >= global.nThreads) {
		printf("Waiting: active threads = %i; nthreads allowed = %i\n",global.nThreads, global.nActiveThreads);
		usleep(100000);
	}


	// Create a new worker thread for this data frame
	// Threads are created detached so we don't have to wait for anything to happen before returning
	// (each thread is responsible for cleaning up its own threadInfo structure when done)

	pthread_mutex_lock(&global.nActiveThreads_mutex);
	global.nActiveThreads += 1;
	pthread_mutex_unlock(&global.nActiveThreads_mutex);

	
	pthread_t		thread;
	pthread_attr_t	threadAttribute;
	int				returnStatus;

	pthread_attr_init(&threadAttribute);
	//pthread_attr_setdetachstate(&threadAttribute, PTHREAD_CREATE_JOINABLE);
	pthread_attr_setdetachstate(&threadAttribute, PTHREAD_CREATE_DETACHED);
	
	returnStatus = pthread_create(&thread, &threadAttribute, worker, (void *)threadInfo); 
	pthread_attr_destroy(&threadAttribute);
	threadInfo = NULL;
	//pthread_detach(thread);
	
	
	
	// Pause 
	printf("Pausing to give time to read the output :-)\n");
	usleep(1000000);
	
}
// End of event data processing block




/*
 *	Stuff that happens at the end
 */
void endcalib() {
	printf("endcalib()\n");
}

void endrun() 
{
	printf("User analysis endrun() routine called.\n");
}

void endjob()
{
	printf("User analysis endjob() routine called.\n");


	/*
	 *	Thread management stuff
	 */
	printf("Waiting for threads to terminate\n");

	while(global.nActiveThreads > 0) {
		printf("\tactive threads = %i\n", global.nActiveThreads);
		usleep(100000);
	}
	pthread_mutex_destroy(&global.nActiveThreads_mutex);
	
	printf("Done with my bit!\n");
}
