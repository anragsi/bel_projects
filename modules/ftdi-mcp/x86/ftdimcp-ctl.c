/*******************************************************************************************
 *  ftdimcp-ctl.c
 *
 *  created : 2023
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 07-Mar-2024
 *
 *  command line program for MCP4725 connected via FT232H
 *
 * ------------------------------------------------------------------------------------------
 * License Agreement for this software:
 *
 * Copyright (C) 2013  Dietrich Beck
 * GSI Helmholtzzentrum für Schwerionenforschung GmbH
 * Planckstraße 1
 * D-64291 Darmstadt
 * Germany
 *
 * Contact: d.beck@gsi.de
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *  
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * For all questions and ideas contact: d.beck@gsi.de
 * Last update: 15-April-2019
 *********************************************************************************************/
// standard includes 
#include <unistd.h> // getopt
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

// ftdi mcp
#include <ftdimcp-lib.h>

// DIM
#ifdef USEDIM
#include <dis.h>

#define DIMCHARSIZE 32                   // standard size for char services
#define DIMMAXSIZE  1024                 // max size for service names

char      disName[DIMMAXSIZE];           // name of DIM server

char      disVersion[DIMCHARSIZE];       // version
uint32_t  disActOutput;                  // actual (non-stretched) comparator output
uint32_t  disTriggered;                  // value of 'stretched' comparator output
uint32_t  disNTrigger;                   // approximate number of comparator 'triggers'
double    disSetLevel;                   // actual comparator level
char      disHostname[DIMCHARSIZE];      // hostname of THIS PC
uint32_t  disDevice;                     // ID of USB device
char      disSerial[DIMCHARSIZE];        // serial string of FTDI device
char      disStatus[DIMCHARSIZE];        // status of server


uint32_t  disVersionId      = 0;
uint32_t  disActOutputId    = 0;                  
uint32_t  disTriggeredId    = 0;
uint32_t  disNTriggerId     = 0;
uint32_t  disSetLevelId     = 0;
uint32_t  disHostnameId     = 0;
uint32_t  disDeviceId       = 0;
uint32_t  disSerialId       = 0;
uint32_t  disStatusId       = 0;
uint32_t  disCmdLevelId     = 0;

#endif //USEDIM

// public variables
const char* program;
FT_HANDLE   cHandle;                     // handle to channel
uint32_t    dacAddr;                     // I2C address of DAC
int         flagBlink;
uint32_t    deviceID;                    // USB device ID
char        deviceSerial[32];            // device serial 


static void die(const char* what) {
  fprintf(stderr, "%s: failed: %s\n", program, what);
  exit(1);
} //die

static void help(void) {
  fprintf(stderr, "Usage: %s <port number> [OPTION]\n", program);
  fprintf(stderr, "\n");
  fprintf(stderr, "  -h                  display this help and exit\n");
  fprintf(stderr, "  -e                  display version\n");
  fprintf(stderr, "  -i                  display information on ftdi module connected via USB\n");
  fprintf(stderr, "  -a <addr>           I2C address of DAC\n");  
  fprintf(stderr, "  -l <level>          set level of DAC [%%]\n");
  fprintf(stderr, "  -o                  get actual state of comparator output\n");
  fprintf(stderr, "  -s                  get 'stretched' state of comparator output\n");
  fprintf(stderr, "  -d <name prefix>    daemonize as DIM server\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Tip: This tool requires libftd2xx.so and libmpsse.so to communicate with the device.\n");
  fprintf(stderr, "     The device must not use 'usbserial' and 'ftdi_sio' kernel drivers.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Example1: '%s 0 -i -l10 -o\n", program);
  fprintf(stderr, "\n");
  fprintf(stderr, "Report software bugs to <d.beck@gsi.de>\n");
  fprintf(stderr, "Version %06x. Licensed under the LGPL v3.\n", FTDIMCP_LIB_VERSION);
} //help


// set comparator level; level [%]
FT_STATUS setLevel(double level)
{
  FT_STATUS ftStatus;

  ftStatus =  ftdimcp_setLevel(cHandle, dacAddr, level, 0, 1);
#ifdef USEDIM
  if (ftStatus == FT_OK) {
    disSetLevel = level;
    dis_update_service(disSetLevelId);
  } // if ftStatus
#endif

  return ftStatus;
} // setLevel


// set comparator level via DIM command
void cmdSetLevel(long *tag, char *cmnd_buffer, int *size)
{
#ifdef USEDIM
  double    level;

  if (cHandle == NULL) {
    printf("%s: invalid FTDI channel\n", program);
    return;
  } // if cHandle
  
  level    = *((double *)cmnd_buffer);

  // set value
  setLevel(level);

  flagBlink = 1;
#endif
} // cmdSetLevel


// add all dim services
void disAddServices(char *prefix)
{
#ifdef USEDIM

  char name[DIMMAXSIZE];
  
  sprintf(name, "%s_version", prefix);
  disVersionId   = dis_add_service(name, "C", disVersion, 8, 0 , 0);

  sprintf(name, "%s_actoutput", prefix);
  disActOutputId = dis_add_service(name, "I:1", &disActOutput, sizeof(disActOutput), 0, 0);

  sprintf(name, "%s_triggered", prefix);
  disTriggeredId = dis_add_service(name, "I:1", &disTriggered, sizeof(disTriggered), 0, 0);

  sprintf(name, "%s_ntrigger", prefix);
  disNTriggerId  = dis_add_service(name, "I:1", &disNTrigger, sizeof(disNTrigger),  0, 0);
  
  sprintf(name, "%s_setlevel", prefix);
  disSetLevelId  = dis_add_service(name, "D:1", &disSetLevel, sizeof(disSetLevel),  0, 0);

  sprintf(name, "%s_hostname", prefix);
  disHostnameId  = dis_add_service(name, "C", disHostname, DIMCHARSIZE, 0, 0);

  sprintf(name, "%s_deviceid", prefix);
  disDeviceId    = dis_add_service(name, "I:1", &disDevice, sizeof(disDevice),  0, 0);

  sprintf(name, "%s_serial", prefix);
  disSerialId    = dis_add_service(name, "C", disSerial, DIMCHARSIZE, 0, 0);

  sprintf(name,  "%s_status", prefix);
  disStatusId    = dis_add_service(name, "C", disStatus, DIMCHARSIZE, 0, 0);  

  sprintf(name, "%s_cmd_setlevel", prefix);
  disCmdLevelId  =  dis_add_cmnd(name, "D:1", cmdSetLevel, 0);
  
#endif //USEDIM
} // dimAddServices


//main
int main(int argc, char** argv) {
  const char* command;
  int         opt, error = 0;
  int         exitCode   = 0;
  char        *tail;
  char        *p;

  int        getVersion    = 0;
  int        getInfo       = 0;
  int        getOutAct     = 0;
  int        getOutStretch = 0;
  int        setDac        = 0;
  int        daemon        = 0;
  int        flagOk        = 1;


  FT_STATUS  ftStatus;                    // status returned by FTDI library calls
  FT_STATUS  ftStatusOld;                 // previous status returned by FTDI library calls
  int        cIdx;                        // index of channel; there can be more than one FTDI channel connected
  uint32_t   verLibFtdi;                  // version of ftdi library (required by mpsse library)
  uint32_t   verLibMpsse;                 // version of mpsse library
  uint32_t   verLibFtdiMcp;               // version of ftdimcp library
  uint32_t   outAct;                      // value of actual comparator output
  uint32_t   outActOld;                   // previous value of actual comparator output
  uint32_t   outStretched;                // value of stretched comparator output
  uint32_t   outStretchedOld;             // previous value of stretched comparator output
  uint32_t   blinkTime_us;                // duration of blink [us]
  double     dacLevel      = 0;           // level of DAC [%]

  char       prefix[1024];                // prefix for DIM services

  program      = argv[0];
  blinkTime_us = FTDIMCP_POLLINTERVAL_US;
  cHandle      = NULL;
  dacAddr      = FTDIMCP_I2CADDR;
  sprintf(prefix, "%s", "na");

  while ((opt = getopt(argc, argv, "l:d:a:oseih")) != -1) {
    switch (opt) {
      case 'e':
        getVersion = 1;
        break;
      case 'i':
        getInfo = 1;
        break;
      case 'a':
        dacAddr = strtod(optarg, &tail);
        break;
      case 'l':
        setDac = 1;
        dacLevel = strtod(optarg, &tail);
        if (*tail != 0) {
          fprintf(stderr, "Specify a proper number, not '%s'!\n", optarg);
          exit(1);
        } // if tail 
        if ((dacLevel < 0.0) || (dacLevel > 100.0)) {
          fprintf(stderr, "value must be within 0..100, not %f!\n", dacLevel);
          exit(1);
        } // if dacLevel
        break;
      case 'd':
        daemon = 1;
        sprintf(prefix, "%s", optarg);
        break;
      case 'o':
        getOutAct = 1;
        break;
      case 's':
        getOutStretch = 1;
        break;
      case 'h':
        help();
        return 0;
      case ':':
      case '?':
        error = 1;
        break;
      default:
        fprintf(stderr, "%s: bad getopt result\n", program);
      return 1;
    } /* switch opt */
  } /* while opt */

  if (error) {
    help();
    return 1;
  }
  
  if (optind >= argc) {
    fprintf(stderr, "%s: expecting one non-optional argument: <port number>\n", program);
    fprintf(stderr, "\n");
    help();
    return 1;
  } // if optint

  cIdx = strtol(argv[optind], &p, 10);

  // start DIM server
#ifdef USEDIM
  if (daemon) {
    sprintf(disStatus, "%s", "OK");
    gethostname(disHostname, 32);
    sprintf(disName, "N/A");
    sprintf(disVersion, "N/A");
    sprintf(disSerial, "N/A");
    disDevice       = 0x0;
    disNTrigger     = 0;
    disSetLevel     = NAN;
    
    printf("%s: starting server using prefix %s\n", program, prefix);
  
    // add services, update 'constant' services
    disAddServices(prefix);
  
    sprintf(disName, "%s", prefix);
    dis_start_serving(disName);
  
    sprintf(disVersion, "%06x", FTDIMCP_LIB_VERSION);
    dis_update_service(disVersionId);
    dis_update_service(disStatusId);
    dis_update_service(disHostnameId);

    ftdimcp_info(cIdx, &disDevice, disSerial, 0);
    dis_update_service(disSerialId);
    dis_update_service(disDeviceId);
  } // if daemon
#endif


  if (optind+1 < argc)  command = argv[++optind];
  else command = NULL;

  if (getVersion) {
    ftdimcp_getVersion(&verLibFtdiMcp, &verLibFtdi, &verLibMpsse);

    printf("ftdi-mcp library version 0x%06x\n", verLibFtdiMcp); 
    printf("ftdi     library version 0x%06x\n", verLibFtdi);
    printf("mpsse    library version 0x%06x\n", verLibMpsse);
  } // if getVersion

  // open, init

  if ((ftStatus   = ftdimcp_open(cIdx, &cHandle, 1)) != FT_OK) {
    
    if (daemon) {
#ifdef USEDIM
    sprintf(disStatus, "%s", "device not found");
#endif
    } // if daemon
    else         die("can't open connection to FTDI channel");
    flagOk = 0;
  } // if ftStatus

  if (flagOk) {
    if ((ftStatus   = ftdimcp_init(cHandle)) != FT_OK) {
      if (daemon) {
#ifdef USEDIM
        sprintf(disStatus, "%s", "can't init channel");
#endif
      } // if daemon
      else         die("can't init channel");
      flagOk = 0;
    } // if ftStatus
  } // if flagOk
  
  // info    
  if (getInfo) {
    if ((ftStatus = ftdimcp_info(cIdx, &deviceID, deviceSerial, 1)) != FT_OK) die("can't get info on FTDI channel");
#ifdef USEDIM
    disDevice = deviceID;
    sprintf(disSerial, "%s", deviceSerial);
#endif
    
  } // if getInfo

  if (setDac) {
    // activity LED -> ON
    ftdimpc_setLed(cHandle, 1);

    // write to DAC
    if ((ftStatus = setLevel(dacLevel)) != FT_OK) die ("can't write to DAC");

    // activity LED -> OFF
    sleep(1);
    ftdimpc_setLed(cHandle, 0);
  } // if setDac

  if (getOutAct) {
    if ((ftStatus = ftdimpc_getCompOutAct(cHandle, &outAct)) != FT_OK) die("can't read GPIO");
    if (outAct) printf("output is HIGH\n");
    else        printf("output is LOW\n");
  } // if getOutput

  if (getOutStretch) {
    if ((ftStatus = ftdimpc_getCompOutStretched(cHandle, &outAct)) != FT_OK) die("can't read GPIO");
    if (outAct) printf("output was HIGH\n");
    else        printf("output was LOW\n");
  } // if getOutput
    
  if (daemon) {
#ifdef USEDIM
    outAct          = 0;
    outActOld       = 0;
    outStretched    = 0;
    outStretchedOld = 0;
    ftStatus        = FT_OK;
    ftStatusOld     = FT_OK;
    flagBlink       = 0;
   
          
    while (1) {
      if (flagOk) {
        // get values
        ftStatus = ftdimpc_getCompOutStretched(cHandle, &outStretched);
        if (ftStatus != ftStatusOld) {
          if (ftStatus != FT_OK) sprintf(disStatus, "%s", "can't read from device");
          else                   sprintf(disStatus, "%s", "OK");
          dis_update_service(disStatusId);
        } // if ftStatus
        
        ftdimpc_getCompOutAct(cHandle, &outAct);
        
        // stretched value change
        if (outStretched != outStretchedOld) {
          disTriggered = outStretched;
          dis_update_service(disTriggeredId);
        } // if outStretched
        
        // act value change
        if (outAct != outActOld) {
          disActOutput = outAct;
          dis_update_service(disActOutputId);
        } // if outAct
        
        // triggered?
        if (outStretched > outStretchedOld) {
          disNTrigger++;
          dis_update_service(disNTriggerId);
          flagBlink = 1;
        } // if outStretched
        
        outStretchedOld = outStretched;
        outActOld       = outAct;

        // blink on changes 
        if (flagBlink) {
          ftdimpc_setLed(cHandle, 1);
          usleep(blinkTime_us);
          ftdimpc_setLed(cHandle, 0);
          flagBlink = 0;
        } // flagBlink
        else usleep(blinkTime_us);
      } // if flagOkd
      else sleep(1);   // keep server publishing its 'bad status' alive
    } // while
#endif // USEDIM
  } // if daemon


  // dummy
  if (command) {
  } //if command
  
  // close connection ...
  ftdimcp_close(cHandle);

  return exitCode;
} // main
