//
//  EpanetLTmodel.h
//  epanet-rtx
//
//  Created by Hyoungmin on 1/31/13.
//
//

#ifndef epanet_rtx_EpanetLTmodel_h
#define epanet_rtx_EpanetLTmodel_h

#include "EpanetModel.h"
#include "Model.h"
//#include "rtxMacros.h"
extern "C" {
#include "epanet/src/toolkit.h"
}

namespace RTX {
  
  /*!
   \class EpanetSyntheticModel
   \brief Forward-simluation model object, based on epanet engine
   
   Provides an epanet-based simulation engine and methods to perform a forward simulation without overriding rules and controls.
   
   */
  
  class EpanetLTmodel : public EpanetModel {
    
  public:
 //   RTX_SHARED_POINTER(EpanetLTmodel);

   EpanetLTmodel();
 //   ~EpanetLTmodel();

//    void loadModelFromFile(const std::string& filename) throw(RtxException);
    virtual void ENinitialize();

    virtual void overrideControls() throw(RtxException);
    virtual std::ostream& toStream(std::ostream &stream);
    
  protected:
    virtual void stepSimulationLT(time_t time);
    virtual time_t nextHydraulicStep(time_t time);

  private:
    time_t _startTime;
//    std::string _modelFile;
//    std::tr1::unordered_map<std::string, int> _nodeIndex;
//    std::tr1::unordered_map<std::string, int> _linkIndex;

  };
} // namespace RTX

#endif
