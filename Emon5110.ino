/*
Emon5110
A Nokia 5110 display connected to a Arduino Mini Pro and RFM12B. 
Used as a disply for Openenergymonitor, http://openenergymonitor.org/emon/ and as 
temp/humidity sensor. 

Based on https://github.com/openenergymonitor/EmonGLCD/blob/master/HomeEnergyMonitor/HomeEnergyMonitor.ino

Schematic also in source folder. 

©Patrik Hermansson 2014
*/

// Nokia 5110 display
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Software SPI (slower updates, more flexible pin options):
// pin 7 - Serial clock out (SCLK)
// pin 6 - Serial data out (DIN)
// pin 5 - Data/Command select (D/C)
// pin 4 - LCD chip select (CS)
// pin 3 - LCD reset (RST)

Adafruit_PCD8544 display = Adafruit_PCD8544(4, 8, 7, 6, 5);

// DHT11 Humidity/Temp sensor
#include <dht.h>
#define dht_dpin A5 //no ; here. Set equal to channel sensor is on
dht DHT;

#include <JeeLib.h>   
// For RFM12B
#define MYNODE 17            // Should be unique on network, node ID 30 reserved for base station
#define RF_freq RF12_433MHZ     // frequency - match to same frequency as RFM12B module (change to 868Mhz or 915Mhz if appropriate)
#define group 210 

//---------------------------------------------------
// Data structures for transfering data between units
//---------------------------------------------------
typedef struct { int temp, power2, power3, Vrms; } PayloadTX;         // neat way of packaging data for RF comms
PayloadTX emontx;

typedef struct { int temperature, humidity; } PayloadGLCD;
PayloadGLCD emonglcd;
//-------------------------------------------------------------------------------------------- 
// Flow control
//-------------------------------------------------------------------------------------------- 
unsigned long last_emontx;                   // Used to count time from last emontx update
unsigned long last_emonbase;                   // Used to count time from last emontx update

// Time
String shour, smin;
int ihour, imin;

boolean answer;

void setup()   {
  Serial.begin(9600);
  Serial.println("Welcome to Emon5110!");
  delay(500); 				   //wait for power to settle before firing up the RF
  rf12_initialize(MYNODE, RF_freq,group);
  delay(100);	
  //wait for RF to settle befor turning on display
   
  Serial.println("Start lcd");
 
  // Start LCD
  display.begin();
  delay(500);
  // you can change the contrast around to adapt the display
  // for the best viewing!
  display.setContrast(50);

  display.display(); // show splashscreen
  delay(2000);
  display.clearDisplay();   // clears the screen and buffer
  
  // Backlight on
  pinMode(9, OUTPUT);     
  digitalWrite(9, HIGH);
  
  // DHT11 on
  pinMode(3, OUTPUT);     
  digitalWrite(3, HIGH);
  
  // draw a single pixel
  display.drawPixel(10, 10, BLACK);
  display.display();
  Serial.println("Dot is there?");
  delay(2000);
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(0,0);
  display.println("Hello, world!Are there a line break here?");
  display.setTextSize(2);
  display.println("Yes");
  display.setTextSize(1);
  display.println("So this was easy!");
  display.display();
  
  Serial.println("Done");
  
  delay(2000);
  display.clearDisplay();
  
  // Larger text as we move on
  display.setTextSize(3);

}
void loop() {
    // Send the values to base after the base has sent her message. 
    if (answer){
      delay(500);
      Serial.println("Calling base...");
      emonglcd.temperature = (int) (DHT.temperature*100);                          // set emonglcd payload
      emonglcd.humidity = (int) (DHT.humidity);
      rf12_sendNow(0, &emonglcd, sizeof emonglcd);                     //send temperature data via RFM12B using new rf12_sendNow wrapper -glynhudson
      //rf12_sendWait(2); 
      answer = false;
      //last_emonbase = millis();
    }
    
    // Read humidity and temp  
    DHT.read11(dht_dpin);
    
    // Print results on serial port...
    Serial.print("Current humidity = ");
    Serial.print(DHT.humidity);
    Serial.print("%  ");
    Serial.print("temperature = ");
    Serial.print(DHT.temperature); 
    Serial.println("C  ");

    // ...and on the 5110-display
    display.setTextColor(BLACK);
    display.setCursor(0,0);
    display.setTextSize(2);
    // Temp
    display.print(DHT.temperature,1);
    display.println("C");
    // Humidity
    display.setTextSize(2);
    display.print(DHT.humidity,0);
    display.println("%");
    display.display();
    // Time goes in on the third row. 
    display.print(shour);
    display.print(":");
    display.println(smin);
    display.display();
    delay(100);
      
    delay(5000);
    display.clearDisplay();

    
  // Signal from the emonTx received. Here we collect and format the current time.   
  if (rf12_recvDone())
  {
    if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)  // and no rf errors
    {
      int node_id = (rf12_hdr & 0x1F);
      
      // Node 16 is the emonBase. It sends out some date, for example the current time. 
      if (node_id == 16)			//Assuming 16 is the emonBase node ID
      {
        Serial.println("Got message from emonBase");
        //RTC.adjust(DateTime(2012, 1, 1, rf12_data[1], rf12_data[2], rf12_data[3]));
        emontx = *(PayloadTX*) rf12_data; 
        
        // Get time from server
        // Hour
        int hour = rf12_data[1];
        //shour = (char)hour;
        ihour = hour;
        Serial.print ("hour: ");
        Serial.println (hour);

        if (hour<10) {
          //Serial.println ("Less than 10");
          shour = "0" + String(hour);
        }  
        else {
          shour = String(hour);
        }
        //Serial.println (shour);
        // Minute
        int min = rf12_data[2];

        Serial.print ("M: ");        
        Serial.println (min);
        if (min<10) {
          smin = "0"+String(min);
          //Serial.print ("SM: ");        
          //Serial.println (smin);          
        }
        else { 
          smin = String(min);
          //Serial.print ("SM: ");        
          //Serial.println (smin);        
        }
        String time = shour + smin;
        int itime = time.toInt();
        Serial.println(itime);
        // Set flag that tells code to answer to this transmission. 
        answer = true;
        last_emonbase = millis();
      } 
      //Serial.print("node_id: ");
      //Serial.println(node_id);

    }
  }
}
//End

