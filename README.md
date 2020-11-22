# __BuzzerD__

## Overview

BuzzerD is a small daemon, which runs a given executable upon the press of a buzzer.

It expects an LED to be connected between the Raspberry PI’s pin 37 and ground (mind the required resistor of at least 220 ohms) as well as any kind of push-button between pin 12 and ground.

When the daemon is started, it monitors the push-button. For each press on the push-button, the executable, which is to be defined in a configuration-file, will be run. Note, that the executions are done sequentially and the button-presses will be counted.

The LED can be pre-configured in the configuration-file to one of four different modes:

 - _on_: The LED will be kept on, as long as the daemon is running.
 - _off_: The LED will be kept off.
 - _alive_: The LED will be blinking.
 - _success_: The LED will be switched off and one depending on the return code of the executable after each run. 

## Command-Line

When calling buzzerd manually without any arguments, it tries to parse the configuration-file and run a daemon in the background.

If command-line parameters are supplied, it tries to connect to a running daemon and pass on the arguments to the daemon in order to change the configuration at run-time.

Following command-line parameters are available:

 - _buzzerd –l (on|off|alive|success)_ Will switch the LED into the according mode.
 - _buzzerd –x <executable>_ Will change the executable to the called upon a buzzer-press.
 - _buzzerd –a <argumens>_ Will change the arguments to be passed on to the executable upon a buzzer-press.
 - _buzzerd –q <executable>_ Shuts down the daemon.

## Configuration

The configuration of the daemon is done in */etc/buzzerd.conf*. In there, the following options have to be defined:

 - _Executable  <executable>_ to set the executable to the called upon a buzzer-press.
 - _ClientOutput <logfile>_ to define a file, in which the client's output will be logged. 
 - _LED  (on|off|alive|success>)_ to set the LED into the according mode.
 - _debug_ to keep the access to the text-console open for debugging reasons.

## Internals

There are three source-files (plus headers):

 - _buzzerd.cpp_ The main executable of this project. It simply checks, whether command-line options are supplied and - depending on that - calls the daemon or the client.
 - _daemon.cpp_ This does the complete handling of the internals of the daemon. It contains the entry code, which reads the configuration, sets up the independent process, sets up a signal handler, which checks the buzzer each 50 ms and handles a server-socket, which allows the configuration to be changed at run-time.
 - _ConfigHandler.cpp_ This is the handler for all configuration-items of the buzzer-deamon. It contains the code the read the configuration-file, parse its arguments and handle the communication with any client, which tries to change settings. 
 - _client.cpp_ This is the code to be run as client. It tries to open the socket to the server and passes on the command-line arguments in order to be processed in the daemon.
 
## Known bugs and further steps

 - Some commands only work, when being called via a script. Thus, if for instance a directory listing is required, the _ls_ command is to be placed in a bash-script, which then can be called as executable of the daemon.
 - Changing the LED state out of the executable script (e.g. to show a status via _buzzerd -l off_) only works, when the daemon is configured to be in debug mode.
 - There is no service-file in _/etc/systemd/system_ available yet, thus this does not work yet via systemd.
 - The HW pins are currently hard-coded. Eventually, they are supposed to be handled via the configuration.

## License
Copyright (C) 2020 Jens Daniel Schlachter (<osw.schlachter@mailbox.org>)  
  
This program is free software: you can redistribute it and/or modify  
it under the terms of the GNU General Public License as published by  
the Free Software Foundation, either version 3 of the License, or  
(at your option) any later version.  
  
This program is distributed in the hope that it will be useful,  
but WITHOUT ANY WARRANTY; without even the implied warranty of  
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
GNU General Public License for more details.  
  
You should have received a copy of the GNU General Public License  
along with this program.  If not, see <https://www.gnu.org/licenses/>.
