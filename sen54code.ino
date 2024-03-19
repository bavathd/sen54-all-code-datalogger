//Data Format in SD Card:
//CO2, SCD30 Temperature, SCD30 Humidity, SHT31 Temperature, SHT31 Humidity, PM1.0 μg/m^3, PM2.5 μg/m^3, PM4.0 μg/m^3, PM10.0 μg/m^3, Particles Count(#/cm3) <=0.5μm,
//Particles Count(#/cm3) >0.5μm, <1μm, Particles Count(#/cm3) >1.0μm, <2.5μm, Particles Count(#/cm3) >2.5μm, <4.0μm, Particles Count(#/cm3) >4.0μm, <10.0μm.


#include <Arduino.h>
#include <SensirionI2CSen5x.h>
#include <Wire.h>
String hostname = "Vyoman IAQ";
#define MAXBUF_REQUIREMENT 48

#if (defined(I2C_BUFFER_LENGTH) &&                 \
     (I2C_BUFFER_LENGTH >= MAXBUF_REQUIREMENT)) || \
    (defined(BUFFER_LENGTH) && BUFFER_LENGTH >= MAXBUF_REQUIREMENT)
#define USE_PRODUCT_INFO
#endif

#include <U8g2lib.h>
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

#include <ESP32WebServer.h>    //https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
#include <ESPmDNS.h>
#include <WiFi.h>
#include <HTTPClient.h>
WiFiClient client;

#include "CSS.h" //Includes headers of the web and de style file

#include "FS.h"
#include "SD.h" 
#include <SPI.h>

ESP32WebServer server(80);

bool   SD_present = false; //Controls if the SD card is present or not

#define servername "Vyoman" //Define the name to your server... 
char ssid[] = "Vyoman";      //  network SSID (name)
char pass[] = "Vyoman@CleanAirIndia";   //  network password
String Device_Token = "64abb8b4-678e-46a4-a78d-c99a27805b06";
int keyIndex = 0;         
char tagoServer[] = "api.tago.io";

#include "RTClib.h"
RTC_DS3231 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

#include "FS.h"
#include "SD.h"

#include <Wire.h>


SensirionI2CSen5x sen5x;

float nox = 0;
float voc = 0;
float t = 0;
float h = 0;
float pm1 = 0;
float pm2 = 0;
float pm4 = 0;
float pm10 = 0;
int count = 0;
String dataMessage;
String timeStamp;
int wifiTime = 0;
String Dates = "";

const int buttonPin = 32;

#define uS_TO_S_FACTOR 1000000  
#define TIME_TO_SLEEP  240


void updateRtc() {
  int ye = Dates.substring(0, 4).toInt();
  int mo = Dates.substring(4, 6).toInt();
  int da = Dates.substring(6, 8).toInt();
  int ho = Dates.substring(8, 10).toInt();
  int mi = Dates.substring(10, 12).toInt();
  int se = Dates.substring(12,14).toInt();          
  rtc.adjust(DateTime( ye , mo , da , ho , mi , se ));
  Serial.println("RTC adjusted! " + String(ye) + " " + String(mo) + " " +  String(da) + " " +  String(ho) + " " +  String(mi) + " " +  String(se));  
}

void print_wakeup_reason()
{
esp_sleep_wakeup_cause_t wakeup_reason;

wakeup_reason = esp_sleep_get_wakeup_cause();
Serial.println();
Serial.println();
Serial.println();
switch(wakeup_reason)
{
case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
}
}

void timeSet() {
  append_page_header();
  webpage += F("<h3>Update RTC (YYYYMMDDHHMMSS)</h3>"); 
  webpage += F("<FORM action='/get' method='get'>");
  webpage += F("<input type='text'name='setTime' id='setTime' value='' style='width:25%'>");
  webpage += F("<button class='buttons' style='width:10%' type='submit'>Update</button><br><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html",webpage);
}

float  batLevel(){
 
  
  float R1 = 100000;
  float R2 = 47000;
  float battv = ((float)analogRead(33) * 3.3 / 4095);
  float batteryVolt = battv * ( R2+R1) / R2 ;
  batteryVolt = batteryVolt * 0.4736;

   Serial.println(batteryVolt);
   delay(100);
  
  
  return(batteryVolt);
}

void readPM()
{

  uint16_t error;
  char errorMessage[256];
  float tempOffset = 0.0;
    error = sen5x.setTemperatureOffsetSimple(tempOffset);
    if (error) {
        Serial.print("Error trying to execute setTemperatureOffsetSimple(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("Temperature Offset set to ");
        Serial.print(tempOffset);
        Serial.println(" deg. Celsius (SEN54/SEN55 only");
    }

    // Start Measurement
    error = sen5x.startMeasurement();
    if (error) {
        Serial.print("Error trying to execute startMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
 

    delay(1000);

    // Read Measurement
    float massConcentrationPm1p0;
    float massConcentrationPm2p5;
    float massConcentrationPm4p0;
    float massConcentrationPm10p0;
    float ambientHumidity;
    float ambientTemperature;
    float vocIndex;
    float noxIndex;

    error = sen5x.readMeasuredValues(
        massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
        massConcentrationPm10p0, ambientHumidity, ambientTemperature, vocIndex,
        noxIndex);

    if (error) {
        Serial.print("Error trying to execute readMeasuredValues(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    } else {
        Serial.print("MassConcentrationPm1p0:");
        Serial.print(massConcentrationPm1p0);
        Serial.print("\t");
        Serial.print("MassConcentrationPm2p5:");
        Serial.print(massConcentrationPm2p5);
        Serial.print("\t");
        Serial.print("MassConcentrationPm4p0:");
        Serial.print(massConcentrationPm4p0);
        Serial.print("\t");
        Serial.print("MassConcentrationPm10p0:");
        Serial.print(massConcentrationPm10p0);
        Serial.print("\t");
        Serial.print("AmbientHumidity:");
        if (isnan(ambientHumidity)) {
            Serial.print("n/a");
        } else {
            Serial.print(ambientHumidity);
        }
        Serial.print("\t");
        Serial.print("AmbientTemperature:");
        if (isnan(ambientTemperature)) {
            Serial.print("n/a");
        } else {
            Serial.print(ambientTemperature);
        }
        Serial.print("\t");
        Serial.print("VocIndex:");
        if (isnan(vocIndex)) {
            Serial.print("n/a");
        } else {
            Serial.print(vocIndex);
        }
        Serial.print("\t");
        Serial.print("NoxIndex:");
        if (isnan(noxIndex)) {
            Serial.println("n/a");
        } else {
            Serial.println(noxIndex);
        }
    }


    pm1 = massConcentrationPm1p0;
    pm2 = massConcentrationPm2p5;
    pm4 = massConcentrationPm4p0;
    pm10 =massConcentrationPm1p0;
    h = ambientHumidity;
    t = ambientTemperature;
    voc = vocIndex;
    nox = noxIndex;
    

  
  delay(1000);
  
  
  error = sen5x.stopMeasurement();
    if (error) {
        Serial.print("Error trying to execute startMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }

}


void setup()
{
Serial.begin(115200);
pinMode(buttonPin, INPUT_PULLUP);
pinMode(33, INPUT);

float batvalue = batLevel();

for(int i =0; i<15; i++) {
 batvalue = batLevel();
}
   
   

if(batvalue > 2.7) {

Wire.begin(); 

delay(500);  


    Wire.begin();

    sen5x.begin(Wire);

    uint16_t error;
    char errorMessage[256];
    error = sen5x.deviceReset();
    if (error) {
        Serial.print("Error trying to execute deviceReset(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }

  readPM();

Serial.print(F("Initializing SD card..."));

delay(500);  
u8g2.begin();


  

  u8g2.clearBuffer();         // clear the internal memory
  u8g2.setFont(u8g2_font_ncenB10_tr); // choose a suitable font  
  u8g2.drawStr(3, 15, "Initializing");
  u8g2.sendBuffer();
  
  
delay(500);

  
//see if the card is present and can be initialised.
//Note: Using the ESP32 and SD_Card readers requires a 1K to 4K7 pull-up to 3v3 on the MISO line, otherwise may not work
if (!SD.begin(4)) 
{
Serial.println("SD Card Initialization Failed!");
  u8g2.drawStr(3, 30, "SD Failed");
  u8g2.sendBuffer();
} 
else
{
Serial.println(F("Card initialised... file access enabled..."));
  u8g2.drawStr(3, 30, "SD Enabled");
  u8g2.sendBuffer();
SD_present = true; 
}
  
/*********  Server Commands  **********/
delay(500);


if (! rtc.begin()) 
{
Serial.println("Couldn't find RTC");
  u8g2.drawStr(3, 45, "RTC Error");
  u8g2.sendBuffer();
}
  u8g2.drawStr(3, 45, "RTC Enabled");
  u8g2.sendBuffer();

//Uncomment to adjust the date and time. Comment it again after uploading in the node.
//rtc.adjust(DateTime(2022, 5, 3, 11, 20, 0));
 print_wakeup_reason();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds"); 


delay(1000);

u8g2.clearBuffer();
u8g2.sendBuffer();
}
else 
{
  u8g2.clearBuffer();
 delay(600);
 u8g2.sendBuffer();
 u8g2.setFontDirection(1);
 u8g2.setFontMode(5);
 u8g2.setFont(u8g2_font_battery19_tn);
 u8g2.drawStr(50, 20, "0");
 u8g2.sendBuffer();
                    
 
  delay(3000);
  esp_deep_sleep_start();
}
}

void loop()
{
bool datack= false;
bool onCondition = true;
bool lowBattery =  false;
u8g2.setPowerSave(0); 



delay(500);

float batvalue = batLevel();
if(batvalue <= 2.7) {
 
 onCondition = false;
  
 u8g2.clearBuffer();
 delay(600);
 u8g2.sendBuffer();
 u8g2.setFontDirection(1);
 u8g2.setFontMode(5);
 u8g2.setFont(u8g2_font_battery19_tn);
 u8g2.drawStr(50, 20, "0");
 u8g2.sendBuffer();
                    
 
  delay(3000);
  esp_deep_sleep_start();
}
else if( batvalue<= 3.0 && batvalue >=2.8) {
  onCondition = true;
  lowBattery =  true;
  delay(100);
  
}
else 
{
  onCondition = true;
   
}
if(onCondition){
// check if the pushbutton is pressed. If it is, the buttonState is HIGH:
if (digitalRead(buttonPin) == 0) 
{
 dataTransfer();  
}
char data[32] = {0};

if(lowBattery) 
{
  for(int j=0; j<3; j++){
  u8g2.clearBuffer();
                    
   u8g2.sendBuffer();
   delay(600);
   u8g2.setFontDirection(1);
   u8g2.setFontMode(5);
   u8g2.setFont(u8g2_font_battery19_tn);
   u8g2.drawStr(50, 20, "2");
   u8g2.sendBuffer();
   delay(400);
   }
   u8g2.setFontMode(1);
   u8g2.setFontDirection(0);
   u8g2.setFont(u8g2_font_6x13B_tf); //u8g2_font_cu12_tr
   u8g2.clearBuffer();
   u8g2.sendBuffer();
    
  }

  else {

  u8g2.drawStr(3, 30, "Sampling....");
  u8g2.sendBuffer();
  datack = true;
  }


    if (datack){
        readTime(); 
        Serial.print("noxIndex:");
        Serial.print(nox);
          
          memset(data, 0, sizeof(data));
          sprintf(data, "nox=%dPPM", nox);
          u8g2.drawStr(3, 30, data);
          
          Serial.print(" temp(C):");
          //Serial.print(airSensor.getTemperature(), 1);
          Serial.print(t);   
        
          memset(data, 0, sizeof(data));
          sprintf(data, "T=%.2fC", t);
          u8g2.drawStr(3, 45, data); 

          Serial.print(" humidity(%):");
          //Serial.print(airSensor.getHumidity(), 1);
          Serial.print(h);
          
          memset(data, 0, sizeof(data));
          snprintf(data, 64, "RH=%.2f%%",h);
          u8g2.drawStr(3, 60, data);   

          u8g2.sendBuffer(); 

          delay(3000);

      Serial.println();

      u8g2.clearBuffer();

      readTime(); 
      float valbat = batLevel();
      memset(data, 0, sizeof(data));
      sprintf(data, "vocIndex=%dPPM", voc);
      u8g2.drawStr(3, 30, data);
      
      snprintf(data, 64, "BatV=%.4f", valbat);
      u8g2.drawStr(3, 60, data);   
      u8g2.sendBuffer(); 
      delay(3000);

      u8g2.clearBuffer();

      memset(data, 0, sizeof(data));
      snprintf(data, 64, "PM1.0=%.1f", pm1);
      u8g2.drawStr(3, 15, data); 

      memset(data, 0, sizeof(data));
      snprintf(data, 64, "PM2.5=%.1f", pm2);
      u8g2.drawStr(3, 30, data); memset(data, 0, sizeof(data));

      memset(data, 0, sizeof(data));
      snprintf(data, 64, "PM4.0=%.1f", pm4);
      u8g2.drawStr(3, 45, data); 

      memset(data, 0, sizeof(data));
      snprintf(data, 64, "PM10.0=%.1f", pm10);
      u8g2.drawStr(3, 60, data); 

      u8g2.sendBuffer(); 
      delay(3000);

      logSDCard(); 

      Serial.println();

      delay(1000);
      u8g2.clearBuffer();

      delay(1000);

      }


    u8g2.setPowerSave(1); 
    esp_deep_sleep_start();
    }
  }
void dataTransfer()
{
u8g2.clearBuffer();
u8g2.drawStr(3, 15, "FTP Enabled"); 
u8g2.sendBuffer(); 

if(WiFi.status() != WL_CONNECTED)
{
connectWiFi(); 

  

server.on("/",         SD_dir);
server.on("/upload",   File_Upload);
server.on("/setTime",  timeSet);
server.on("/get", HTTP_GET,[](){
 
  
    server.send(200, "text/plain", "this works as well");
    
    Serial.println(server.arg(0));
    Dates = server.arg(0);
    Serial.println(server.arg(0));
    updateRtc();
    });
server.on("/fupload",  HTTP_POST,[](){ server.send(200);}, handleFileUpload);
server.begin();

//Set your preferred server name, if you use "mcserver" the address would be http://mcserver.local/
/*if (!MDNS.begin(servername)) 
{          
Serial.println(F("Error setting up MDNS responder!")); 
ESP.restart(); 
}*/
}
for(;;){
server.handleClient();
}
}

void connectWiFi() 
{
WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
WiFi.setHostname(hostname.c_str()); //define hostname

WiFi.begin(ssid, pass);
Serial.println("Connecting");
u8g2.drawStr(3, 30, "Connecting WiFi"); 
u8g2.sendBuffer(); 
while(WiFi.status() != WL_CONNECTED) 
{
delay(1000);
Serial.print(".");
wifiTime++;
if (wifiTime > 20)
{
Serial.println("WiFi not connected. Locally logged");
}
}
Serial.println("");
Serial.print("Connected to WiFi network with IP Address: ");
Serial.println(WiFi.localIP());
u8g2.drawStr(3, 45, "IP:"); 
String ips = WiFi.localIP().toString();
const char* ip = ips.c_str();
u8g2.drawStr(3, 60, ip); 
u8g2.sendBuffer(); 
}



void readTime()
{
DateTime now = rtc.now();
Serial.print(now.year(), DEC);
Serial.print('/');
Serial.print(now.month(), DEC);
Serial.print('/');
Serial.print(now.day(), DEC);
Serial.print(" (");
Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
Serial.print(") ");
Serial.print(now.hour(), DEC);
Serial.print(':');
Serial.print(now.minute(), DEC);
Serial.print(':');
Serial.print(now.second(), DEC);
Serial.println();
timeStamp = String(now.day()) + "/" + String(now.month())+ "/" + String(now.year()) + "," + String(now.hour())+ ":" + String(now.minute()) + ":" + String(now.second()) + ", " ;
const char* ts = timeStamp.c_str();

u8g2.drawStr(3, 15, ts);  
u8g2.sendBuffer();
 
}

void logSDCard() 
{
dataMessage =  timeStamp + String(t)+ "," + String(h) + "," + String(voc) + "," + String(nox) + "," + String(pm1) + "," + String(pm2) + "," + String(pm4) + 
"," + String(pm10) + "\r\n";
Serial.print("Save data: ");
Serial.println(dataMessage);
appendFile(SD, "/VM-Indoor-01.txt", dataMessage.c_str());
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) 
{
Serial.printf("Writing file: %s\n", path);
File file = fs.open(path, FILE_WRITE);
if(!file) {
Serial.println("Failed to open file for writing");
return;
}
if(file.print(message)) {
Serial.println("File written");
} 
else 
{
Serial.println("Write failed");
}
file.close();
}


// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) 
{
Serial.printf("Appending to file: %s\n", path);
File file = fs.open(path, FILE_APPEND);
if(!file) {
Serial.println("Failed to open file for appending");
return;
}
if(file.print(message)) {
Serial.println("Message appended");
delay(1000);
} 
else 
{
Serial.println("Append failed");
}
file.close();
}



//Initial page of the server web, list directory and give you the chance of deleting and uploading
void SD_dir()
{
  if (SD_present) 
  {
    //Action acording to post, dowload or delete, by MC 2022
    if (server.args() > 0 ) //Arguments were received, ignored if there are not arguments
    { 
      Serial.println(server.arg(0));
  
      String Order = server.arg(0);
      Serial.println(Order);
      
      if (Order.indexOf("download_")>=0)
      {
        Order.remove(0,9);
        SD_file_download(Order);
        Serial.println(Order);
      }
  
      if ((server.arg(0)).indexOf("delete_")>=0)
      {
        Order.remove(0,7);
        SD_file_delete(Order);
        Serial.println(Order);
      }
    }

    File root = SD.open("/");
    if (root) {
      root.rewindDirectory();
      SendHTML_Header();    
      webpage += F("<table align='center'>");
      webpage += F("<tr><th>Name/Type</th><th style='width:20%'>Type File/Dir</th><th>File Size</th></tr>");
      printDirectory("/",0);
      webpage += F("</table>");
      SendHTML_Content();
      root.close();
    }
    else 
    {
      SendHTML_Header();
      webpage += F("<h3>No Files Found</h3>");
    }
    append_page_footer();
    SendHTML_Content();
    SendHTML_Stop();   //Stop is needed because no content length was sent
  } else ReportSDNotPresent();
}

//Upload a file to the SD
void File_Upload()
{
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>"); 
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:25%' type='file' name='fupload' id = 'fupload' value=''>");
  webpage += F("<button class='buttons' style='width:10%' type='submit'>Upload File</button><br><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html",webpage);
}

//Prints the directory, it is called in void SD_dir() 
void printDirectory(const char * dirname, uint8_t levels)
{
  
  File root = SD.open(dirname);

  if(!root){
    return;
  }
  if(!root.isDirectory()){
    return;
  }
  File file = root.openNextFile();

  int i = 0;
  while(file){
    if (webpage.length() > 1000) {
      SendHTML_Content();
    }
    if(file.isDirectory()){
      webpage += "<tr><td>"+String(file.isDirectory()?"Dir":"File")+"</td><td>"+String(file.name())+"</td><td></td></tr>";
      printDirectory(file.name(), levels-1);
    }
    else
    {
      webpage += "<tr><td>"+String(file.name())+"</td>";
      webpage += "<td>"+String(file.isDirectory()?"Dir":"File")+"</td>";
      int bytes = file.size();
      String fsize = "";
      if (bytes < 1024)                     fsize = String(bytes)+" B";
      else if(bytes < (1024 * 1024))        fsize = String(bytes/1024.0,3)+" KB";
      else if(bytes < (1024 * 1024 * 1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
      else                                  fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
      webpage += "<td>"+fsize+"</td>";
      webpage += "<td>";
      webpage += F("<FORM action='/' method='post'>"); 
      webpage += F("<button type='submit' name='download'"); 
      webpage += F("' value='"); webpage +="download_"+String(file.name()); webpage +=F("'>Download</button>");
      webpage += "</td>";
      webpage += "<td>";
      webpage += F("<FORM action='/' method='post'>"); 
      webpage += F("<button type='submit' name='delete'"); 
      webpage += F("' value='"); webpage +="delete_"+String(file.name()); webpage +=F("'>Delete</button>");
      webpage += "</td>";
      webpage += "</tr>";

    }
    file = root.openNextFile();
    i++;
  }
  file.close();

 
}

//Download a file from the SD, it is called in void SD_dir()
void SD_file_download(String filename)
{
  if (SD_present) 
  { 
    File download = SD.open("/"+filename);
    if (download) 
    {
      server.sendHeader("Content-Type", "text/text");
      server.sendHeader("Content-Disposition", "attachment; filename="+filename);
      server.sendHeader("Connection", "close");
      server.streamFile(download, "application/octet-stream");
      download.close();
    } else ReportFileNotPresent("download"); 
  } else ReportSDNotPresent();
}

//Handles the file upload a file to the SD
File UploadFile;
//Upload a new file to the Filing system
void handleFileUpload()
{ 
  HTTPUpload& uploadfile = server.upload(); //See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
                                            //For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
  if(uploadfile.status == UPLOAD_FILE_START)
  {
    String filename = uploadfile.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("Upload File Name: "); Serial.println(filename);
    SD.remove(filename);                         //Remove a previous version, otherwise data is appended the file again
    UploadFile = SD.open(filename, FILE_WRITE);  //Open the file for writing in SD (create it, if doesn't exist)
    filename = String();
  }
  else if (uploadfile.status == UPLOAD_FILE_WRITE)
  {
    if(UploadFile) UploadFile.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
  } 
  else if (uploadfile.status == UPLOAD_FILE_END)
  {
    if(UploadFile)          //If the file was successfully created
    {                                    
      UploadFile.close();   //Close the file again
      Serial.print("Upload Size: "); Serial.println(uploadfile.totalSize);
      webpage = "";
      append_page_header();
      webpage += F("<h3>File was successfully uploaded</h3>"); 
      webpage += F("<h2>Uploaded File Name: "); webpage += uploadfile.filename+"</h2>";
      webpage += F("<h2>File Size: "); webpage += file_size(uploadfile.totalSize) + "</h2><br><br>"; 
      webpage += F("<a href='/'>[Back]</a><br><br>");
      append_page_footer();
      server.send(200,"text/html",webpage);
    } 
    else
    {
      ReportCouldNotCreateFile("upload");
    }
  }
}

//Delete a file from the SD, it is called in void SD_dir()
void SD_file_delete(String filename) 
{ 
  if (SD_present) { 
    SendHTML_Header();
    File dataFile = SD.open("/"+filename, FILE_READ); //Now read data from SD Card 
    if (dataFile)
    {
      if (SD.remove("/"+filename)) {
        Serial.println(F("File deleted successfully"));
        webpage += "<h3>File '"+filename+"' has been erased</h3>"; 
        webpage += F("<a href='/'>[Back]</a><br><br>");
      }
      else
      { 
        webpage += F("<h3>File was not deleted - error</h3>");
        webpage += F("<a href='/'>[Back]</a><br><br>");
      }
    } else ReportFileNotPresent("delete");
    append_page_footer(); 
    SendHTML_Content();
    SendHTML_Stop();
  } else ReportSDNotPresent();
} 

//SendHTML_Header
void SendHTML_Header()
{
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate"); 
  server.sendHeader("Pragma", "no-cache"); 
  server.sendHeader("Expires", "-1"); 
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); 
  server.send(200, "text/html", ""); //Empty content inhibits Content-length header so we have to close the socket ourselves. 
  append_page_header();
  server.sendContent(webpage);
  webpage = "";
}

//SendHTML_Content
void SendHTML_Content()
{
  server.sendContent(webpage);
  webpage = "";
}

//SendHTML_Stop
void SendHTML_Stop()
{
  server.sendContent("");
  server.client().stop(); //Stop is needed because no content length was sent
}

//ReportSDNotPresent
void ReportSDNotPresent()
{
  SendHTML_Header();
  webpage += F("<h3>No SD Card present</h3>"); 
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//ReportFileNotPresent
void ReportFileNotPresent(String target)
{
  SendHTML_Header();
  webpage += F("<h3>File does not exist</h3>"); 
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//ReportCouldNotCreateFile
void ReportCouldNotCreateFile(String target)
{
  SendHTML_Header();
  webpage += F("<h3>Could Not Create Uploaded File (write-protected?)</h3>"); 
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}

//File size conversion
String file_size(int bytes)
{
  String fsize = "";
  if (bytes < 1024)                 fsize = String(bytes)+" B";
  else if(bytes < (1024*1024))      fsize = String(bytes/1024.0,3)+" KB";
  else if(bytes < (1024*1024*1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
  else                              fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
  return fsize;
}
