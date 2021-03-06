#ifndef XTCPROOFREADERPKG_XTCPROOFREADER_MOD_H
#define XTCPROOFREADERPKG_XTCPROOFREADER_MOD_H

//--------------------------------------------------------------------------
// File and Version Information:
// 	$Id$
//
// Description:
//	Class xtcProofReader_mod.
//
//------------------------------------------------------------------------

//-----------------
// C/C++ Headers --
//-----------------

//----------------------
// Base Class Headers --
//----------------------
#include "psana/Module.h"

//-------------------------------
// Collaborating Class Headers --
//-------------------------------

//------------------------------------
// Collaborating Class Declarations --
//------------------------------------

//		---------------------
// 		-- Class Interface --
//		---------------------

namespace xtcProofReaderPkg {

/// @addtogroup xtcProofReaderPkg

/**
 *  @ingroup xtcProofReaderPkg
 *
 *  @brief Example module class for psana
 *
 *  This software was developed for the LCLS project.  If you use all or 
 *  part of it, please give an appropriate acknowledgment.
 *
 *  @version \$Id$
 *
 *  @author Chunhong Yoon
 */

class xtcProofReader_mod : public Module {
public:

  // Default constructor
  xtcProofReader_mod (const std::string& name) ;

  // Destructor
  virtual ~xtcProofReader_mod () ;

  /// Method which is called once at the beginning of the job
  virtual void beginJob(Event& evt, Env& env);
  
  /// Method which is called at the beginning of the run
  virtual void beginRun(Event& evt, Env& env);
  
  /// Method which is called at the beginning of the calibration cycle
  virtual void beginCalibCycle(Event& evt, Env& env);
  
  /// Method which is called with event data, this is the only required 
  /// method, all other methods are optional
  virtual void event(Event& evt, Env& env);
  
  /// Method which is called at the end of the calibration cycle
  virtual void endCalibCycle(Event& evt, Env& env);

  /// Method which is called at the end of the run
  virtual void endRun(Event& evt, Env& env);

  /// Method which is called once at the end of the job
  virtual void endJob(Event& evt, Env& env);

protected:

private:

  // Data members, this is for example purposes only
  
  Source m_src;         // Data source set from config file
  unsigned m_maxEvents;
  bool m_filter;
  long m_count;

// Data members
  std::string m_key;
  Source m_srcEvr;
  Source m_srcBeam;
  Source m_srcFee;
  Source m_srcCav;
  Source m_srcAcq;
  Source m_srcCam;

};

} // namespace xtcProofReaderPkg

#endif // XTCPROOFREADERPKG_XTCPROOFREADER_MOD_H
