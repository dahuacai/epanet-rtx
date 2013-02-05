//
//  validator.cpp
//  rtx-validator
//
//  Created by the EPANET-RTX Development Team
//  See README.md and license.txt for more information
//  

#include <iostream>
#include <time.h>
#include <boost/foreach.hpp>

#include "ConfigFactory.h"

using namespace std;
using namespace RTX;

void runSimulationUsingConfig(const string& path, time_t someTime, long duration);
void runSimulationUsingConfig2(const string& path, time_t someTime, long duration);

int main (int argc, const char * argv[])
{
  string forwardSimulationConfig(""), realtimeConfig(""), waterqualityConfig("");
  if (argc > 1) {
    forwardSimulationConfig = string( argv[1] );
  }
  if (argc > 2) {
    realtimeConfig = string( argv[2] );
  }
  if (argc > 3) {
    waterqualityConfig = string( argv[3] );
  }
  
  time_t someTime = 1222873200; // unix-time 2008-10-01 15:00:00 GMT
  long duration = 60 * 60 * 24; // 1 day
  
  // test the forward synthetic simulation
  //runSimulationUsingConfig(forwardSimulationConfig, someTime-3600, 3600);
  runSimulationUsingConfig2(forwardSimulationConfig, someTime-3600, 3600);

  // test the real-time methods
  //runSimulationUsingConfig(realtimeConfig, someTime, duration);

  // test for the LT model simulation
  //runSimulationUsingConfig2(forwardSimulationConfig, someTime-3600, duration+7200);
  
  return 0;
}




void runSimulationUsingConfig(const string& filePath, time_t someTime, long duration) {
  
  ConfigFactory config;
  Model::sharedPointer model;
  
  try {
    config.loadConfigFile(filePath);
    
    model = config.model();
    
    cout << "RTX: Running simulation for..." << endl;
    cout << *model;
    
    PointRecord::sharedPointer record = config.defaultRecord();
    
    // erase all points in the db
    record->reset();
  
    model->runExtendedPeriod(someTime, someTime + duration);
    
    cout << "... done" << endl;
    
  } catch (string err) {
    cerr << err << endl;
  }
}

void runSimulationUsingConfig2(const string& filePath, time_t someTime, long duration) {
  
  ConfigFactory config;
  Model::sharedPointer model;
  
  try {
    config.loadConfigFile(filePath);
    
    model = config.model();
    
    cout << "RTX: Running simulation for..." << endl;
    cout << *model;
    
    PointRecord::sharedPointer record = config.defaultRecord();
    
    // erase all points in the db
    record->reset();
    
    model->runExtendedPeriodLT(someTime, someTime + duration);
  
    cout << "... done" << endl;
    
  } catch (string err) {
    cerr << err << endl;
  }
}


