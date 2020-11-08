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
#include <pthread.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <bcm2835.h>

#include "daemon.h"

/** Local Defines: ******************************************************************/

#define PIN_LED         RPI_V2_GPIO_P1_37
#define PIN_BTN         RPI_V2_GPIO_P1_12
#define BUFFSIZE        1024
#define SOCK_FILE       (char*) "/tmp/BuzzerD.sock"

/** Global Variables: ***************************************************************/

volatile bool           b_Debug;
volatile bool           b_Alive;
volatile bool           b_LastResult;
volatile bool           b_ExecRunning;
volatile unsigned char  ub_LedMode;
volatile unsigned char  ub_BuzzCount;
volatile char           s_Executable[1024];
volatile char           s_Arguments [1024];
volatile pid_t          p_ClientPid;
pthread_mutex_t         mutex_BuzzCount; 

/** Forward Declarations: ***********************************************************/

int  RunDemon      ();
void HandleClient  (int sockfd);
void RunExecutable ();
void SendString    (int sockfd, const char* Message);
bool ReadConfig    (char* sFileName);
bool CheckCfgCmd   (char* sInput, const char* sCommand, char* sResult);
void SIG_Alarm     (int signum);
void SIG_ChildTerm (int signum);
void SIG_Quit      (int signum);

/** Main-Function: ******************************************************************/

int RunDemon() {
    /** Variables:                                                                  */
    pid_t     pid, sid;
    struct    sigaction sa;
    struct    itimerval timer;
    int       iServerID, iClientId;
    struct    sockaddr_un SocketAddress;
    socklen_t AddressLen;
    struct    pollfd pfds[1];
    bool      b_RunExecutable = false;
    int       iResult;
    
    /** Read configuration: *********************************************************/
    b_Debug       = false;
    b_ExecRunning = false;
    if (! ReadConfig((char*)"/etc/buzzerd.conf") ) {
        printf ("ERR: Unable to read configuration!\n");        
        return -2;
    }
    
    /** Set up the demon: ***********************************************************/
    pid = fork();
    if (pid < 0) {
        return -2;
    }

    /** If we got a good PID, then we can exit the parent process:                  */
    if (pid > 0) {
        printf("Starting Buzzer-Deamon as PID %i.\n", pid);
        return 0;
    }
    
    /** Close out the standard file descriptors:                                    */
    if (! b_Debug) {        
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
    
    /** Open syslog:                                                                */
    openlog( "BuzzerD", LOG_PID | LOG_CONS | LOG_NDELAY, LOG_LOCAL0 );
    
    /** Prepare the Mutex:                                                          */
    if (pthread_mutex_init(&mutex_BuzzCount, NULL) != 0) {
        syslog(LOG_ERR + LOG_DAEMON, "FAILURE CREATING MUTEX!");
        return -2;
    }

    /* Change the file mode mask */
    umask(0);
            
    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        /* Log the failure and exit:                                                */
        syslog(LOG_ERR + LOG_DAEMON, "FAILURE SETTING UP CLIENT-PROCCESS!");
        return -2;
    }

    /* Change the current working directory */
    if ((chdir("/")) < 0) {
        /* Log the failure and exit:                                                */
        syslog(LOG_ERR + LOG_DAEMON, "FAILURE SETTING UP CLIENT-PROCCESS!");
        return -2;
    }    
    
    /** Setup BCM hardware-library: *************************************************/
    if (!bcm2835_init()) {
        /* Log the failure and exit:                                                */
        syslog(LOG_ERR + LOG_DAEMON, "FAILURE ACCESSING THE BCM HW LIBRARY!");
        return -2;
    }
    
    /** Prepare the LED pin:                                                        */
    bcm2835_gpio_set_pad(BCM2835_PAD_GROUP_GPIO_0_27,BCM2835_PAD_DRIVE_16mA);
    bcm2835_gpio_fsel(PIN_LED, BCM2835_GPIO_FSEL_OUTP);
    
    /** Prepare the button pin:                                                     */
    bcm2835_gpio_fsel(PIN_BTN, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_set_pud(PIN_BTN, BCM2835_GPIO_PUD_UP);
    
    /** Setup Signal Handler for quit: **********************************************/
    memset(&sa, 0, sizeof (sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = &SIG_Quit;
    sigaction(SIGINT , &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGKILL, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    /** Setup Signal Handler for child-term: ****************************************/
    sa.sa_handler = &SIG_ChildTerm ;
    sigaction (SIGCHLD, &sa, NULL );
    
    /** Setup 50 ms timer: **********************************************************/
    sa.sa_handler = &SIG_Alarm ;
    sigaction ( SIGALRM, &sa, NULL );
    timer.it_value.tv_sec = 0 ;
    timer.it_value.tv_usec = 50000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 50000 ;
    setitimer ( ITIMER_REAL, &timer, NULL ) ;

    /** Create Socket: **************************************************************/
    if((iServerID=socket (AF_LOCAL, SOCK_STREAM, 0)) == 0) {
        syslog(LOG_ERR + LOG_DAEMON, "FAILURE CREATING A SOCKET!");
        return -2;
    }
    /** Bind socket to file:                                                        */
    unlink(SOCK_FILE);
    SocketAddress.sun_family = AF_LOCAL;
    strcpy(SocketAddress.sun_path, SOCK_FILE);
    if (bind ( iServerID, (struct sockaddr *) &SocketAddress, sizeof (SocketAddress)) != 0) {
        syslog(LOG_ERR + LOG_DAEMON, "FAILURE BINDING SOCKET!");
        return -2;
    }

    /** Activate the socket:                                                        */     
    listen (iServerID, 5);
    AddressLen = sizeof (struct sockaddr_in);
    
    /** Prepare polling-structure: */
    pfds[0].fd     = iServerID;
	pfds[0].events = POLLIN;

    /** Note the successful initialization:                                         */
    syslog(LOG_NOTICE + LOG_DAEMON, "Sucessfully initialized.");
    
    /* Main-Loop: *******************************************************************/
    b_Alive = true;
    while(b_Alive) {
        /** Poll the socket with a 10ms timeout:                                    */
        iResult = poll(pfds, 1, 10);
        /** Check, if there was a request on the poll:                              */
        if (iResult>0) {
            /** There was, so try to accept it:                                     */
            iClientId = accept ( iServerID, (struct sockaddr *) &SocketAddress, &AddressLen );
            if (iClientId < 1) continue;
            /** It connected, so run the client-handler on it:                      */
            HandleClient(iClientId);
            close (iClientId);
        }
        /** Check, if there was a buzzer-press:                                     */
        pthread_mutex_lock(&mutex_BuzzCount);
        if ((ub_BuzzCount > 0) && (! b_ExecRunning)) {
            ub_BuzzCount --;
            b_RunExecutable = true;
        }
        pthread_mutex_unlock(&mutex_BuzzCount);
        /** If there was, run the executable:                                       */
        if (b_RunExecutable) {
            b_RunExecutable = false;
            RunExecutable();
        }
    }
    
    /** Shutdown: *******************************************************************/
    
    bcm2835_gpio_fsel(PIN_LED, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(PIN_LED, LOW);
    bcm2835_close();
    
    pthread_mutex_destroy(&mutex_BuzzCount);

    syslog(LOG_NOTICE + LOG_DAEMON, "Received SigInt and closed.");
    closelog();
    return 0;    
}

void HandleClient(int sockfd){
    /** Variables:                                                                  */
    char    Command[BUFFSIZE];
    ssize_t RxLen;
    
    /** Try to fetch the command:                                                   */    
    RxLen = recv (sockfd, Command, sizeof(Command)-1, 0);
    /** If something was received, try to parse it:                                 */
    if( RxLen > 0) {
        if ((Command[0] == '-') && (Command[1] == 'q')){
            /** It is an exit-command:                                              */
            b_Alive = false;
            SendString(sockfd, "Received quit.");
        }else if ((Command[0] == '-') && (Command[1] == 'x')){
            /** It is a command to replace the executable:                          */
            Command[RxLen] = 0;
            strcpy((char*)s_Executable, (&Command[3]));
            SendString(sockfd, "Updated executable.");
        }else if ((Command[0] == '-') && (Command[1] == 'a')){
            /** It is a command to replace the arguments:                           */
            Command[RxLen] = 0;
            strcpy((char*)s_Arguments, (&Command[3]));
            SendString(sockfd, "Updated arguments.");
        }else if ((Command[0] == '-') && (Command[1] == 'l')){
            /** It is an LED command, so parse it:                                  */
            Command[RxLen] = 0;
            if (strlen(Command) < 5) {
                SendString(sockfd, "Missing LED parameter!");
            }else if (strcmp ((&Command[3]), (char*) "on")==0) {
                ub_LedMode = LED_MODE_ON;
                SendString(sockfd, "Set LED Mode on!");
            }else if (strcmp((&Command[3]), (char*) "off")==0) {
                ub_LedMode = LED_MODE_OFF;
                SendString(sockfd, "Set LED Mode off!");
            }else if (strcmp((&Command[3]), (char*) "success")==0) {
                ub_LedMode = LED_MODE_SUCCESS;
                SendString(sockfd, "Set LED Mode success!");
            }else if (strcmp((&Command[3]), (char*) "alive")==0) {
                ub_LedMode = LED_MODE_ALIVE;
                SendString(sockfd, "Set LED Mode alive!");
            }else{
                SendString(sockfd, "ERR: Unable to parse LED parameter!");
            }
        }else{
            /** It was no valid command at all:                                     */
            SendString(sockfd, "Unable to parse command!");
        }
    }
}

void SendString   (int sockfd, const char* Message){
    send(sockfd, Message, strlen(Message), 0);
}
    
void RunExecutable(){
    /** Variables:                                                                  */     
    int   pid;
    /** Try to fork to run the executable as client-proccess:                       */        
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR + LOG_DAEMON, "FAILURE FORKING FOR EXECUTABLE CLIENT!");
        return;
    }
    /** If we got a good PID, then we can exit the parent process:                  */
    if (pid > 0) {
        b_ExecRunning = true;
        return;
    }
    /** In the client, replace the proccess with a call to bash:                    */
    execlp("bash", "bash", (char*) s_Executable, (char*) s_Arguments, NULL);    
}

bool ReadConfig(char* sFileName) {
    /** Variables:                                                                  */
    FILE *fp;
    char sBuffer[1024], sResult[1024];
    bool bExeSet = false;    
    bool bArgSet = false;    
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
        if (CheckCfgCmd(sBuffer, (char*) "Executable", sResult)) {
            strcpy((char*)s_Executable, sResult);
            bExeSet = true;
        }
        /** Check for the arguments:                                                */
        if (CheckCfgCmd(sBuffer, (char*) "Arguments", sResult)) {
            strcpy((char*)s_Arguments, sResult);
            bArgSet = true;
        }
        /** Check for the LED command:                                              */
        if (CheckCfgCmd(sBuffer, (char*) "LED", sResult)) {
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
        if (CheckCfgCmd(sBuffer, "debug", sResult)) {
            b_Debug = true;
        }
    }
    fclose(fp);
    return (bExeSet && bArgSet && bLedSet);
}

bool CheckCfgCmd(char* sInput, const char* sCommand, char* sResult) {
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

void SIG_Alarm (int signum) {
    static int ul_AliveCount    = 19;
    static int ul_DebounceCount = 0;   
    
    /** Handle the buzzer-state:                                                    */
    if (! bcm2835_gpio_lev(PIN_BTN)) {
        /** The level is low, thus the buzzer was pressed:                          */
        if (ul_DebounceCount == 0) {
            /** It was not pressed before:                                          */
            pthread_mutex_lock(&mutex_BuzzCount);
            ub_BuzzCount ++;
            pthread_mutex_unlock(&mutex_BuzzCount);
        }
        ul_DebounceCount = 3;
    }
    
    /** Handle the debounce-counter:                                                */
    if (ul_DebounceCount > 0) ul_DebounceCount--; 
    
    /** Switch the LED according to its state:                                      */
    if (ub_LedMode == LED_MODE_ON) {
        bcm2835_gpio_write(PIN_LED, HIGH);
    }else if (ub_LedMode == LED_MODE_OFF) {
        bcm2835_gpio_write(PIN_LED, LOW);
    }else if (ub_LedMode == LED_MODE_SUCCESS) {
        if (b_LastResult) {
            bcm2835_gpio_write(PIN_LED, HIGH);
        }else{
            bcm2835_gpio_write(PIN_LED, LOW);
        }
    }else{
        ul_AliveCount ++;
        if (ul_AliveCount == 20) {
            ul_AliveCount = 0;
            bcm2835_gpio_write(PIN_LED, LOW );
        }else if (ul_AliveCount == 10) {;
            bcm2835_gpio_write(PIN_LED, HIGH);
        }
    }
}

void SIG_ChildTerm (int signum) {
    int   iStatus;
    pid_t pid;
    /** Try to fetch the return-code of the client-process:                         */
    pid = waitpid(-1, &iStatus, WNOHANG);
    if ((pid == 0) || (pid == -1)) {
        /** Return code unknown:                                                    */
        b_ExecRunning = false;        
        b_LastResult  = false;
    }else{
        b_ExecRunning = false;        
        if (WEXITSTATUS(iStatus) == 0) {
            /** Return code success:                                                */
            b_LastResult = true;
        }else{
            /** Return code error:                                                  */
            b_LastResult = false;
        }
    }
}

void SIG_Quit (int signum) {
    b_Alive = false;    
}
