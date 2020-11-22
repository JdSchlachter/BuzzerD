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

/** Type-Definitions: ***************************************************************/

#define LED_MODE_ON      1
#define LED_MODE_OFF     2
#define LED_MODE_SUCCESS 3
#define LED_MODE_ALIVE   4

/** Class Definition: ***************************************************************/

class CConfigHandler {
public:
    // Properties:
    bool           b_Shutdown;
    bool           b_Debug;
    unsigned char  ub_LedMode;
    char           s_Executable[1024];
    char           s_ClientLog [1024];
    // Methods:
    CConfigHandler();
    ~CConfigHandler();
    bool ReadConfig  (char* sFileName);
    void HandleClient(int sockfd);
private:
    bool CheckCmd    (char* sInput, const char* sCommand, char* sResult);
    void SendToSocket(int sockfd, const char* Message);
};
