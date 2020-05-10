//PICフォーマット書庫に含まれる picl.c をベースにしています。
//https://www.vector.co.jp/soft/dl/data/art/se003198.html
#ifndef GRAPHIC_LOADER_MAIN

#include <M5Stack.h>
#include "SD.h"
#include <M5StackUpdater.h>  // https://github.com/tobozo/M5Stack-SD-Updater/
#include <map>

#define PIC_DIRECTORY "/pic"

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
  randomDraw();
}

void randomDraw() {
  File picRoot;
  picRoot = SD.open(PIC_DIRECTORY);
  int fileCount = 0;
  while (1) {
    File entry =  picRoot.openNextFile();
    if (!entry) { // no more files
      break;
    }
    //ファイルのみ取得
    if (!entry.isDirectory()) {
      String fileName = entry.name();
      fileName.toUpperCase();
      if (fileName.endsWith("PIC") == true) {
        M5.Lcd.fillScreen(TFT_BLACK);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(0, 220);
        M5.Lcd.print(fileName);

        picLoad(entry);
        delay(1000);
      }
    }
    entry.close();
  }
  picRoot.close();
}

#endif

File curDataFile;

#define    SIZE_OF_X 512   /* 画面の幅   */
#define   SIZE_OF_Y 512   /* 画面の高さ    */

#define   SIZE_OF_BUFF 2048 /* ファイルバッファのサイズ */

//int handle;     /* 低レベルファイルのハンドル    */

int bit_len;    /* ビットバッファのビット長   */
char  *buff_p;    /* ファイルバッファのポインタ    */
int buff_len;   /* ファイルバッファの残りデータ数  */
char  buff[SIZE_OF_BUFF]; /* ファイルバッファ     */
int x_wid, y_wid;   /* ＰＩＣデータの画像サイズ   */

int squareMode; /*正方形モードだと1*/

struct  {     /* 色キャッシュよう */
  uint16_t color;
  uint16_t next;
  uint16_t prev;
} table[128];
uint16_t color_p;    /* 色キャッシュの最新色を指す    */

uint16_t *point; //グラフィック画面
std::map<uint32_t, uint16_t> thunder;

/*
   エラー脱出
*/
void
error( char *s)
{
  Serial.println(s);
  M5.Lcd.setCursor(0, 220);
  M5.Lcd.print(s);
}


/*
   int c = point(int x,int y)
   (x,y) のカラーコードを返す関数

   x : screen X ( 0 .. SIZE_OF_X - 1)
   y : screen Y ( 0 .. SIZE_OF_Y - 1)
   c : color code 16bit
      0 : bright
      1..5  : blue
      6..10 : red
      11..15  : green
*/

/*
   void pset(int x,int y,int c)
   (x, y)座標に色cを書く関数

   x : screen X ( 0 .. SIZE_OF_X - 1)
   y : screen Y ( 0 .. SIZE_OF_Y - 1)
   c : color code 16bit
      0 : bright
      1..5  : blue
      6..10 : red
      11..15  : green
*/

/*
   グラフィックの画面を設定して、クリアする関数
*/
void
ginit( void)
{
  //C_WIDTH(5); /* x68k 512x512 65536 */
  point = (uint16_t*)malloc(SIZE_OF_X * 2 * sizeof(uint16_t));
  memset(point, 0, SIZE_OF_X * 2 * sizeof(uint16_t));
  squareMode = 0;
}


/*
   バッファから１バイトをビットバッファに読み込む
*/
void
buff_next( void)
{
  if ( buff_len == 0 ) {
    if ( (buff_len = curDataFile.read((byte*)buff, SIZE_OF_BUFF)) == 0 ) {
      error("file read error");
    }
    buff_p = buff;
  } else {
    buff_p++;
  }
  buff_len--;
  bit_len = 8;
}


/*
   sizeビット読み込み
*/
long
bit_load( int size)
{
  int i;
  long  a;

  a = 0;
  while ( size > bit_len ) {
    for ( i = 0; i < bit_len; i++ ) {
      a = a + a;
      if ( *buff_p & 0x80 ) a++;
      *buff_p = *buff_p + *buff_p;
      size--;
    }
    buff_next();
  }
  for ( i = 0; i < size; i++ ) {
    a = a + a;
    if ( *buff_p & 0x80 ) a++;
    *buff_p = *buff_p + *buff_p;
    bit_len--;
  }
  return ( a);
}


/*
   色キャッシュの初期化
*/
void
color_cash_init( void)
{
  int i;

  for ( i = 0; i < 128; i++ ) {
    table[i].color = 0;
    table[i].prev = i + 1;
    table[i].next = i - 1;
  }
  table[127].prev = 0;
  table[0].next = 127;
  color_p = 0;
}

/*
   キャッシュから色を取り出し
   その色が最新になるように更新する。
*/
int
get_color( int idx)
{
  if ( color_p != idx ) {
    /* まず位置idxをキャッシュから切り離す */
    table[table[idx].prev].next = table[idx].next;
    table[table[idx].next].prev = table[idx].prev;
    /* 最新色の次にidxを新たにセット */
    table[table[color_p].prev].next = idx;
    table[idx].prev = table[color_p].prev;
    table[color_p].prev = idx;
    table[idx].next = color_p;
    /* 最新色位置を更新 */
    color_p = idx;
  }
  /* ２倍するのは、キャッシュに入っている色は bit0..14 で
     いるのは bit1..15 だから
  */
  return ( table[idx].color * 2);
}


/*
    新しい色をキャッシュに登録
*/
int
new_color( int c)
{
  color_p = table[color_p].prev;
  table[color_p].color = c;
  return ( c << 1);
}

/*
   色の読み込み
*/
int
read_color( void)
{
  if ( bit_load(1) == 0 ) {
    /* キャッシュミス */
    return ( new_color( bit_load(15)));
  } else {
    /* キャッシュヒット */
    return ( get_color( bit_load(7)));
  }
}


/*
   長さの読み込み
*/
long
read_len( void)
{
  int a;

  a = 1;
  while ( bit_load(1) != 0 ) {
    a++;
  }
  return ( bit_load(a) + (1 << a) - 1);
}


/*
   連鎖の展開
*/
void
expand_chain( int x, int y, int c)
{
  uint16_t rgb565 = ((c >> 6) & 0x1F) << 11 | ((c >> 11) & 0x1F) << 6 | ((c >> 1) & 0x1F);

  int y_over; /* 画像サイズが画面サイズを超えた時のフラグ */

  y_over = 0; /* まだ超えていないよ */

  for ( ;; ) {
    switch ( bit_load(2) ) {
      case 0: if ( bit_load(1) == 0 ) return; /* 終わり */
        if ( bit_load(1) == 0 ) x -= 2; /* 左２つ */
        else x += 2;  /* 右２つ */
        break;
      case 1: x--; break; /* 左１つ */
      case 2:      break; /* 真下   */
      case 3: x++; break;     /* 右１つ */
    }
    if ( ++y >= y_wid ) y_over = 1; /* 画面を超えちゃった */

    /* 画面を超えていないのなら連鎖を書き込む */
    if ( y_over == 0) {
      thunder[y<<16|x] = c;
      if (squareMode) {
        M5.Lcd.drawPixel((x >> 1) +32, (y >> 1) - 8, rgb565);
      } else {
        M5.Lcd.drawPixel((x * 2 / 3) -10, (y >> 1) - 8, rgb565);
      }
    }
  }
}

int getNextThunderX(int x, int y) {
  uint32_t idx = y<<16|x;
  while (!thunder.empty() && thunder.begin()->first <= idx) {
    thunder.erase(thunder.begin());
  }
  if (thunder.empty()) return -1;
  return thunder.begin()->first & 0xFFFF;
}

/*
    展開するぞ
*/
void
expand( void)
{
  int x;  /* 展開中の位置 X */
  int y;  /* 展開中の位置 Y */
  int c;  /* 現在の色   */
  long  l;  /* 変化点間の長さ */
  int a;
  int thunder_x = -1;
  x = -1;
  y = 0;
  c = 0;    /* 色の初期値は 0ね */
  for ( ;; ) {
    l = read_len(); /* 長さ読み込み */

    /* 次の変化点まで繰り返す */
    while ( --l ) {
      /* 右端の処理 */
      if ( ++x == x_wid ) {
        if ( ++y == y_wid ) return; /* (^_^;) */
        x = 0;
        thunder_x = getNextThunderX(x, y);
      }
      /* 連鎖点上を通過した時は、現在の色を変更 */
      if (thunder_x == x) {
        c = 0xfffe & thunder.begin()->second;
        thunder_x = getNextThunderX(x, y);
      }
      /* 現在の色を書き込む */
      //pset(x, y, c);
      point[x + (y&1) * SIZE_OF_X] = c;
      if (y&1)
        drawM5StackPixel(x, y);
    }
    /* 右端の処理 */
    if ( ++x == x_wid ) {
      if ( ++y == y_wid ) return; /* (^_^;) */
      x = 0;
    }
    /* 新しい色の読み込み */
    c = read_color();

    /* それを書いて */
    //pset(x, y, c);
    point[x + (y&1) * 512] = c;
    if (y&1)
      drawM5StackPixel(x, y);

    /* 連鎖ありなら、連鎖の展開 */
    if ( bit_load(1) != 0) {
      expand_chain(x, y, c);
      thunder_x = getNextThunderX(x, y);
    }
  }
}

/*
   ヘッダのよみこみ
*/
void
header_read( void)
{
  int c;
  String comment = "";
  if ( bit_load( 8) != 'P' ) error("File is not PIC");
  if ( bit_load( 8) != 'I' ) error("File is not PIC");
  if ( bit_load( 8) != 'C' ) error("File is not PIC");
  while ( (c = bit_load( 8)) != 26 ) {
    //putchar(c); /*　コメントの表示 */
    comment = comment + (char)c;
  }

  if (comment.indexOf("/XSS/") > 0) {
    squareMode = 1;
  } else {
    squareMode = 0;
  }
  while ( bit_load( 8) != 0 ) /* ここは読み飛ばす */
    ; /* null loop */

  /* ここは 0 のはず */
  if ( bit_load( 8) != 0) error("File is not PIC");

  /* タイプ/モードともに 0しか対応していない */
  if ( bit_load( 8) != 0 ) error("Not supported format");

  /* 15bit色しか対応していない */
  if ( bit_load( 16) != 15 ) error("Not supported format(16bit)");
  if (( x_wid = bit_load( 16)) > SIZE_OF_X ) error("File is too Big");
  if (( y_wid = bit_load( 16)) > SIZE_OF_Y ) {
    y_wid = SIZE_OF_Y;  /* Yが多い分には、画面分迄は読める */
  }
}

//縦横比を変更して描画する。TODO:まだ、1:1.5描画しか対応していません。
//横方向は 512/ 1.5 = 341.333、上下10ドット描画無しで320
//縦方向は 512 / 2 = 256、上下8ドット描画無しで240

void drawM5StackPixel(int x, int y) {

  if (squareMode == 0) {
    //横方向は、x1(M5Stackの1ドットに多く含む方):x2(M5Stackの1ドットに少なく含む方) = 1:0.5 で色計算
    //X680003個に対し、M5Stack1個
    //X68000 ●●●|●●●|
    //M5Stack ●● |●●|
    //X68000側で中心のドットを描画した場合、M5Stackでは関連する2つのドットを描画する必要があります。

    switch (x % 3) {
      case 0://x:(x+1)=1:0.5で x / 1.5の位置を描画
        drawPixelBrend(x, x + 1, y, x / 1.5, y / 2);
        break;
      case 1://(x-1):x=1:0.5で x / 1.5の位置を描画
        //x:(x+1)=0.5:1で x / 1.5 + 1 の位置を描画
        drawPixelBrend(x - 1, x, y, x / 1.5, y / 2);
        drawPixelBrend(x + 1, x, y, x / 1.5 + 1, y / 2);
        break;
      case 2://(x-1):x=0.5:1で x / 1.5の位置を描画
        drawPixelBrend(x, x - 1 , y, x / 1.5, y / 2);
        break;
    }
  }
  else { //正方形モード
    drawPixelSquare(x, y);
  }
}

//x1:x2 = 1:0.5 で、m5x,m5y の位置にドットを描く
void drawPixelBrend(int x1, int x2, int y, int m5x, int m5y) {
  int m5offsetX = -10;
  int m5offsetY = -8;

  if (m5x + m5offsetX < 0 || m5x + m5offsetX >= 320 ||
      m5y + m5offsetY < 0 || m5y + m5offsetY >= 240 )
  {
    return;
  }

  int y1 = 0;
  int y2 = 0;

  if ( y % 2 == 0) {
    y1 = y;
    y2 = y + 1;
  } else {
    y1 = y - 1;
    y2 = y;
  }
  int p11 = point[x1 + (y1 & 1) * SIZE_OF_X];
  int p12 = point[x1 + (y2 & 1) * SIZE_OF_X];
  int p21 = point[x2 + (y1 & 1) * SIZE_OF_X];
  int p22 = point[x2 + (y2 & 1) * SIZE_OF_X];

  //GGGGGRRRRRBBBBBI から RGB565作成
  uint16_t c = makeRGB565(
                 (((float)(getR(p11)) + (float)(getR(p12)) +
                   ((float)(getR(p21)) + (float)(getR(p22))) * 0.5f ) / 1.5f / 2 )
                 ,
                 (((float)(getG(p11)) + (float)(getG(p12)) +
                   ((float)(getG(p21)) + (float)(getG(p22))) * 0.5f ) / 1.5f / 2 )
                 ,
                 (((float)(getB(p11)) + (float)(getB(p12)) +
                   ((float)(getB(p21)) + (float)(getB(p22))) * 0.5f ) / 1.5f / 2 ) );

  M5.Lcd.drawPixel(m5x + m5offsetX, m5y + m5offsetY, c);
}

//X68000横2ドットをM5Stack1ドットとして、256 x 256 で正方形モードでの描画。512 / 2 = 256 なので、横32ドットずらして、画面真ん中に正方形で描く
void drawPixelSquare(int x, int y) {
  int m5offsetX = 32;
  int m5offsetY = -8;

  int m5x = x / 2;
  int m5y = y / 2;

  if (m5x + m5offsetX < 0 || m5x + m5offsetX >= 320 ||
      m5y + m5offsetY < 0 || m5y + m5offsetY >= 240 )
  {
    return;
  }

  int y1 = 0;
  int y2 = 0;

  if ( y % 2 == 0) {
    y1 = y;
    y2 = y + 1;
  } else {
    y1 = y - 1;
    y2 = y;
  }

  int p1 = point[x + (y1 & 1) * SIZE_OF_X];
  int p2 = point[x + (y2 & 1) * SIZE_OF_X];

  //GGGGGRRRRRBBBBBI から RGB565作成
  uint16_t c = makeRGB565(
                 ((float)(getR(p1)) + (float)(getR(p2))) / 2 ,
                 ((float)(getG(p1)) + (float)(getG(p2))) / 2 ,
                 ((float)(getB(p1)) + (float)(getB(p2))) / 2 );

  M5.Lcd.drawPixel(m5x + m5offsetX, m5y + m5offsetY, c);
}



//X68000 GRBI = GGGGGRRRRRBBBBBI(16bit)
uint16_t getR( uint16_t grbi) {
  return (grbi >> 6) & 0b11111;
}

uint16_t getG( uint16_t grbi) {
  return (grbi >> 11) & 0b11111;
}

uint16_t getB( uint16_t grbi) {
  return (grbi >> 1) & 0b11111;
}

uint16_t makeRGB565(uint16_t R, uint16_t G, uint16_t B)
{
  uint16_t ret  = (R) << 11;  // 5 bits
  ret |= (G) << 6;  // 6 bits
  ret |= (B) ;  // 5 bits
  return ( ret);
}

void picLoad(File dataFile) {

  curDataFile = dataFile;
  bit_len = buff_len = 0;   /* ファイルバッファの初期化 */
  ginit();      /* 画面モードの初期化    */
  color_cash_init();    /* 色キャッシュ初期化    */
  header_read();      /* ヘッダの読み込み   */
  expand();     /* 展開       */
  free(point);

}
