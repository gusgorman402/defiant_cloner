# defiant_cloner
### Cloning Defiant wireless switch with CC1101 and ESP8266
Defiant Models YLT-42A and YLT-42, sold at Home Depot. FCC ID 2AEQYYLT-40-3C and 2AEQYYLT-42A-3C

Schematic can be found here https://github.com/LSatan/SmartRC-CC1101-Driver-Lib

The repo contains 3 programs. Most of the required libraries are in the Arduino Library Manager, or download info is listed in the .ino files
    
**defiant_rx.ino** - Receives Defiant wireless remote signal to decode & display the device ID; outputs to serial monitor. Paste the 
                 device ID into the transmit programs. The CC1101 chip has trouble syncing with this remote signal. You may have to press 
                 the remote button several times, for several seconds to get a decode. Sometimes it works the first time, sometimes 
                 it takes multiple button presses; sometimes reseting the ESP8266 helps too. RTL-SDR with URH can be used to receive and
                 decode the device ID also
                 
**defiant_tx.ino** - Creates a webpage for controlling Defiant switch using CC1101. Accessible via your local wifi network

**defiant_weather.ino** - Program for turning Defiant switch on when outdoor temperature is below freezing. Uses temperature data 
                      from OpenWeatherMap. Time-stamped (NTP) temp is output to WebSerial page, accessible on local wifi network. 
                      WebSerial has option to turn the switch on with "Turn power off" and "Turn power on" commands. Temp data
                      is updated every 15 minutes. Edit variables for wifi name, OpenWeatherMap API key, 
                      Defiant device ID, UTC timezone offset, and optional refresh time (set to 15 minutes) before compiling.
                      I use this program to control my birdbath heater during the winter


![Defiant package](/images/defiant_box.jpg)
![Older_back](/images/olderModel_back.jpg)
![Newer back](/iamges/newerModel_back.jpg)
![Older_front](/images/olderModel_front.jpg)
![Newer_front](/images/newerModel_front.jpg)
![NodeMCU](/images/breadboard1.jpg)
![NodeMCU](/images/breadboard2.jpg)
