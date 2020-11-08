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

/** Global Includes: ****************************************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>

#include "client.h"

/** Local Defines: ******************************************************************/

#define BUFFFERSIZE 1024
#define SOCK_FILE (char*) "/tmp/BuzzerD.sock"

/** Main-Function: ******************************************************************/

int RunClient (int argc, char **argv) {
    int    iSocketID;
    char   Buffer [BUFFFERSIZE];
    struct sockaddr_un SocketAddress;
    int    i, RxLen;
    
    /** Check, that there is at least one argument:                                 */
    if (argc < 2) return -1;
    
    /** Prepare Buffer with full argument:                                          */
    strcpy(Buffer, argv[1]);
    for (i=1; i<(argc-1); i++) {
        strcat(Buffer, " ");
        strcat(Buffer, argv[i+1]);
    }
    
    /** Setup socket:                                                               */
    if((iSocketID=socket (PF_LOCAL, SOCK_STREAM, 0)) == 0) {
        printf ("ERR: Unable to create socket!\n");
        return -2;
    }
    SocketAddress.sun_family = AF_LOCAL;
    strcpy(SocketAddress.sun_path, SOCK_FILE);
    
    /** Try to connect:                                                             */
    if (connect ( iSocketID, (struct sockaddr *) &SocketAddress, sizeof (SocketAddress)) == 0) {
        /** If connected, send the command:                                         */
        send(iSocketID, Buffer, strlen (Buffer), 0);
        /** Try to receive the reply:                                               */
        RxLen = recv (iSocketID, Buffer, sizeof(Buffer)-1, 0);            
        if( RxLen > 0) {
            Buffer[RxLen] = 0;
            printf ("BuzzerD: %s\n", Buffer);
        }else{
            printf ("ERR: No reply received from deamon!\n");
        }
    }else{
        printf ("ERR: Unable to connect to deamon!\n");
    }
    close (iSocketID);
    return 0;
}

/** Check function to avoid multiple instances:                                     */

bool CheckSocket(){
    int    iSocketID;
    struct sockaddr_un SocketAddress;
    bool   bResult;
        
    /** Setup socket:                                                               */
    if((iSocketID=socket (PF_LOCAL, SOCK_STREAM, 0)) == 0) return false;
    SocketAddress.sun_family = AF_LOCAL;
    strcpy(SocketAddress.sun_path, SOCK_FILE);
    
    /** Try to connect:                                                             */
    if (connect ( iSocketID, (struct sockaddr *) &SocketAddress, sizeof (SocketAddress)) == 0) {
        bResult = true;
    }else{
        bResult = false;
    }
    
    /** Close the socket and return, what we got:                                   */
    close (iSocketID);
    return bResult;
}
