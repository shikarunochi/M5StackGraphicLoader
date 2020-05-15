// MAG表示ロジックは、https://emk.name/2015/03/magjs.html で公開されているロジックを移植したものです。
#include<SPI.h>
#include <Seeed_FS.h>
#include "SD/Seeed_SD.h"

#include"TFT_eSPI.h"
TFT_eSPI tft = TFT_eSPI();

__attribute__ ((always_inline)) inline static
uint16_t swap565( uint8_t r, uint8_t g, uint8_t b) {
  return ((b >> 3) << 8) | ((g >> 2) << 13) | ((g >> 5) | ((r >> 3) << 3));
}

#define MAG_DIRECTORY "/mag"

void setup(void) {
  tft.init();
  tft.setRotation(3);
  while (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
    Serial.println("SD card error!\n");
    while (1);
  }
  delay(1000);
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
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(0, 220);
        tft.setTextSize(1);

        tft.print(entry.name());
        magLoad(entry);
        delay(1000);
      }
    }
    entry.close();
  }
  magRoot.close();
}


void magLoad(File dataFile) {
  uint8_t maki02[8];
  dataFile.read(maki02, 8);
  if (0 != memcmp (maki02, "MAKI02  ", 8)) {
    //MAG画像ではない
    tft.print(" is Not MAG Format.");
    return;
  }

  //ヘッダ先頭まで読み捨て
  int headerOffset = 8;
  dataFile.seek(headerOffset, SeekSet);
  while (dataFile.available() && 0x1A != dataFile.read()) ++headerOffset;

  headerOffset++;

  dataFile.seek( headerOffset, SeekSet);

  struct mag_info_t {
    // 先頭32バイト分はファイル内の順序通り。順序変更不可。
    uint8_t top;
    uint8_t machine;
    uint8_t flags;
    uint8_t mode;
    uint16_t sx;
    uint16_t sy;
    uint16_t ex;
    uint16_t ey;
    uint32_t flagAOffset;
    uint32_t flagBOffset;
    uint32_t flagBSize;
    uint32_t pixelOffset;
    uint32_t pixelSize;

    uint32_t flagASize;
    uint16_t colors;
    uint8_t pixelUnitLog;
    uint16_t width;
    uint16_t height;
    uint16_t flagSize;

    void init(void) {
      flagASize = flagBOffset - flagAOffset;

      colors = mode & 0x80 ? 256 : 16;
      pixelUnitLog = mode & 0x80 ? 1 : 2;
      width = (ex | 7) - (sx & 0xFFF8) + 1;
      height = ey - sy + 1;
      flagSize = width >> (pixelUnitLog + 1);
    }
  } __attribute__((packed)); // 1バイト境界にアライメントを設定

  mag_info_t mag;

  dataFile.read((uint8_t*)&mag, 32);
  mag.init();

  // 16ライン分の画像データ展開領域
  uint8_t *data = (uint8_t *)malloc(mag.width * 16);

  // カラーパレット展開領域
  uint8_t *palette = (uint8_t *)malloc(mag.colors * 3);
  dataFile.read(palette, mag.colors * 3);

  uint8_t *flagABuf = (uint8_t *)malloc(mag.flagASize);
  dataFile.seek( headerOffset + mag.flagAOffset, SeekSet);
  dataFile.read(flagABuf, mag.flagASize);

  uint8_t *flagBBuf = (uint8_t *)malloc(mag.flagBSize);
  dataFile.seek( headerOffset + mag.flagBOffset, SeekSet);
  dataFile.read(flagBBuf, mag.flagBSize);

  uint8_t *flagBuf = (uint8_t *)malloc(mag.flagSize);
  memset(flagBuf, 0, mag.flagSize);

  static constexpr int pixel_bufsize = 4096;
  static constexpr auto halfbufsize = pixel_bufsize >> 1;
  uint8_t *pixel = (uint8_t *)malloc(pixel_bufsize);
  dataFile.seek( headerOffset + mag.pixelOffset, SeekSet);

  uint32_t src = 0; // (headerOffset + mag.pixelOffset) % (pixel_bufsize);
  dataFile.read(&pixel[src], pixel_bufsize - src);

  uint_fast16_t flagAPos = 0;
  uint_fast16_t flagBPos = 0;
  int32_t dest = 0;
  // コピー位置の計算
  static constexpr uint8_t copyx[] = {0, 1, 2, 4, 0, 1, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0};
  static constexpr uint8_t copyy[] = {0, 0, 0, 0, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16};
  int32_t copypos[16];

  for (int i = 0; i < 16; ++i) {
    copypos[i] = -(copyy[i] * mag.width + (copyx[i] << mag.pixelUnitLog));
  }

  int copysize = 1 << mag.pixelUnitLog;
  uint_fast8_t mask = 0x80;

  //  uint16_t *linebuf= (uint16_t *)malloc(320, MALLOC_CAP_DMA);
  uint16_t linebuf[320];

  Serial.printf("width=%d:height=%d\n", mag.width, mag.height);

  int32_t destdiff = 0;

  int wid = mag.width >> 1;
  if (wid > 320) wid = 320;
  tft.setAddrWindow(0, 0, wid, 240);
  int height = mag.height;
  if (height > 480) height = 480;
  for (int y = 0; y < height; ++y) {
    if (0 != dest && (0 == (y & 1) || (y + 1 == height))) {
      int dy = (y - 2) & 15;
      int x = 0;
      for (; x < wid; x++) {
        if ((x * 2 + 1) + (dy + 1) * mag.width > dest) break;
        int c1 = data[ x * 2      +  dy      * mag.width] * 3;
        int c2 = data[(x * 2 + 1) +  dy      * mag.width] * 3;
        int c3 = data[ x * 2      + (dy + 1) * mag.width] * 3;
        int c4 = data[(x * 2 + 1) + (dy + 1) * mag.width] * 3;
        linebuf[x] = swap565(
                       (palette[c1 + 1] + palette[c2 + 1] + palette[c3 + 1] + palette[c4 + 1]) >> 2,
                       (palette[c1    ] + palette[c2    ] + palette[c3    ] + palette[c4    ]) >> 2,
                       (palette[c1 + 2] + palette[c2 + 2] + palette[c3 + 2] + palette[c4 + 2]) >> 2
                     );
      }
      tft.pushColors(linebuf, x, false);
      if (y + 1 == height) break;
      if ((y & 15) == 0) {
        destdiff = dest;
        dest = 0;
      }
    }

    // フラグを1ライン分展開
    int x = 0;
    int xend = mag.flagSize;
    do {
      // フラグAを1ビット調べる
      if (flagABuf[flagAPos] & mask) {
        // 1ならフラグBから1バイト読んでXORを取る
        flagBuf[x] ^= flagBBuf[flagBPos++];
      }
      if ((mask >>= 1) == 0) {
        mask = 0x80;
        ++flagAPos;
      }
    } while (++x < xend);

    x = 0;
    xend <<= 1;
    do {
      // フラグを1つ調べる
      uint_fast8_t v = flagBuf[x >> 1];
      if (x & 1) v &= 0x0F;
      else v >>= 4;

      if (!v) {
        if (src == pixel_bufsize) {
          dataFile.read(pixel, pixel_bufsize);
          src = 0;
        }
        // 0ならピクセルデータから1ピクセル(2バイト)読む
        if (mag.colors == 16) {
          auto tmp = pixel[src]; // 一時変数を使う事で効率向上
          data[dest    ] = tmp >> 4;
          data[dest + 1] = tmp & 0xF;
          tmp = pixel[src + 1];
          data[dest + 2] = tmp >> 4;
          data[dest + 3] = tmp & 0xF;
          dest += 4;
          src += 2;
        } else {
          memcpy(&data[dest], &pixel[src], 2);
          dest += 2;
          src += 2;
        }
      } else {
        // 0以外なら指定位置から1ピクセル(16色なら4ドット/256色なら2ドット)コピー
        int32_t copySrc = dest + copypos[v];
        if (copySrc < 0) copySrc += destdiff;
        memcpy(&data[dest], &data[copySrc], copysize);
        dest += copysize;
      }
    } while (++x < xend);
  }

  free(palette);
  free(flagABuf);
  free(flagBBuf);
  free(flagBuf);
  free(pixel);
  free(data);
}
