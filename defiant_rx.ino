#include <ELECHOUSE_CC1101_SRC_DRV.h>
//#include <cc1101_debug_service.h>

//datarate is set for newer remotes model YLT-42AC
//if you are using older remote YLT-40C, change olderRemote variable to true
bool olderRemote = false;

int gdo0;
uint8_t bytesInBuffer = 0;
byte smallRXarray[64];
byte bigRXarray[1024];
int bigRXsize = 0;
int finalSize = 0;
int counter = 0;
int decibel;
bool packetChecker;
float baud;
int baudDelay;

void setup(void){
  Serial.begin(115200);

  //Setup CC1101 Radio
  gdo0 = 5; //for esp8266 gdo0 on pin 5 = D1
  
  if(olderRemote == true){
    baud = 3.3333;
    baudDelay = 30000;
  }
  else{
    baud = 5;
    baudDelay = 20000;
  }

  ELECHOUSE_cc1101.Init();  
  ELECHOUSE_cc1101.setGDO0(gdo0);  //set gdo0 pinMode to INPUT
  
  ELECHOUSE_cc1101.setSyncMode(4); //turn off preamble/sync. Carrier sense above threshold
  ELECHOUSE_cc1101.setDcFilterOff(false);  //MDMCFG2 DC filter enabled
  ELECHOUSE_cc1101.setManchester(false);  //MDMCFG2 Manchester Encoding off
  
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG0, 0x01); //Sets how GDO0 will assert based on size of FIFO bytes (pages 62, 71 in datasheet)
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_FIFOTHR, 0x00); //FIFO threshold; sets how many bytes TX & RX FIFO holds (pages 56, 72 in datasheet)
  
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG1, 0x03);  //GDO1 isn't used
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_IOCFG2, 0x03);  //GDO2 isn't used
  
  ELECHOUSE_cc1101.setFEC(false); //MDMCFG1 Forward error correction disabled
  ELECHOUSE_cc1101.setPRE(0x00);  //MDMCFG1 Minimum number of preamble bytes
  ELECHOUSE_cc1101.setChsp(199.95); //MDMCFG0 channel spacing kHz
  ELECHOUSE_cc1101.setChannel(0x00); //CHANNR sets channel number. channel 0 stays on base frequency
  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_DEVIATN,  0x00); //irrelevant in ASK OOK mode
  ELECHOUSE_cc1101.setDeviation(0);
  
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_FREND1,   0x56); 
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_MCSM0 ,   0x18);  //when to calibrate chip
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_FOCCFG,   0x14);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_BSCFG,    0x6C);
  
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL2, 0xC7); //Texas Instrument document DN022 and DN010 describes optimum settings for ASK reception
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL1, 0x00); //SmartRF Studio will also list optimum register settings
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCCTRL0, 0x92);  //Original value was 0x92. Not noticing much difference between 92 and B2
  
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
  ELECHOUSE_cc1101.setAddr(0);
  
  ELECHOUSE_cc1101.setPacketLength(0xFF);  //packet length 256 bytes. Irrelevant in infinite mode

  ELECHOUSE_cc1101.setSyncWord(0x00, 0x00); //SYNC0 and SYNC1 sync words
  //ELECHOUSE_cc1101.SpiWriteReg(CC1101_FSCTRL0, 0x00); //if you have a cheaper chip, you will need to adjust this, frequency offset
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_MCSM2, 0x07);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_MCSM1, 0x0C); //stay in receive mode after getting a "packet"
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_WOREVT1, 0x87);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_WOREVT0, 0x6B);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_WORCTRL, 0xFB);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_RCCTRL1, 0x41);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_RCCTRL0, 0x00);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_PTEST, 0x7F);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_AGCTEST, 0x3F);
  

  //These values come from LSatan Calibration tool. RTLSDR required. https://github.com/LSatan/CC1101-Debug-Service-Tool/tree/master/Calibrate_frequency
  //setClb must be called before setMHz is called the first time
  ELECHOUSE_cc1101.setClb(1,13,15); //pink case
  ELECHOUSE_cc1101.setClb(2,16,19);
  ELECHOUSE_cc1101.setClb(3,33,38);
  ELECHOUSE_cc1101.setClb(4,38,39);
  
  
  ELECHOUSE_cc1101.setCCMode(0);  //any value besides 1
  ELECHOUSE_cc1101.setModulation(2); //modulation is ASK by default
  //ELECHOUSE_cc1101.setIdle();
  ELECHOUSE_cc1101.setPA(12); 
  
  ELECHOUSE_cc1101.setRxBW(58);
  ELECHOUSE_cc1101.setMHZ(315.00);
  ELECHOUSE_cc1101.setDRate(baud);
  ELECHOUSE_cc1101.SpiWriteReg(CC1101_PKTCTRL0, 0x02);  //set to infinite packet length mode

  ELECHOUSE_cc1101.SetRx(); //start listening for signals

}

void loop(void){ 
  //cc1101_debug.debug();
  
  while(digitalRead(gdo0)){    
    bytesInBuffer = ELECHOUSE_cc1101.SpiReadStatus(CC1101_RXBYTES);

    if(counter == 0){
      decibel = ELECHOUSE_cc1101.getRssi();
      Serial.print("RSSI: ");Serial.print(decibel); Serial.print("  Bytes: ");Serial.println(bytesInBuffer);
    }

    //if(bytesInBuffer == 0){
      //Serial.println("Zero bytes in FIFO. GDO shouldn't be asserted, you shouldn't be seeing this message");
      //delayMicroseconds(1000);
      //continue;
    //}

    if(bytesInBuffer & 0x80){
      Serial.println("FIFO Buffer overflow. Flushing buffer");
      ELECHOUSE_cc1101.setSidle();
      ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
      delayMicroseconds(100);
      ELECHOUSE_cc1101.SetRx();
      break;
    }
    
    ELECHOUSE_cc1101.SpiReadBurstReg(CC1101_RXFIFO, smallRXarray, bytesInBuffer);
    counter++;
    
    if((bigRXsize + bytesInBuffer) > 1024 ){
      Serial.println("Big array can't fit remaining bytes");
      break;
    }

    //copy small rx_FIFO array into big array
    for(int x = 0; x < bytesInBuffer; x++){
      bigRXarray[x + bigRXsize] = smallRXarray[x];
    }

    bigRXsize = bigRXsize + bytesInBuffer;
    memset(smallRXarray, 0, 64);
    delayMicroseconds(baudDelay);  //wait for 4+ more bytes (FIFO GDO threshold) to be received, consider baud rate
    //this value is important to catch the entire 16 byte packet. While loop is going faster than the radio baud???
    //delayMicroseconds(20000); //newer remote
  }

  
  if(counter > 0){
    packetChecker = true;
    for(int y=0; y < bigRXsize; y++){
      if( (bigRXarray[y] != 0x00) && (bigRXarray[y] != 0x08) && (bigRXarray[y] != 0x0E) && (bigRXarray[y] != 0xE0) && (bigRXarray[y] != 0x80)
          && (bigRXarray[y] != 0x8E) && (bigRXarray[y] != 0xE8) && (bigRXarray[y] != 0x88) && (bigRXarray[y] != 0xEE)){ packetChecker = false; }
    }

    if( (bigRXsize < 16) || (bigRXarray[12] != 0x80) || (bigRXarray[13] != 0x00)
         || (bigRXarray[14] != 0x00) || (bigRXarray[15] != 0x00) ){ packetChecker = false; }

    if(packetChecker == true){
      Serial.println();Serial.println("************************** Defiant packet detected **************************");
      Serial.print("Copy these values: 0x80, 0x00, 0x00, 0x00,");
      
      for(int x=0; x < 12; x++){
        Serial.print(" 0x");
        Serial.print(bigRXarray[x], HEX);
        if(x != 11){ Serial.print(","); }
      }
      Serial.println();Serial.println();
    }
    else{
      Serial.println("Not syncing. Press hold the button or different button again until synced");
    }
    
    memset(bigRXarray, 0, 1024);
    memset(smallRXarray, 0, 64);
    bigRXsize = 0;
    counter = 0;
  }
  
}
