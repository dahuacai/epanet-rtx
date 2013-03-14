//
//  ScadaPointRecord.h
//  epanet-rtx
//
//  Created by the EPANET-RTX Development Team
//  See README.md and license.txt for more information
//  

#ifndef epanet_rtx_ScadaPointRecord_h
#define epanet_rtx_ScadaPointRecord_h

#define MAX_SCADA_TAG 50

#include "DbPointRecord.h"

#include <deque>

#ifdef WIN32
#include "windows.h"
#endif

#include <sql.h>
#include <sqlext.h>

namespace RTX {
  
  /*! \class ScadaPointRecord
   \brief A persistence class for SCADA databases
   
   Primarily to be used for data acquisition. Polls an ODBC-based SCADA connection for data and creates Points from that data.
   
   */
  
  class  ScadaPointRecord : public DbPointRecord {
  public:
    RTX_SHARED_POINTER(ScadaPointRecord);
    ScadaPointRecord();
    virtual ~ScadaPointRecord();
    
    void setSyntax(const string& table, const string& dateCol, const string& tagCol, const string& valueCol, const string& qualityCol);
    virtual void connect() throw(RtxException);
    virtual bool isConnected();
    virtual std::vector<std::string> identifiers();
    virtual bool isPointAvailable(const string& identifier, time_t time);
    virtual Point point(const string& identifier, time_t time);
    virtual Point pointBefore(const string& identifier, time_t time);
    virtual Point pointAfter(const string& identifier, time_t time);
    virtual void addPoint(const string& identifier, Point point) {};
    virtual void addPoints(const string& identifier, std::vector<Point> points) {};
    virtual void reset() {};
    virtual std::ostream& toStream(std::ostream &stream);
    
  private:
    bool _connectionOk;
    std::vector<Point> selectRange(const string& identifier, time_t startTime, time_t endTime = 0);
    std::vector<Point> pointsWithStatement(const string& identifier, SQLHSTMT statement, time_t startTime, time_t endTime = 0);
    typedef struct {
      SQLCHAR tagName[MAX_SCADA_TAG];
      SQL_TIMESTAMP_STRUCT time;
      double value;
      int quality;
      SQLLEN tagNameInd, timeInd, valueInd, qualityInd;
    } ScadaRecord;
    typedef struct {
      SQL_TIMESTAMP_STRUCT start;
      SQL_TIMESTAMP_STRUCT end;
      char tagName[MAX_SCADA_TAG];
      SQLLEN startInd, endInd, tagNameInd;
    } ScadaQuery;
    
    SQLHENV _SCADAenv;
    SQLHDBC _SCADAdbc;
    SQLHSTMT _SCADAstmt, _rangeStatement, _lowerBoundStatement, _upperBoundStatement, _SCADAtimestmt;
    std::string _dsn;
    ScadaRecord _tempRecord;
    ScadaQuery _query;
    
    void bindOutputColumns(SQLHSTMT statement, ScadaRecord* record);
    SQL_TIMESTAMP_STRUCT sqlTime(time_t unixTime);
    time_t unixTime(SQL_TIMESTAMP_STRUCT sqlTime);
    SQLRETURN SQL_CHECK(SQLRETURN retVal, std::string function, SQLHANDLE handle, SQLSMALLINT type) throw(std::string);
    std::string extract_error(std::string function, SQLHANDLE handle, SQLSMALLINT type);
    time_t time_to_epoch ( const struct tm *ltm, int utcdiff );
  };

  
}

#endif
