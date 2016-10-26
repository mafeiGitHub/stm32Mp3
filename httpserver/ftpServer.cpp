//{{{  includes
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "ftpServer.h"
#include "cmsis_os.h"

#include "cLcd.h"

#ifdef STM32F746G_DISCO
  #include "stm32746g_discovery.h"
  #include "stm32746g_discovery_ts.h"
  #include "stm32746g_discovery_audio.h"
  #include "stm32746g_discovery_sd.h"
#else
  #include "stm32f769i_discovery.h"
  #include "stm32f769i_discovery_ts.h"
  #include "stm32f769i_discovery_audio.h"
  #include "stm32f769i_discovery_sd.h"
#endif
#include "../fatfs/fatFs.h"
//}}}
//{{{  defines
#define FTP_VERSION              "FTP-2015-07-31"
#define FTP_USER                 "nnn"
#define FTP_PASS                 "nnn"

#define FTP_SERVER_PORT          21
#define FTP_DATA_PORT            55600         // Data port in passive mode
#define FTP_TIME_OUT             10            // Disconnect client after 5 minutes of inactivity
#define FTP_PARAM_SIZE           256 + 8
#define FTP_CWD_SIZE             256 + 8  // max size of a directory name

#define FTP_NBR_CLIENTS          5
#define FTP_BUF_SIZE             512
#define SERVER_THREAD_STACK_SIZE 256
#define FTP_THREAD_STACK_SIZE    (1600 + FTP_BUF_SIZE + (5 * 256))
#define FTP_THREAD_PRIORITY      (LOWPRIO + 2)
//}}}

class cFtpServer {
public:
  //{{{
  void service (int8_t n, struct netconn* ctrlcn) {

    strcpy (cwdName, "/");  // Set the root directory
    cwdRNFR[0] = 0;
    num = n;
    ctrlconn = ctrlcn;
    listdataconn = NULL;
    dataconn = NULL;
    dataPort = FTP_DATA_PORT + num;
    cmdStatus = 0;
    dataConnMode = NOTSET;

    //  Get the local and peer IP
    uint16_t dummy;
    netconn_addr (ctrlconn, &ipserver, &dummy);
    struct ip_addr ippeer;
    netconn_peer (ctrlconn, &ippeer, &dummy);

    sendBegin ("220---   Welcome to FTP Server!   ---\r\n");
    sendCat ("   ---  for ChibiOs & STM32-E407  ---\r\n");
    sendCat ("   ---   by Jean-Michel Gallego   ---\r\n");
    sendCat ("220 --   Version ");
    sendCat (FTP_VERSION);
    sendCatWrite ("   --");

    //  Wait for user commands, disconnect if FTP_TIME_OUT minutes of inactivity
    netconn_set_recvtimeout (ctrlconn, FTP_TIME_OUT * 60 * 1000);
    while (true) {
      int8_t err = readCommand();
      if (err == -4) // time out
        goto close;
      if (err < 0)
        goto close;
      if (!processCommand (command, parameters))
        goto bye;
      }

  bye:
    sendWrite ("221 Goodbye");

  close:
    dataClose();
    if (listdataconn != NULL) {
      netconn_close (listdataconn);
      netconn_delete (listdataconn);
      }
    }
  //}}}
  cFatFs* mFatFs = nullptr;

private:
  //{{{
  char* int2strZ (char* s, uint32_t i, int8_t z ) {

    char* psi = s + abs( z );

    * -- psi = 0;
    if (i == 0)
      * -- psi = '0';

    for (; i; i /= 10)
      * -- psi = '0' + i % 10;

    if (z < 0)
      while (psi > s)
        * -- psi = '0';

    return psi;
    }
  //}}}
  //{{{
  char* int2str (char* s, int32_t i, int8_t ls ) {

    if (i >= 0)
      return int2strZ (s, i, ls);

    char* pstr = int2strZ (s + 1, -i, ls - 1);
    * -- pstr = '-';

    return pstr;
    }
  //}}}
  //{{{
  char* i2str (int32_t i) {
    return int2str (str, i, 12);
    }
  //}}}

  //{{{
  void sendBegin (const char* s) {
    strncpy (buf, s, FTP_BUF_SIZE);
    }
  //}}}
  //{{{
  void sendCat (const char* s) {
    size_t len = FTP_BUF_SIZE - strlen (buf);
    strncat (buf, s, len);
    }
  //}}}
  //{{{
  void sendWrite (const char* s ) {
    sendBegin (s);
    sendWrite();
    }
  //}}}
  //{{{
  void sendCatWrite (const char* s) {
    sendCat (s);
    sendWrite();
    }
  //}}}
  //{{{
  void sendWrite() {

    if (strlen (buf ) + 2 < FTP_BUF_SIZE)
      strcat (buf, "\r\n");

    netconn_write (ctrlconn, buf, strlen(buf), NETCONN_COPY);
    }
  //}}}

  //{{{
  char* makeDateTimeStr (uint16_t date, uint16_t time) {
  // Create string YYYYMMDDHHMMSS from date and time

    int2strZ (str, ((date & 0xFE00 ) >> 9 ) + 1980, -5);
    int2strZ (str + 4, (date & 0x01E0 ) >> 5, -3);
    int2strZ (str + 6, date & 0x001F, -3);
    int2strZ (str + 8, (time & 0xF800 ) >> 11, -3);
    int2strZ (str + 10, (time & 0x07E0 ) >> 5, -3);
    int2strZ (str + 12, (time & 0x001F ) << 1, -3);
    return str;
    }
  //}}}
  //{{{
  int8_t getDateTime (uint16_t * pdate, uint16_t* ptime) {
  // Calculate date and time from first parameter sent by MDTM command (YYYYMMDDHHMMSS)
  // Date/time are expressed as a 14 digits long string
  //   terminated by a space and followed by name of file

    if (strlen (parameters) < 15 || parameters[14] != ' ' )
      return 0;

    for (uint8_t i = 0; i < 14; i++ )
      if (!isdigit (parameters[i]))
        return 0;

    parameters[14]  = 0;
    * ptime = atoi (parameters + 12) >> 1;       // seconds
    parameters[12] = 0;
    * ptime |= atoi (parameters + 10) << 5;      // minutes
    parameters[10] = 0;
    * ptime |= atoi (parameters + 8) << 11;      // hours
    parameters[8] = 0;
    * pdate = atoi (parameters + 6);             // days
    parameters[6] = 0;
    * pdate |= atoi (parameters + 4) << 5;       // months
    parameters[4] = 0;
    * pdate |= (atoi (parameters) - 1980 ) << 9; // years

    return 15;
    }
  //}}}

  //{{{
  int8_t readCommand() {
  // return: -4 time out
  //         -3 error receiving data
  //         -2 command line too long
  //         -1 syntax error
  //          0 command without parameters
  //          >0 length of parameters

    char* pbuf;
    uint16_t buflen;
    int8_t rc = 0;
    int8_t i;
    char car;

    command[0] = 0;
    parameters[0] = 0;
    nerr = netconn_recv (ctrlconn, & inbuf);
    if (nerr == ERR_TIMEOUT)
      return -4;
    if (nerr != ERR_OK)
      return -3;
    netbuf_data (inbuf, (void**)&pbuf, &buflen);
    if (buflen == 0)
      goto deletebuf;

    i = 0;
    car = pbuf[0];
    do {
      if (!isalpha (car))
        break;
      command[i++] = car;
      car = pbuf[i];
      }

    while (i < buflen && i < 4);
    command[i] = 0;
    if (car != ' ')
      goto deletebuf;

    do
      if (i > buflen + 2)
        goto deletebuf;
      while (pbuf[i++] == ' ');

    rc = i;
    do
      car = pbuf [rc++];
      while (car != '\n' && car != '\r' && rc < buflen);

    if (rc == buflen) {
      rc = -1;
      goto deletebuf;
      }

    if (rc - i - 1 >= FTP_PARAM_SIZE) {
      rc = -2;
      goto deletebuf;
      }

    strncpy (parameters, pbuf + i - 1, rc - i);
    parameters[rc - i] = 0;
    rc = rc - i;

    deletebuf:

    cLcd::debug ("rxcmd " + cLcd::dec (num) + " " + command + " " + parameters);

    netbuf_delete (inbuf);
    return rc;
    }
  //}}}
  //{{{
  bool listenDataConn() {

    bool ok = true;

    // If this is not already done, create the TCP connection handle to listen to client to open data connection
    if (listdataconn == NULL) {
      listdataconn = netconn_new (NETCONN_TCP);

      ok = listdataconn != NULL;
      if (ok) {
        // Bind listdataconn to port (FTP_DATA_PORT+num) with default IP address
        nerr = netconn_bind (listdataconn, IP_ADDR_ANY, dataPort );
        ok = nerr == ERR_OK;
        }

      if (ok) {
        // Put the connection into LISTEN state
        nerr = netconn_listen (listdataconn );
        ok = nerr == ERR_OK;
        }
      }

    return ok;
    }
  //}}}
  //{{{
  bool dataConnect() {

    nerr = ERR_CONN;

    if (dataConnMode == NOTSET)
      goto error;

    if (dataConnMode == PASSIVE) {
      if (listdataconn == NULL)
        goto error;

      // Wait for connection from client during 500 ms
      netconn_set_recvtimeout (listdataconn, 500);
      nerr = netconn_accept (listdataconn, & dataconn);
      if (nerr != ERR_OK)
        goto error;
      }

    else {
      //  Create a new TCP connection handle
      dataconn = netconn_new (NETCONN_TCP);
      if (dataconn == NULL)
        goto error;

      nerr = netconn_bind (dataconn, IP_ADDR_ANY, 0); //dataPort );
      //  Connect to data port with client IP address
      if (nerr != ERR_OK )
        goto delconn;
      nerr = netconn_connect (dataconn, & ipclient, dataPort);
      if (nerr != ERR_OK )
        goto delconn;
      }

    return true;

  delconn:
    if (dataconn != NULL) {
      netconn_delete (dataconn);
      dataconn = NULL;
      }

  error:
    sendWrite ("425 No data connection");
    return false;
    }
  //}}}
  //{{{
  void dataClose() {

    dataConnMode = NOTSET;
    if (dataconn == NULL)
      return;

    netconn_close (dataconn);
    netconn_delete (dataconn);

    dataconn = NULL;
    }
  //}}}
  //{{{
  void dataWrite (const char* data) {
    netconn_write (dataconn, data, strlen (data), NETCONN_COPY);
    }
  //}}}

  //{{{
  bool makePathFrom (char* fullName, char* param) {
  // Make complete path/name from cwdName and parameters
  // 3 possible cases:
  //   parameters can be absolute path, relative path or only the name
  // parameters:
  //   fullName : where to store the path/name
  // return:
  //   true, if done

    // Root or empty?
    if (!strcmp (param, "/") || strlen (param) == 0) {
      strcpy (fullName, "/");
      return true;
      }

    // If relative path, concatenate with current dir
    if (param[0] != '/') {
      strcpy( fullName, cwdName );
      if (fullName[strlen (fullName) - 1] != '/' )
        strncat (fullName, "/", FTP_CWD_SIZE);
      strncat (fullName, param, FTP_CWD_SIZE);
      }
    else
      strcpy (fullName, param);

    // If ends with '/', remove it
    uint16_t strl = strlen (fullName) - 1;
    if (fullName[strl] == '/' && strl > 1)
      fullName[strl] = 0;

    if (strlen (fullName) < FTP_CWD_SIZE)
      return true;

    sendWrite ("500 Command line too long");

    return false;
    }
  //}}}
  //{{{
  bool makePath (char* fullName) {
    return makePathFrom (fullName, parameters);
    }
  //}}}
  //{{{
  void closeTransfer() {

    if (bytesTransfered > 0) {
      sendBegin ("226-File successfully transferred\r\n");
      sendCat ("226 ");
      sendCat (" xxx ms, ");
      sendCatWrite (" bytes/s");
      }
    else
      sendWrite ("226 File successfully transferred");
    }
  //}}}

  //{{{
  bool fsExists (char* path) {
  // Return true if a file or directory exists
  // parameters:
  //   path : absolute name of file or directory

    if (!strcmp( path, "/"))
      return true;

    char* path0 = path;
    return mFatFs->stat (path0, mFileInfo) == FR_OK;
    }
  //}}}
  //{{{
  bool processCommand (char* command, char* parameters) {

    if (!strcmp( command, "PWD") || (!strcmp( command, "CWD") && !strcmp( parameters, ".")))  // 'CWD .' same as PWD
    //{{{  PWD - working directory
    {
    sendBegin ("257 \"");
    sendCat (cwdName);
    sendCatWrite ("\" is your current directory");
    }
    //}}}
    else if (!strcmp( command, "CWD"))
    //{{{  CWD - Change Working Directory
    {
    if( strlen( parameters ) == 0 )
      sendWrite( "501 No directory name");
    else if( makePath( path )) {
      if( fsExists( path )) {
        strcpy( cwdName, path );
        sendWrite( "250 Directory successfully changed.");
        }
      else
        sendWrite( "550 Failed to change directory.");
      }
    }
    //}}}
    else if (!strcmp( command, "CDUP"))
    //{{{  CDUP - Change to Parent Directory
    {
      bool ok = false;

      if( strlen( cwdName ) > 1 )  // do nothing if cwdName is root
      {
        // if cwdName ends with '/', remove it (must not append)
        if( cwdName[ strlen( cwdName ) - 1 ] == '/' )
          cwdName[ strlen( cwdName ) - 1 ] = 0;
        // search last '/'
        char * pSep = strrchr( cwdName, '/' );
        ok = pSep > cwdName;
        // if found, ends the string on its position
        if( ok )
        {
          * pSep = 0;
          ok = fsExists( cwdName );
        }
      }
      // if an error appends, move to root
      if (!ok )
        strcpy( cwdName, "/");
      sendBegin( "200 Ok. Current directory is ");
      sendCatWrite( cwdName );
    }
    //}}}
    else if (!strcmp( command, "DELE")) {
    //{{{  DELE - Delete a File
    if( strlen( parameters ) == 0 )
      sendWrite( "501 No file name");
    else if( makePath( path )) {
      if (!fsExists( path )) {
        sendBegin( "550 File ");
        sendCat( parameters );
        sendCatWrite( " not found");
        }
      else {
        uint8_t ffs_result = mFatFs->unlink (path);
        if( ffs_result == FR_OK ) {
          sendBegin( "250 Deleted ");
          sendCatWrite( parameters );
          }
        else {
          sendBegin( "450 Can't delete ");
          sendCatWrite( parameters );
          }
        }
      }
    }
    //}}}
    else if (!strcmp( command, "FEAT")) {
    //{{{  FEAT - New Features
    sendBegin ( "211-Extensions supported:\r\n");
    sendCat (" MDTM\r\n");
    sendCat (" MLSD\r\n");
    sendCat (" SIZE\r\n");
    sendCat (" SITE FREE\r\n");
    sendCatWrite ("211 End.");
    }
    //}}}
    else if (!strcmp( command, "LIST") || ! strcmp( command, "NLST"))
    //{{{  LIST and NLST - List
    {
      uint16_t nm = 0;
      cDirectory dir (cwdName);
      if (dir.getError()) {
        sendBegin( "550 Can't open directory ");
        sendCatWrite( cwdName );
        }
      else if( dataConnect()) {
        sendWrite( "150 Accepted data connection");
        for (;;) {
          if ((dir.findNext (mFileInfo) != FR_OK) || mFileInfo.mShortFileName[0] == 0)
            break;
          if (mFileInfo.mShortFileName[0] == '.')
            continue;

          if (!strcmp (command, "LIST")) {
            if (mFileInfo.mAttribute & AM_DIR)
              strcpy (buf, "+/");
            else {
              strcpy (buf, "+r,s");
              strcat (buf, i2str( mFileInfo.mFileSize));
              }
            strcat (buf, ",\t");
            strcat (buf, mFileInfo.mLongFileName[0] == 0 ? mFileInfo.mShortFileName : mFileInfo.mLongFileName);
            }
          else
            strcpy (buf, mFileInfo.mLongFileName[0] == 0 ? mFileInfo.mShortFileName : mFileInfo.mLongFileName);
          strcat ( buf, "\r\n");
          dataWrite (buf);
          nm ++;
          }

        sendWrite ("226 Directory send OK.");
        dataClose();
        }
      }
    //}}}
    else if (!strcmp( command, "MDTM"))
    //{{{  MDTM - File Modification Time RFC 3659
    {
      char * fname;
      uint16_t date, time;
      uint8_t gettime;

      gettime = getDateTime( & date, & time );
      fname = parameters + gettime;

      if( strlen( fname ) == 0 )
        sendWrite( "501 No file name");
      else if( makePathFrom( path, fname )) {
        if (!fsExists( path )) {
          sendBegin( "550 File ");
          sendCat( fname );
          sendCatWrite( " not found");
        }
        else if (gettime) {
          mFileInfo.mDate = date;
          mFileInfo.mTime = time;
          if (mFatFs->utime (path, mFileInfo) == FR_OK )
            sendWrite( "200 Ok");
          else
            sendWrite( "550 Unable to modify time");
        }
        else {
          sendBegin( "213 ");
          sendCatWrite( makeDateTimeStr( mFileInfo.mDate, mFileInfo.mTime ));
        }
      }
    }
    //}}}
    else if (!strcmp( command, "MLSD")) {
    //{{{  MLSD - Listing for Machine Processing RFC 3659
    uint16_t nm = 0;

    cDirectory dir (cwdName);
    if (dir.getError()) {
      sendBegin ("550 Can't open directory ");
      sendCatWrite (parameters);
      }

    else if (dataConnect()) {
      sendWrite( "150 Accepted data connection");
      while (dir.find (mFileInfo) == FR_OK && !mFileInfo.getEmpty()) {
        if (mFileInfo.mShortFileName[0] == '.')
          continue;
        strcpy (buf, "Type=");
        strcat (buf, mFileInfo.mAttribute & AM_DIR ? "dir" : "file");
        strcat (buf, ";Size=");
        strcat (buf, i2str (mFileInfo.mFileSize));
        if (mFileInfo.mDate != 0) {
          strcat( buf, ";Modify=");
          strcat( buf, makeDateTimeStr (mFileInfo.mDate, mFileInfo.mTime));
          }
        strcat (buf, "; ");
        strcat (buf, mFileInfo.mLongFileName[0] == 0 ? mFileInfo.mShortFileName : mFileInfo.mLongFileName);
        strcat (buf, "\r\n");
        dataWrite (buf);
        nm++;
        }

      sendBegin ("226-options: -a -l\r\n");
      sendCat ("226 ");
      sendCat (i2str (nm));
      sendCatWrite (" matches total");
      dataClose();
      }
    }
    //}}}
    else if (!strcmp( command, "MKD"))
    //{{{  MKD - Make Directory
    {
      if( strlen( parameters ) == 0 )
        sendWrite( "501 No directory name");
      else if( makePath (path)) {
        if (fsExists (path)) {
          sendBegin("521 \"");
          sendCat (parameters);
          sendCatWrite ("\" directory already exists");
        }
        else {
          //DEBUG_PRINT(  "Creating directory %s\r\n", parameters );
          uint8_t ffs_result = mFatFs->makeSubDirectory (path);

          //RTCDateTime timespec;
          //struct tm stm;
          //rtcGetTime( & RTCD1, & timespec );
          //rtcConvertDateTimeToStructTm( & timespec, & stm, NULL );
          //DEBUG_PRINT( "Date/Time: %04u/%02u/%02u %02u:%02u:%02u\r\n",
          //             stm.tm_year + 1900, stm.tm_mon + 1, stm.tm_mday,
          //             stm.tm_hour, stm.tm_min, stm.tm_sec );


          if (ffs_result == FR_OK) {
            sendBegin ("257 \"");
            sendCat (parameters);
            sendCatWrite ("\" created");
            }
          else {
            sendBegin ("550 Can't create \"");
            sendCat (parameters);
            sendCatWrite ("\"");
          }
        }
      }
    }
    //}}}
    else if (!strcmp( command, "MODE"))
    //{{{  MODE - Transfer Mode
    {
      if (!strcmp( parameters, "S"))
        sendWrite( "200 S Ok");
      // else if (!strcmp( parameters, "B"))
      //  sendWrite( "200 B Ok");
      else
        sendWrite( "504 Only S(tream) is suported");
    }
    //}}}
    else if (!strcmp( command, "NOOP"))
    //{{{  NOOP
    {
      sendWrite( "200 Zzz...");
    }
    //}}}
    else if (!strcmp( command, "PASV"))
    //{{{  PASV - Passive Connection management
    {
      if( listenDataConn())
      {
        dataClose();
        sendBegin( "227 Entering Passive Mode (");
        sendCat( i2str( ip4_addr1( & ipserver ))); sendCat( ",");
        sendCat( i2str( ip4_addr2( & ipserver ))); sendCat( ",");
        sendCat( i2str( ip4_addr3( & ipserver ))); sendCat( ",");
        sendCat( i2str( ip4_addr4( & ipserver ))); sendCat( ",");
        sendCat( i2str( dataPort >> 8 )); sendCat( ",");
        sendCat( i2str( dataPort & 255 )); sendCatWrite( ").");
        //DEBUG_PRINT( "Data port set to %U\r\n", dataPort );
        dataConnMode = PASSIVE;
      }
      else
      {
        sendWrite( "425 Can't set connection management to passive");
        dataConnMode = NOTSET;
      }
    }
    //}}}
    else if (!strcmp( command, "PORT"))
    //{{{  PORT - Data Port
    {
      uint8_t ip[4];
      uint8_t i;
      dataClose();
      // get IP of data client
      char * p = NULL;
      if( strlen( parameters ) > 0 )
      {
        p = parameters - 1;
        for( i = 0; i < 4 && p != NULL; i ++ )
        {
          ip[ i ] = atoi( ++ p );
          p = strchr( p, ',' );
        }
        // get port of data client
        if( i == 4 && p != NULL )
        {
          dataPort = 256 * atoi( ++ p );
          p = strchr( p, ',' );
          if( p != NULL )
            dataPort += atoi( ++ p );
        }
      }
      if( p == NULL )
      {
        sendWrite( "501 Can't interpret parameters");
        dataConnMode = NOTSET;
      }
      else
      {
        IP4_ADDR( & ipclient, ip[0], ip[1], ip[2], ip[3] );
        sendWrite( "200 PORT command successful");
        //DEBUG_PRINT( "Data IP set to %u:%u:%u:%u\r\n", ip[0], ip[1], ip[2], ip[3] );
        //DEBUG_PRINT( "Data port set to %U\r\n", dataPort );
        dataConnMode = ACTIVE;
      }
    }
    //}}}
    else if (!strcmp( command, "RETR"))
    //{{{  RETR - Retrieve
    {
      if (strlen (parameters) == 0 )
        sendWrite ( "501 No file name");
      else if (makePath (path)) {
        if (!fsExists (path)) {
          sendBegin ("550 File ");
          sendCat (parameters);
          sendCatWrite (" not found");
          }
        else {
          cFile file (path, FA_READ);
          if (file.getError() != FR_OK) {
            sendBegin( "450 Can't open ");
            sendCatWrite( parameters );
            }
          else if (dataConnect()) {
            int nb;

            //DEBUG_PRINT( "Sending %s\r\n", parameters );
            sendBegin ("150-Connected to port ");
            sendCat (i2str( dataPort));
            sendCat ("\r\n150 ");
            sendCat (i2str (file.getSize()));
            sendCatWrite (" bytes to download");
            bytesTransfered = 0;

            //DEBUG_PRINT( "Start transfert\r\n");
            while (file.read (buf, FTP_BUF_SIZE, nb) == FR_OK && nb > 0 ) {
              netconn_write( dataconn, buf, nb, NETCONN_COPY );
              bytesTransfered += nb;
              //DEBUG_PRINT( "Sent %u bytes\r", bytesTransfered );
              }
            //DEBUG_PRINT( "\n");
            closeTransfer();
            dataClose();
          }
        }
      }
    }
    //}}}
    else if (!strcmp( command, "RMD"))
    //{{{  RMD - Remove a Directory
    {
      if( strlen( parameters ) == 0 )
        sendWrite( "501 No directory name");
      else if( makePath( path ))
      {
        //DEBUG_PRINT(  "Deleting %s\r\n", path );
        if (!fsExists( path )) {
          sendBegin( "550 Directory \"");
          sendCat( parameters );
          sendCatWrite( "\" not found");
        }
        else if( mFatFs->unlink( path ) == FR_OK) {
          sendBegin( "250 \"");
          sendCat( parameters );
          sendCatWrite( "\" removed");
        }
        else {
          sendBegin( "501 Can't delete \"");
          sendCat( parameters );
          sendCatWrite( "\"");
        }
      }
    }
    //}}}
    else if (!strcmp( command, "RNFR"))
    //{{{  RNFR - Rename From
    {
      cwdRNFR[ 0 ] = 0;
      if( strlen( parameters ) == 0 )
        sendWrite( "501 No file name");
      else if( makePath( cwdRNFR )) {
        if (!fsExists( cwdRNFR )) {
          sendBegin( "550 File ");
          sendCat( parameters );
          sendCatWrite( " not found");
        }
        else
        { //DEBUG_PRINT( "Renaming %s\r\n", cwdRNFR );
          sendWrite( "350 RNFR accepted - file exists, ready for destination");
        }
      }
    }
    //}}}
    else if (!strcmp( command, "RNTO"))
    //{{{  RNTO - Rename To
    {
      char sdir[ FTP_CWD_SIZE ];
      if( strlen( cwdRNFR ) == 0 )
        sendWrite( "503 Need RNFR before RNTO");
      else if( strlen( parameters ) == 0 )
        sendWrite( "501 No file name");
      else if( makePath( path )) {
        if( fsExists( path )) {
          sendBegin( "553 ");
          sendCat( parameters );
          sendCatWrite( " already exists");
          }
        else {
          strcpy( sdir, path );
          char * psep = strrchr( sdir, '/' );
          bool fail = psep == NULL;
          if (!fail ) {
            if( psep == sdir )
              psep++;
            *psep = 0;
            fail = ! ( fsExists( sdir ) && ( mFileInfo.mAttribute & AM_DIR || ! strcmp( sdir, "/")));
            if( fail ) {
              sendBegin( "550 \"");
              sendCat( sdir );
              sendCatWrite( "\" is not directory");
              }
            else {
              if (mFatFs->rename (cwdRNFR, path ) == FR_OK)
                sendWrite( "250 File successfully renamed or moved");
              else
                fail = true;
              }
            }
          if (fail)
            sendWrite( "451 Rename/move failure");
          }
        }
      }
    //}}}
    else if (!strcmp( command, "SITE"))
    //{{{  SITE - System command
    {
      if (!strcmp( parameters, "FREE")) {
        //uint32_t free_clust;
        mFatFs->getFreeSectors();
        sendBegin( "211 ");
        //sendCat( i2str( free_clust * fs->csize >> 11 ));
        sendCat( i2str (100));
        sendCat( " MB free of ");
        //sendCat( i2str((fs->n_fatent - 2) * fs->csize >> 11 ));
        sendCat( i2str(1000));
        sendCatWrite( " MB capacity");
      }
      else {
        sendBegin( "500 Unknow SITE command ");
        sendCatWrite( parameters );
      }
    }
    //}}}
    else if (!strcmp( command, "SIZE")) {
    //{{{  SIZE - Size of the file
    if (strlen (parameters) == 0)
      sendWrite ("501 No file name");
    else if (makePath (path)) {
      if (!strcmp (path, "/"))
        sendWrite ( "550 No such file");
      else if ((mFatFs->stat (path, mFileInfo) == FR_OK) && !mFileInfo.isDirectory()) {
        sendBegin ("213 ");
        sendCatWrite (i2str (mFileInfo.mFileSize));
        }
      else
        sendWrite ( "550 No such file");
      }
    }
    //}}}
    else if (!strcmp( command, "STAT"))
    //{{{  STAT - Status command
    {
      sendBegin( "211-FTP server status\r\n");
      sendCat( " Local time is ");
      sendCat( "\r\n ");
      sendCat( " xx user(s) currently connected to up to ");
      sendCat( i2str( FTP_NBR_CLIENTS ));
      sendCat( "\r\n You will be disconnected after ");
      sendCat( i2str( FTP_TIME_OUT ));
      sendCat( " minutes of inactivity\r\n");
      sendCatWrite( "211 End.");
    }
    //}}}
    else if (!strcmp( command, "STOR"))
    //{{{  STOR - Store
      {
      if (strlen  (parameters) == 0)
        sendWrite ("501 No file name");
      else if (makePath (path)) {
        cFile file (path, FA_CREATE_ALWAYS | FA_WRITE );
        if (file.getError() != FR_OK ) {
          sendBegin( "451 Can't open/create ");
          sendCatWrite( parameters );
          }
        else if (dataConnect()) {
          struct pbuf* rcvbuf = NULL;
          void* prcvbuf;
          uint16_t buflen = 0;
          uint16_t off = 0;
          uint16_t copylen;
          int8_t  ferr = 0;
          UINT nb;

          sendBegin ("150 Connected to port ");
          sendCatWrite (i2str (dataPort));
          bytesTransfered = 0;
          do {
            nerr = netconn_recv_tcp_pbuf (dataconn, &rcvbuf);
            if (nerr != ERR_OK)
              break;
            prcvbuf = rcvbuf->payload;
            buflen = rcvbuf->tot_len;
            while (buflen > 0) {
              if (buflen <= FTP_BUF_SIZE - off)
                copylen = buflen;
              else
                copylen = FTP_BUF_SIZE - off;
              buflen -= copylen;
              memcpy (buf + off, prcvbuf, copylen);
              prcvbuf += copylen;
              off += copylen;
              if (off == FTP_BUF_SIZE) {
                if (ferr == 0 )
                  ferr = file.write (buf, FTP_BUF_SIZE, nb);
                off = 0;
                }
              bytesTransfered += copylen;
              }
            } while (true); // ferr == 0 );

          if (off > 0 && ferr == 0)
            ferr = file.write (buf, off, nb);

          if (nerr != ERR_CLSD) {
            sendBegin ("451 Requested action aborted: communication error ");
            sendCatWrite (i2str (abs(nerr)));
            }
          if (ferr != 0) {
            sendBegin ("451 Requested action aborted: file error ");
            sendCatWrite (i2str (abs (ferr)));
            }
          dataClose();
          closeTransfer();
          }
        }
      }
    //}}}
    else if (!strcmp( command, "STRU"))
    //{{{  STRU - File Structure
    {
      if (!strcmp( parameters, "F"))
        sendWrite( "200 F Ok");
      else
        sendWrite( "504 Only F(ile) is suported");
    }
    //}}}
    else if (!strcmp( command, "SYST")) {
      //{{{  SYST - command
      sendWrite ("215 WIN32 ftpdmin v. 0.96");
      }
      //}}}
    else if (!strcmp( command, "TYPE")) {
    //{{{  TYPE - Data Type
    if (!strcmp (parameters, "A"))
      sendWrite ("200 TYPE is now ASCII");
    else if (!strcmp (parameters, "I"))
      sendWrite ("200 TYPE is now 8-bit binary");
    else
      sendWrite ("504 Unknow TYPE");
    }
    //}}}
    else if (!strcmp( command, "USER")) {
      //{{{  USER - command
      //if (!strcmp (parameters, FTP_USER))
        sendWrite ("331 OK. Password required");
      //else
      //  sendWrite ("530 ");
      }
      //}}}
    else if (!strcmp( command, "PASS")) {
      //{{{  PASS - command
      //if (!strcmp (parameters, FTP_PASS))
        sendWrite ("230 OK.");
      //else
      //  sendWrite ("530 ");
      }
      //}}}
    else if (! strcmp( command, "QUIT"))
      return false;
    else
      sendWrite ("500 Unknow command");
    return true;
    }
  //}}}

  //{{{  private vars
  struct netconn* listdataconn;
  struct netconn* dataconn;
  struct netconn* ctrlconn;
  struct netbuf* inbuf;
  struct ip_addr ipclient;
  struct ip_addr ipserver;

  cFileInfo mFileInfo;

  uint16_t dataPort;
  int8_t cmdStatus;                 // status of ftp command connection

  char command [5];                 // command sent by client
  char parameters [FTP_PARAM_SIZE]; // parameters sent by client
  char cwdName [FTP_CWD_SIZE];      // name of current directory
  char cwdRNFR [FTP_CWD_SIZE];      // name of origin directory for Rename command
  char path [FTP_CWD_SIZE];
  char str [25];

  uint32_t bytesTransfered;
  int8_t nerr;
  uint8_t num;
  char buf [FTP_BUF_SIZE];           // data buffer for communication
  uint16_t pbuf;

  enum dcm_type { NOTSET  = 0, PASSIVE = 1, ACTIVE  = 2, };
  dcm_type dataConnMode;
  //}}}
  };

//{{{
static void ftpServerThread (void const* argument) {

  cLcd::debug ("ftpServerThread");

  BSP_SD_Init();
  if (BSP_SD_IsDetected() == SD_PRESENT)
    cLcd::debug ("ftpServer SD CARD");

  cFtpServer ftpServer;

  ftpServer.mFatFs = cFatFs::create();
  if (ftpServer.mFatFs->mount() != FR_OK) {
    // fatfs mount error, return
    cLcd::debug ("fatFs mount problem");
    osThreadTerminate (NULL);
    return;
    }
  cLcd::debug (ftpServer.mFatFs->getLabel() +
               " vsn:" + cLcd::hex (ftpServer.mFatFs->getVolumeSerialNumber()) +
               " freeSectors:" + cLcd::dec (ftpServer.mFatFs->getFreeSectors()));

  // Create the TCP connection handle
  struct netconn* ftpsrvconn = netconn_new (NETCONN_TCP);
  LWIP_ERROR("http_server: invalid ftpsrvconn", (ftpsrvconn != NULL), return;);

  // Bind to port 21 (FTP) with default IP address, put connection into LISTEN state
  netconn_bind (ftpsrvconn, NULL, FTP_SERVER_PORT);
  netconn_listen (ftpsrvconn);

  while (true) {
    struct netconn* ftpNetConn;
    if (netconn_accept (ftpsrvconn, &ftpNetConn) == ERR_OK) {
      ftpServer.service (1, ftpNetConn);
      netconn_delete (ftpNetConn);
      cLcd::debug ("ftpServer connection dropped");
      }
    }
  }
//}}}
//{{{
void ftpServerInit() {

  const osThreadDef_t osFtpThread = { (char*)"ftp", ftpServerThread, osPriorityNormal, 0, 16000 };
  osThreadCreate (&osFtpThread, NULL);
  }
//}}}
