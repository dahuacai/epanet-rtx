//
//  lemontiger.h
//  epanet-rtx
//
//  Created by Hyoungmin on 2/1/13.
//
//

#ifndef epanet_rtx_lemontiger_h
#define epanet_rtx_lemontiger_h

long timestepLT();
/* computes the length of the time step to next hydraulic simulation, but don't
 update tank volumne and tank levels. During a sync HQ simulation,
 nextqual() will update the tank vols */

int  nexthydLT(long *tstep);
/* finds length of next time step but don't save
 results to hydraulics file. ignore reporting functions. */

void updateTanklevels();
//Prior to running hydraulic simulation, update the tank levels.

#endif
