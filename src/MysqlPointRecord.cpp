//
//  MysqlPointRecord.cpp
//  epanet-rtx
//
//  Created by the EPANET-RTX Development Team
//  See README.md and license.txt for more information
//  

#include <iostream>
#include <sstream>//for string ostream
#include <fstream>// for file ostream
#include "string.h"
#include "malloc.h"
#include<time.h>//test time waste for writing data to .txt
#include <assert.h>// for file wR
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <mysql_connection.h>
#include <mysql_driver.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/warning.h>
#include <cppconn/metadata.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/resultset_metadata.h>
#include <cppconn/statement.h>
#include "MysqlPointRecord.h"
using namespace RTX;
using namespace std;
vector <PPoint>v_PPoint;

vector<std::pair<std::string,std::string> > v_ids;
long int RecordTsNum=0;
long int RecordElmtNum=0;
long int CountRegesiterId=0;
extern long int BatchProcessSize;
long int PreAllocateSize=200*BatchProcessSize;
long int countBatchInsert=0;
bool Getnamecount=false;
std::map<string,long int> id_map;
std::map<string,Point> recordpoint_map;
std::map< string, string> registerid_map;
extern long int Record_num;
extern bool f_CompleteSaveState;
extern bool f_CompleteRecordTs;
extern bool f_CompleteRecordElmt;
extern bool f_updateid;

unsigned int batchfirst_num;
unsigned int batchlast_num;

#define RTX_CREATE_POINT_TABLE_STRING "\
    CREATE TABLE IF NOT EXISTS `points` (\
    `time` int(11) unsigned NOT NULL,\
    `series_id` int(11) NOT NULL,\
    `value` double NOT NULL,\
    `confidence` double NOT NULL,\
    `quality` tinyint(4) NOT NULL,\
    PRIMARY KEY (`series_id`,`time`)\
    ) ENGINE=myisam DEFAULT CHARSET=utf8 COLLATE=utf8_bin;"

#define RTX_CREATE_TSKEY_TABLE_STRING "\
    CREATE TABLE IF NOT EXISTS `timeseries_meta` ( \
    `series_id`      int(11)      NOT NULL AUTO_INCREMENT, \
    `name`           varchar(255) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL, \
    `units`          varchar(12)  CHARACTER SET utf8 COLLATE utf8_bin NOT NULL, \
    `regular_period` int(11)      NOT NULL DEFAULT '0', \
    `regular_offset` int(10)      NOT NULL DEFAULT '0', \
    PRIMARY KEY (`series_id`), \
    UNIQUE KEY `name` (`name`) \
    ) ENGINE=myisam  DEFAULT CHARSET=utf8 COLLATE=utf8_bin AUTO_INCREMENT=1 ;"

string RecordIdBatchSelect="SELECT series_id,name from  timeseries_meta where ";
char *RecordIdmemcpySelect=(char*)malloc(PreAllocateSize);
char* RecordIdmemcpyInsert=(char*)malloc(PreAllocateSize);
char* memcpyInsert=(char*)malloc(PreAllocateSize);

string RecordIdBatchInsert="\r\n INSERT IGNORE INTO timeseries_meta (name,units) VALUES ";
string batchInsert="\r\n INSERT ignore INTO points (time, series_id,value) Values ";
string transactionstart="START TRANSACTION;";
char *Bufferidselect="SELECT series_id,name from  timeseries_meta where ";
char* BufferidInsert="\r\n INSERT IGNORE INTO timeseries_meta (name,units) VALUES ";
char *BuffermemcpyInsert="\r\n INSERT ignore INTO points (time, series_id,value) Values ";

MysqlPointRecord::MysqlPointRecord() {
  _connected = false;

//RecordIdBatchInsert.reserve(300*BatchProcessSize);
//RecordIdBatchSelect.reserve(300*BatchProcessSize);
batchInsert.reserve(PreAllocateSize);
v_ids.reserve(PreAllocateSize);
//RecordIdmemcpySelect="SELECT series_id,name from  timeseries_meta where ";
  memcpy(RecordIdmemcpySelect,Bufferidselect,strlen(Bufferidselect)+1);
  memcpy(RecordIdmemcpyInsert,BufferidInsert,strlen(BufferidInsert)+1);
  memcpy(memcpyInsert,BuffermemcpyInsert,strlen(BuffermemcpyInsert)+1);

}

MysqlPointRecord::~MysqlPointRecord() {
  /*
  if (_driver) {
    _driver->threadEnd();
  }
   */
}

#pragma mark - Public

void MysqlPointRecord::dbConnect() throw(RtxException) {
  
  bool databaseDoesExist = false;


 /* 
  if (RTX_STRINGS_ARE_EQUAL(this->uid(), "") ||
      RTX_STRINGS_ARE_EQUAL(this->pwd(), "") ||
      RTX_STRINGS_ARE_EQUAL(this->db(), "") ) {
    errorMessage = "Incomplete Login";
    return;
  }*/ //dhc disable 
  std::string tokenizedString = this->connectionString();
  std::map<std::string, std::string> kvPairs;
  boost::regex kvReg("([^=]+)=([^;]+);?"); // key - value pair
  boost::sregex_iterator it(tokenizedString.begin(), tokenizedString.end(), kvReg), end;
  for ( ; it != end; ++it){
	  kvPairs[(*it)[1]] = (*it)[2];
	  //cout << "key: " << (*it)[1] << "   value: " << (*it)[2] << endl;
  }
 // cout<<"print completed"<<endl;
  // if any of the keys are missing, just return.
  if (kvPairs.find("HOST") == kvPairs.end() ||
	  kvPairs.find("UID") == kvPairs.end() ||
	  kvPairs.find("PWD") == kvPairs.end() ||
	  kvPairs.find("DB") == kvPairs.end() )
  {
	  cout<<"Bad,It seems lack of one of license !"<<endl;//dhc modify
	  return;  
	  // todo -- th  row something?
  }
  else 
  {
	  cout<<"Good,All licenses are fair ! "<<endl;//dhc modify 
  }

  setHost( kvPairs["HOST"]);
  setUid( kvPairs["UID"]);
  setPwd(kvPairs["PWD"]);
  setDb(kvPairs["DB"]);
  
  try {
    _driver = get_driver_instance();// 初始化驱动-dhc
    _driver->threadInit();
    _mysqlCon.reset( _driver->connect(_connectionInfo.host, _connectionInfo.uid, _connectionInfo.pwd) );// 建立链接-dhc 

    cout<<"MYSQL Result DB Connect Success !"<<endl;//Add for look result dhc at 2015-1-12 
	_mysqlCon->setAutoCommit(false);
    
    // test for database exists
    //遍历mysql数据库名称，匹配给定数据库
    boost::shared_ptr<sql::Statement> st( _mysqlCon->createStatement() );
    sql::DatabaseMetaData *meta =  _mysqlCon->getMetaData();
    boost::shared_ptr<sql::ResultSet> results( meta->getSchemas() );
    while (results->next()) {
      std::string resultString(results->getString("TABLE_SCHEM"));
      if ( RTX_STRINGS_ARE_EQUAL(_connectionInfo.db, resultString) ) {
        databaseDoesExist = true;
        break;
      }
    }
    results->close();
    
    
    if (!databaseDoesExist) {
      string updateString;
      
      // create database
      updateString = "CREATE DATABASE ";
      updateString += _connectionInfo.db;
      st->executeUpdate(updateString);
      _mysqlCon->commit();
      _mysqlCon->setSchema(_connectionInfo.db);//设定当前数据库为_connectionInfo.db
      // create tables
      st->executeUpdate(RTX_CREATE_POINT_TABLE_STRING);
      st->executeUpdate(RTX_CREATE_TSKEY_TABLE_STRING);
      _mysqlCon->commit();
      cout << "Created new Database: " << _connectionInfo.db<< endl;
    }
    
    _mysqlCon->setSchema(_connectionInfo.db);//设定当前数据库为_connectionInfo.db
    
    
    // build the queries, since preparedStatements can't specify table names.
    //string rangeSelect = "SELECT time, value FROM " + tableName + " WHERE series_id = ? AND time > ? AND time <= ?";
    string preamble = "SELECT time, value, quality, confidence FROM points INNER JOIN timeseries_meta USING (series_id) WHERE name = ? AND ";
    string singleSelect = preamble + "time = ? order by time asc";
    string rangeSelect = preamble + "time >= ? AND time <= ? order by time asc";
    string nextSelect = preamble + "time > ? order by time asc LIMIT 1";
    string prevSelect = preamble + "time < ? order by time desc LIMIT 1";
    string singleInsert = "INSERT ignore INTO points (time, series_id, value, quality, confidence) SELECT ?,series_id,?,?,? FROM timeseries_meta WHERE name = ?";

    string firstSelectStr = "SELECT time, value, quality, confidence FROM points INNER JOIN timeseries_meta USING (series_id) WHERE name = ? order by time asc limit 1";
    string lastSelectStr = "SELECT time, value, quality, confidence FROM points INNER JOIN timeseries_meta USING (series_id) WHERE name = ? order by time desc limit 1";
    //for batch process
	
	//add for testing batch process
    _rangeSelect.reset( _mysqlCon->prepareStatement(rangeSelect) );
    _singleSelect.reset( _mysqlCon->prepareStatement(singleSelect) );
    _nextSelect.reset( _mysqlCon->prepareStatement(nextSelect) );
    _previousSelect.reset( _mysqlCon->prepareStatement(prevSelect) );
    _singleInsert.reset( _mysqlCon->prepareStatement(singleInsert) );
    
    _firstSelect.reset( _mysqlCon->prepareStatement(firstSelectStr) );
    _lastSelect.reset( _mysqlCon->prepareStatement(lastSelectStr) );
    _connected = true;
    errorMessage = "OK";
  }
  catch (sql::SQLException &e) {
		_connected = false;
    handleException(e);
  }
}

bool MysqlPointRecord::isConnected() {
  if (!_mysqlCon || !checkConnection()) {
    return false;
  }
  boost::shared_ptr<sql::Statement> stmt;
  boost::shared_ptr<sql::ResultSet> rs;
  bool connected = false;
  try {
    stmt.reset(_mysqlCon->createStatement());
    rs.reset( stmt->executeQuery("SELECT 1") );
    if (rs->next()) {
      connected = true; // connection is valid
    }
  }
  catch (sql::SQLException &e) {
    // TODO : log the exception ...
    connected = false;
  }
  
  return connected;
}

//std::string MysqlPointRecord::registerAndGetIdentifier(std::string recordName, Units dataUnits) {
//	if (f_CompleteRecordTs)
//	{
//		RecordElmtNum++;
//		string strIdSelecttemp=" name= '";       
//		strIdSelecttemp=strIdSelecttemp.append(recordName.c_str()).append("' OR");
//		char* strSelect=(char*)strIdSelecttemp.c_str();
//		char* str_end=";";
//		int lenSelect=strlen(RecordIdmemcpySelect);
//		//select id from  memcpy method is more fast than .append /+=/sprintf/strcpy/...
//		memcpy(RecordIdmemcpySelect+lenSelect,strSelect,strlen(strSelect)+1);
//		if (f_updateid)
//		{
//		string strIdInserttemp="( '";
//        strIdInserttemp=strIdInserttemp.append(recordName.c_str()).append("','").append(dataUnits.unitString()).append("' ),");
//        char* strInsert=(char*)strIdInserttemp.c_str();
//		int lenInsert=strlen(RecordIdmemcpyInsert);
//		// insert id from  memcpy method is more fast than .append /+=/sprintf/strcpy/...
//		memcpy(RecordIdmemcpyInsert+lenInsert,strInsert,strlen(strInsert)+1);
//		 if (RecordElmtNum%BatchProcessSize==0||f_CompleteRecordElmt){
//              lenInsert=strlen(RecordIdmemcpyInsert);
//              memcpy(RecordIdmemcpyInsert+lenInsert-1,str_end,strlen(str_end)+1);
//			  if (isConnected()) {
//				  try {
//              boost::shared_ptr<sql::Statement> StartTranctionStmt,RecordIdmemcpyInsertStmt;
//              StartTranctionStmt.reset(_mysqlCon->createStatement());
//			  RecordIdmemcpyInsertStmt.reset(_mysqlCon->createStatement());
//			  int affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
//			  affected =RecordIdmemcpyInsertStmt->executeUpdate(RecordIdmemcpyInsert);//batch insert id
//			_mysqlCon->commit();
//				  }catch (sql::SQLException &e) {
//					  handleException(e);
//				  }
//			}
//					 char* BufferidInsert="\r\n INSERT IGNORE INTO timeseries_meta (name,units) VALUES ";
//					 memcpy(RecordIdmemcpyInsert,BufferidInsert,strlen(BufferidInsert)+1);
//				 
//		  }		
//	  }
//	  if (RecordElmtNum%BatchProcessSize==0||f_CompleteRecordElmt){//if the count num >buffersize or have yet complete record then batch process- dhc
//			
//           
//			  lenSelect=strlen(RecordIdmemcpySelect);
//			
//			  memcpy(RecordIdmemcpySelect+lenSelect-2,str_end,strlen(str_end)+1);
//			if (isConnected()) {
//				try {
//					boost::shared_ptr<sql::Statement> StartTranctionStmt;
//					boost::shared_ptr<sql::PreparedStatement>	RecordIdmemcpySelectStmt;
//					StartTranctionStmt.reset(_mysqlCon->createStatement());
//					RecordIdmemcpySelectStmt.reset( _mysqlCon->prepareStatement(RecordIdmemcpySelect));
//					int affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
//					boost::shared_ptr<sql::ResultSet> results(RecordIdmemcpySelectStmt->executeQuery());//batch select id
//					while (results->next())
//					{
//						id_map.insert(make_pair(results->getString("name"),results->getInt("series_id")));
//					}
//					_mysqlCon->commit();
//				} catch (sql::SQLException &e) {
//					handleException(e);
//				}
//
//			}
//				
//			if (f_CompleteRecordElmt)//if complete record id then free  ptr
//			{
//				RecordElmtNum=0;
//				free(RecordIdmemcpyInsert);
//				free(RecordIdmemcpySelect);
//				RecordIdmemcpyInsert= NULL;
//				RecordIdmemcpySelect = NULL;
//			}
//			else 
//			{
//				RecordElmtNum=0;
//				char *Bufferidselect="SELECT series_id,name from  timeseries_meta where ";
//				memcpy(RecordIdmemcpySelect,Bufferidselect,strlen(Bufferidselect)+1);
//			}
//		}
//	}	
//  DB_PR_SUPER::registerAndGetIdentifier(recordName, dataUnits);
//  return recordName;
//}
/*
function:将模型元素的名称，单位注册到mysql数据库的表`timeseries_meta`当中，并自动生成series_id号
in:name,unit
out:name
process:
1.judge whether needed to update registing
(if already excuteed once time for the same network ,then do not update recommendly,because it will do repeated work and waster some time 
but,for a new network and  run rtx firstly,we must select update this register) 
2.if need ,then puck back all recordnames which needed to registered
3.the process that insert the recordnames by batchprocess ,so it will executed more than one time,The MySQL statement use preparedstatament ,so it will insert values fastly
for every time,the num of batchprocess is "batchprocess_num",and because the (total num%batchprocess_num) maybe not be zero ,so,for the last time, the less records will inserted separately by the other way.
4,last,dhc add an Affair:select all series_ids and the corresponding names  from  the table  timeseries_meta,it will be used in function:batchprocessinser()
*/
//std::string MysqlPointRecord::registerAndGetIdentifier(std::string recordName, Units dataUnits) {
//	int countfirst =0;
//	int affected = 0;
//	long int batchprocess_num=0;
//	
//	 if (f_updateid){
//             v_ids.push_back(make_pair<string,string>(recordName,dataUnits.unitString()));
//		 if (f_CompleteRecordElmt)
//		 {
//        long int v_size= v_ids.size();
//		char * fn="../../src/batchinsertidsstring.txt";
//		ostringstream buf;//read buf from file and write to str
//		char ch;
//		string batchidInsertfirst="\r\n INSERT IGNORE INTO timeseries_meta (name,units) VALUES  ";
//		string batchidInsertlast=batchidInsertfirst;
//		string partone="(?,?),";
//		string str="";
//		string tempstr="'";
//		ifstream ifile(fn);
//		while(buf&&ifile.get(ch))buf.put(ch);
//		str.reserve(PreAllocateSize);
//		str= buf.str();//str="('?','?'),('?','?'),....."
//		batchidInsertfirst.reserve(PreAllocateSize);
//		batchidInsertlast.reserve(PreAllocateSize);
//		batchfirst_num=v_size/BatchProcessSize;
//		batchlast_num=v_size%BatchProcessSize;
//		//string tempstring=(const string)(str) ;
//		if (batchfirst_num)batchprocess_num=BatchProcessSize;
//		else batchprocess_num=0;
//		batchidInsertfirst=batchidInsertfirst.append(str.c_str(),0,batchprocess_num*strlen(partone.c_str()));
//		batchidInsertfirst=batchidInsertfirst.substr(0,batchidInsertfirst.length()-1);
//		batchidInsertfirst=batchidInsertfirst.append(";");
//		batchidInsertlast=batchidInsertlast.append(str.c_str(),0, batchlast_num*strlen(partone.c_str()));
//		batchidInsertlast=batchidInsertlast.substr(0,batchidInsertlast.length()-1);
//		batchidInsertlast=batchidInsertlast.append(";");
//		 boost::shared_ptr<sql::PreparedStatement> _Insertidbatchfirst,_Insertidbatchlast;
//		 boost::shared_ptr<sql::Statement> StartTranctionStmt;
//		  this->checkConnection();
//		 StartTranctionStmt.reset(_mysqlCon->createStatement());
//		if (batchprocess_num>0){
//			_Insertidbatchfirst.reset(_mysqlCon->prepareStatement(batchidInsertfirst));					 
//		}
//		if (batchlast_num>0){
//			_Insertidbatchlast.reset(_mysqlCon->prepareStatement(batchidInsertlast));
//		}
//		vector<std::pair<std::string,std::string> >::const_iterator _iter=v_ids.begin();	
//		if (batchprocess_num>0){
//			 for (int i=0;i<batchfirst_num;i++){
//			if (checkConnection()) {
//						 countfirst=0;
//						
//						for (;;_iter++){
//							_Insertidbatchfirst->setString(1+2*countfirst,_iter->first);
//							_Insertidbatchfirst->setString(2+2*countfirst,_iter->second);
//							  countfirst++;
//							  if (countfirst>=batchprocess_num)
//							  {
//								  break;
//							  }
//						   }
//						try {
//                            affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction,promote insert effeciency
//							affected =_Insertidbatchfirst->execute();
//						} catch (...) {
//							cout << "whoops!" << endl;
//						}
//						_mysqlCon->commit();	
//					}
//		       }
//		}
//		if (batchlast_num>0){
//			if (checkConnection()) {
//				countfirst=0;
//				for (;;_iter++){
//					_Insertidbatchlast->setString(1+2*countfirst,_iter->first);
//					_Insertidbatchlast->setString(2+2*countfirst,_iter->second);
//					countfirst++;
//					if (countfirst>=batchlast_num)
//					{
//						break;
//					}
//				}
//				try {
//					affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote the  insert process more efficient
//					affected =_Insertidbatchlast->execute();
//				} catch (...) {
//					cout << "whoops!" << endl;
//				}
//				_mysqlCon->commit();
//			} 	
//		}
//        }  	
//	 }
//	
//	    if (f_CompleteRecordElmt){ 
//			if (isConnected()) {
//				try {
//					string strselect="SELECT series_id,NAME FROM timeseries_meta ORDER BY series_id;";
//					boost::shared_ptr<sql::Statement> StartTranctionStmt;
//					boost::shared_ptr<sql::PreparedStatement>	RecordIdSelectStmt;
//					StartTranctionStmt.reset(_mysqlCon->createStatement());
//					RecordIdSelectStmt.reset( _mysqlCon->prepareStatement(strselect));
//					int affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
//					boost::shared_ptr<sql::ResultSet> results(RecordIdSelectStmt->executeQuery());//batch select id
//					while (results->next())
//					{
//						id_map.insert(make_pair(results->getString("name"),results->getInt("series_id")));
//					}
//					_mysqlCon->commit();
//				} catch (sql::SQLException &e) {
//					handleException(e);
//				}
//
//			}
//	 }
//	DB_PR_SUPER::registerAndGetIdentifier(recordName, dataUnits);
//	return recordName;
//}


//std::string MysqlPointRecord::registerAndGetIdentifier(std::string recordName, Units dataUnits) {
//	int countfirst =0;
//	int affected = 0;
//	long int batchprocess_num=0;
//    static long int count_num=0;
//   	if (f_updateid){
//		v_ids.push_back(make_pair<string,string>(recordName,dataUnits.unitString()));
//		if (f_CompleteRecordElmt){
//			char ch;
//			ostringstream buf;//read buf from file and write to str
//			char * fn="../../src/batchinsertidsstring.txt";
//			long int v_size= v_ids.size();
//			string str="";
//			string tempstr=str;
//			string partone="(?,?),";
//			string batchidInsertfirst="\r\n INSERT IGNORE INTO timeseries_meta (name,units) VALUES  ";
//			string batchidInsertlast=batchidInsertfirst;
//			ifstream ifile(fn);
//			while(buf&&ifile.get(ch))buf.put(ch);//get out all strings from .txt to str
//			str.reserve(PreAllocateSize);
//			str= buf.str();//str="('?','?'),('?','?'),....."
//			batchidInsertfirst.reserve(PreAllocateSize);
//			batchidInsertlast.reserve(PreAllocateSize);
//			batchfirst_num=v_size/BatchProcessSize;
//			batchlast_num=v_size%BatchProcessSize;
//
//			//string tempstring=(const string)(str) ;
//			if (batchfirst_num)batchprocess_num=BatchProcessSize;else batchprocess_num=0;
//
//			batchidInsertfirst=batchidInsertfirst.append(str.c_str(),0,batchprocess_num*strlen(partone.c_str()));
//			batchidInsertfirst=batchidInsertfirst.substr(0,batchidInsertfirst.length()-1);
//			batchidInsertfirst=batchidInsertfirst.append(";");
//
//			batchidInsertlast=batchidInsertlast.append(str.c_str(),0, batchlast_num*strlen(partone.c_str()));
//			batchidInsertlast=batchidInsertlast.substr(0,batchidInsertlast.length()-1);
//			batchidInsertlast=batchidInsertlast.append(";");
//			
//			boost::shared_ptr<sql::Statement> StartTranctionStmt;
//			boost::shared_ptr<sql::PreparedStatement> _Insertidbatchfirst,_Insertidbatchlast;
//			
//			this->checkConnection();
//			StartTranctionStmt.reset(_mysqlCon->createStatement());
//			if (batchfirst_num>0)_Insertidbatchfirst.reset(_mysqlCon->prepareStatement(batchidInsertfirst));					 
//			if (batchlast_num>0)_Insertidbatchlast.reset(_mysqlCon->prepareStatement(batchidInsertlast));
//			
//			count_num=0;
//			if (batchfirst_num>0){
//				for (int i=0;i<batchfirst_num;i++){
//					if (checkConnection()) {	
//
//						for (int j=0;j<batchprocess_num;j++){
//							
//								_Insertidbatchfirst->setString(1+2*j,v_ids[count_num].first);
//								_Insertidbatchfirst->setString(2+2*j,v_ids[count_num].second);	
//								count_num++;
//						}
//						try {
//							affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction,promote insert effeciency
//							affected =_Insertidbatchfirst->execute();
//						} catch (...) {
//							cout << "whoops!" << endl;
//						}
//						_mysqlCon->commit();	
//						
//						}
//						
//						
//				}
//			}
//			if (batchlast_num>0){
//				if (checkConnection()) {	
//					for(int j=0;j<batchlast_num;j++){
//							_Insertidbatchlast->setString(1+2*j,v_ids[count_num].first);
//							_Insertidbatchlast->setString(2+2*j,v_ids[count_num].second);	
//							count_num++;
//						}
//						
//							try {
//								affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote the  insert process more efficient
//								affected =_Insertidbatchlast->execute();
//							} catch (...) {
//								cout << "whoops!" << endl;
//							}
//							_mysqlCon->commit();
//						
//				}		
//			}
//		}
//	}
//
//	if (f_CompleteRecordElmt){ 
//		if (isConnected()) {
//			try {
//				string strselect="SELECT series_id,NAME FROM timeseries_meta ORDER BY series_id;";
//				boost::shared_ptr<sql::Statement> StartTranctionStmt;
//				boost::shared_ptr<sql::PreparedStatement>	RecordIdSelectStmt;
//				StartTranctionStmt.reset(_mysqlCon->createStatement());
//				RecordIdSelectStmt.reset( _mysqlCon->prepareStatement(strselect));
//				int affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
//				boost::shared_ptr<sql::ResultSet> results(RecordIdSelectStmt->executeQuery());//batch select id
//				while (results->next())
//				{
//					id_map.insert(make_pair(results->getString("name"),results->getInt("series_id")));
//				}
//				_mysqlCon->commit();
//			} catch (sql::SQLException &e) {
//				handleException(e);
//			}
//
//		}
//		f_CompleteRecordElmt=!f_CompleteRecordElmt;
//	}
//	DB_PR_SUPER::registerAndGetIdentifier(recordName, dataUnits);
//	return recordName;
//}


std::string MysqlPointRecord::registerAndGetIdentifier(std::string recordName, Units dataUnits) {
	int affected = 0;
	
	if (f_updateid){

		
		static char ch;
		static ostringstream buf;//read buf from file and write to str
		static char * fn="../../src/batchinsertidsstring.txt";
		static string partone="(?,?),";
		static string str="";
		str.reserve(PreAllocateSize);
		static int count_num=0;
		static int count_num1=0;
		static int count_num2=0;
		static string batchidInsertfirst="\r\n INSERT IGNORE INTO timeseries_meta (name,units) VALUES  ";
		static string batchidInsertlast=batchidInsertfirst;
		batchidInsertfirst.reserve(PreAllocateSize);
		batchidInsertlast.reserve(PreAllocateSize); 

		if (!count_num)//only excute once  
		{
			
			ifstream ifile(fn);
			while(buf&&ifile.get(ch))buf.put(ch);//get out all strings from .txt to str
			str= buf.str();//str="('?','?'),('?','?'),....."
			count_num=1;
		}
		v_ids.push_back(make_pair<string,string>(recordName,dataUnits.unitString()));
		if (v_ids.size()>=BatchProcessSize){
	        	if (!count_num1)//only excute once  
				{
					batchidInsertfirst=batchidInsertfirst.append(str.c_str(),0,BatchProcessSize*strlen(partone.c_str()));
					batchidInsertfirst=batchidInsertfirst.substr(0,batchidInsertfirst.length()-1);
					batchidInsertfirst=batchidInsertfirst.append(";");
					count_num1=1;
				}	
			this->checkConnection();
			boost::shared_ptr<sql::Statement> StartTranctionStmt;
			StartTranctionStmt.reset(_mysqlCon->createStatement());
			this->checkConnection();
			boost::shared_ptr<sql::PreparedStatement> _Insertidbatchfirst;
            _Insertidbatchfirst.reset(_mysqlCon->prepareStatement(batchidInsertfirst));	
			if (checkConnection()) {	
				for (int j=0;j<v_ids.size();j++){
					_Insertidbatchfirst->setString(1+2*j,v_ids[j].first);
					_Insertidbatchfirst->setString(2+2*j,v_ids[j].second);	
				}
				try {
					affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction,promote insert effeciency
					affected =_Insertidbatchfirst->execute();
				} catch (...) {
					cout << "whoops!" << endl;
				}
				_mysqlCon->commit();	
               v_ids.clear();
			}
		}
		else if(f_CompleteRecordElmt)
		{
			if (!count_num2)
			{
				batchidInsertlast=batchidInsertlast.append(str.c_str(),0, v_ids.size()*strlen(partone.c_str()));
				batchidInsertlast=batchidInsertlast.substr(0,batchidInsertlast.length()-1);
				batchidInsertlast=batchidInsertlast.append(";");
				count_num2=1;
			}
			this->checkConnection();
			boost::shared_ptr<sql::Statement> StartTranctionStmt;
			StartTranctionStmt.reset(_mysqlCon->createStatement());
			this->checkConnection();
			boost::shared_ptr<sql::PreparedStatement> _Insertidbatchlast;
			_Insertidbatchlast.reset(_mysqlCon->prepareStatement(batchidInsertlast));

			if (checkConnection()) {	
				for(int j=0;j<v_ids.size();j++){
					_Insertidbatchlast->setString(1+2*j,v_ids[j].first);
					_Insertidbatchlast->setString(2+2*j,v_ids[j].second);
				}
				try {
					affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote the  insert process more efficient
					affected =_Insertidbatchlast->execute();
				} catch (...) {
					cout << "whoops!" << endl;
				}
				_mysqlCon->commit();

			}
		}

}		
	if (f_CompleteRecordElmt){ 
		if (isConnected()) {
			try {
				string strselect="SELECT series_id,NAME FROM timeseries_meta ORDER BY series_id;";
				boost::shared_ptr<sql::Statement> StartTranctionStmt;
				boost::shared_ptr<sql::PreparedStatement>	RecordIdSelectStmt;
				StartTranctionStmt.reset(_mysqlCon->createStatement());
				RecordIdSelectStmt.reset( _mysqlCon->prepareStatement(strselect));
				int affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
				boost::shared_ptr<sql::ResultSet> results(RecordIdSelectStmt->executeQuery());//batch select id
				while (results->next())
				{
					id_map.insert(make_pair(results->getString("name"),results->getInt("series_id")));
				}
				_mysqlCon->commit();
			} catch (sql::SQLException &e) {
				handleException(e);
			}

		}
		f_CompleteRecordElmt=!f_CompleteRecordElmt;
	}
	DB_PR_SUPER::registerAndGetIdentifier(recordName, dataUnits);
	return recordName;
}

PointRecord::time_pair_t MysqlPointRecord::range(const string& id) {
  Point last;
  Point first;
  
  //cout << "range: " << id << endl;
  
  if (checkConnection()) {
    
    _firstSelect->setString(1, id);
    boost::shared_ptr<sql::ResultSet> rResults(_firstSelect->executeQuery());
    vector<Point> fsPoints = pointsFromResultSet(rResults);
    if (fsPoints.size() > 0) {
      first = fsPoints.front();
    }
    
    _lastSelect->setString(1, id);
    boost::shared_ptr<sql::ResultSet> lResults(_lastSelect->executeQuery());
    vector<Point> lsPoints = pointsFromResultSet(lResults);
    if (lsPoints.size() > 0) {
      last = lsPoints.front();
    }
  }
  return make_pair(first.time, last.time);
}

#pragma mark - simple set/get

string MysqlPointRecord::host() {
  return _connectionInfo.host;
}

void MysqlPointRecord::setHost(string host) {
  _connectionInfo.host = host;
}


string MysqlPointRecord::uid() {
  return _connectionInfo.uid;
}

void MysqlPointRecord::setUid(string uid) {
  _connectionInfo.uid = uid;
}


string MysqlPointRecord::pwd() {
  return _connectionInfo.pwd;
}

void MysqlPointRecord::setPwd(string pwd) {
  _connectionInfo.pwd = pwd;
}


string MysqlPointRecord::db() {
  return _connectionInfo.db;
}

void MysqlPointRecord::setDb(string db) {
  _connectionInfo.db = db;
}


#pragma mark - db meta

vector< pair<string, Units> > MysqlPointRecord::availableData() {
  vector< pair<string, Units> > available;
  
  if (isConnected()) {
    
    boost::shared_ptr<sql::Statement> selectNamesStatement( _mysqlCon->createStatement() );
    boost::shared_ptr<sql::ResultSet> results( selectNamesStatement->executeQuery("SELECT name,units FROM timeseries_meta WHERE 1") );
    while (results->next()) {
      // add the name to the list.
      std::string theName = results->getString("name");
      std::string theUnits = results->getString("units");
      //std::cout << "found: " << thisName << std::endl;
      available.push_back(make_pair(theName, Units::unitOfType(theUnits)));
    }
    
  }
  

  return available;
  
}


std::vector<std::string> MysqlPointRecord::identifiers() {
  std::vector<std::string> ids;
  if (!isConnected()) {
    return ids;
  }
  boost::shared_ptr<sql::Statement> selectNamesStatement( _mysqlCon->createStatement() );
  boost::shared_ptr<sql::ResultSet> results( selectNamesStatement->executeQuery("SELECT name FROM timeseries_meta WHERE 1") );
  while (results->next()) {
    // add the name to the list.
    std::string thisName = results->getString("name");
    //std::cout << "found: " << thisName << std::endl;
    ids.push_back(thisName);
  }
  return ids;
}



/*
void MysqlPointRecord::fetchRange(const std::string& id, time_t startTime, time_t endTime) {
  DbPointRecord::fetchRange(id, startTime, endTime);
}

void MysqlPointRecord::fetchNext(const std::string& id, time_t time) {
  DbPointRecord::fetchNext(id, time);
}

void MysqlPointRecord::fetchPrevious(const std::string& id, time_t time) {
  DbPointRecord::fetchPrevious(id, time);
}
 */

// select just returns the results (no caching)
std::vector<Point> MysqlPointRecord::selectRange(const std::string& id, time_t start, time_t end) {
  //cout << "mysql range: " << start << " - " << end << endl;
  
  std::vector<Point> points;
  
  if (!checkConnection()) {
    this->dbConnect();
  }
  
  if (checkConnection()) {
    _rangeSelect->setString(1, id);
    _rangeSelect->setInt(2, (int)start);
    _rangeSelect->setInt(3, (int)end);
    boost::shared_ptr<sql::ResultSet> results(_rangeSelect->executeQuery());
    points = pointsFromResultSet(results);
  }
  
  return points;
}


Point MysqlPointRecord::selectNext(const std::string& id, time_t time) {
  return selectSingle(id, time, _nextSelect);
}


Point MysqlPointRecord::selectPrevious(const std::string& id, time_t time) {
  return selectSingle(id, time, _previousSelect);
}

//void MysqlPointRecord::insertByBatchProcess(const std::string& id, Point point)
//{
//	/************************************************
//	purpose:insert points by batch process
//	input:point which need to insert to DB 
//	out:no
//	process:
//	1.pushback point
//	2.when the size of points >InsertBufferSize
//		1.get the series_id related to name and store into map
//		2.create the batch process sql query like:insert ignore into points (time, series_id, value, quality, confidence)
//		values(,,,,,),
//			     (,,,,,),
//				 ...
//				 (,,,,,);
//		3.execute
//
//	************************************************/
// 	Point t;
//	PPoint temp_PPoint(id,point.time,point.value,point.quality,point.confidence);
//    v_PPoint.push_back(temp_PPoint);
//	if (v_PPoint.size() >=InsertBufferSize)//may define the size of batch process insert
//	{
//		int affected = 0;
//		if (checkConnection()) {
//			string str = batchInsert;
//			int  id_result=0;
//			map <string,int> id_map;
//			
//			string select_id="SELECT series_id,name from  timeseries_meta where ";
//			BOOST_FOREACH(PPoint _PPoint,v_PPoint)
//		    //generate the bulk statement for extracting series_id,record the name and id of the point 
//            {
//				select_id=select_id+" name = "+"'"+_PPoint.id+"'"+" "+"OR";				
//            }
//			select_id = select_id.substr(0,select_id.length() - 2);
//			select_id =select_id+';';
//			boost::shared_ptr<sql::PreparedStatement> select_idStmt;
//			select_idStmt.reset( _mysqlCon->prepareStatement(select_id));
//			boost::shared_ptr<sql::ResultSet> results(select_idStmt->executeQuery());
//			while (results->next())
//			{
//				id_map.insert(make_pair(results->getString("name"),results->getInt("series_id")));
//			}	
//			BOOST_FOREACH(PPoint _PPoint,v_PPoint)
//		    //generate the bulk insert statement
//			{
//				stringstream s[5];
//				s[0]<<_PPoint.time;s[1]<<id_map[_PPoint.id];s[2]<<_PPoint.value;s[3]<<_PPoint.quality;s[4]<<_PPoint.confidence;
//				str =str+'('+s[0].str()+ ',' +s[1].str()+','+s[2].str()+','+s[3].str()+','+s[4].str()+ ")," ; 
//			}
//			long int lenth=str.length();
//			str = str.substr(0,str.length() - 1);//delete the last char ','
//			lenth=str.length();
//			str=str+';';         
//			boost::shared_ptr<sql::Statement> StartTranctionStmt, batchinsertStmt;
//			StartTranctionStmt.reset(_mysqlCon->createStatement());
//			batchinsertStmt.reset( _mysqlCon->createStatement() );   
//			try {
//				affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
//				affected = batchinsertStmt->executeUpdate(str);
//			} catch (...) {
//				cout << "whoops!" << endl;
//			}
//			_mysqlCon->commit();
//		}
//		v_PPoint.clear();
//		
//    }
//
//}
//best method 1---------------------------------------------------------------------------------
//void MysqlPointRecord::insertByBatchProcess(const std::string& id, Point point)
//{
//	/************************************************
//	purpose:insert points by batch process
//	input:point which need to insert to DB 
//	out:no
//	process:
//	1.pushback point
//	2.when the size of points >InsertBufferSize
//		1.get the series_id related to name and store into map
//		2.create the batch process sql query like:insert ignore into points (time, series_id, value, quality, confidence)
//		values(,,,,,),
//			     (,,,,,),
//				 ...
//				 (,,,,,);
//		3.execute
//
//	************************************************/
//    stringstream s[3];
//	static string str=batchInsert;
//    s[0]<<point.time;s[1]<<id_map[id];s[2]<<point.value;
//	str =str+'('+s[0].str()+ ',' +s[1].str()+','+s[2].str()+ ")," ; 
//	if (/*v_PPoint.size()*/ countBatchInsert++>=BatchProcessSize||f_CompleteSaveState)//may define the size of batch process insert
//	{
//		int affected = 0;
//		if (checkConnection()) {
//			str = str.substr(0,str.length() - 1);//delete the last char ','
//			str=str+';';         
//			boost::shared_ptr<sql::Statement> StartTranctionStmt, batchinsertStmt;
//			StartTranctionStmt.reset(_mysqlCon->createStatement());
//			batchinsertStmt.reset( _mysqlCon->createStatement() );   
//			try {
//				affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
//				affected = batchinsertStmt->executeUpdate(str);
//			} catch (...) {
//				cout << "whoops!" << endl;
//			}
//			_mysqlCon->commit();
//			str=batchInsert;
//			countBatchInsert=0;
//		}
//    }
//
//}
//best method the good ==============================================
//void MysqlPointRecord::insertByBatchProcess(const std::string& id, Point point)
// {
//	/**********************************************************************************
//	purpose:insert points by batch process
//	input:point whitch need to insert to DB 
//	out:no
//	process:
//	1.pushback point
//	2.when the size of points >InsertBufferSize
//		1.get the series_id related to name and store into map
//		2.create the batch process sql query like:insert ignore into points (time, series_id, value, quality, confidence)
//		values(,,,,,),
//			     (,,,,,),
//				 ...
//				 (,,,,,);
//		3.execute
//  author:dhc
//	************************************************************************************/   
//	 //insert id by memcpy method is more fast than .append /+=/sprintf/strcpy/...
//	
//	 stringstream s[3];
//	 s[0]<<point.time;s[1]<<id_map[id];s[2]<<point.value;
//	 string tempstr='('+s[0].str()+ ',' +s[1].str()+','+s[2].str()+ ")," ; 
//     char* memcpyinserttemp=(char*)tempstr.c_str();
//	 char* templast=";";
//	 memcpy(memcpyInsert+strlen(memcpyInsert),memcpyinserttemp,strlen(memcpyinserttemp)+1);
//        if (countBatchInsert++>=BatchProcessSize||f_CompleteSaveState){
//				if (checkConnection())
//				{
//					int affected = 0;
//					memcpy(memcpyInsert+strlen(memcpyInsert)-1,templast,strlen(templast)+1);//replace "," to ";"
//					boost::shared_ptr<sql::Statement> StartTranctionStmt, memcpyinsertStmt;
//					StartTranctionStmt.reset(_mysqlCon->createStatement());
//					memcpyinsertStmt.reset( _mysqlCon->createStatement() );   
//					try {
//						affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
//						affected =memcpyinsertStmt->executeUpdate(memcpyInsert);
//					} catch (...) {
//						cout << "whoops!" << endl;
//					}
//					_mysqlCon->commit();
//				}
//					memcpy(memcpyInsert,BuffermemcpyInsert,strlen(BuffermemcpyInsert)+1);
//				    countBatchInsert=0;
//			}
//}

//best method the good ==============================================
//void MysqlPointRecord::insertByBatchProcess(const std::string& id, Point point)
// {
//	/**********************************************************************************
//	purpose:insert points by batch process
//	input:point whitch need to insert to DB 
//	out:no
//	process:
//	1.pushback point
//	2.when the size of points >InsertBufferSize
//		1.get the series_id related to name and store into map
//		2.create the batch process sql query like:insert ignore into points (time, series_id, value, quality, confidence)
//		values(,,,,,),
//			     (,,,,,),
//				 ...
//				 (,,,,,);
//		3.execute
//  author:dhc
//	************************************************************************************/   
//	 //insert id by memcpy method is more fast than .append /+=/sprintf/strcpy/...
//	
//	 stringstream s[7];
//	 static int oncestart=0;
//	 string tempstr="";
//	 s[0]<<f_CompleteSaveState;s[1]<<BatchProcessSize;
//	 s[2]<<point.time;s[3]<<id_map[id];s[4]<<point.value;s[5]<<point.quality;s[6]<<point.confidence;
//     tempstr= tempstr.append("CALL BatchInsertProcedure( ").append(s[0].str()).append(",").append(s[1].str()).append(",").append(s[2].str()).append(",")/**/;
//	 tempstr= tempstr.append(s[3].str()).append(",").append(s[4].str()).append(",").append(s[5].str()).append(",");
//	 tempstr= tempstr.append(s[6].str()).append(" )");
//	//if (checkConnection())
//			//	{
//					int affected = 0;
//                    if(!oncestart)
//					{
//					boost::shared_ptr<sql::Statement> StartTranctionStmt;
//					StartTranctionStmt.reset(_mysqlCon->createStatement());
//				    affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
//					oncestart=!oncestart;
//					}
//					boost::shared_ptr<sql::Statement> storedprocedureStmt;
//					storedprocedureStmt.reset(_mysqlCon->createStatement());
//					try {
//						affected = storedprocedureStmt->executeUpdate(tempstr);//start transaction，promote insert effeciency
//					} catch (...) {
//						cout << "whoops!" << endl;
//					}
//				//}
//}


// insertions or alterations may choose to ignore / deny

//void MysqlPointRecord::insertByBatchProcess(const std::string& id, Point point)
// {
//	/**********************************************************************************
//	purpose:insert points by batch process
//	input:point whitch need to insert to DB 
//	out:no
//	process:
//	1.pushback point
//	2.when the size of points >InsertBufferSize
//		1.get the series_id related to name and store into map
//		2.create the batch process sql query like:insert ignore into points (time, series_id, value, quality, confidence)
//		values(,,,,,),
//			     (,,,,,),
//				 ...
//				 (,,,,,);
//		3.execute
//  author:dhc
//	************************************************************************************/   
//	    int affected = 0;
//        int lenInsert=strlen(memcpyInsert);
//		char t[11],s[255],v[255],q[11],c[11];
//		char *memcpyinserttemp=new char[BatchProcessSize];
//		char *tempinsert1="( ";
//        char *tempinsert2=",";
//        char *tempinsert3= " ),";
//		char *tempinsert4=";";
//		sprintf(t, "%d", point.time); // replace type int  to type char
//		sprintf(s, "%d",id_map[id]); // 
//		sprintf(v, "%.12f", point.value); 
//		//sprintf(q, "%d", point.quality); 
//		//sprintf(c, "%d", point.confidence); 	
//		string tempstr="";
//		tempstr=tempstr.append(tempinsert1).append(t).append(tempinsert2).append(s).append(tempinsert2).append(v).append(tempinsert3);
//		//sprintf(memcpyinserttemp,"%s%s%s%s%s%s%s%s%s%s%s",tempinsert1,t,tempinsert2,s,tempinsert2, v,tempinsert2,q,tempinsert2,c,tempinsert3);
//		//insert id from  memcpy method is more fast than .append /+=/sprintf/strcpy/...
//		memcpyinserttemp=(char*)tempstr.c_str();
//		//memcpy(memcpyInsert+lenInsert,memcpyinserttemp,strlen(memcpyinserttemp)+1);
//		//memcpy(memcpyInsert+lenInsert,memcpyinserttemp,strlen(memcpyinserttemp)+1);
//		batchInsert=batchInsert.append(tempstr);
//		countBatchInsert++;
//        if (countBatchInsert%BatchProcessSize==0||f_CompleteSaveState){
//				if (checkConnection())
//				{
//					
//					//lenInsert=strlen(memcpyInsert);
//					//memcpy(memcpyInsert+lenInsert-1,tempinsert4,strlen(tempinsert4)+1);//replace "," to ";"
//					lenInsert=batchInsert.length();
//                    batchInsert=batchInsert.substr(0,batchInsert.length()-1);
//				    batchInsert=batchInsert.append(tempinsert4);
//					lenInsert=batchInsert.length();
//					boost::shared_ptr<sql::Statement> StartTranctionStmt, memcpyinsertStmt;
//					StartTranctionStmt.reset(_mysqlCon->createStatement());
//					memcpyinsertStmt.reset( _mysqlCon->createStatement() );   
//					try {
//						affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
//						//affected =memcpyinsertStmt->executeUpdate(memcpyInsert);
//						 affected =memcpyinsertStmt->executeUpdate(batchInsert);
//					} catch (...) {
//						cout << "whoops!" << endl;
//					}
//					_mysqlCon->commit();
//				}
//				if (f_CompleteSaveState)//if complete record id then free  ptr
//				{
//					f_CompleteSaveState=!f_CompleteSaveState;
//				}
//				  //  char *BuffermemcpyInsert="\r\n INSERT ignore INTO points (time, series_id,value, quality, confidence) Values ";
//				//	memset(memcpyInsert, 0, sizeof(memcpyInsert));//reset dhc
//					//memcpy(memcpyInsert,BuffermemcpyInsert,strlen(BuffermemcpyInsert)+1);
//				    batchInsert="\r\n INSERT ignore INTO points (time, series_id,value) Values ";
//				    countBatchInsert=0;
//			}
//}
//void MysqlPointRecord::insertByBatchProcess(const std::string& id, Point point)
// {
//	/**********************************************************************************
//	purpose:insert points by batch process
//	input:point whitch need to insert to DB 
//	out:no
//	process:
//	1.pushback point
//	2.when the size of points >InsertBufferSize
//		1.get the series_id related to name and store into map
//		2.create the batch process sql query like:insert ignore into points (time, series_id, value, quality, confidence)
//		values(,,,,,),
//			     (,,,,,),
//				 ...
//				 (,,,,,);
//		3.execute
//  author:dhc
//	************************************************************************************/  
//        
//		 int countfirst =0;
//		 int countlast =0;
//		 int affected = 0;
//		 static long int batchprocess_num=0;
//		 static int countall=0;
//		 static bool f_once =false;
//		 recordpoint_map[id]=point;
//		 long int map_size= recordpoint_map.size();
//		 if (countall++>=Record_num)
//		 {
//			 if (!_mysqlCon||!f_once)// only need excute once unless we have disconnect did we must create again
//			 {
//
//				 this->checkConnection();
//				 batchfirst_num=map_size/BatchProcessSize;
//				 batchlast_num=map_size%BatchProcessSize;
//				 char* batchInsertfirst_str ="\r\n INSERT ignore INTO points (time, series_id, value, quality, confidence) Values ";
//                 char* partone="(?,?,?,?,?),";
//				 char* batchInsertlast_str = batchInsertfirst_str;
//				 batchInsertfirst_str.reserve(PreAllocateSize);
//				 batchInsertlast_str.reserve(PreAllocateSize);
//
//				 if (batchfirst_num)batchprocess_num=BatchProcessSize;
//				 else batchprocess_num=0;
//				
//				 for (int i=0;i<batchprocess_num;i++)
//				 {
//					 batchInsertfirst_str=batchInsertfirst_str.append("( ?,?,?,?,? ),");
//				 }
//				 batchInsertfirst_str=batchInsertfirst_str.substr(0,batchInsertfirst_str.length()-1);
//				 batchInsertfirst_str= batchInsertfirst_str.append(";");
//				 for (int i=0;i<batchlast_num;i++)
//				 {
//					 batchInsertlast_str=batchInsertlast_str.append("( ?,?,?,?,? ),");
//				 }
//				 batchInsertlast_str=batchInsertlast_str.substr(0,batchInsertlast_str.length()-1);
//				 batchInsertlast_str=batchInsertlast_str.append(";");
//				 if (batchprocess_num>0){
//					 _Insertbatchfirst.reset(_mysqlCon->prepareStatement(batchInsertfirst_str));					 
//				 }
//				 if (batchlast_num>0){
//				 _Insertbatchlast.reset(_mysqlCon->prepareStatement(batchInsertlast_str));
//				 }
//			 } 
//			 f_once=true;
//			 map<string,Point>::const_iterator _iter=recordpoint_map.begin();
//		    // map<string,Point>::size_type BatchLength=BatchProcessSize;
//			// map<string,Point>::const_iterator _iterend=recordpoint_map.begin()+BatchLength;
//			 if (batchprocess_num>0) {
//				 for (int i=0;i<batchfirst_num;i++){
//					 if (checkConnection()) {
//						 countfirst=0;
//						 for (;;_iter++){
//							 _Insertbatchfirst->setInt(1+5*countfirst, (int)((_iter->second).time));
//							 // todo -- check this: _singleInsert->setUInt64(1, (uint64_t)time);
//							 _Insertbatchfirst->setInt64(2+5*countfirst, id_map[_iter->first]);
//							 _Insertbatchfirst->setDouble(3+5*countfirst,(_iter->second).value);
//							 _Insertbatchfirst->setInt(4+5*countfirst, (_iter->second).quality);
//							 _Insertbatchfirst->setDouble(5+5*countfirst, (_iter->second).confidence);
//							 countfirst++;
//							 if (countfirst>=batchprocess_num)
//							 {
//								 break;
//							 }
//						 }
//						 try {
//							 affected =_Insertbatchfirst->execute();
//						 } catch (...) {
//							 cout << "whoops!" << endl;
//						 }
//						 _mysqlCon->commit();
//					 }
//				 }
//			 }
//					if (batchlast_num>0){
//						if (checkConnection()) {
//							countlast=0;
//							map<string,Point>::size_type BatchLength=batchlast_num;
//
//							for (;;_iter++){
//								_Insertbatchlast->setInt(1+5*countlast, (int)((_iter->second).time));
//								// todo -- check this: _singleInsert->setUInt64(1, (uint64_t)time);
//								_Insertbatchlast->setInt64(2+5*countlast, id_map[_iter->first]);
//								_Insertbatchlast->setDouble(3+5*countlast, (_iter->second).value);
//								_Insertbatchlast->setInt(4+5*countlast, (_iter->second).quality);
//								_Insertbatchlast->setDouble(5+5*countlast,(_iter->second).confidence);			
//								countlast++;
//								if (countlast>=batchlast_num)
//								{
//									break;
//								}
//							}
//							try {
//								affected =_Insertbatchlast->execute();
//							} catch (...) {
//								cout << "whoops!" << endl;
//							}
//							_mysqlCon->commit();
//						} 	
//					}
//	countall=0;		 
//	}
//	
// }
//void MysqlPointRecord::insertByBatchProcess(const std::string& id, Point point)
// {
//	/**********************************************************************************
//	purpose:insert points by batch process
//	input:point whitch need to insert to DB 
//	out:no
//	process:
//	1.pushback point
//	2.when the size of points >InsertBufferSize
//		1.get the series_id related to name and store into map
//		2.create the batch process sql query like:insert ignore into points (time, series_id, value, quality, confidence)
//		values(,,,,,),
//			     (,,,,,),
//				 ...
//				 (,,,,,);
//		3.execute
//  author:dhc
//	************************************************************************************/  
//        
//		 int countfirst =0;
//		 int countlast =0;
//		 int affected = 0;
//		 static long int batchprocess_num=0;
//		 static int countall=0;
//		 static bool f_once =false;
//		 recordpoint_map[id]=point;
//		 long int map_size= recordpoint_map.size();
//		 if (countall++>=Record_num)
//		 {
//			 if (!_mysqlCon||!f_once)// only need excute once unless we have disconnect did we must create again
//			 {
//				 this->checkConnection();
//				 char * fn="../../src/batchinsertpointsstring.txt";
//				 ostringstream buf;//将文件读入到ostringstream对象buf中
//				 char ch;
//				 string batchInsertfirst_str ="\r\n INSERT ignore INTO points (time, series_id, value, quality, confidence) Values ";
//				 string batchInsertlast_str = batchInsertfirst_str;
//				 string partone="(?,?,?,?,?),";
//				 string str="";
//				 ifstream ifile(fn);
//				 while(buf&&ifile.get(ch))buf.put(ch);
//				 batchfirst_num=map_size/BatchProcessSize;
//				 batchlast_num=map_size%BatchProcessSize;
//				 batchInsertfirst_str.reserve(PreAllocateSize);
//				 batchInsertlast_str.reserve(PreAllocateSize);
//				 str.reserve(PreAllocateSize);
//				 str= buf.str();//str="(?,?,?,?,?),(?,?,?,?,?),....."
//				 //string tempstring=(const string)(str) ;
//				 if (batchfirst_num)batchprocess_num=BatchProcessSize;
//				 else batchprocess_num=0;
//				 batchInsertfirst_str=batchInsertfirst_str.append(str.c_str(),0,batchprocess_num*strlen(partone.c_str()));
//				 batchInsertfirst_str=batchInsertfirst_str.substr(0,batchInsertfirst_str.length()-1);
//				 batchInsertfirst_str= batchInsertfirst_str.append(";");
//
//				 batchInsertlast_str=batchInsertlast_str.append(str.c_str(),0, batchlast_num*strlen(partone.c_str()));
//				 batchInsertlast_str=batchInsertlast_str.substr(0,batchInsertlast_str.length()-1);
//				 batchInsertlast_str=batchInsertlast_str.append(";");
//				 if (batchprocess_num>0){
//					 _Insertbatchfirst.reset(_mysqlCon->prepareStatement(batchInsertfirst_str));					 
//				 }
//				 if (batchlast_num>0){
//				 _Insertbatchlast.reset(_mysqlCon->prepareStatement(batchInsertlast_str));
//				 }
//			 } 
//			 f_once=true;
//			 map<string,Point>::const_iterator _iter=recordpoint_map.begin();
//		    // map<string,Point>::size_type BatchLength=BatchProcessSize;
//			// map<string,Point>::const_iterator _iterend=recordpoint_map.begin()+BatchLength;
//			 if (batchprocess_num>0) {
//				 for (int i=0;i<batchfirst_num;i++){
//					 if (checkConnection()) {
//						 countfirst=0;
//						 for (;;_iter++){
//							 _Insertbatchfirst->setInt(1+5*countfirst, (int)((_iter->second).time));
//							 // todo -- check this: _singleInsert->setUInt64(1, (uint64_t)time);
//							 _Insertbatchfirst->setInt64(2+5*countfirst, id_map[_iter->first]);
//							 _Insertbatchfirst->setDouble(3+5*countfirst,(_iter->second).value);
//							 _Insertbatchfirst->setInt(4+5*countfirst, (_iter->second).quality);
//							 _Insertbatchfirst->setDouble(5+5*countfirst, (_iter->second).confidence);
//							 countfirst++;
//							 if (countfirst>=batchprocess_num)
//							 {
//								 break;
//							 }
//						 }
//						 try {
//							 affected =_Insertbatchfirst->execute();
//						 } catch (...) {
//							 cout << "whoops!" << endl;
//						 }
//						 _mysqlCon->commit();
//					 }
//				 }
//			 }
//					if (batchlast_num>0){
//						if (checkConnection()) {
//							countlast=0;
//							map<string,Point>::size_type BatchLength=batchlast_num;
//
//							for (;;_iter++){
//								_Insertbatchlast->setInt(1+5*countlast, (int)((_iter->second).time));
//								// todo -- check this: _singleInsert->setUInt64(1, (uint64_t)time);
//								_Insertbatchlast->setInt64(2+5*countlast, id_map[_iter->first]);
//								_Insertbatchlast->setDouble(3+5*countlast, (_iter->second).value);
//								_Insertbatchlast->setInt(4+5*countlast, (_iter->second).quality);
//								_Insertbatchlast->setDouble(5+5*countlast,(_iter->second).confidence);			
//								countlast++;
//								if (countlast>=batchlast_num)
//								{
//									break;
//								}
//							}
//							try {
//								affected =_Insertbatchlast->execute();
//							} catch (...) {
//								cout << "whoops!" << endl;
//							}
//							_mysqlCon->commit();
//						} 	
//					}
//	countall=0;	
//	}
//	
// }
/*-------------------------------------------------------------
for sx netpipe next version of insertByBatchProcess :
pushback data waste:240s
write data to file waste:20s
load data to mysql waste:20s
dhc record at 2015-12-2    
*/

//void MysqlPointRecord::insertByBatchProcess(const std::string& id, Point point)
//{
//   	static clock_t pushbackstart,pushbackend,loaddatabegin,loaddatamid,loaddataend;
//	static int countall=0;
//	if(countall==0)pushbackstart=clock();
//	recordpoint_map[id]=point;
//	char *  _dataFilename="D:\\points.txt";
//	if (countall++>=Record_num)
//	{
//      pushbackend=clock();
//	  loaddatabegin=clock();
//	  printf("\n\tpushback data time wasted: %.2f 秒\n", (double)(pushbackend- pushbackstart)/CLOCKS_PER_SEC );
//		//table col type:time series_id value c quality
//		long int map_size= recordpoint_map.size();
//		FILE *pFile=fopen(_dataFilename,"w");
//		assert(pFile!=NULL);//如果打开文件失败就终止程序，检测pFile!=NULL是否为真，真就跳到下调语句，假就终止程序
//		//申请50MB的缓冲
//		char * const pBuffer=(char *)malloc(1024*1024*50);
//		assert(pBuffer!=NULL);
//		char * const pBufferEnd=pBuffer+1024*1024*50-100;
//		char * pCurrent=pBuffer;
//	    for(std::map <string,Point>::const_iterator _iter=recordpoint_map.begin();_iter!=recordpoint_map.end();_iter++){
//			
//			if(pCurrent>=pBufferEnd){//判断 创建的临时内部存储空间是否满了？
//
//				fwrite(pBuffer,pCurrent-pBuffer,1,pFile);//fwrite函数形参:数据地址、写入内容的单字节数、数据个数、目标文件指针
//				pCurrent=pBuffer;
//			}
//			pCurrent+=sprintf(pCurrent,"%d\t",(_iter->second).time);//sprintf函数形参:char 型号指针，指向将要写入的字符串的缓冲区、格式化字符串、可选参数，...可以是任意类型的数据;返回值：字符串长度
//			pCurrent+=sprintf(pCurrent,"%d\t",id_map[_iter->first]);
//			pCurrent+=sprintf(pCurrent,"%.12f\t",(_iter->second).value);
//			pCurrent+=sprintf(pCurrent,"%d\t",(_iter->second).quality);
//			pCurrent+=sprintf(pCurrent,"%d\t\n",(_iter->second).confidence);
//		}
//	   fwrite(pBuffer,pCurrent-pBuffer,1,pFile);//把数据写入到文件
//	   fclose(pFile);
//	   free(pBuffer);
//	  loaddatamid=clock();
//	  printf("\n\twrite data to file time wasted: %.2f 秒\n", (double)(loaddatamid- loaddatabegin)/CLOCKS_PER_SEC );
//
//		boost::shared_ptr<sql::Statement> StartTranctionStmt, _insertbyloaddataStmt;
//		StartTranctionStmt.reset(_mysqlCon->createStatement());
//		_insertbyloaddataStmt.reset( _mysqlCon->createStatement() );   
//		if (checkConnection()) {
//			int affected=1;
//			string insertbylodadata_str="LOAD DATA LOCAL INFILE '";
//			string tempdatafilename=_dataFilename;
//			string insertbylodadata_str2="' IGNORE INTO TABLE points;";
//            insertbylodadata_str.append( tempdatafilename).append( insertbylodadata_str2);
//		
//		try {
//			 affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
//			 affected =_insertbyloaddataStmt->executeUpdate(insertbylodadata_str);
//		} catch (...) {
//			cout << "whoops! Error happened when loading batch data into table points" << endl;
//		}
//		_mysqlCon->commit();
//		}
//		loaddataend=clock();
//		printf("\n\tload data to DB time wasted: %.2f 秒\n\n", (double)(loaddataend-loaddatamid)/CLOCKS_PER_SEC );
//	  countall=0;	
//
//	}
//
//}

/*---------------------------------------------------------------
compared to above batchprocess this method try to write data to ptr directly and overless
the process of pushing back data to record_map[],bcause it waste too much time

pushback data waste:16s
write data to file waste:0.2s
load data to mysql waste:8s
dhc record at 2015-12-2    
*/
void MysqlPointRecord::insertByBatchProcess(const std::string& id, Point point)
{  
	char *  _dataFilename="D:\\points.txt";//不要放在C盘，因为可能无权限，不要用相对路径，因为load data 时mysql不好找
	static int countall=0;
	static clock_t loaddatabegin_t=clock();
	static clock_t loaddatamid_t,loaddataend_t;
	static char * const pBuffer=(char *)malloc(1024*1024*30);//申请30MB的缓冲
	assert(pBuffer!=NULL);
	static char * const pBufferEnd=pBuffer+(1024*1024*30-100);
    assert(pBufferEnd!=NULL);
	static char * pCurrent=pBuffer;
	if(pCurrent>=pBufferEnd){//判断 创建的指针的内存是否满了,满了的话，直接写数据到文件
		FILE *pFile=NULL;
		pFile=fopen(_dataFilename,"w");
		fwrite(pBuffer,pCurrent-pBuffer,1,pFile);//fwrite函数形参:数据地址、写入内容的单字节数、数据个数、目标文件指针
		pCurrent=pBuffer;
		fclose(pFile);
		pFile=NULL;
	}
	pCurrent+=sprintf(pCurrent,"%d\t",point.time);
	pCurrent+=sprintf(pCurrent,"%d\t",id_map[id]);
	pCurrent+=sprintf(pCurrent,"%.12f\t",point.value);
	pCurrent+=sprintf(pCurrent,"%d\t",point.quality);
	pCurrent+=sprintf(pCurrent,"%d\t\n",point.confidence);
	if (f_CompleteSaveState){
		loaddatamid_t=clock();
		cout<<endl;
		cout<<"write data to ptr wasted: "<<(double)(loaddatamid_t- loaddatabegin_t)/CLOCKS_PER_SEC<<endl;
		loaddatabegin_t=clock();
		FILE *pFile=NULL;
		pFile=fopen(_dataFilename,"w");
		fwrite(pBuffer,pCurrent-pBuffer,1,pFile);//把数据写入到文件
		fclose(pFile);
		pFile=NULL;
		memset(pBuffer,0,sizeof(pBuffer));//清空上一次存的内容
		pCurrent=pBuffer;
		loaddatamid_t=clock();
		cout<<endl;
		cout<<"write data to file time wasted:"<<(double)(loaddatamid_t- loaddatabegin_t)/CLOCKS_PER_SEC<<endl;
		        this->checkConnection();
				boost::shared_ptr<sql::Statement> StartTranctionStmt, _insertbyloaddataStmt;
				StartTranctionStmt.reset(_mysqlCon->createStatement());
				_insertbyloaddataStmt.reset( _mysqlCon->createStatement() );   
				if (checkConnection()) {
					int affected=1;
					string insertbylodadata_str="LOAD DATA LOCAL INFILE '";
					string tempdatafilename=_dataFilename;
					string insertbylodadata_str2="' IGNORE INTO TABLE points;";
		            insertbylodadata_str.append( tempdatafilename).append( insertbylodadata_str2);
				
				try {
					 affected = StartTranctionStmt->executeUpdate(transactionstart);//start transaction，promote insert effeciency
					 affected =_insertbyloaddataStmt->executeUpdate(insertbylodadata_str);
				} catch (...) {
					cout << "whoops! Error happened when loading batch data into table points" << endl;
				}
				_mysqlCon->commit();
				}
		loaddataend_t=clock();
		cout<<endl;
		cout<<"load point data to mysql DB time wasted: "<<(double)(loaddataend_t-loaddatamid_t)/CLOCKS_PER_SEC<<endl ;
		countall=0;	
       loaddatabegin_t=clock();
	   f_CompleteSaveState=!f_CompleteSaveState;
 }

}

void MysqlPointRecord::insertSingle(const std::string& id, Point point) {
	if (checkConnection()) {
		insertSingleNoCommit(id, point);
		_mysqlCon->commit();
	}
}


void MysqlPointRecord::insertRange(const std::string& id, std::vector<Point> points) {
  
  // first get a list of times already stored here, so that we don't have any overlaps.
  vector<Point> existing;
  if (checkConnection()) {
    if (points.size() > 1) {
      existing = this->selectRange(id, points.front().time, points.back().time);
    }
  }
  
  vector<time_t> timeList;
  timeList.reserve(existing.size());
  BOOST_FOREACH(Point p, existing) {
    timeList.push_back(p.time);
  }
  
  // premature optimization is the root of all evil
  bool existingRange = (timeList.size() > 0)? true : false;
  
  if (checkConnection()) {
    BOOST_FOREACH(Point p, points) {
      if (existingRange && find(timeList.begin(), timeList.end(), p.time) != timeList.end()) {
        // have it already
        continue;
      }
      else {
        insertSingleNoCommit(id, p);
      }
    }
    _mysqlCon->commit();
  }
}

void MysqlPointRecord::insertSingleNoCommit(const std::string& id, Point point) {
	_singleInsert->setInt(1, (int)point.time);
  // todo -- check this: _singleInsert->setUInt64(1, (uint64_t)time);
  _singleInsert->setDouble(2, point.value);
  _singleInsert->setInt(3, point.quality);
  _singleInsert->setDouble(4, point.confidence);
  _singleInsert->setString(5, id);
  int affected = 0;
  try {
    affected = _singleInsert->execute();
  } catch (...) {
    cout << "whoops!" << endl;
  }
  if (affected == 0) {
    // this may happen if there's already data matching this time/id primary index.
    // the insert was ignored.
 // cerr << "zero rows inserted" << endl;//dhc enable
  }
   //_connection->commit();
}

void MysqlPointRecord::removeRecord(const string& id) {
  DB_PR_SUPER::reset(id);
  if (checkConnection()) {
    string removePoints = "delete p, m from points p inner join timeseries_meta m on p.series_id=m.series_id where m.name = \"" + id + "\"";
    boost::shared_ptr<sql::Statement> removePointsStmt;
    try {
      removePointsStmt.reset( _mysqlCon->createStatement() );
      removePointsStmt->executeUpdate(removePoints);
      _mysqlCon->commit();
    } catch (sql::SQLException &e) {
      handleException(e);
    }
  }
}

void MysqlPointRecord::truncate() {
	try {
    string truncatePoints = "TRUNCATE TABLE points";
    string truncateKeys = "TRUNCATE TABLE timeseries_meta";
    
    boost::shared_ptr<sql::Statement> truncatePointsStmt, truncateKeysStmt;
    
    truncatePointsStmt.reset( _mysqlCon->createStatement() );
    truncateKeysStmt.reset( _mysqlCon->createStatement() );
    
    truncatePointsStmt->executeUpdate(truncatePoints);
    truncateKeysStmt->executeUpdate(truncateKeys);//dhc disenable comment 2015-1-7 for testing sql resuat buffer clearing 
    
    _mysqlCon->commit();
  }
  catch (sql::SQLException &e) {
    handleException(e);
  }
}




#pragma mark - Private

// caution -- result will be freed here
std::vector<Point> MysqlPointRecord::pointsFromResultSet(boost::shared_ptr<sql::ResultSet> result) {
  std::vector<Point> points;
  while (result->next()) {
    time_t time = result->getInt("time");
    double value = result->getDouble("value");
    double confidence = result->getDouble("confidence");
    int quality = result->getInt("quality");
    Point::Qual_t qtype = Point::Qual_t(quality);
    Point point(time, value, qtype, confidence);
    points.push_back(point);
  }
  return points;
}


Point MysqlPointRecord::selectSingle(const string& id, time_t time, boost::shared_ptr<sql::PreparedStatement> statement) {
  //cout << "mysql single: " << id << " -- " << time << endl;
  Point point;
  if (!_connected) {
    if(!checkConnection()) {
      return Point();
    }
  }
  statement->setString(1, id);
  statement->setInt(2, (int)time);
  boost::shared_ptr<sql::ResultSet> results(statement->executeQuery());
  vector<Point> points = pointsFromResultSet(results);
  if (points.size() > 0) {
    point = points.front();
  }
  return point;
}

bool MysqlPointRecord::checkConnection() {

  if(!_mysqlCon) {
    cerr << "mysql connection was closed. attempting to reconnect." << endl;
    this->dbConnect();
  }
  if (_mysqlCon) {
    return !(_mysqlCon->isClosed());
  }
  else {
    return false;
  }
}

void MysqlPointRecord::handleException(sql::SQLException &e) {
  /*
   The MySQL Connector/C++ throws three different exceptions:
   
   - sql::MethodNotImplementedException (derived from sql::SQLException)
   - sql::InvalidArgumentException (derived from sql::SQLException)
   - sql::SQLException (derived from std::runtime_error)
   */
  
  cerr << endl << "# ERR: DbcException in " << __FILE__ << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
  /* Use what(), getErrorCode() and getSQLState() */
  cerr << "# ERR: " << e.what() << " (MySQL error code: " << e.getErrorCode() << ", SQLState: " << e.getSQLState() << " )" << endl;
  
  errorMessage = std::string(e.what());
  
  if (e.getErrorCode() == 1047) {
    /*
     Error: 1047 SQLSTATE: 08S01 (ER_UNKNOWN_COM_ERROR)
     Message: Unknown command
     */
    cerr << "# ERR: Your server seems not to support PS at all because its MYSQL <4.1" << endl;
  }
  cerr << "not ok" << endl;
}




bool MysqlPointRecord::supportsBoundedQueries() {
  return true;
}



#pragma mark - Protected

std::ostream& MysqlPointRecord::toStream(std::ostream &stream) {
  stream << "Mysql Point Record (" << _connectionInfo.db << ")" << endl;
  
  
  if (!_mysqlCon || _mysqlCon->isClosed()) {
    stream << "no connection" << endl;
    return stream;
  }
    
  sql::DatabaseMetaData *meta = _mysqlCon->getMetaData();
  
	stream << "\t" << meta->getDatabaseProductName() << " " << meta->getDatabaseProductVersion() << endl;
	stream << "\tUser: " << meta->getUserName() << endl;
  
	stream << "\tDriver: " << meta->getDriverName() << " v" << meta->getDriverVersion() << endl;
  
	stream << endl;
  return stream;
}
/*********************快速写数据到文件*****************************/
// lpszFilePath 文件名
// pData 浮点数数组
// iLength 浮点数个数
void MysqlPointRecord::writeData(const char * const lpszFilePath, const double * const pData, const int iLength)
{
	int iFileSize=0;
	FILE *pFile=fopen(lpszFilePath,"w");
	assert(pFile!=NULL);//如果打开文件失败就终止程序，检测pFile!=NULL是否为真，真就跳到下调语句，假就终止程序

	//申请200MB的缓冲
	char * const pBuffer=(char *)malloc(1024*1024*200);
	assert(pBuffer!=NULL);
	char * const pBufferEnd=pBuffer+1024*1024*200-100;
	char * pCurrent=pBuffer;

	for (int i=0;i<iLength;i++)
	{
		if(pCurrent>=pBufferEnd)//判断 创建的临时内部存储空间是否满了？
		{
			fwrite(pBuffer,pCurrent-pBuffer,1,pFile);//函数形参:数据地址、写入内容的单字节数、数据个数、目标文件指针
			pCurrent=pBuffer;
		}
		pCurrent+=sprintf(pCurrent,"%f",pData[i]);
		pCurrent[0]='\n';
		pCurrent++;
	}
	fwrite(pBuffer,pCurrent-pBuffer,1,pFile);//把数据写入到文件
	fclose(pFile);
	free(pBuffer);
}




