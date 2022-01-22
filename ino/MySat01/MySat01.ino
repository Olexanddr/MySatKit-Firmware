#define CAMERA_MODEL_AI_THINKER
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif

#define CORE_DEBUG_LEVEL 3
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include <Wire.h>
#include <MPU9250_asukiaaa.h> // http://www.esp32learning.com/code/esp32-and-mpu-9250-sensor-example.php

// RTC
#include <RtcDS3231.h>

#include "Adafruit_INA219.h" //https://github.com/Makuna/Rtc
#include <Adafruit_BME280.h>

Adafruit_INA219 ina219;
MPU9250_asukiaaa mySensor(0x69);
Adafruit_BME280 bme;
float temperature;
float humidity;
float pressure;

// File system with html and css
// https://randomnerdtutorials.com/esp32-web-server-spiffs-spi-flash-file-system/

float shuntvoltage = 0;
float busvoltage = 0;
float current = 0;
float power = 0;

// I2C
#define I2C_SDA 15
#define I2C_SCL 13
TwoWire I2Cnew = TwoWire(0);

// RTC
RtcDS3231<TwoWire> Rtc(I2Cnew);
RtcDateTime currentTime;
RtcTemperature rtcTemperature;
String sTime;

float aX, aY, aZ, aSqrt, gX, gY, gZ, mDirection, mX, mY, mZ;
#define SEALEVELPRESSURE_HPA (1013.25)
int ph1,ph2,ph3,ph4;
// Replace with your network credentials
//const char* ssid = "LUNAR_WIFI";
//const char* password = "ElonMars2024?";
const char* ssid = "Tenda_C000C0";
const char* password = "12345678";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create an Event Source on /events
AsyncEventSource events("/events"); //https://randomnerdtutorials.com/esp32-web-server-sent-events-sse/

boolean isPhotoNeeded = false;
// Photo File Name to save in SPIFFS
//https://randomnerdtutorials.com/esp32-cam-take-photo-display-web-server/
#define FILE_PHOTO "/photo.jpg" 
File myFile;
// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void getSensorReadings()
{
  temperature = bme.readTemperature();
  // Convert temperature to Fahrenheit
  //temperature = 1.8 * bme.readTemperature() + 32;
  humidity = bme.readHumidity();
  pressure = bme.readPressure()/ 100.0F;
}

#include "log.h"

String processor(const String& var)
{
  getSensorReadings();
  currentTime = Rtc.GetDateTime();
  Log::printDateTimeS(currentTime);
  Log::printValuesBME();
  Log::printValues9250();
  Log::printValuesINA219();
  Serial.println(var);
  if(var == "TEMPERATURE"){
    return String(temperature);
  }
  else if(var == "HUMIDITY"){
    return String(humidity);
  }
  else if(var == "PRESSURE"){
    return String(pressure);
  }
  else if(var == "TIME"){
    return String(sTime);
  }
  else if(var == "AX"){
    return String(aX);
  }
  else if(var == "AY"){
    return String(aY);
  }
  else if(var == "AZ"){
    return String(aZ);
  }
  else if(var == "GX"){
    return String(gX);
  }
  else if(var == "GY"){
    return String(gY);
  }
  else if(var == "GZ"){
    return String(gZ);
  }
  else if(var == "MX"){
    return String(mX);
  }
  else if(var == "MY"){
    return String(mY);
  }
  else if(var == "MZ"){
    return String(mZ);
  }
   else if(var == "PH1"){
    return String(ph1);
  }
   else if(var == "PH2"){
    return String(ph2);
  }
   else if(var == "PH3"){
    return String(ph3);
  }
   else if(var == "PH4"){
    return String(ph4);
  }
   else if(var == "SHUNTVOLTAGE"){
    return String(shuntvoltage);
  }
   else if(var == "BUSVOLTAGE"){
    return String(busvoltage);
  }
   else if(var == "CURRENT"){
    return String(current);
  }
   else if(var == "POWER"){
    return String(power);
  }
 
  return String("No data");
}

/*
  Check if photo capture was successful
*/
bool checkPhoto( fs::FS &fs ) 
{
  Serial.println("checkPhoto");
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

/* 
  Capture Photo and Save it to SPIFFS
*/
void capturePhotoSaveSpiffs( void ) 
{
  Serial.println("capturePhotoSaveSpiffs");
  camera_fb_t * fb = NULL; // pointer
  bool isPictureCorrectly = 0; // Boolean indicating if the picture has been taken correctly

  do 
  {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) 
    {
      Serial.println("Camera capture failed");
      return;
    }
    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);
    // Insert the data in the photo file
    if (!file)
      Serial.println("Failed to open file in writing mode");
    else 
    {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);
   //  check if file has been correctly saved in SPIFFS
    isPictureCorrectly = checkPhoto(SPIFFS);
  } while ( !isPictureCorrectly );
}

void initServer()
{
 server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    isPhotoNeeded = true;
    request->send_P(200, "text/plain", "Taking Photo");
  });

  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });  

    // Handle Web Server Events
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  
  server.addHandler(&events);
  server.begin();
}

void initCamera(camera_config_t &config)
{
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
}

void initCamera()
{
  // OV2640 camera module
  camera_config_t config;
  initCamera(config);

  if (psramFound()) 
  {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } 
  else 
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) 
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
  else
    Serial.println("Camera init... OK");

}

void connectWiFI()
{
  String logn, pas;
  readConfig(SPIFFS, logn, pas);

  WiFi.begin(logn.c_str(), pas.c_str());
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    //Serial.println(logn+", "+pas);
  }
}

void sendEvents()
{
  events.send("ping",NULL,millis());
  events.send(String(temperature).c_str(),"temperature",millis());
  Serial.print("Send event: ");
  Serial.println(String(temperature).c_str());
  
  events.send(String(humidity).c_str(),"humidity",millis());
  events.send(String(pressure).c_str(),"pressure",millis());
  events.send(String(sTime).c_str(),"sTime",millis());
  
  events.send(String(aX).c_str(),"ax_id",millis());
  events.send(String(aY).c_str(),"ay_id",millis());
  events.send(String(aZ).c_str(),"az_id",millis());

  events.send(String(gX).c_str(),"gx_id",millis());
  events.send(String(gY).c_str(),"gy_id",millis());
  events.send(String(gZ).c_str(),"gz_id",millis());

  events.send(String(mX).c_str(),"mx_id",millis());
  events.send(String(mY).c_str(),"my_id",millis());
  events.send(String(mZ).c_str(),"mz_id",millis());

  events.send(String(ph1).c_str(),"ph1_id",millis());
  events.send(String(ph2).c_str(),"ph2_id",millis());
  events.send(String(ph3).c_str(),"ph3_id",millis());
  events.send(String(ph4).c_str(),"ph4_id",millis());
  
  events.send(String(shuntvoltage).c_str(),"shuntvoltage_id",millis());
  events.send(String(busvoltage).c_str(),"busvoltage_id",millis());
  events.send(String(current).c_str(),"current_id",millis());
  events.send(String(power).c_str(),"power_id",millis());
}

void initBME()
{
  Serial.print("Check BME...");
  bool status = bme.begin(0x76, &I2Cnew);  
  Log::checkStatus(status);
}

void initMPU9250()
{
  Serial.print("Check MPU...");
  mySensor.setWire(&I2Cnew);
  mySensor.beginAccel();
  mySensor.beginGyro();
  mySensor.beginMag();
  Serial.println(" TBD.");
} 

void initINA219()
{
  Serial.print("Check INA219...");
  bool status = ina219.begin(&I2Cnew);  
  Log::checkStatus(status);
}

void initSPIFFS()
{
  if (!SPIFFS.begin(true)) 
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else 
  {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
}

void takePhoto()
{
  if (isPhotoNeeded) 
  {
    capturePhotoSaveSpiffs();
    isPhotoNeeded = false;
  }
}

void writeConfig(fs::FS &fs){
  myFile = fs.open("/config.txt", FILE_WRITE);
  myFile.print("Tenda_C000C0,12345678");
  myFile.close();
  }

void readConfig(fs::FS &fs, String& logn, String&  pas){

   bool flag = true;
   myFile = fs.open("/config.txt", FILE_READ);
   if(!myFile){
    writeConfig(SPIFFS);
    }
   String textFile = myFile.readString();
   for(int i = 0; i < textFile.length(); i++){
       if (textFile[i] == ','){
          flag = false; 
       }else{
           if (flag){
               if (isalnum(textFile[i]) || textFile[i] == '_'){
                   logn +=textFile[i];
               }else{
                    writeConfig(SPIFFS);
                }
           }else{
               if(textFile[i] != ' ' || textFile[i]!='\n'){
                   pas+=textFile[i];
               }else{
                    writeConfig(SPIFFS);
                }
           }
       }
       
   }
   if(flag){
    writeConfig(SPIFFS);
    }
   myFile.close();
}

void setup() 
{
  Serial.begin(115200);
  delay(100);
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.println("");
  Serial.println("Start Init"); 

  I2Cnew.begin(I2C_SDA, I2C_SCL, 400000);  // Init I2C
  initSPIFFS();
  connectWiFI();
  initCamera();
  initServer();
  initBME();
  initMPU9250();
  initINA219();
  Rtc.Begin();

  Log::printSetupInfo();
}

void loop() 
{
  takePhoto();
  currentTime = Rtc.GetDateTime();
  rtcTemperature = Rtc.GetTemperature();
  getSensorReadings();

  // Send Events to the Web Server with the Sensor Readings
  sendEvents();
  Log::printPeriferiaInfo();
  delay(1000); 
}
