//
//  EpanetLTmodel.cpp
//  epanet-rtx
//
//  Created by Hyoungmin on 1/31/13.
//
//
#include <iostream>

#include "EpanetLTmodel.h"
using namespace RTX;
using namespace std;

EpanetLTmodel::EpanetLTmodel() {
  _startTime = 0;
}

//EpanetLTmodel::EpanetLTmodel() : Model() {
  // nothing to do, right?
  //_modelFile = "";
//}
//EpanetLTmodel::EpanetLTmodel() : EpanetModel() {
  // nothing to do, right?
//  _modelFile = "";
//}
//EpanetLTmodel::~EpanetLTmodel() {
//}

#pragma mark - Loading

void EpanetLTmodel::ENinitialize(){
  //ENcheck(ENopeninitHQ(), "ENopeninitHQ");
}

std::ostream& EpanetLTmodel::toStream(std::ostream &stream) {
  // epanet-specific printing
  stream << "Synthetic model (no exogenous inputs)" << std::endl;
  EpanetModel::toStream(stream);
  return stream;
}


void EpanetLTmodel::overrideControls() throw(RtxException) {
  // make sure we do nothing.
  std::cerr << "ignoring control override" << std::endl;
}



// mostly copied from the epanetmodel class, but altered epanet clock-setting
// so that the simulation evolves with its builtin controlls and patterns.


void EpanetLTmodel::stepSimulationLT(time_t time) {
  long step = (long)(time - currentSimulationTime());
  long timestep;
  double value;
  long tleft;
  //std::cout << "set step to: " << step << std::endl;
  if (step) ENcheck( ENsettimeparam(EN_HYDSTEP, step), "ENsettimeparam(EN_HYDSTEP)" );
  ENcheck(ENopeninitHQ(), "ENopeninitHQ");

  do{
    ENcheck( ENrunnextHQ(&timestep, &tleft), "ENrunnextHQ()" );
    
    ENcheck( ENgetnodevalue(2, EN_HEAD, &value), "ENgetnodevalue" );
    cout << "value :"<<value<<" timestep:"<<timestep<<" tleft:"<<tleft<<" time:"<<time<<endl;
    
  } while (tleft >0);
  
  //ENcheck( ENrunnextHQ(&step, &tleft), "ENrunnextHQ()" );
  long supposedStep = time - currentSimulationTime();
  if (step != supposedStep) {
    // it's an intermediate step
    //cerr << "model returned step: " << step << ", expecting " << supposedStep << endl;
  }
  setCurrentSimulationTime( currentSimulationTime() + step );
}

// adjust duration of epanet toolkit simulation if necessary.
time_t EpanetLTmodel::nextHydraulicStep(time_t time) {
  
  long duration = 0;
  ENcheck(ENgettimeparam(EN_DURATION, &duration), "ENgettimeparam(EN_DURATION)");
  
  if (duration <= (time - _startTime) ) {
    std::cerr << "had to adjust the sim duration to accomodate the requested step" << std::endl;
    ENcheck(ENsettimeparam(EN_DURATION, (duration + hydraulicTimeStep()) ), "ENsettimeparam(EN_DURATION)");
  }
  
  // call base class method
  return EpanetModel::nextHydraulicStep(time);
}