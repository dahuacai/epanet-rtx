Building EPANET RTX C++ library from source code on Windows

March 12, 2013
Jinduan Chen, University of Cincinnati (jinduan.uc@gmail.com)

Notice:
* Currently only Windows 7 and Visual Studio 2012 (i.e., 11.0) toolchain is supported 
* Currently only 64-bit builds (both Debug and Release) are tested 

Prerequisites and environment setup:
(1) Download and unzip the Boost library (www.boost.org)
(2) Build the boost library via command line:
        .\bootstrap.bat 
        .\bjam --toolset=msvc-11.0 address-model=64 --build-type=complete
    these will build the whole boost library binary (64bit) into .\stage\lib

(3) Download and unzip the libconfig library (http://www.hyperrealm.com/libconfig/)
(4) Build the libconfig++ library with the Visual Studio .sln file (can be either static library or dll, 
    whatever you like)   

(5) Download and install MySQL database server (http://dev.mysql.com/downloads/installer/). Using the Mysql CE Workbench to create a database accont with name "db-rtx-agent" and password "db-rtx-agent" with DBA privillage.

(6) Download and install MySQL Connector C 64-bit MSI (http://dev.mysql.com/downloads/connector/c/). This will install header files and library to (C:\Program Files\MySQL\MySQL Connector C 6.0.2)

(7) Download the source code of MySQL Connector C++ (http://dev.mysql.com/downloads/connector/cpp). Make sure you download the source code, not the MSI installer (which has bugs)
(8) Download the project file generation tool CMake (http://cmake.org/cmake/resources/software.html), win32 installer.  
(9) Install CMake.
(10) Open CMake, in the UI, set the "source code" path to be the MySQL Connector C++ source code path. Select a proper binary path. Click configure. set generator to be Visual Studio 11.
(11) CMake asks for boost diretory, boost debug library directory (see step 2), mysql directory, mysql library directory (see Step 5). fill the blanks and click "generate". This will generator a project file for VS.
(12) Open the VS project file and build the MySQL Connector C++ (can be either static lib or dll). 


Building RTX library and the example:
        
(13) Open the .sln file in this directory, there are three projects.  epanet-rtx builds a static library of RTX.  timeseries-demo and validator are two examples.
(14) To build the RTX library, ensure the path to the header files of boost (Step 1), libconfig++ (Step 3), libmysql (Step 6), and mysqlcppconn (Step 7) exist in the project properties->C/C++->general->Additional include dirs. Build epanet-rtx will generate a static library epanet-rtx.lib
(15) To build the examples, ensure the Step (14) requirements of header file paths are met. Also ensure the path to the library files of boost (Step 2), libconfig++ (Step 4), libmysql (Step 6), mysqlcppconn (Step 12), and epanet-rtx (Step 14) exist in the project properties->linker->general->Additional library dirs. Also ensure the names of the libraries appear in project properties->linker->input->Additional dependencies. Build timeseries-demo and validator will generate executables.


Run the example:
(16) Make sure MySQL service is running (you can check this by typing services.msc in Start menu->Run...). 
(17) Copy dlls of boost (Step 2), libconfig++ (Step 4), libmysql (Step 6), mysqlcppconn (Step 12) into the path your executable resides. Then run.


Building RTX applications of your own:
(18) Make sure epanet-rtx.lib (Step 14) is generated.
(19) Create a project of your own. Change platform to "x64". Add include file dirs (see Step 14). Add epanet-rtx.lib to project properties->linker->input->Additional dependencies. Add epanet-rtx.lib's path to project properties->linker->general->Additional library dirs.
(20) Write your application and build.
(21) When running/deploying your application, the minimal set of required dlls are:
    boost_filesystem-vc110-mt-gd-1_53.dll   ("gd" for debug version, no "gd" for release version)
    boost_system-vc110-mt-gd-1_53.dll   
    boost_regex-vc110-mt-gd-1_53.dll   
    boost_math_tr1-vc110-mt-gd-1_53.dll   (These 4 come from Step 2)
    libconfig++.dll  (Step 4)
    libmysql.dll (Step 6)
    mysqlcppconn.dll (Step 12)







