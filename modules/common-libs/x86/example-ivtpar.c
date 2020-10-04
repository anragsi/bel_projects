/********************************************************************************************
 *  example-ivtpar.c
 *
 *  created : 2020
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 03-October-2020
 *
 *  command-line example for a menue using ivtpar
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
 * Last update: 03-October-2020
 *********************************************************************************************/
#define EXAMPLE_X86_VERSION  "00.00.01"

// standard includes 
#include <unistd.h> // getopt
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// common example
#include <common-lib.h>                  // common API
#include <common-defs.h>                 // FW
#include <ivtpar.h>                      // ivtpar

const char*  program;

static void help(void) {
  fprintf(stderr, "Usage: %s \n", program);
  fprintf(stderr, "\n");
  fprintf(stderr, "Version %s. Licensed under the LGPL v3.\n", EXAMPLE_X86_VERSION);
} //help

int main(int argc, char** argv) {

  // CLI
  int opt, error = 0;
  int exitCode   = 0;

  // ivtpar
  int  i, l0, lchange[100];

  int  j;
  
  // local variables

  program = argv[0];    

  while ((opt = getopt(argc, argv, "h")) != -1) {
    switch (opt) {
    case 'h':
      help();
      return 0;
      case ':':
      case '?':
        error = 1;
      break;
    default:
      return 1;
    } // switch opt
  } // while opt

  if (error) {
    help();
    return 1;
  }

  i = ivtpar ("example-ivtpar.txt", "example-ivtpar.par", &l0, lchange);

  printf("ivtpar returned:\n");
  printf("-- menu item selected   : %d\n", i);
  if (l0) {
    printf("-- changes of parameters:\n");
    for (j=0; j<100; j++) if (lchange[j]) printf("-- parameter %d changed...\n", j);
  } // if l0

  return exitCode;
}
