//
//  This file is part of Buzzer-Deamon project
//  Copyright (C)2020 Jens Daniel Schlachter <osw.schlachter@mailbox.org>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

/** Notes: *************************************************************************** 

Read log with: sudo tail -f /var/log/daemon.log -n 10

*************************************************************************************/


/** Global Includes: ****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include "daemon.h"
#include "client.h"

/** Forward Declarations: ***********************************************************/

void ShowHelp();

/** Main-Function: ******************************************************************/

int main (int argc, char **argv) {
    /** Variables:                                                                  */
    int iResult;
    /** Check, if the deamon shall be started:                                      */
    if (argc == 1) {
        if (CheckSocket()) {
            printf ("ERR: Deamon already running!\n");
            return -1;
        }
        iResult = RunDemon();
        if (iResult == 0) {
            return (EXIT_SUCCESS);
        }
        if (iResult == -1) {
            ShowHelp();
            return -1;
        }
        return iResult; 
    }else{
        iResult = RunClient(argc, argv);
        if (iResult == 0) {
            return (EXIT_SUCCESS);
        }
        if (iResult == -1) {
            ShowHelp();
            return -1;
        }
        return iResult; 
    }
}

void ShowHelp(){
    printf("\nUsage:\n  BuzzerD -d <executable> [alive|on|off|success] [--debug]\n");
}
