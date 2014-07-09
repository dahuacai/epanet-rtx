
Firstly ,Thanks very much for Epanet-RTX team ! I  wished  my work  would be helpful to your term .

This package is based on author Jinduan Chen's package:epanet-rtx-jc.
I made five important revision:

1,AggregatorTimeSeries.cpp 
add line 21 to line 27 :

 typedef std::pair<TimeSeries::sharedPointer,double> tsDoublePairType;
  
  BOOST_FOREACH(tsDoublePairType ts, _tsList) {
	
  if (timeSeries == ts.first){
return;
	  
    }

  }
the added cycle is to prevent creating a same timeseries.it is related to  following  "4": zone .cpp ,if we add line 115-120 in zone.cpp but don't add this cycle in AggregatorTimeSeries.cpp,it would create a same timeseries source and  that would result in an error.

2,model.cpp
line 416:  The original statement " tank->level()->point(time).value()" is be changed to "Tank->levelMeasure()->point(time).value()" ,if the tank have Head Measure,the tank's pointer should point to levelMeasure(),if not amend,the tank level value will be negative.

3,tank.cpp
line 42, add a sentence:  _resetLevel.reset(new Clock(0, 0));If there is a hydraulic head measurements, we must initialize reset clock ,it is related to mode.cpp line 415.

4,zone.cpp
line 115-120 ,add a cycle to continue following junctions:
if (directionIsOut) {

followJunction(boost::static_pointer_cast<Junction>( pipe->to() ) ); 
}
	
  else {

followJunction(boost::static_pointer_cast<Junction>( pipe->from() ) );
}
//when we have added The time series Source ,We should continue to follow Junctions to create a complete zone
;if not ,extra zone will be create ,that would result in an error.

5,sampletown_realtime.cfg
modify the file:sampletown_realtime.cfg, :type = "Resampler";//The original version is "Resample".More details are available in the saved configuration file.

Now the program has been successful debuged,the correct results are written to the database  successfully.

In addition, Now my program  is based on epanet-rtx-VS2008-win32 .if you want it ,my email is chqahut@live.cn.

Lastly,I am looking forward to you and the Epanet-RTX team immediate answer.

