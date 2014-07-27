//
//  TsdbPointRecord.cpp
//  epanet-rtx
//
//  Created by Sam Hatchett on 6/28/13.
//
//

#include "TsdbPointRecord.h"
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <sys/types.h>
//#include<Winsock2.h>//#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>//in windows it is windows.h
#include <errno.h>
#include <string.h>
#include <ws2tcpip.h>//include :windows.h��winsock2.h��ws2def.h 
//#include <netdb.h>
//#include <netinet/in.h>

using namespace std;
using namespace RTX;

void TsdbPointRecord::dbConnect() throw(RtxException) {
  // split the tokenized string. we're expecting something like "HOST=127.0.0.1;PORT=4242;"
  std::string tokenizedString = this->connectionString();
  if (RTX_STRINGS_ARE_EQUAL(tokenizedString, "")) {
    return;
  }
  
  // de-tokenize
  
  std::map<std::string, std::string> kvPairs;
  {
    boost::regex kvReg("([^=]+)=([^;]+);?"); // key - value pair
    boost::sregex_iterator it(tokenizedString.begin(), tokenizedString.end(), kvReg), end;
    for ( ; it != end; ++it) {
      kvPairs[(*it)[1]] = (*it)[2];
    }
    
    // if any of the keys are missing, just return.
    if (kvPairs.find("HOST") == kvPairs.end() ||
        kvPairs.find("PORT") == kvPairs.end() )
    {
      return;
    }
  }
  const std::string& host = kvPairs["HOST"];
  const std::string& port = kvPairs["PORT"];
  
  
  
   
 const int maxDataSize = 2560;//dhc-add "const"
  int numbytes;
  char buf[maxDataSize];
  struct addrinfo hints, *servinfo, *p;
  int rv;
  char s[INET6_ADDRSTRLEN];
  
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  
  
  
  if ((rv = getaddrinfo(host.c_str(), port.c_str(), &hints, &servinfo)) != 0) {
    cout << "getaddrinfo: " << gai_strerror(rv) << endl;
    return;
  }
  
  // loop through all the results and connect to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == INVALID_SOCKET) {//dhc-modify for windows
      perror("client: socket");
      continue;
    }
    
    if (connect(_sock, p->ai_addr, p->ai_addrlen) == -1) {
      closesocket(_sock);//dhc-modify for windows
      perror("client: connect");
      continue;
    }
    
    break;
  }
  
  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return ;
  }
  
  inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s, sizeof s);
  printf("client: connecting to %s\n", s);
  
  freeaddrinfo(servinfo); // all done with this structure
  
  if ((numbytes = recv(_sock, buf, maxDataSize-1, 0)) == -1) {//dhc modify
    perror("recv");
    exit(1);
  }
  
  buf[numbytes] = '\0';
  
  printf("client: received '%s'\n",buf);
  
  closesocket(_sock);//dhc modify
  
}



bool TsdbPointRecord::isConnected() {
  return (_sock !=INVALID_SOCKET);//dhc modify 
}

//dhc enable comment 
//std::vector<Point> TsdbPointRecord::selectRange(const std::string& id, time_t startTime, time_t endTime) {
//  
//}
//
//Point TsdbPointRecord::selectNext(const std::string& id, time_t time) {
//  
//}
//
//Point TsdbPointRecord::selectPrevious(const std::string& id, time_t time) {
//  
//}
//
//void TsdbPointRecord::insertSingle(const std::string& id, Point point) {
//  
//}
//
//void TsdbPointRecord::insertRange(const std::string& id, std::vector<Point> points) {
//  
//}
//dhc add function for modifyinfg error
void *get_in_addr( SOCKADDR *sa)
{
	//if(sa->sa_family==AF_INET)
	return &(((SOCKADDR_IN *)sa)->sin_addr);
	//return &(((struct sockaddr_in6 *)sa)->sin6_addr);

}
























