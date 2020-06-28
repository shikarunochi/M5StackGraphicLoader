
#include <M5Stack.h>
#include "SD.h"
#include <M5StackUpdater.h>  // https://github.com/tobozo/M5Stack-SD-Updater/
#include <vector>
#include <random>
#include <algorithm>

#define GRAPHIC_LOADER_MAIN
#define CG_DIRECTORY "/cgData"

//__attribute__ ((always_inline)) inline static
//uint16_t swap565( uint8_t r, uint8_t g, uint8_t b) {
//  return ((b >> 3) << 8) | ((g >> 2) << 13) | ((g >> 5) | ((r >> 3) << 3));
//}

namespace PILOADER{
  void piLoad(File dataFile);
}

String preSelectDirectory = "";

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
}

void loop() {
  String targetDirectory = selectDirectory(preSelectDirectory);
  preSelectDirectory = targetDirectory;
  if (targetDirectory.startsWith("RANDOM")) {
    randomAllDirectory();
  } else {
    int drawMode = selectDrawMode(targetDirectory);

    if (drawMode == 1) {
      M5.Lcd.fillScreen(BLACK);     // CLEAR SCREEN
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.setTextSize(2);
      M5.Lcd.println(targetDirectory);
      M5.Lcd.print("Sequencial Mode\npreparing...");
      if (sequencialDraw(targetDirectory)) {
        delay(3000);
      }
    } else if (drawMode == 2) {
      M5.Lcd.fillScreen(BLACK);     // CLEAR SCREEN
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.setTextSize(2);
      M5.Lcd.println(targetDirectory);
      M5.Lcd.print("Random Mode\npreparing...");
      if (randomDraw(targetDirectory)) {
        delay(3000);
      }
    }
  }
  delay (100);

}

int countFile(File dirEntity, int countTimeMillis) {
  int fileCount = 0;
  long startMillis = millis();
  while (1) {
    long curMillis = millis();
    File entry =  dirEntity.openNextFile();
    if (!entry) { // no more files
      if (curMillis - startMillis < countTimeMillis) {
        delay(countTimeMillis - (curMillis - startMillis));
      }
      return fileCount;
    }
    //ファイルのみ取得
    if (!entry.isDirectory()) {
      fileCount++;
    }
    if (curMillis - startMillis > countTimeMillis) {
      return fileCount;
    }
  }
}

bool sequencialDraw(String targetDirectory) {
  File cgRoot;
  String cgRootDirectory = targetDirectory;
  bool endFlag = true;
  Serial.println("SD OPEN");
  cgRoot = SD.open(cgRootDirectory);

  File cgRootForCount = SD.open(cgRootDirectory);

  Serial.println("SD OPEN OK");
  int fileCount = 0;

  int totalFileCount = countFile(cgRootForCount, 1000);

  //Serial.print("COUNT OK:");
  //Serial.println(totalFileCount);
  //cgRoot.rewindDirectory();

  //Serial.println("rewindDirectory OK");
  boolean countEndFlag = false;
  while (1) {
    File entry =  cgRoot.openNextFile();
    if (!entry) { // no more files
      break;
    }
    //ファイルのみ取得
    if (!entry.isDirectory()) {
      fileCount++;
      String fileName = entry.name();
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(0, 220);
      M5.Lcd.setTextSize(1);
      M5.Lcd.printf("[%d/%d]", fileCount, totalFileCount);
      M5.Lcd.print(fileName);

      Serial.printf("[%d/%d]", fileCount, totalFileCount);
      Serial.println(fileName);

      fileName.toUpperCase();
      if (fileName.endsWith("MAG") == true) {
        magLoad(entry);
      } else if (fileName.endsWith("PIC") == true) {
        picLoad(entry);
      } else if (fileName.endsWith("PI") == true) {
        PILOADER::piLoad(entry);
      }
      if (countEndFlag == false) {
        int deltaFileCount = countFile(cgRootForCount, 1000);
        if (deltaFileCount > 0) {
          totalFileCount = totalFileCount + deltaFileCount;
        } else {
          countEndFlag = true;
        }
      } else {
        delay(1000);
      }
    }
    entry.close();
    M5.update();
    if (M5.BtnC.isPressed()) {
      drawPause();
    }
    if (M5.BtnB.isPressed()) {
      endFlag = false;
      break;
    }
  }
  cgRoot.close();
  cgRootForCount.close();
  return endFlag;
}

int addFileName(File dirEntity, String cgRootDirectory, std::vector<String> *fileNameList, int countTimeMillis) {
  int fileCount = 0;
  long startMillis = millis();
  while (1) {
    long curMillis = millis();
    File entry =  dirEntity.openNextFile();
    if (!entry) { // no more files
      if (curMillis - startMillis < countTimeMillis) {
        delay(countTimeMillis - (curMillis - startMillis));
      }
      return fileCount;
    }
    //ファイルのみ取得
    if (!entry.isDirectory()) {
      String fileName = entry.name();
      fileName = fileName.substring(cgRootDirectory.length());
      fileNameList->push_back(fileName);
      fileCount++;
    }
    if (curMillis - startMillis > countTimeMillis) {
      return fileCount;
    }
  }
}

bool randomDraw(String targetDirectory) {
  File cgRoot;
  bool endFlag = true;
  std::vector<String> fileNameList;
  String cgRootDirectory = targetDirectory;
  cgRoot = SD.open(cgRootDirectory);

  int deltaFileCount = addFileName(cgRoot, cgRootDirectory, &fileNameList, 1000);

  boolean countEndFlag = false;

  //ファイル追加しつつ、現在取得しているファイル内でランダム表示
  int totalFileCount = fileNameList.size();

  int fileCount = 0;
  while (1) {
    int fileNameCount = fileNameList.size();
    if (fileNameCount == 0) {
      break;
    }
    int targetFileIndex = random(fileNameCount);
    String fileName = fileNameList[targetFileIndex];
    fileNameList.erase(fileNameList.begin() + targetFileIndex);

    File entry =  SD.open(cgRootDirectory + fileName);
    if (!entry) { // file not exist
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
    } else if (fileName.endsWith("PIC") == true) {
      picLoad(entry);
    } else if (fileName.endsWith("PI") == true) {
      PILOADER::piLoad(entry);
    }
    if (countEndFlag == false) {
      deltaFileCount = addFileName(cgRoot, cgRootDirectory, &fileNameList, 1000);
      if (deltaFileCount > 0) {
        totalFileCount = totalFileCount + deltaFileCount;
      } else {
        countEndFlag = true;
      }
    } else {
      delay(1000);
    }
    entry.close();
    M5.update();
    if (M5.BtnC.isPressed()) {
      drawPause();
    }
    if (M5.BtnB.isPressed()) {
      endFlag = false;
      break;
    }
  }
  cgRoot.close();
  return endFlag;
}

void drawPause() {
  while (1) {
    M5.update();
    if (M5.BtnC.isPressed() == false) {
      break;
    }
    delay(100);
  }
  while (1) {
    M5.update();
    if (M5.BtnC.isPressed()) {
      return;
    }
    delay(100);
  }
}

void randomAllDirectory() {
  File cgRoot = SD.open("/");
  std::vector<String> directoryList;
  while (1) {
    File entry =  cgRoot.openNextFile();
    if (!entry) { // no more files
      break;
    }
    //ディレクトリのみ取得
    if (entry.isDirectory()) {
      String directoryName = entry.name();
      directoryName.toUpperCase();
      if (directoryName.startsWith("/CG")) {
        directoryList.push_back(entry.name());
      }
    }
  }
  cgRoot.close();
  std::random_device seed_gen;
  std::mt19937 engine(seed_gen());
  std::shuffle(directoryList.begin(), directoryList.end(), engine);

  for (String directoryName : directoryList) {
    if (randomDraw(directoryName) == false) {
      break;
    }
  }

}

String selectDirectory(String preSelectDirectory) {

  File cgRoot = SD.open("/");

  std::vector<String> directoryList;

  while (1) {
    File entry =  cgRoot.openNextFile();
    if (!entry) { // no more files
      break;
    }
    //ディレクトリのみ取得
    if (entry.isDirectory()) {
      String directoryName = entry.name();
      directoryName.toUpperCase();
      if (directoryName.startsWith("/CG")) {
        directoryList.push_back(entry.name());
      }
    }
  }
  std::sort(directoryList.begin(), directoryList.end());

  directoryList.insert(directoryList.begin(), "RANDOM ALL");
  
  int cursorPos = 0;
  int directoryCount = directoryList.size();
  
  for (int directoryIndex = 0; directoryIndex < directoryCount; directoryIndex++) {
    if (preSelectDirectory.compareTo(directoryList[directoryIndex]) == 0) {
      cursorPos = directoryIndex;
      break;
    }
  }

  bool needUpdate = true;
  while (1) {
    M5.update();
    if (M5.BtnC.wasPressed()) {
      cursorPos++;
      if (cursorPos > directoryCount - 1) {
        cursorPos = 0;
      }
      needUpdate = true;
    }
    if (M5.BtnA.wasPressed()) {
      cursorPos--;
      if (cursorPos < 0) {
        cursorPos = directoryCount - 1;
      }
      needUpdate = true;
    }
    if (M5.BtnB.wasPressed()) {
      return  directoryList[cursorPos];
    }
    if (needUpdate == true) {
      M5.Lcd.fillScreen(BLACK);     // CLEAR SCREEN
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.setTextSize(2);

      int startIndex = 0;
      int endIndex;
      //カーソル位置がなるべく真ん中になるように。
      if (cursorPos > 6) {
        startIndex = cursorPos - 6;
      } else if (directoryCount - cursorPos < 6 ) {
        startIndex = directoryCount - cursorPos;
      }
      if (startIndex < 6) {
        startIndex = 0;
      }
      endIndex = startIndex + 12;
      if (endIndex > directoryCount - 1) {
        endIndex =  directoryCount - 1;
      }

      for (int index = startIndex; index <= endIndex; index++) {
        if (index == cursorPos) {
          M5.Lcd.setTextColor(TFT_GREEN);
        } else {
          M5.Lcd.setTextColor(TFT_WHITE);
        }
        String directoryName = directoryList[index];
        M5.Lcd.println(directoryName.substring(0, 25));
      }

      M5.Lcd.setTextColor(TFT_WHITE);
      M5.Lcd.drawRect(0, 240 - 19, 100, 18, TFT_WHITE);
      M5.Lcd.drawCentreString("U P", 53, 240 - 17, 1);
      M5.Lcd.drawRect(110, 240 - 19, 100, 18, TFT_WHITE);
      M5.Lcd.drawCentreString("SELECT", 159, 240 - 17, 1);
      M5.Lcd.drawRect(220, 240 - 19, 100, 18, TFT_WHITE);
      M5.Lcd.drawCentreString("DOWN", 266, 240 - 17, 1);

      needUpdate = false;
    }
    delay(100);
  }
  cgRoot.close();
}

int selectDrawMode(String targetDirectory) {
  M5.Lcd.fillScreen(BLACK);     // CLEAR SCREEN
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_GREEN);
  M5.Lcd.println(targetDirectory);

  M5.Lcd.setTextColor(TFT_WHITE);

  M5.Lcd.drawRect(0, 240 - 19, 100, 18, TFT_WHITE);
  M5.Lcd.drawCentreString("RAMDOM", 50, 240 - 17, 1);
  M5.Lcd.drawRect(110, 240 - 19, 100, 18, TFT_WHITE);
  M5.Lcd.drawCentreString("NORMAL", 163, 240 - 17, 1);
  M5.Lcd.drawRect(220, 240 - 19, 100, 18, TFT_WHITE);
  M5.Lcd.drawCentreString("CANCEL", 274, 240 - 17, 1);

  M5.Lcd.println();
  //ファイルを10個だけ出す。
  File cgRoot = SD.open(targetDirectory);
  int fileCount = 0;
  while (1) {
    File entry =  cgRoot.openNextFile();
    if (!entry) { // no more files
      break;
    }
    if (!entry.isDirectory()) {
      String fileName = entry.name();
      fileName = fileName.substring(targetDirectory.length() + 1);//ディレクトリ名は除いて表示
      fileName.substring(0, 25);
      M5.Lcd.print(" ");
      M5.Lcd.println(fileName);
      fileCount++;
      if (fileCount > 10) {
        break;
      }
    }
  }
  cgRoot.close();

  while (1) {
    M5.update();
    if (M5.BtnA.wasPressed()) {
      return 2;
    }
    if (M5.BtnB.wasPressed()) {
      return 1;
    }
    if (M5.BtnC.wasPressed()) {
      return 0;
    }
    delay(100);
  }
}
