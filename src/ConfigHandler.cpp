//
//  This file is part of PeaCalc++ project
//  Copyright (C)2018 Jens Daniel Schlachter <osw.schlachter@mailbox.org>
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
#include <arpa/inet.h>
#include <unistd.h>

#include "ConfigHandler.h"

/** Local Defines: ******************************************************************/

#define BUFFSIZE        1024

/** Public Functions: ***************************************************************/

CConfigHandler::CConfigHandler() {
    b_Debug       = false;
    b_Shutdown    = false;
}

CConfigHandler::~CConfigHandler() {

}

bool CConfigHandler::ReadConfig(char* sFileName) {
    /** Variables:                                                                  */
    FILE *fp;
    char sBuffer[1024], sResult[1024];
    bool bExeSet = false;
    bool bLogSet = false;
    bool bLedSet = false;
    /** Try to open the configuration-file:                                         */
    fp = fopen(sFileName,"r");
    if (fp == 0) return false;
    /** Cycle through the file:                                                     */
    while (! feof(fp)) {
        /** Try to fetch a line:                                                    */
        if (!fgets(sBuffer, sizeof(sBuffer), fp)) break;
        /** Check, if it is a comment:                                              */
        if ((sBuffer[0] == ';') || (sBuffer[0] == '#')) continue;
        /** Check for the executable:                                               */
        if (CheckCmd(sBuffer, (char*) "Executable", sResult)) {
            strcpy((char*)s_Executable, sResult);
            bExeSet = true;
        }
        /** Check for the client-log:                                               */
        if (CheckCmd(sBuffer, (char*) "ClientOutput", sResult)) {
            strcpy((char*)s_ClientLog, sResult);
            bLogSet = true;
        }
        /** Check for the LED command:                                              */
        if (CheckCmd(sBuffer, (char*) "LED", sResult)) {
            if (strcmp(sResult, (char*) "on")==0) {
                ub_LedMode = LED_MODE_ON;
                bLedSet    = true;
            }else if (strcmp(sResult, (char*) "off")==0) {
                ub_LedMode = LED_MODE_OFF;
                bLedSet    = true;
            }else if (strcmp(sResult, (char*) "success")==0) {
                ub_LedMode = LED_MODE_SUCCESS;
                bLedSet    = true;
            }else if (strcmp(sResult, (char*) "alive")==0) {
                ub_LedMode = LED_MODE_ALIVE;
                bLedSet    = true;
            }
        }
        /** Check for a debug-command:                                              */
        if (CheckCmd(sBuffer, "debug", sResult)) {
            b_Debug = true;
        }
    }
    fclose(fp);
    return (bExeSet && bLogSet && bLedSet);
}

void CConfigHandler::HandleClient(int sockfd){
    /** Variables:                                                                  */
    char    Command[BUFFSIZE];
    ssize_t RxLen;
    
    /** Try to fetch the command:                                                   */    
    RxLen = recv (sockfd, Command, sizeof(Command)-1, 0);
    /** If something was received, try to parse it:                                 */
    if( RxLen > 0) {
        if ((Command[0] == '-') && (Command[1] == 'q')){
            /** It is an exit-command:                                              */
            b_Shutdown = true;
            SendToSocket(sockfd, "Received quit.");
        }else if ((Command[0] == '-') && (Command[1] == 'x')){
            /** It is a command to replace the executable:                          */
            Command[RxLen] = 0;
            strcpy((char*)s_Executable, (&Command[3]));
            SendToSocket(sockfd, "Updated executable.");
        }else if ((Command[0] == '-') && (Command[1] == 'l')){
            /** It is an LED command, so parse it:                                  */
            Command[RxLen] = 0;
            if (strlen(Command) < 5) {
                SendToSocket(sockfd, "Missing LED parameter!");
            }else if (strcmp ((&Command[3]), (char*) "on")==0) {
                ub_LedMode = LED_MODE_ON;
                SendToSocket(sockfd, "Set LED Mode on!");
            }else if (strcmp((&Command[3]), (char*) "off")==0) {
                ub_LedMode = LED_MODE_OFF;
                SendToSocket(sockfd, "Set LED Mode off!");
            }else if (strcmp((&Command[3]), (char*) "success")==0) {
                ub_LedMode = LED_MODE_SUCCESS;
                SendToSocket(sockfd, "Set LED Mode success!");
            }else if (strcmp((&Command[3]), (char*) "alive")==0) {
                ub_LedMode = LED_MODE_ALIVE;
                SendToSocket(sockfd, "Set LED Mode alive!");
            }else{
                SendToSocket(sockfd, "ERR: Unable to parse LED parameter!");
            }
        }else{
            /** It was no valid command at all:                                     */
            SendToSocket(sockfd, "ERR: Unable to parse command!");
        }
    }
}

/** Private Functions: **************************************************************/

bool CConfigHandler::CheckCmd(char* sInput, const char* sCommand, char* sResult) {
    int n, i;
    /** Check, if the length of the command is in the input:                        */
    if (strlen(sInput) < strlen(sCommand)) return false;
    /** Check, if the command is at the beginning of the input:                     */
    i = 0;
    n = strlen(sCommand) - 1;
    while ((i<n) && (sInput[i] == sCommand[i])) i++;
    if (sInput[i] != sCommand[i]) return false;
    /** The command is alright, so go one character behind it:                      */
    i++;
    /** Trim the left side of the parameter:                                        */
    n = strlen(sInput);
    while ((i<n) && ((sInput[i] == ' ') || (sInput[i] == '\t'))) i++;
    /** Trim the right side of the parameter:                                       */
    while ((n>0                                                             )&&
           ((sInput[n-1] == ' ') || (sInput[n-1] == '\r') || (sInput[n-1] == '\n'))  ) n--;
    sInput[n] = 0;
    /** Copy the output:                                                            */
    strcpy(sResult, &sInput[i]);
    return true;
}

void CConfigHandler::SendToSocket(int sockfd, const char* Message){
    send(sockfd, Message, strlen(Message), 0);
}
