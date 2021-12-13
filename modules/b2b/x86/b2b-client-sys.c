/*******************************************************************************************
 *  b2b-client-sys.c
 *
 *  created : 2021
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 22-Nov-2021
 *
 * subscribes to and displays status of a b2b system (CBU, PM, KD ...)
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
#define B2B_CLIENT_SYS_VERSION 0x000311

// standard includes 
#include <unistd.h> // getopt
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

// dim
#include <dic.h>

// b2b
#include <common-lib.h>                  // COMMON
#include <b2blib.h>                      // API
#include <b2b.h>                         // FW

const char* program;

#define B2BNSYS     16                   // number of B2B systems

#define DIMCHARSIZE 32                   // standard size for char services
#define DIMMAXSIZE  1024                 // max size for service names
#define SCREENWIDTH 1024                 // width of screen

uint32_t no_link_32    = 0xdeadbeef;
uint64_t no_link_64    = 0xdeadbeefce420651;
char     no_link_str[] = "NO_LINK";

char    disB2bPrefix[DIMMAXSIZE];

char     title[SCREENWIDTH+1];                              // title line to be printed
char     footer[SCREENWIDTH+1];                             // footer line to be printed
char     header[SCREENWIDTH+1];                             // header line to be printed

const char * sysShortNames[] = {
  "sis18-cbu",
  "sis18-pm",
  "sis18-kde",
  "sis18-raw",
  "sis18-cal",
  "esr-cbu",
  "esr-pm",
  "esr-kdx",
  "esr-raw",
  "esr-cal",
  "yr-cbu",
  "yr-pm",
  "yr-kdi",
  "yr-kde",
  "yr-raw",
  "yr-cal"
};

const char * ringNames[] = {
  " SIS18",
  " SIS18",
  " SIS18",
  " SIS18",
  " SIS18",
  "   ESR",
  "   ESR",
  "   ESR",
  "   ESR",
  "   ESR",
  "    YR",
  "    YR",
  "    YR",
  "    YR",
  "    YR",
  "    YR"
};

const char * typeNames[] = {
  "CBU",
  " PM",
  "KDE",
  "DAQ",
  "CAL",
  "CBU",
  " PM",
  "KDX",
  "DAQ",
  "CAL",
  "CBU",
  " PM",
  "KDI",
  "KDE",
  "DAQ",
  "CAL"
};

struct b2bSystem_t {
  char      version[DIMCHARSIZE];
  char      state[DIMCHARSIZE];
  char      hostname[DIMCHARSIZE];
  uint64_t  status;
  uint32_t  nTransfer;

  uint32_t  versionId;
  uint32_t  stateId;
  uint32_t  hostnameId;
  uint32_t  statusId;
  uint32_t  nTransferId;
}; // struct b2bSystem

struct b2bSystem_t dicSystem[B2BNSYS];


// help
static void help(void) {
  fprintf(stderr, "Usage: %s [OPTION] [PREFIX]\n", program);
  fprintf(stderr, "\n");
  fprintf(stderr, "  -h                  display this help and exit\n");
  fprintf(stderr, "  -e                  display version\n");
  fprintf(stderr, "  -s                  subscribe and display system info\n");
  fprintf(stderr, "  -o                  print info only once and exit (useful with '-s')\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Use this tool to display system information on the B2B system\n");
  fprintf(stderr, "Example1: '%s -s pro'\n", program);
  fprintf(stderr, "\n");
  fprintf(stderr, "Report software bugs to <d.beck@gsi.de>\n");
  fprintf(stderr, "Version %s. Licensed under the LGPL v3.\n", b2b_version_text(B2B_CLIENT_SYS_VERSION));
} //help


// add all dim services
void dicSubscribeServices(char *prefix)
{
  char name[DIMMAXSIZE];
  int  i;

  for (i=0; i<B2BNSYS; i++) {
    sprintf(name, "%s_%s_version_fw", prefix, sysShortNames[i]);
    dicSystem[i].versionId   = dic_info_service(name, MONITORED, 0, (dicSystem[i].version), 8, 0, 0, no_link_str, strlen(no_link_str));
    sprintf(name, "%s_%s_state", prefix, sysShortNames[i]);
    dicSystem[i].stateId     = dic_info_service(name, MONITORED, 0, (dicSystem[i].state), 10, 0, 0, no_link_str, strlen(no_link_str));
    sprintf(name, "%s_%s_hostname", prefix, sysShortNames[i]);
    dicSystem[i].hostnameId  = dic_info_service(name, MONITORED, 0, (dicSystem[i].hostname), 32, 0, 0, no_link_str, strlen(no_link_str));
    sprintf(name, "%s_%s_status", prefix, sysShortNames[i]);
    dicSystem[i].statusId    = dic_info_service(name, MONITORED, 0, &(dicSystem[i].status), sizeof(uint64_t), 0, 0, &no_link_64, sizeof(no_link_64));
    sprintf(name, "%s_%s_ntransfer", prefix, sysShortNames[i]);
    dicSystem[i].nTransferId = dic_info_service(name, MONITORED, 0, &(dicSystem[i].nTransfer), sizeof(dicSystem[i].nTransferId), 0, 0, &no_link_32, sizeof(no_link_32));
  } // for i
} // dicSubscribeServices


// send 'clear diag' command to server
void dicCmdClearDiag(char *prefix, uint32_t indexServer)
{
  char name[DIMMAXSIZE];

  sprintf(name, "%s_%s_cmd_cleardiag", prefix, sysShortNames[indexServer]);
  dic_cmnd_service(name, 0, 0);
} // dicCmdClearDiag


// print services to screen
void printServices(int flagOnce)
{
  int i;

  char   cTransfer[10];
  char   cStatus[17];
  char   buff[100];
  time_t time_date;              

  //printf("12345678901234567890123456789012345678901234567890123456789012345678901234567890\n");
  
  time_date = time(0);
  strftime(buff,50,"%d-%b-%y %H:%M",localtime(&time_date));
  sprintf(title, "\033[7m B2B System Status --------------------------------------------------- v%8s\033[0m", b2b_version_text(B2B_CLIENT_SYS_VERSION));
  sprintf(header, "  #   ring sys  version     state  transfers           status               node");
  sprintf(footer, "\033[7m exit <q> | clear status <digit> | print status <s>              %s\033[0m", buff);
  
  comlib_term_curpos(1,1);
  
  if (!flagOnce) printf("%s\n", title);
  printf("%s\n", header);

  for (i=0; i<B2BNSYS; i++) {
    if (dicSystem[i].nTransfer == no_link_32) sprintf(cTransfer, "%9s",         no_link_str);
    else                                      sprintf(cTransfer, "%9u",         dicSystem[i].nTransfer);
    if (dicSystem[i].status    == no_link_64) sprintf(cStatus,  "%16s",         no_link_str);
    else                                      sprintf(cStatus,   "%16"PRIx64"", dicSystem[i].status);
    printf(" %2d %6s %3s %8s %10s %9s %16s %18s\n", i, ringNames[i], typeNames[i], dicSystem[i].version, dicSystem[i].state, cTransfer, cStatus, dicSystem[i].hostname);
  } // for i
  
  if (!flagOnce) printf("\n\n\n%s\n", footer);
} // printServices


// print status text to screen
void printStatusText()
{
  int i,j;
  uint64_t status;

  for (i=0; i<B2BNSYS; i++) {
    status = dicSystem[i].status;
    if ((status != 0x1) && (status != no_link_64)) {
      printf(" %6s %3s:\n", ringNames[i], typeNames[i]);
      for (j = COMMON_STATUS_OK + 1; j<(int)(sizeof(status)*8); j++) {
        if ((status >> j) & 0x1)  printf("  ---------- status bit is set : %s\n", b2b_status_text(j));
      } // for j
    } // if status
  } // for i
  printf("press any key to continue\n");
  while (!comlib_term_getChar()) {usleep(200000);}
} // printStatusText


int main(int argc, char** argv) {
  int opt, error = 0;
  int exitCode   = 0;
  //  char *tail;

  int      getVersion;
  int      subscribe;
  int      once;

  char     userInput;
  int      sysId;
  int      quit;

  char     prefix[DIMMAXSIZE];

  program    = argv[0];
  getVersion = 0;
  subscribe  = 0;
  once       = 0;
  quit       = 0;

  while ((opt = getopt(argc, argv, "seho")) != -1) {
    switch (opt) {
      case 'e':
        getVersion = 1;
        break;
      case 's':
        subscribe = 1;
        break;
      case 'o':
        once = 1;
        break;
      case 'h':
        help();
        return 0;
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

  // no parameters, no command: just display help and exit
  if ((optind == 1) && (argc == 1)) {
    help();
    return 0;
  } // if optind

  if (optind< argc) sprintf(prefix, "b2b_%s", argv[optind]);
  else              sprintf(prefix, "b2b");

  comlib_term_clear();    

  if (getVersion) printf("%s: version %s\n", program, b2b_version_text(B2B_CLIENT_SYS_VERSION));

  if (subscribe) {
    printf("b2b-client-sys: starting client using prefix %s\n", prefix);

    dicSubscribeServices(prefix);

    while (!quit) {
      if (once) {sleep(1); quit=1;}                 // wait a bit to get the values
      printServices(once);
      if (!quit) {
        sysId = 0xffff;
        userInput = comlib_term_getChar();
        switch (userInput) {
          case 'a' ... 'f' :
            sysId = userInput - 87;                 // no break on purpose
          case '0' ... '9' :
            if (sysId == 0xffff) sysId = userInput - 48; // ugly
            dicCmdClearDiag(prefix, sysId);
            break;
          case 'q'         :
            quit = 1;
            break;
          case 's'         :
            printStatusText();
            break;
          default          :
            usleep(1000000);
        } // switch
      } // if !once
    } // while
  } // if subscribe

  return exitCode;
}
