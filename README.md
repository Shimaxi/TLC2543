# はじめに

ガスセンサ『Mics5524(Adafruit)』11個から得たアナログ信号をArduino Unoで受け取れるようにするために、『TLC2543(Texas Instruments)』というAD変換器を使って作業を行った際の備忘録兼引継ぎ資料です。

後半ではさらに『LTC6820(Analog Device)』というSPI通信延長モジュールを導入した際の作業などについても触れています。

# TLC2453を使う

今回用いるAD変換器が『TLC2543』というものです．仕様として「12bitの分解能・11inputまでに対応・ブレッドボード対応」のあたりが魅力であったので、これを採用しました．

>TLC2543の販売ページ
>
>https://jp.rs-online.com/web/p/analogue-to-digital-converters/5172087

ArduinoUnoのAD変換は10bitの分解能であったため、これを使うことでより高い分解能のデータを取得できるというメリットもあります。

TLC2543の簡単な図を下に示します．これを用いたAD変換の仕組みとしては、番号1から9,11,12に差した11本のアナログ信号をSPI信号に変換し15から18の4本の線に変換してデジタル信号でデータを送るようになっています．

![スクリーンショット 2023-05-17 112830-1](https://github.com/Shimaxi/TLC2543/assets/61856215/8ab5dd63-39cc-4bb3-b542-46733be4cd88)

SPI通信ってなんぞや？って場合には以下のサイトが参考になります．

>SPIの基本を学ぶ
>
>https://www.analog.com/jp/analog-dialogue/articles/introduction-to-spi-interface.html

簡単に言うとSPI通信というのは複数の信号を4つの信号に変換して送信し、受け取り側でその複数の信号に復元させるような通信方式です。

このAD変換器をArduinoに繋げる方法はとても簡単です．
Arduinoはデジタルインプットの10番がSS(スレーブ選択)、11番がMOSI(Master Out, Slave In)、12番がMISO(Master In, Slave Out)、13番がSCLK(シリアルクロック)となっているので、ADコンバータのCS(15番)を10番、DATA OUT(16番)を12番、DATA INPUT(17番)を11番、I/O CLOCK(18番)を13番に差せば良いという事になります．

(ArduinoのSSは自由に設定して良いことになっているので、好きな番号を選んでくれれば良いのですが、今回は説明の都合上10番とします)

![11_communication_03_SPI-port-1](https://github.com/Shimaxi/TLC2543/assets/61856215/44645b96-6fd5-4bfa-bc01-8beca25afb5c)

あとは、AD変換器のVcc(20番)とGND(10番)をArduinoの5VとGNDに差して、REF+(14番)とREF-(15番)もArduinoの5VとGNDに差せば良いです．(この辺りはブレッドボード等を使って上手くやってください)

VccはこのAD変換器を動かすために必要な電力のことです．GNDとセットで与える必要があります．
REF+/-というのは参照電圧のことで、これを電圧の基準とするよーってやつなので、5VとGNDに差しとけばとりあえず大丈夫です．

これでひとまず線を繋げることが出来たので、以下の.unoファイルを書き込んで実行してください．
```c
#include <SPI.h>
const int chipSelectPin = 10; //Select CS pin(今回は10に設定)
uint16_t values[11] = { 0 }; //uint16_t:2バイトの符号なし整数 計算容量節約
float Vref = 5.0; //参照電圧は5V
float floatvalues[11] = { 0 }; //今回はArduino側で電圧までの変換を行う
SPISettings MySetting (1000000, MSBFIRST, SPI_MODE0); //iso-SPIの最大クロック周波数が1MHz

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); //大きいほど高速通信 ※注シリアルモニタの表示も一緒に変えるように
  SPI.begin();
  pinMode(chipSelectPin, OUTPUT);
}

void loop() {
  readAdcAll(); 
  for(uint8_t channel = 0; channel < 11; channel++)
  {    
    floatvalues[channel] = values[channel]*Vref/4095; //12bit表記を実数に変換
    Serial.print(String(floatvalues[channel],2) + ","); //string(values,n)で小数点以下どこまで書くか決められる
  }  
  Serial.print("\n");
}

void readAdcAll() {
  uint16_t value = 0;

  readAdc(0); //readAdc(1)をして得る値がAIN0の物になるように調整
  for(uint8_t channel = 1; channel < 12; channel++)
  {
    values[channel - 1] = readAdc(channel); //SPI通信の都合上valuesに入る値とreadAdcで読む値が一つズレる
  }
}

uint16_t readAdc(uint8_t chx) {
  uint16_t ad;
  uint8_t ad_l;  

  SPI.beginTransaction(MySetting);
  digitalWrite(chipSelectPin, LOW);
  delayMicroseconds(10);
  ad = SPI.transfer((chx << 4) | 0x0C); //チャネル番号に対応した2進数コードを0を4つ分左にシフト、そして空いてる4つの0に0x0C = 16-Bit, LSB-First, Unipolarを入れる
  ad_l = SPI.transfer(0); //
  digitalWrite(chipSelectPin, HIGH);
  SPI.endTransaction();
  ad <<= 8; //adには欲しい12bitのデータのうち前から8bitが入る
  ad |= ad_l; //ad_lにはadの後の8bitが入ってくるので、それを右からくっつけて16bitとする
  ad >>= 4; //欲しいのは12bitの情報なので、後ろの4bit分は不要．よって削る
  
  delayMicroseconds(10);  // EOC
  return ad;
}
```

これを実行すれば、11個のデジタル値が行ごとにばーーっと表示されるはずです．(はず...です)

少し解説するところがあるとしたら、readAdc内の "ad = SPI.transfer((chx << 4) | 0x0C);"の部分でしょうか．これはSPI信号の特徴によるものです．

"spi.h"のライブラリを使ったSPI通信では、SPI.transfer()という形で、欲しいデータについて8bitのパスワードのような形で要求することで、データを取得できるようになっています．

例えば、『AIN0を16bitsの長さでMSB firstの形式でUnipolarで欲しいよー』ってなったら、SPI.transfer(00001100);とすることでAIN0の情報を取得できるようになります．(下の図参考)
(取得する時も8bitの形式で返してくるので、全部のデータを取得するにははad_lというのも付けて、合計16bitの情報を取得して12bitに直すみたいな回りくどいやり方が必要になるのですが....まあ仕方ないでしょう)

![スクリーンショット 2023-05-22 134405](https://github.com/Shimaxi/TLC2543/assets/61856215/b727472a-06f1-4285-bf7d-d93a09d415c9)

単純にTLC2543を使うだけならこれでおしまいとなります．

# LTC6820 isoSPIの導入

これで問題が無ければよかったのですが、実験環境の都合上SPI信号の通信線を3m程に延長して繋いでみたところ上手くいかない問題が発生しました．

上のコードを実行すると、送られてくるデータが定期的に0Vや5Vを示すバグが発生し、どうしてもうまくいきません．
原因はSPI通信が長距離のデータ移送に対応しておらず、1mを超えるケーブルを使って信号を送ると定期的に間違った値が送られるようになるから、ということでした．

>SPIによって、3mのケーブルを介した通信を実現することは可能ですか？
>
>https://www.analog.com/jp/education/landing-pages/003/bbs/bbs_18.html

この問題を解決するために用いたのが『LTC6820 isoSPI 絶縁型SPI延長モジュール』です．
これはisoSPI絶縁型通信インタフェースであるLTC6820を使いやすいようにモジュール化したものです．なんじゃそりゃって感じですが、SPI信号を2本の差動信号に変換してLANケーブルを用いて安全に長距離の信号移送を行えるようにしたもの、という認識で十分です．
(なおLANケーブルは中身が8本のツイステッドペアケーブルになっており、ノイズに強いのも利点としてあります.)

>販売サイト
>
>https://strawberry-linux.com/catalog/items?code=16820

接続方法としてはサイトに日本語の説明書があるのでそれを読んでください.

ここでも軽く説明すると、このモジュールはMasterとSlaveの2つで1つのセットとなっており、Arduino側に繋げるのがMaster、AD変換器側に繋げるのがSlaveとなっています．

繋げ方としては
Arduino側はSS(10番)をモジュールの7番、MOSI(11番)を6番、MISO(12番)を3番、SCLK(13番)を5番に差してください.
そして、AD変換器側はCS(15番)を7番、DATA OUT(16番)を3番、DATA INPUT(17番)を6番、I/O CLOCK(18番)を5番に差せば良いです．

![スクリーンショット 2023-05-17 121608](https://github.com/Shimaxi/TLC2543/assets/61856215/6be390fd-6628-4ecc-affc-d229d0956f96)

...わっかりにくいですね．
番号を書き込んだ図を載せるので、それを参考にしてください．
後はモジュールのGNDとVDDSを適当にGNDと5Vに繋いでください．

![tugou](https://github.com/Shimaxi/TLC2543/assets/61856215/4a7849c4-ac5f-4ebe-b460-bc591df3bc1f)

(VccとGNDは略)

これで接続が完了したので、コードをArduinoに書き込みましょう．

コードは以下のようです．

```c
#include <SPI.h>
const int chipSelectPin = 10; //Select CS pin(今回は10に設定)
uint16_t values[11] = { 0 }; //uint16_t:2バイトの符号なし整数 計算容量節約
float Vref = 5.0; //参照電圧は5V
float floatvalues[11] = { 0 }; //今回はArduino側で電圧までの変換を行う
SPISettings MySetting (1000000, MSBFIRST, SPI_MODE0); //iso-SPIの最大クロック周波数が1MHz

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); //大きいほど高速通信 ※注シリアルモニタの表示も一緒に変えるように
  SPI.begin();
  pinMode(chipSelectPin, OUTPUT);
}

void loop() {
  readAdcAll(); 
  for(uint8_t channel = 0; channel < 11; channel++)
  {    
    floatvalues[channel] = values[channel]*Vref/4095; //12bit表記を実数に変換
    Serial.print(String(floatvalues[channel],2) + ","); //string(values,n)で小数点以下どこまで書くか決められる
  }  
  Serial.print("\n");
}

void readAdcAll() {
  uint16_t value = 0;

  readAdc(0); //readAdc(1)をして得る値がAIN0の物になるように調整
  for(uint8_t channel = 1; channel < 12; channel++)
  {
    values[channel - 1] = readAdc(channel); //SPI通信の都合上valuesに入る値とreadAdcで読む値が一つズレる
  }
}

uint16_t readAdc(uint8_t chx) {
  uint16_t ad;
  uint8_t ad_l;  

  SPI.beginTransaction(MySetting);
  digitalWrite(chipSelectPin, LOW);
  delayMicroseconds(10);
  ad = SPI.transfer((chx << 4) | 0x0E); // チャネル番号に対応した2進数コードを0を4つ分左にシフト、そして空いてる4つの0に0x0E = 16-Bit, LSB-First, Unipolarを入れる
  ad_l = SPI.transfer(0); //
  digitalWrite(chipSelectPin, HIGH);
  SPI.endTransaction();
  ad <<= 8; //adには欲しい12bitのデータのうち前から8bitが入る
  ad |= ad_l; //ad_lにはadの後の8bitが入ってくるので、それを右からくっつけて16bitとする

  ad = ad & 0x0fff; //LSB-Firstでデータを受け取っているのでMSB-Firstになるようデータの方向を反転させる
  ad = (((ad & 0xaaaa) >> 1) | ((ad & 0x5555) << 1));
  ad = (((ad & 0xcccc) >> 2) | ((ad & 0x3333) << 2));
  ad = (((ad & 0xf0f0) >> 4) | ((ad & 0x0f0f) << 4));
  ad = ((ad >> 8) | (ad << 8));
  ad >>= 4; //欲しいのは12bitの情報なので、後ろの4bit分は不要．よって削る

  delayMicroseconds(10);  // EOC
  return ad;
}
```

これで、さっきと同様にデータがバーッと出るはずです．
ここで注意してほしいのは、コード中のreadAdcです．

今まで通り、MSB-Firstでデータを取得しようとすると、なぜか最上位のbitに1が必ず印加されてしまうバグが発生しました．
差動信号を通したことによって起こるバグみたいですが、詳しくは分かりません．
そこで、仕方が無いのでデータ自体はLSB-Firstで受け取り(ad = SPI.transfer((chx << 4) | 0x0E);)、コードを用いて順番を反転して無理やりMSB-Firstにする方法を使いました．

本来はSPISettingでLSBFirstを選択すれば出来るはずなのですが、そうすると何故か上手くいかなかったのでこうしています．

なんか上手くいかないのでゴリ押しで解決したことだらけで、己の未熟さを痛感する結果となりましたが、とりあえず動いたのでヨシ！！！！！！

ここから先は好きにコードを書き換えてもらって大丈夫です．

コードに関してはとある方がGitHub上に公開していたものを参考にさせていただいたのですが、何らかの事情で非公開になっています．ここで強く感謝の念を述べさせていただきます．
