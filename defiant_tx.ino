#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
//#include <cc1101_debug_service.h>


/* Wifi Client mode */
const char* ssid = "YOUR WIFI SSID";  // Enter SSID here
const char* password = "YOUR WIFI PASSWORD";  //Enter Password here

byte deviceID[16] = { INSERT YOUR DEVICE ID BYTES HERE. GET VALUES FROM DEFIANT_RX PROGRAM };
//byte deviceID[16] = {0x80, 0x00, 0x00, 0x00, 0xEE, 0x88, 0x88, 0xEE, 0xEE, 0xEE, 0xE8, 0xE8, 0xEE, 0xE8, 0x88, 0x8E}; //example deviceID


//datarate is set for newer remotes model YLT-42AC
//if you are using older remote YLT-40C, change olderRemote variable to true
bool olderRemote = false;

ESP8266WebServer server(80);   //instantiate server at port 80 (http port)


int gdo0;
float baud = 5;
byte bytesToSend[1036];
byte sendPacket[60];

String defiant_menu;
String menu_htmlHeader;

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

void handleMainMenu() {
 Serial.println("User called the main menu");
 //String s = MAIN_page; //Read HTML contents
 server.send(200, "text/html", defiant_menu); //web page
}

void handleDefiantButton(){
  String button = server.arg("remotebutton");
  if( server.arg("remotebutton").equals("powerOn") ){ turnOnPower(); }
  if( server.arg("remotebutton").equals("powerOff") ){ turnOffPower(); }
  if( server.arg("remotebutton").equals("syncSignalOldModel") ){ syncRemoteOldModel(); }
  if( server.arg("remotebutton").equals("syncSignalNewModel") ){ syncRemoteNewModel(); }
  server.send(200, "text/html", menu_htmlHeader + "<h1>Signal is sent</h1><p><a href=\"/\">Return to Main Menu</a></p></body></html>");
}

void turnOnPower(){
  sendDefiantSignal(0x88, 0xE8, 14, 0);  //send the packet 15 times, set packet length to 240 
}

void turnOffPower(){
  if(olderRemote == false){ sendDefiantSignal(0x8E, 0xE8, 14, 0); }
  else{ sendDefiantSignal(0x88, 0x8E, 14, 0); }
}


//sync is work in progress. still debugging newer model
void syncRemoteOldModel(){
  sendDefiantSignal(0x88, 0xEE, 80, 2);
}

void syncRemoteNewModel(){
  sendDefiantSignal(0x8E, 0xE8, 80, 2);
  delayMicroseconds(900000);
  sendDefiantSignal(0x88, 0xE8, 14, 0);
}

void sendDefiantSignal(byte actionOneByte, byte actionTwoByte, int repeatSignal, byte lengthMode){
  uint8_t chipstate;
  digitalWrite(16, LOW);
  Serial.println("Sending signal");
  ELECHOUSE_cc1101.setLengthConfig(lengthMode);
  deviceID[14] = actionOneByte;
  deviceID[15] = actionTwoByte;
  ELECHOUSE_cc1101.SpiWriteBurstReg(CC1101_TXFIFO, deviceID, 16);
  ELECHOUSE_cc1101.SetTx();

  for(int x=0; x < repeatSignal; x++){
    for(int y=0; y < 16; y++){
      while(digitalRead(gdo0)); //wait for the TX FIFO to be below 61 bytes, then we can start adding more bytes. GDOx_CFG = 0x02 page 62 datasheet
      ELECHOUSE_cc1101.SpiWriteReg(CC1101_TXFIFO, deviceID[y]);
      yield();
    }
  }
  if(lengthMode == 0x02){ ELECHOUSE_cc1101.setSidle(); } //end the transmission with bytes still in the buffer. Just need 2 seconds worth of packets

  chipstate = 0xFF;
  while(chipstate != 0x01){ chipstate = (ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & 0x1F); }
  if( ELECHOUSE_cc1101.SpiReadStatus(CC1101_TXBYTES) > 0 ){ ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX); delayMicroseconds(100); }
  digitalWrite(16, HIGH);
  Serial.println("Signal sent");
}


void setup(void){
  Serial.begin(115200);

  //Setup CC1101 Radio
  gdo0 = 5; //for esp8266 gdo0 on pin 5 = D1

  if( olderRemote == true ){ baud = 3.3333; }

  ELECHOUSE_cc1101.Init();  
  ELECHOUSE_cc1101.setGDO0(gdo0);  //set esp8266 gdo0 pinMode to INPUT
  
  ELECHOUSE_cc1101.setSyncMode(0); //turn off preamble/sync
  ELECHOUSE_cc1101.setDcFilterOff(false);  //MDMCFG2 DC filter enabled
  ELECHOUSE_cc1101.setManchester(false);  //MDMCFG2 Manchester Encoding off
  
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG0, 0x03); //Sets how GDO0 will assert based on size of FIFO bytes (pages 62, 71 in datasheet)
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_FIFOTHR, 0x00); //FIFO threshold; sets how many bytes TX & RX FIFO holds (pages 56, 72 in datasheet)
  
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG1, 0x03);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG2, 0x03);
 
 
  ELECHOUSE_cc1101.setWhiteData(false); //PKTCTRL0 data whitening off
  ELECHOUSE_cc1101.setPktFormat(0); //PKTCTRL0 use FIFO for TX and RX
  ELECHOUSE_cc1101.setCrc(false); //PKTCTRL0 CRC disabled
  ELECHOUSE_cc1101.setLengthConfig(0); //PKTCTRL0 Fixed packet length. Use PKTLEN register to set packet length
  
  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_MDMCFG1,  0x02); //2 preamble bytes (least amount)
  ELECHOUSE_cc1101.setFEC(false); //MDMCFG1 Forward error correction disabled
  ELECHOUSE_cc1101.setPRE(0x00);  //MDMCFG1 Minimum number of preamble bytes
  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_MDMCFG0,  0xF8); //channel spacing. Irrelevant for rfmoggy
  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_CHANNR,   0x00); //channel 0. Only relevant when communicating with another CC1101
  ELECHOUSE_cc1101.setChsp(12.5); //MDMCFG0 channel spacing kHz
  ELECHOUSE_cc1101.setChannel(0x00); //CHANNR sets channel number. channel 0 stays on base frequency
  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_DEVIATN,  0x00); //irrelevant in ASK OOK mode
  ELECHOUSE_cc1101.setDeviation(0);
  
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_FREND1,   0x56); 
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_MCSM0 ,   0x18);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_FOCCFG,   0x14);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_BSCFG,    0x6C);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL2, 0x07); //Texas Instrument document DN022 describes optimum settings for ASK reception
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL1, 0x00);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL0, 0x92);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_FSCAL3,   0xE9);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_FSCAL2,   0x2A);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_FSCAL1,   0x00);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_FSCAL0,   0x1F);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_FSTEST,   0x59);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_TEST2,    0x81);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_TEST1,    0x35);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_TEST0,    0x09);
  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_PKTCTRL1, 0x00);
  ELECHOUSE_cc1101.setPQT(0); //PKTCTRL1 Preamble quality estimator disabled
  ELECHOUSE_cc1101.setCRC_AF(false); //PKTCTRL1 Disable automatic flush of RX FIFO if CRC is not OK
  ELECHOUSE_cc1101.setAppendStatus(false); //PKTCTRL1 Status bytes (RSSI and LQI values) appeneded to payload is disabled
  ELECHOUSE_cc1101.setAdrChk(0); //PKTCTRL1 No address check
  
  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_ADDR,     0x00);
  ELECHOUSE_cc1101.setAddr(0);
  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_PKTLEN,   0x3D); //max packet size of 256. Related to TX FIFO buffer and FIFOTHR, include length & address byte(s) if enabled
  ELECHOUSE_cc1101.setPacketLength(0xF0); //Transmit 240 bytes. Defiant packets are 16bytes, repeated 15 times

  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_SYNC1, 0x00);
  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_SYNC0, 0x00);
  ELECHOUSE_cc1101.setSyncWord(0x00, 0x00); //SYNC0 and SYNC1 sync words
  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_FSCTRL0, 0x00); //if you have a cheaper chip, you will need to adjust this, frequency offset
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_MCSM2, 0x07);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_MCSM1, 0x00); //goto idle after TX
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_WOREVT1, 0x87);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_WOREVT0, 0x6B);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_WORCTRL, 0xFB);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_RCCTRL1, 0x41);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_RCCTRL0, 0x00);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_PTEST, 0x7F);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCTEST, 0x3F);
  

  //These values come from LSatan Calibration tool. RTLSDR required. https://github.com/LSatan/CC1101-Debug-Service-Tool/tree/master/Calibrate_frequency
  //setClb must be called before setMHz is called the first time
  //ELECHOUSE_cc1101.setClb(1,13,15); 
  //ELECHOUSE_cc1101.setClb(2,16,19);
  //ELECHOUSE_cc1101.setClb(3,33,38);
  //ELECHOUSE_cc1101.setClb(4,38,39);
    
  //ELECHOUSE_cc1101.setCCMode(0);  //any value besides 1
  ELECHOUSE_cc1101.setModulation(2); //modulation is ASK by default
  ELECHOUSE_cc1101.setMHZ(315.00);
  //ELECHOUSE_cc1101.set_rxbw(100);
  ELECHOUSE_cc1101.setDRate(baud);
  ELECHOUSE_cc1101.setPA(12);
  

//wifi client mode
  WiFi.mode(WIFI_STA); //ESP8266 will transmit most recent AP SSID without this line
  WiFi.begin(ssid, password);     //Connect to your WiFi router
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
 
  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to "); Serial.println(ssid);
  Serial.print("IP address: "); Serial.println(WiFi.localIP());  //IP address assigned to your ESP */

/*************************************************************************/

  server.on("/", handleMainMenu);
  server.on("/sendDefiant", handleDefiantButton);
  server.onNotFound(handle_NotFound);
  server.begin();                  //Start server
  Serial.println("HTTP server started");  

  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);

  menu_htmlHeader = "<!DOCTYPE html> <html>";
  menu_htmlHeader += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">";
  menu_htmlHeader += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;} ";
  menu_htmlHeader += "body{margin-top: 10px;} h1 {color: #000000;margin: 20px auto 30px;} h3 {color: #000000;margin-bottom: 50px;} ";
  menu_htmlHeader += "p {font-size: 16px;color: #000000;margin-bottom: 30px;} ";
  menu_htmlHeader += "button { background-color: #008CBA; border: none; color: white; padding: 12px 28px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px;} ";
  menu_htmlHeader += "</style></head>";
  
  defiant_menu += menu_htmlHeader;
  defiant_menu += "<title>Defiant Switch</title>";
  defiant_menu += "<body><h1>Defiant Wireless Power Switch</h1>";
  defiant_menu += "<h3>Defiant remote control CC1101 clone</h3>";
  defiant_menu += "<form action=\"sendDefiant\">";
  defiant_menu += "<button name=\"remotebutton\" type=\"submit\" value=\"powerOn\">Power On</button></a><br><br>";
  defiant_menu += "<button name=\"remotebutton\" type=\"submit\" value=\"powerOff\">Power Off</button></a><br><br>";
  defiant_menu += "<button name=\"remotebutton\" type=\"submit\" value=\"syncSignalOldModel\">Sync Remote Old Model</button></a><br><br>";
  defiant_menu += "<button name=\"remotebutton\" type=\"submit\" value=\"syncSignalNewModel\">Sync Remote New Model</button></a><br><br>";
  defiant_menu += "</form></body></html>";

}

void loop(void){
  server.handleClient();          //Handle client requests
  //cc1101_debug.debug();
}
