
#include <M5Stack.h>
#include "SD.h"
#include <M5StackUpdater.h>  // https://github.com/tobozo/M5Stack-SD-Updater/
#include <vector>
#include <random>
#include <algorithm>

#define GRAPHIC_LOADER_MAIN
#define CG_DIRECTORY "/cgData"

__attribute__ ((always_inline)) inline static 
uint16_t swap565( uint8_t r, uint8_t g, uint8_t b) {
  return ((b >> 3) << 8) | ((g >> 2) << 13) | ((g >> 5) | ((r>>3)<<3));
}

void setup(void) {
  M5.begin(); 
  if (digitalRead(BUTTON_A_PIN) == 0) {
    Serial.println("Will Load menu binary");
    updateFromFS(SD);
    ESP.restart();
  }
  M5.Lcd.setBrightness(200);    // BRIGHTNESS = MAX 255
  M5.Lcd.fillScreen(BLACK);     // CLEAR SCREEN
  SD.begin();
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.print("A: Start / Stop");
}

void loop() {
  M5.update();
  if(M5.BtnA.wasReleased()){ 
    M5.Lcd.fillScreen(BLACK);     // CLEAR SCREEN
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextSize(2);
    M5.Lcd.print("preparing...");
    randomDraw();
  }
  delay(100);
}

void randomDraw() {
  File cgRoot;
  std::vector<String> fileNameList; 
  String cgRootDirectory = CG_DIRECTORY;
  cgRoot = SD.open(cgRootDirectory);
  while (1) {
    File entry =  cgRoot.openNextFile();
    if (!entry) { // no more files
      break;
    }
    //ファイルのみ取得
    if (!entry.isDirectory()) {
      String fileName = entry.name();
      //CG_DIRECTORYは飛ばす
      fileName = fileName.substring(cgRootDirectory.length());
      fileNameList.push_back(fileName);
    }    
  }
  cgRoot.close();
  
  //ランダムソート
  std::mt19937 random( millis() );
  std::shuffle( fileNameList.begin(), fileNameList.end(), random );

  int totalFileCount = fileNameList.size();

  int fileCount = 0;
  for(auto fileName:fileNameList){
    File entry =  SD.open(cgRootDirectory + fileName);
    if (!entry) { // no more files
      continue;
    }
    fileCount++;
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 220);
    M5.Lcd.setTextSize(1);
    M5.Lcd.printf("[%d/%d]", fileCount, totalFileCount);
    M5.Lcd.print(cgRootDirectory + fileName);

    Serial.printf("[%d/%d]", fileCount, totalFileCount);
    Serial.println(cgRootDirectory + fileName);
    
    fileName.toUpperCase();
    if (fileName.endsWith("MAG") == true) {
      magLoad(entry);
      delay(1000);
    }else if (fileName.endsWith("PIC") == true) {
      picLoad(entry);
      delay(1000);
    }
    entry.close();
    M5.update();
    if(M5.BtnA.wasReleased()){ 
      break;
    }
  }

}
