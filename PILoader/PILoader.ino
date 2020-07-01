// PI表示ロジックは、https://www.vector.co.jp/soft/dos/art/se002805.html で公開されているロジックを移植したものです。

#ifndef GRAPHIC_LOADER_MAIN

#include <M5Stack.h>
#include "SD.h"
#include <M5StackUpdater.h>  // https://github.com/tobozo/M5Stack-SD-Updater/

#endif

#include <esp_heap_caps.h>

namespace PILOADER {
void color_table_init(void);
bool header_read(void);
void nmemcpy(char *d, char *s, long l);
void expand();
long read_len(void);
int read_color(int c);
int getcol(int c, int i);
unsigned int bit_load(int size);
void error(char *s);
void buff2scrn();
void ginit();

#ifndef GRAPHIC_LOADER_MAIN

void piLoad(File dataFile);
void randomDraw();

__attribute__ ((always_inline)) inline static
uint16_t swap565( uint8_t r, uint8_t g, uint8_t b) {
  return ((b >> 3) << 8) | ((g >> 2) << 13) | ((g >> 5) | ((r >> 3) << 3));
}
}
#define PI_DIRECTORY "/pi"

void setup(void) {
  M5.begin();
  if (digitalRead(BUTTON_A_PIN) == 0) {
    Serial.println("Will Load menu binary");
    updateFromFS(SD);
    ESP.restart();
  }
  M5.Lcd.setBrightness(200);    // BRIGHTNESS = MAX 255
  M5.Lcd.fillScreen(TFT_BLACK);     // CLEAR SCREEN
  SD.begin();
}

void loop() {
  PILOADER::randomDraw();
}

namespace PILOADER {
void randomDraw() {
  File piRoot;
  piRoot = SD.open(PI_DIRECTORY);
  int fileCount = 0;
  while (1) {
    File entry =  piRoot.openNextFile();
    if (!entry) { // no more files
      break;
    }
    //ファイルのみ取得
    if (!entry.isDirectory()) {
      String fileName = entry.name();
      fileName.toUpperCase();
      if (fileName.endsWith("PI") == true) {
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setCursor(0, 220);
        M5.Lcd.setTextSize(1);

        M5.Lcd.print(entry.name());
        piLoad(entry);
        delay(1000);
      }
    }
    entry.close();
  }
  piRoot.close();
}
#endif

#define		SIZE_OF_BUFF	(16*1024)	/* ファイルバッファ サイズ */

unsigned char 	palette[16][3];			/* パレットのバッファ */

File curDataFile;
int			bit_length;			/* bit buffer rest				*/
long		buff_length;		/* buff rest					*/
unsigned int	dbuf;			/* 1byte bit buffer for read	*/
unsigned char	*buff_p;		/* buff's read pointer			*/
unsigned char 	buff[SIZE_OF_BUFF];	/* file read /write buffer	*/

char		color_table[16][16];	/* color table				*/
char		*gbuffer;

int			cm0, cm1, x_wid, y_wid;	/* 比率等面倒なので使ってない (^^;	*/
int			x_offset, y_offset;		/* 表示位置オフセット */

bool header_read(void)
{
  int	i, c;

  if ( bit_load(8) != 'P') {
    error("Sorry! no pi file");
    return false;
  }
  if ( bit_load(8) != 'i') {
    error("Sorry! no pi file");
    return false;
  }
  //  fputs(" Comment   : ", stdout);
  while ((c = bit_load(8)) != 26) { 		/* text eof check */
    //   putchar(c);
  }
  while (bit_load(8) != 0)
    ;	/* null loop */
  if ( bit_load(8) != 0) {
    error("Sorry?? unknown mode");
    return false;
  }
  cm0 = bit_load(8);			/* 比率 */
  cm1 = bit_load(8);
  if ( bit_load(8) != 4) {
    error("Sorry! no pi file");
    return false;
  }

  bit_load(16); /* 機種名 */
  bit_load(16);
  i = bit_load(16); /* 機種予約エリア長*/
  while ( i--) bit_load(8);

  x_wid = bit_load(16);
  y_wid = bit_load(16);

  Serial.printf("\n Pic_size  : %4u x %4u\n", x_wid, y_wid);

  if (( gbuffer = (char*)malloc( x_wid * 8 * sizeof(char))) == NULL ) {
    error("Sorry! no enough memory");
    return false;
  }

  for ( i = 0; i < 16; i++) {
    palette[i][0] = bit_load(8);
    palette[i][1] = bit_load(8);
    palette[i][2] = bit_load(8);
  }
  return true;
}

/*-------------------------------------------------------------------------*/

void nmemcpy(char *d, char *s, long l)
{
  auto dst = (uint16_t*)d;
  auto src = (uint16_t*)s;
  long i = 0;
  do {
    dst[i] = src[i];
  } while ( ++i < l );
}

void expand()
{
  long	l;
  int		w, a, b, c, d;
  int y = 0;
  int pp = 0;
  char 	*p;
  int offset = x_wid * 2;

  uint16_t linebuf[320];
  int height = y_wid / 2;
  if (height > 240) {
    height = 240;
  }
  M5.Lcd.setAddrWindow(0, 0, 320, height);

  a = read_color( 0);
  b = read_color( a);
  for ( l = 0; l < x_wid; l++ ) {	/*バッファの頭２ライン分を始めの２ドット*/
    gbuffer[pp++] = a;					/*と同じ色で埋めます					*/
    gbuffer[pp++] = b;
  }
  w = -1;
  for (;;) {
    if ( (b = bit_load( 2)) == 3) {	/* 位置の読み込みで～す */
      b += bit_load(1);
    }
    if ( w == b ) {
      do {
        w = -1;
        a = gbuffer[pp-1];
        a = gbuffer[pp++] = read_color( a);
        gbuffer[pp++] = read_color( a);
        /* ０を余分に出しているので終了チェックは無くても大丈夫*/
      } while ( bit_load( 1));
    } else {
      w = b;
      l = read_len();
      switch ( b) {
        case 0:
          a = gbuffer[pp-1];
          b = gbuffer[pp-2];

          if ( gbuffer[pp-1] == gbuffer[pp-2]) {
            while ( --l >= 0) {
              gbuffer[pp++] = b;
              gbuffer[pp++] = a;
            }
          } else {
            d = gbuffer[pp-4];
            c = gbuffer[pp-3];
            while ( --l >= 0) {
              gbuffer[pp++] = d;
              gbuffer[pp++] = c;
              if ( --l < 0 )break;
              gbuffer[pp++] = b;
              gbuffer[pp++] = a;
            }
          }
          break;
        case 1:
          nmemcpy( &gbuffer[pp], &gbuffer[pp - x_wid], l);
          pp += l << 1;
          break;
        case 2:
          nmemcpy( &gbuffer[pp], &gbuffer[pp - (x_wid * 2)], l);
          pp += l << 1;
          break;
        case 3:
          nmemcpy( &gbuffer[pp], &gbuffer[pp - (x_wid - 1)], l);
          pp += l << 1;
          break;
        case 4:
          nmemcpy( &gbuffer[pp], &gbuffer[pp - (x_wid + 1)], l);
          pp += l << 1;
          break;
      }
    }
    if (pp >= x_wid * 4)
    {
      for (int x = 0; x < 320; x++) {
        if ((x * 2 + 1) + (y * 2 + 1) * x_wid <= (x_wid * 2) + x_wid * y_wid) {
          uint8_t c1 = gbuffer[offset +  x * 2      ] ;
          uint8_t c2 = gbuffer[offset + (x * 2 + 1) ] ;
          uint8_t c3 = gbuffer[offset +  x * 2 +      x_wid] ;
          uint8_t c4 = gbuffer[offset + (x * 2 + 1) + x_wid] ;
          linebuf[x] = swap565(
                         (palette[c1][0] + palette[c2][0] + palette[c3][0] + palette[c4][0]) >> 2,
                         (palette[c1][1] + palette[c2][1] + palette[c3][1] + palette[c4][1]) >> 2,
                         (palette[c1][2] + palette[c2][2] + palette[c3][2] + palette[c4][2]) >> 2
                       );
        }
      }
      M5.Lcd.pushColors(linebuf, 320, false);
      if (++y >= height) return;
      pp -= offset;
      memmove(gbuffer, &gbuffer[offset], pp);
    }
  }
}


/* 長さを読み込み */

long
read_len(void)
{
  int	a;

  a = 0;
  while ( bit_load(1) ) {
    a++;
  }
  if ( a == 0 ) return (1);
  return ( bit_load( a) + (1L << a) );
}

/* 色を読み込みます

   c は１ドット左の色なのね
*/
int
read_color(int c)
{
  if ( bit_load(1) ) {
    return ( getcol(c, bit_load(1) ) );
  }
  if ( bit_load(1) == 0 ) {
    return ( getcol(c, bit_load(1) + 2) );
  }
  if ( bit_load(1) == 0 ) {
    return ( getcol(c, bit_load(2) + 4) );
  }
  return ( getcol(c, bit_load(3) + 8) );
}

/* 色テーブルの更新と実際の色の取り出しだよ */

int
getcol(int c, int i)
{
  int	k;

  k = color_table[c][i];

  while ( i-- ) {
    color_table[c][i + 1] = color_table[c][i];
  }
  color_table[c][0] = k;
  return (k);
}


/* カラーテーブルを初期化する */

void color_table_init(void)
{
  int	i, j;

  for ( j = 0; j < 16; j++) {
    for ( i = 0; i < 16; i++) {
      color_table[j][i] = (16 - i + j) & 15;
    }
  }
}

/* size ビットだけ読み込みます */

unsigned int
bit_load(int size)
{
  unsigned int	a;

  dbuf &= 0xff;
  a = 0;
  while ( size > bit_length) {
    a = a << bit_length;
    dbuf = dbuf << bit_length;
    a = a + ( dbuf >> 8);
    size -= bit_length;

    if ( buff_length == 0) {
      if ( (buff_length = curDataFile.read((byte*)buff, SIZE_OF_BUFF)) == 0 ) {
        error("Sorry! can't read file");
      }
      buff_p = buff;
    } else {
      buff_p++;
    }
    dbuf = *buff_p;
    buff_length--;
    bit_length = 8;
  }
  a = a << size;
  dbuf = dbuf << size;
  a = a + ( dbuf >> 8);
  bit_length -= size;
  return ( a);
}

/* エラーを出力して　抜けてしまいます */

void error(char *s)
{
  Serial.println(s);
  M5.Lcd.setCursor(0, 220);
  M5.Lcd.print(s);
}

/*-------------------------------------------------------------------------*/

//バッファから画面描画
void buff2scrn()
{
  int		r, g, b, y, i;
  char	*p;

  uint16_t linebuf[320];
  int height = y_wid / 2;
  if (height > 240) {
    height = 240;
  }
  M5.Lcd.setAddrWindow(0, 0, 320, height);
  int offset = x_wid * 2;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < 320; x++) {
      if ((x * 2 + 1) + (y * 2 + 1) * x_wid <= (x_wid * 2) + x_wid * y_wid) {
        uint8_t c1 = gbuffer[offset + x * 2  + y * 2 * x_wid] ;
        uint8_t c2 = gbuffer[offset + (x * 2 + 1) + y * 2 * x_wid] ;
        uint8_t c3 = gbuffer[offset + x * 2 + (y * 2 + 1) * x_wid] ;
        uint8_t c4 = gbuffer[offset + (x * 2 + 1) + (y * 2 + 1) * x_wid] ;
        linebuf[x] = swap565(
                       (palette[c1][0] + palette[c2][0] + palette[c3][0] + palette[c4][0]) >> 2,
                       (palette[c1][1] + palette[c2][1] + palette[c3][1] + palette[c4][1]) >> 2,
                       (palette[c1][2] + palette[c2][2] + palette[c3][2] + palette[c4][2]) >> 2
                     );
      }
    }
    M5.Lcd.pushColors(linebuf, 320, false);
  }

  //  for ( i = 0 ; i < 16; i++ ) {
  //    r = palette[i][0];
  //    g = palette[i][1];
  //    b = palette[i][2];
  //    GrSetColor(i, r, g, b);
  //  }
  //
  //  p = gbuffer + (x_wid * 2);
  //  for ( y = 0; y < y_wid; y++) {
  //    gtrans( p, y);
  //    p += x_wid;
  //  }
}

void ginit()
{
  //  int	disp_x, disp_y;
  //
  //  if ((x_wid <= 640) && (y_wid <= 480)) {
  //    disp_x = 640;
  //    disp_y = 480;
  //  } else if ((x_wid <= 800) && (y_wid <= 600)) {
  //    disp_x = 800;
  //    disp_y = 600;
  //  } else if ((x_wid <= 1024) && (y_wid <= 768)) {
  //    disp_x = 1024;
  //    disp_y = 768;
  //  } else if ((x_wid <= 1280) && (y_wid <= 960)) {
  //    disp_x = 1280;
  //    disp_y = 960;
  //  } else if ((x_wid <= 1280) && (y_wid <= 1024)) {
  //    disp_x = 1280;
  //    disp_y = 1024;
  //  } else {
  //    error("Sorry! picture is too large");
  //  }
  //
  //  GrSetMode(GR_width_height_color_graphics, disp_x, disp_y, 16);
  //
  //  disp_x = GrScreenX();
  //  disp_y = GrScreenY();
  //
  //  printf(" Disp_size : %4u x %4u\n", disp_x, disp_y);
  //
  //  if ((x_offset = (disp_x - x_wid) / 2) < 0) {
  //    x_offset = 0;
  //  }
  //  if ((y_offset = (disp_y - y_wid) / 2) < 0) {
  //    y_offset = 0;
  //  }
}

void piLoad(File dataFile) {
  Serial.println(dataFile.name());
  PILOADER::curDataFile = dataFile;
  PILOADER::color_table_init();    /* カラーテーブルの初期化    */
  PILOADER::dbuf = PILOADER::bit_length = PILOADER::buff_length = 0;  /* file buffer init*/
  if (PILOADER::header_read()) {      /* ヘッダー読み込み       */
    PILOADER::ginit();        /* 画面設定           */
    PILOADER::expand();       /* 展開             */
//  PILOADER::buff2scrn();      /* バッファから画面へ転送    */
    free(PILOADER::gbuffer);
  }
}
}