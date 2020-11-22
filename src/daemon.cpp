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
#include "ConfigHandler.h"

/** Local Defines: ******************************************************************/

#define PIN_LED         RPI_V2_GPIO_P1_37
#define PIN_BTN         RPI_V2_GPIO_P1_12
#define SOCK_FILE       (char*) "/tmp/BuzzerD.sock"

/** Global Variables: ***************************************************************/

CConfigHandler          Config;
volatile bool           b_Alive;
volatile bool           b_LastResult;
volatile bool           b_ExecRunning;
volatile unsigned char  ub_BuzzCount;
volatile pid_t          p_ClientPid;
pthread_mutex_t         mutex_BuzzCount; 

/** Forward Declarations: ***********************************************************/

int  RunDemon      ();
void RunExecutable ();
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
    if (! Config.ReadConfig((char*)"/etc/buzzerd.conf") ) {
        printf ("ERR: Unable to read configuration!\n");        
        return -2;
    }
    
    /** Set up the demon: ***********************************************************/
    b_ExecRunning = false;
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
    if (! Config.b_Debug) {        
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }
    
    /** Open syslog:                                                                */
    openlog( "BuzzerD", LOG_PID | LOG_CONS | LOG_NDELAY, LOG_LOCAL0 );
    
    /** Prepare the Mutex:                                                          */
    if (pthread_mutex_init(&mutex_BuzzCount, NULL) != 0) {
        syslog(LOG_ERR | LOG_DAEMON, "FAILURE CREATING MUTEX!");
        return -2;
    }

    /* Change the file mode mask */
    umask(0);
            
    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        /* Log the failure and exit:                                                */
        syslog(LOG_ERR | LOG_DAEMON, "FAILURE SETTING UP CLIENT-PROCCESS!");
        return -2;
    }

    /* Change the current working directory */
    if ((chdir("/")) < 0) {
        /* Log the failure and exit:                                                */
        syslog(LOG_ERR | LOG_DAEMON, "FAILURE SETTING UP CLIENT-PROCCESS!");
        return -2;
    }    
    
    /** Setup BCM hardware-library: *************************************************/
    if (!bcm2835_init()) {
        /* Log the failure and exit:                                                */
        syslog(LOG_ERR | LOG_DAEMON, "FAILURE ACCESSING THE BCM HW LIBRARY!");
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
        syslog(LOG_ERR | LOG_DAEMON, "FAILURE CREATING A SOCKET!");
        return -2;
    }
    /** Bind socket to file:                                                        */
    unlink(SOCK_FILE);
    SocketAddress.sun_family = AF_LOCAL;
    strcpy(SocketAddress.sun_path, SOCK_FILE);
    if (bind ( iServerID, (struct sockaddr *) &SocketAddress, sizeof (SocketAddress)) != 0) {
        syslog(LOG_ERR | LOG_DAEMON, "FAILURE BINDING SOCKET!");
        return -2;
    }

    /** Activate the socket:                                                        */     
    listen (iServerID, 5);
    AddressLen = sizeof (struct sockaddr_in);
    
    /** Prepare polling-structure: */
    pfds[0].fd     = iServerID;
	pfds[0].events = POLLIN;

    /** Note the successful initialization:                                         */
    syslog(LOG_NOTICE | LOG_DAEMON, "Sucessfully initialized.");
    
    /* Main-Loop: *******************************************************************/
    b_Alive = true;
    while((b_Alive) && (! Config.b_Shutdown)){
        /** Poll the socket with a 10ms timeout:                                    */
        iResult = poll(pfds, 1, 10);
        /** Check, if there was a request on the poll:                              */
        if (iResult>0) {
            /** There was, so try to accept it:                                     */
            iClientId = accept ( iServerID, (struct sockaddr *) &SocketAddress, &AddressLen );
            if (iClientId < 1) continue;
            /** It connected, so run the client-handler on it:                      */
            Config.HandleClient(iClientId);
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

    syslog(LOG_NOTICE | LOG_DAEMON, "Received SigInt and closed.");
    closelog();
    return 0;    
}
   
void RunExecutable(){
    /** Variables:                                                                  */     
    int   pid;
    char  buffer[2048];
    int   iResult;
    FILE  *fp;
    /** Try to fork to run the executable as client-proccess:                       */        
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR | LOG_DAEMON, "FAILURE FORKING FOR EXECUTABLE CLIENT!");
        return;
    }
    /** If we got a good PID, then we can return to the main-loop:                  */
    if (pid > 0) {
        b_ExecRunning = true;
        return;
    }
    /** Build the execuable command:                                                */
    strcpy(buffer, "bash ");
    strcat(buffer, Config.s_Executable);
    if (Config.s_ClientLog[0] != 0) {
        strcat(buffer, " >");
        strcat(buffer, Config.s_ClientLog);
    }
    /** Run it:                                                                     */    
    iResult = system(buffer);
    /** If a log-file is configured, add the exit-code:                             */
    if (Config.s_ClientLog[0] != 0) {
        fp = fopen(Config.s_ClientLog,"a");
        if (fp == 0) _exit(1);        
        sprintf(buffer, "\nbuzzerd: Client exited with code %i.", WEXITSTATUS(iResult));
        fputs(buffer, fp);
        fclose(fp);
    }
    _exit(WEXITSTATUS(iResult));
}

void SIG_Alarm (int signum) {
    /** Variables:                                                                  */
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
    if (Config.ub_LedMode == LED_MODE_ON) {
        bcm2835_gpio_write(PIN_LED, HIGH);
    }else if (Config.ub_LedMode == LED_MODE_OFF) {
        bcm2835_gpio_write(PIN_LED, LOW);
    }else if (Config.ub_LedMode == LED_MODE_SUCCESS) {
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
