#include <Arduino.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <WebSerial.h>  //use this tutorial for links to ESPAsync libraries https://randomnerdtutorials.com/esp8266-nodemcu-webserial-library/
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266HTTPClient.h> 
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <ArduinoJson.h>
//#include <cc1101_debug_service.h>



/* Wifi Client mode */
const char* ssid = "YOUR WIFI SSID";  // Enter SSID here
const char* password = "YOUR WIFI PASSWORD";  //Enter Password here

String Location = "YOUR CITY, COUNTRY"; //example Chicago,US
String API_Key = "YOUR API KEY";  //API key for OpenWeatherMap

byte deviceID[16] = { INSERT YOUR DEVICE ID BYTES HERE. GET VALUES FROM DEFIANT_RX PROGRAM };
//byte deviceID[16] = {0x80, 0x00, 0x00, 0x00, 0xEE, 0x88, 0x88, 0xEE, 0xEE, 0xEE, 0xE8, 0xE8, 0xEE, 0xE8, 0x88, 0x8E}; //example deviceID


bool olderRemote = false;  //if you are using older remote YLT-40C(YLT-42), change olderRemote variable to true. New model is YLT-42AC(YLT-42A)

const long interval = 900000; //time in millseconds to wait between temperature checks. Wait 15minutes

const long utcOffsetInSeconds = -21600; //change for your timezone UTC. https://lastminuteengineers.com/esp8266-ntp-server-date-time-tutorial/






AsyncWebServer server(80);   //instantiate server at port 80 (http port)
unsigned long previousMillis = 0;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
int dayOfWeekInt;
String timeOfDay;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

int gdo0;
float baud = 5;


void turnOnPower(){
  sendDefiantSignal(0x88, 0xE8, 14);  //send the packet 15 times, set packet length to 240
}

void turnOffPower(){
  if(olderRemote == false){ sendDefiantSignal(0x8E, 0xE8, 14); }
  else{ sendDefiantSignal(0x88, 0x8E, 14); }
}



void sendDefiantSignal(byte actionOneByte, byte actionTwoByte, int repeatSignal){
  uint8_t chipstate;
  digitalWrite(16, LOW);
  Serial.println("Sending signal");

  deviceID[14] = actionOneByte;
  deviceID[15] = actionTwoByte;
  ELECHOUSE_cc1101.SpiWriteBurstReg(CC1101_TXFIFO, deviceID, 16);
  ELECHOUSE_cc1101.SetTx();

  for(int x=0; x < repeatSignal; x++){
    for(int y=0; y < 16; y++){
      while(digitalRead(gdo0)); //wait for the TX FIFO to be below 61 bytes, then we can start adding more bytes. GDOx_CFG = 0x02 page 62 datasheet
      ELECHOUSE_cc1101.SpiWriteReg(CC1101_TXFIFO, deviceID[y]);
      //yield(); //yield crashing when using webserial
    }
  }
  
  chipstate = 0xFF;
  //yield();
  while(chipstate != 0x01){ chipstate = (ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & 0x1F); }
  if( ELECHOUSE_cc1101.SpiReadStatus(CC1101_TXBYTES) > 0 ){ ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX); delayMicroseconds(100); }
  digitalWrite(16, HIGH);
  Serial.println("Signal sent");
}

void recvMsg(uint8_t *data, size_t len){
  WebSerial.println("Received Data...");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  WebSerial.println(d);
  Serial.println(d);
  if (d == "Turn power on"){
    Serial.println("*** Manual override power on ***");
    turnOnPower();
    WebSerial.println("Manual override power on");
  }
  if (d=="Turn power off"){
    Serial.println("*** Manual overrride power off ***");
    turnOffPower();
    WebSerial.println("*** Manual override power off ***");
  }
}


void setup(void){
  Serial.begin(115200);

  //Setup CC1101 Radio
  gdo0 = 5; //for esp8266 gdo0 on pin 5 = D1
  
  //blinks LED when radio signal is sent
  pinMode(16, OUTPUT);
  digitalWrite(16, HIGH);

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
  ELECHOUSE_cc1101.setPacketLength(0xF0); //Transmit 240 bytes. Defiant packets are 16bytes, repeat 15 times

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

  WebSerial.begin(&server);
  WebSerial.msgCallback(recvMsg);
  server.begin();

  timeClient.begin();

}

void loop(void){ 
  //cc1101_debug.debug();
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    timeClient.update();
    
    if (WiFi.status() == WL_CONNECTED){
      HTTPClient http;
      http.begin("http://api.openweathermap.org/data/2.5/weather?units=imperial&q=" + Location + "&APPID=" + API_Key); // !!
      int httpCode = http.GET();
      
      if (httpCode > 0){
        String payload = http.getString();

        //ArduinoJson version 5
        //DynamicJsonDocument jsonBuffer(512);
        //JsonObject& doc = jsonBuffer.parseObject(payload);
        //if (!doc.success()) { Serial.println(F("Parsing failed!")); return; }

        //ArduinoJson version 6
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }

       
        float temp = (float)(doc["main"]["temp"]);
        String prettyTemp = String(temp, 1);
        dayOfWeekInt = timeClient.getDay();
        timeOfDay = timeClient.getFormattedTime();
        Serial.print(daysOfTheWeek[dayOfWeekInt]); Serial.print(" ");
        WebSerial.print(daysOfTheWeek[dayOfWeekInt]); WebSerial.print(" ");
        Serial.print(timeOfDay); Serial.print("  Temp: ");
        WebSerial.print(timeOfDay);WebSerial.print(" Temp: ");
        Serial.print(prettyTemp);Serial.println("F");
        WebSerial.print(prettyTemp);WebSerial.println("F");

        if(temp <= 34.0){
          Serial.println("  Turning power on");
          WebSerial.println("  Turning power on");
          turnOnPower();
        }
        else{
          Serial.println("  Turning power off");
          WebSerial.println("  Turning power off");
          turnOffPower();
        }
      }
      http.end();
    }
  }
}
