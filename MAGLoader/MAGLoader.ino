// MAG表示ロジックは、https://emk.name/2015/03/magjs.html で公開されているロジックを移植したものです。

#include <M5Stack.h>
#include "SD.h"
#include <M5StackUpdater.h>  // https://github.com/tobozo/M5Stack-SD-Updater/

#include "esp_heap_alloc_caps.h"

#define MAG_DIRECTORY "/mag"

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
  randomDraw();
}

void randomDraw() {
  File magRoot;
  magRoot = SD.open(MAG_DIRECTORY);
  int fileCount = 0;
  while (1) {
    File entry =  magRoot.openNextFile();
    if (!entry) { // no more files
      break;
    }
    //ファイルのみ取得
    if (!entry.isDirectory()) {
      String fileName = entry.name();
      fileName.toUpperCase();
      if (fileName.endsWith("MAG") == true) {
        magLoad(entry);
        delay(1000);
      }
    }
    entry.close();
  }
  magRoot.close();
}

void magLoad(File dataFile) {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 220);
  M5.Lcd.setTextSize(1);
  String fileName = dataFile.name();

  String buf = "";

  while (dataFile.available() && buf.length() < 8) {
    char nextChar = char(dataFile.read());
    buf += nextChar;
  }
  if (buf.startsWith("MAKI02  ") == false) {
    //MAG画像ではない
    M5.Lcd.print(fileName);
    M5.Lcd.print(" is Not MAG Format.");
    return;
  }
  M5.Lcd.print(fileName);

  //ヘッダ先頭まで読み捨て
  int headerOffset = 30;
  dataFile.seek(headerOffset, SeekSet);
  while (dataFile.available()) {
    if (dataFile.read() == 0) {
      break;
    }
    headerOffset++;
  }

  dataFile.seek( headerOffset, SeekSet);
  uint8_t top = readUint8(dataFile);
  uint8_t machine = readUint8(dataFile);
  uint8_t flags = readUint8(dataFile);
  uint8_t mode  =  readUint8(dataFile);

  uint16_t sx = readUint16(dataFile);
  uint16_t sy = readUint16(dataFile);
  uint16_t ex = readUint16(dataFile);
  uint16_t ey = readUint16(dataFile);

  uint32_t flagAOffset = readUint32(dataFile);
  uint32_t flagBOffset = readUint32(dataFile);
  uint32_t flagASize = flagBOffset - flagAOffset;
  uint32_t flagBSize = readUint32(dataFile);
  uint32_t pixelOffset = readUint32(dataFile);
  uint32_t pixelSize = readUint32(dataFile);
  uint16_t colors = mode & 0x80 ? 256 : 16;
  uint8_t pixelUnitLog = mode & 0x80 ? 1 : 2;

  uint8_t *palette = (uint8_t *)heap_caps_malloc(colors * 3, MALLOC_CAP_8BIT);
  dataFile.seek( headerOffset + 32, SeekSet);
  dataFile.read(palette, colors * 3);

  uint8_t *flagABuf = (uint8_t *)heap_caps_malloc(flagASize, MALLOC_CAP_8BIT);
  dataFile.seek( headerOffset + flagAOffset, SeekSet);
  dataFile.read(flagABuf, flagASize);

  uint8_t *flagBBuf = (uint8_t *)heap_caps_malloc(flagBSize, MALLOC_CAP_8BIT);
  dataFile.seek( headerOffset + flagBOffset, SeekSet);
  dataFile.read(flagBBuf, flagBSize);

  uint16_t width = (ex | 7) - (sx & 0xFFF8) + 1;
  uint16_t flagSize = width >> (pixelUnitLog + 1);
  uint16_t height = ey - sy + 1;

  uint8_t *flagBuf = (uint8_t *)heap_caps_malloc(flagSize, MALLOC_CAP_8BIT);
  memset(flagBuf, 0, flagSize);

  static constexpr int pixel_bufsize = 1024;
  uint8_t *pixel = (uint8_t *)heap_caps_malloc(pixel_bufsize, MALLOC_CAP_8BIT);
  dataFile.seek( headerOffset + pixelOffset, SeekSet);
  dataFile.read(pixel, pixel_bufsize);

  uint16_t flagAPos = 0;
  uint16_t flagBPos = 0;
  uint32_t src = 0;
  int32_t dest = 0;
  // コピー位置の計算
  uint8_t copyx[] = {0, 1, 2, 4, 0, 1, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0};
  uint8_t copyy[] = {0, 0, 0, 0, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16};
  int32_t copypos[16];

  for (int i = 0; i < 16; ++i) {
    copypos[i] = -(copyy[i] * width + (copyx[i] << pixelUnitLog));
  }

  int copysize = 1 << pixelUnitLog;
  uint8_t mask = 0x80;

  uint8_t *data = (uint8_t *)heap_caps_malloc(width * 16, MALLOC_CAP_8BIT);

//  uint16_t *linebuf= (uint16_t *)heap_caps_malloc(320, MALLOC_CAP_DMA);
  uint16_t linebuf[320];

  Serial.printf("width=%d:height=%d\n", width, height);

//Serial.printf("linebuf: %08x \r\n", (int32_t)linebuf);
//Serial.printf("data: %08x \r\n", (int32_t)data);
//Serial.printf("pixel: %08x \r\n", (int32_t)pixel);
//Serial.printf("palette: %08x \r\n", (int32_t)palette);
//Serial.printf("flagBuf: %08x \r\n", (int32_t)flagBuf);
//Serial.printf("flagABuf: %08x \r\n", (int32_t)flagABuf);
//Serial.printf("flagBBuf: %08x \r\n", (int32_t)flagBBuf);


  int32_t destdiff = 0;

  int wid = width>>1;
  if (wid > 320) wid = 320;
  M5.Lcd.setAddrWindow(0, 0, wid, 240);
  if (height > 480) height = 480;
  for (int y = 0; y < height; ++y) {
    if (dest && ((y & 1) == 0 || (y + 1 == height))) {
      int dy = (y-2) & 15;
      int x = 0;
      for (;x < wid; x++) {
        if ((x * 2 + 1) + (dy + 1) * width > dest) break;
        uint32_t c1 = (uint32_t)data[ x * 2      +  dy      * width] * 3;
        uint32_t c2 = (uint32_t)data[(x * 2 + 1) +  dy      * width] * 3;
        uint32_t c3 = (uint32_t)data[ x * 2      + (dy + 1) * width] * 3;
        uint32_t c4 = (uint32_t)data[(x * 2 + 1) + (dy + 1) * width] * 3;
        linebuf[x] = __builtin_bswap16(rgb565(
                           (palette[c1 + 1] + palette[c2 + 1] + palette[c3 + 1] + palette[c4 + 1]) >> 2,
                           (palette[c1    ] + palette[c2    ] + palette[c3    ] + palette[c4    ]) >> 2,
                           (palette[c1 + 2] + palette[c2 + 2] + palette[c3 + 2] + palette[c4 + 2]) >> 2
                         ));
      }
      M5.Lcd.pushColors((uint8_t*)linebuf, x*2);
      if (y + 1 == height) break;
      if ((y & 15) == 0) {
        destdiff = dest;
        dest = 0;
      }
    }

    // フラグを1ライン分展開
    for (int x = 0; x < flagSize; ++x) {
      // フラグAを1ビット調べる
      if (flagABuf[flagAPos] & mask) {
        // 1ならフラグBから1バイト読んでXORを取る
        flagBuf[x] ^= flagBBuf[flagBPos++];
      }
      if ((mask >>= 1) == 0) {
        mask = 0x80;
        ++flagAPos;
      }
    }
    for (int x = 0; x < flagSize; ++x) {
      // フラグを1つ調べる
      uint8_t vv = flagBuf[x];
      uint8_t v = vv >> 4;
      int loop = 2;
      do {
        if (!v) {
          if (src > pixel_bufsize>>1) {
            memcpy(pixel, &pixel[pixel_bufsize>>1], pixel_bufsize>>1);
            dataFile.read(&pixel[pixel_bufsize>>1], pixel_bufsize>>1) ;
            src -= pixel_bufsize>>1;
          }

          // 0ならピクセルデータから1ピクセル(2バイト)読む
          if (colors == 16) {
            data[dest    ] = pixel[src  ] >> 4;
            data[dest + 1] = pixel[src++] & 0xF;
            data[dest + 2] = pixel[src  ] >> 4;
            data[dest + 3] = pixel[src++] & 0xF;
            dest += 4;
          } else {
            data[dest    ] = pixel[src++];
            data[dest + 1] = pixel[src++];
            dest += 2;
          }
        } else {
          // 0以外なら指定位置から1ピクセル(16色なら4ドット/256色なら2ドット)コピー
          int32_t copySrc = dest + copypos[v];
          if (copySrc < 0) copySrc += destdiff;
          memcpy(&data[dest], &data[copySrc], copysize);
          dest += copysize;
        }
        v = vv & 0xF;
      } while (--loop);
    }
  }

  heap_caps_free(palette);
  heap_caps_free(flagABuf);
  heap_caps_free(flagBBuf);
  heap_caps_free(flagBuf);
  heap_caps_free(pixel);
  heap_caps_free(data);
}

uint8_t readUint8(File dataFile) {
  uint8_t buffer[1] = {0};
  dataFile.read((byte*)buffer, 1) ;
  return *(uint8_t*)buffer;
}
uint16_t readUint16(File dataFile) {
  uint8_t buffer[2] = {0};
  dataFile.read((byte*)buffer, 2) ;
  return *(uint16_t*)buffer;
}

uint32_t readUint32(File dataFile) {
  uint8_t buffer[4] = {0};
  dataFile.read((byte*)buffer, 4) ;
  return *(uint32_t*)buffer;
}

//https://forum.arduino.cc/index.php?topic=487698.0
uint16_t rgb565( uint8_t R, uint8_t G, uint8_t B)
{
  uint16_t ret  = (R & 0xF8) << 8;  // 5 bits
  ret |= (G & 0xFC) << 3;  // 6 bits
  ret |= (B & 0xF8) >> 3;  // 5 bits

  return ( ret);
}
