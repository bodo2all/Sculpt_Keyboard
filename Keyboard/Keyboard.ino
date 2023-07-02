#include <BleKeyboard.h>
#include <esp_bt.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include <soc/rtc_wdt.h>
#include <esp_task_wdt.h>
#include <esp32/rom/rtc.h>
#include <string.h>
#include <ctype.h>

//#define RAW_TABLE_DEBUG
//#define LEVEL_TRIGGER
#define BLE_DELAY_MS 15
#define SCN_DELAY_MS 15
#define ADVERTISE_DELAY_MS 1000
#define SLP_TIMEOUT_MS 232
//we could search for the "key" in the table at setup.
#define WAKEUP_KEY_COL 2
#define WAKEUP_KEY_ROW 0 //3
#define DEEP_SLEEP_TIMEOUT_MS (1*1000*60)


#define FN_PIN      34
#define LED_PIN     0
#define BRD_HIST_SZ 5

// note gpio 18 for row3 doesn't support rtc wake
BleKeyboard bleKeyboard("Sculpt ESP32", "Microsoft", 100);
const RTC_RODATA_ATTR uint8_t cols[]={       23,   22,    13,   21,     5,    16,    17,  19,   4,   15,  2,  27    };
const RTC_RODATA_ATTR uint8_t rows[]={34, /* 9     paus   del   0       8     Bspc   7    tab  Q    2    1         */
                      12, /* mins  pgup   F12  lbrc     rbrc  ins    Y    F5   F3   W    4    F6   */
                      18, /* O     home   calc  P       I            U    R    E    caps 3    T    */
                      14, /* L     slck   ent   scln    K     bsls   J    F    D    <    A    lgui */
                      32, /* quot         app   slsh    ralt  left   H    G    F4   S    esc  lalt */
                      33, /* dot   end    pgdn          comm  rshift M    V    C    X    Z    lsft */
                      25, /* rctl  right  up    down          rspc   N    B    lspc           lctl */
                      26};/* F9    pscr   F11   eql     F8    F10    F7   5    F2   F1   grv`  6    */
/*
TBD - might not the right ones:
KEY_PAUSE
KEY_SCROLL_LOCK

BleKEyboard doesn't match codes to any of these:
https://cdn.sparkfun.com/datasheets/Wireless/Bluetooth/RN-HID-User-Guide-v1.0r.pdf
https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf

Aparently the dev code implementation is
 /Users/<username>/Library/Arduino15/packages/esp32/hardware/esp32/2.0.9/tools/sdk/esp32/include/soc/esp32/include/soc/soc.h
*/
#define KEY_PAUSE    0x13
#define KEY_NUM_LOCK 0xDB
#define KEY_SCROLL_LOCK 0xCF
#define KEY_RIGHT_MNU KEY_RIGHT_GUI


// 0x00 0x00 is a placeholder for no key
// all keys need to be 2 bytes cause print needs strings not chars, also calculator is two byte character
// and I couln't use "b" notation cause the compiler didn't like the implicit conversion
const RTC_RODATA_ATTR uint8_t layout[sizeof(rows)][sizeof(cols)][2] = {
//       23,                      22,                     13,                  21,                       5,                     16,                    17,            19,               4,             15,                      2,              27
/*34*/{{'9',            0x00}, {KEY_PAUSE,      0x00}, {KEY_DELETE,   0x00}, {'0',             0x00}, {'8',           0x00}, {KEY_BACKSPACE,  0x00}, {'7',    0x00}, {KEY_TAB, 0x00}, {'q',    0x00}, {'2',           0x00}, {'1',     0x00}, {0x00,           0x00}},
/*12*/{{'-',            0x00}, {KEY_PAGE_UP,    0x00}, {KEY_F12,      0x00}, {'[',             0x00}, {']',           0x00}, {KEY_INSERT,     0x00}, {'y',    0x00}, {KEY_F5,  0x00}, {KEY_F3, 0x00}, {'w',           0x00}, {'4',     0x00}, {KEY_F6,         0x00}},
/*18*/{{'o',            0x00}, {KEY_HOME,       0x00}, {0x00,         0x02}, {'p',             0x00}, {'i',           0x00}, {0x00,           0x00}, {'u',    0x00}, {'r',     0x00}, {'e',    0x00}, {KEY_CAPS_LOCK, 0x00}, {'3',     0x00}, {'t',            0x00}},
/*14*/{{'l',            0x00}, {KEY_SCROLL_LOCK,0x00}, {KEY_RETURN,   0x00}, {';',             0x00}, {'k',           0x00}, {0x00,           0x00}, {'j',    0x00}, {'f',     0x00}, {'d',    0x00}, {'<',           0x00}, {'a',     0x00}, {KEY_LEFT_GUI,   0x00}},
/*32*/{{'\'',           0x00}, {0x00,           0x00}, {KEY_RIGHT_MNU,0x00}, {'/',             0x00}, {KEY_RIGHT_ALT, 0x00}, {KEY_LEFT_ARROW, 0x00}, {'h',    0x00}, {'g',     0x00}, {KEY_F4, 0x00}, {'s',           0x00}, {KEY_ESC, 0x00}, {KEY_LEFT_ALT,   0x00}},
/*33*/{{'.',            0x00}, {KEY_END,        0x00}, {KEY_PAGE_DOWN,0x00}, {'\\',            0x00}, {',',           0x00}, {KEY_RIGHT_SHIFT,0x00}, {'m',    0x00}, {'v',     0x00}, {'c',    0x00}, {'x',           0x00}, {'z',     0x00}, {KEY_LEFT_SHIFT, 0x00}},
/*25*/{{KEY_RIGHT_CTRL, 0x00}, {KEY_RIGHT_ARROW,0x00}, {KEY_UP_ARROW, 0x00}, {KEY_DOWN_ARROW,  0x00}, {0x00,          0x00}, {' ',            0x00}, {'n',    0x00}, {'b',     0x00}, {' ',    0x00}, {0x00,          0x00}, {0x00,    0x00}, {KEY_LEFT_CTRL,  0x00}},
/*26*/{{KEY_F9,         0x00}, {KEY_PRTSC,      0x00}, {KEY_F11,      0x00}, {'=',             0x00}, {KEY_F8,        0x00}, {KEY_F10,        0x00}, {KEY_F7, 0x00}, {'5',     0x00}, {KEY_F2, 0x00}, {KEY_F1,        0x00}, {'`',     0x00}, {'6',            0x00}}
};

char RTC_DATA_ATTR mod_layout[2] = {'*',0x00}; //just a tmp buffer
uint8_t RTC_DATA_ATTR connected[2] = {0,0};

const RTC_RODATA_ATTR char* msg1 = "Configured GPIOS";


RTC_DATA_ATTR uint8_t board[sizeof(cols)][sizeof(rows)][BRD_HIST_SZ];
RTC_DATA_ATTR uint8_t led_state;

uint8_t fn_state;
uint8_t modifier;

#ifdef RAW_TABLE_DEBUG
unsigned display_period=0;
#endif

void esp_delay( unsigned ms) {
esp_sleep_enable_timer_wakeup(ms*1000);
//REG_SET_FIELD(RTC_CNTL_BIAS_CONF_REG, RTC_CNTL_DBG_ATTEN, 3);
esp_light_sleep_start();
}

// validates key presses through some filtering 
int key_press(int column, int row) {
  int tmp = 0;
  int tmp2= 0;
  int i;

#ifdef LEVEL_TRIGGER
  return board[column][row][0];// &&  board[column][row][1]);
  for( i=0; i<BRD_HIST_SZ; i++) {
    tmp+=board[column][row][i];
  }
  //remember there's BRD_HIST_SZ values of 0 or 1
  //as far as we're above average it's probably a stable high
  return tmp > (BRD_HIST_SZ/2);

#else // EDGE_TRIGGER
  //add up the fresh data
  for( i=0; i<(BRD_HIST_SZ/2+1); i++) {
    tmp+=board[column][row][i];
  }
  //add up the stale data
  for( i=(BRD_HIST_SZ/2); i<BRD_HIST_SZ; i++) {
    tmp2+=board[column][row][i];
  }
  //basically new data has more 1s then old data has more 0s
  return (tmp>(BRD_HIST_SZ/2)) && (tmp2<(BRD_HIST_SZ/2));
#endif
}

//validates key releases (used for modifiers)
int key_release(int column, int row) {
  int tmp = 0;
  int tmp2= 0;
  int i;

#ifdef LEVEL_TRIGGER
  return ((board[column][row][0]==0) &&
          (board[column][row][1]==0));
  for( i=0; i<BRD_HIST_SZ; i++) {
    tmp+=board[column][row][i];
  }
  //remember there's BRD_HIST_SZ values of 0 or 1
  //as far as we're above average it's probably a stable high
  return tmp < (BRD_HIST_SZ/2+1);

#else //EDGE_TRIGGER

  //add up the fresh data
  for( i=0; i<(BRD_HIST_SZ/2+1); i++) {
    tmp+=board[column][row][i];
  }
  //add up the stale data
  for( i=(BRD_HIST_SZ/2); i<BRD_HIST_SZ; i++) {
    tmp2+=board[column][row][i];
  }
  //basically new data has more 1s then old data has more 0s
  return (tmp2>(BRD_HIST_SZ/2)) && (tmp<(BRD_HIST_SZ/2));

#endif
}

int isModifier(uint8_t c) {

  switch (c) {
  case KEY_CAPS_LOCK  :
  case KEY_LEFT_GUI   :
  case KEY_RIGHT_ALT  :
  case KEY_LEFT_ALT   :
  case KEY_RIGHT_SHIFT:
  case KEY_LEFT_SHIFT :
  case KEY_RIGHT_CTRL :
  case KEY_LEFT_CTRL  :
    return 1;
  default:
    return 0;
  }
  return 0;
}

void scan_board() {
  uint8_t tmp, c, c1;
  
  for (uint8_t i=0; i<sizeof(cols); i++) {
    //every column is set high, one by one
    //goes low at the end of the iteration
    pinMode(cols[i], OUTPUT);
    digitalWrite(cols[i],   HIGH);
    //delay(1);//we need to allow this column and the previous to stabilize electrically

    for (uint8_t j=0; j<sizeof(rows); j++) {//for every column read all rows
      for(uint8_t k=(BRD_HIST_SZ-1); k>0; k--) {
        //keep a rolling window of all historical values of this key (for filtering)
        board[i][j][k]=board[i][j][k-1];
      }
      //read the raw line input (for that column)
      board[i][j][0]=digitalRead(rows[j]);
#ifndef RAW_TABLE_DEBUG
      c = layout[j][i][0];
      c1= layout[j][i][1];
      if(key_press(i,j) && (c || c1)) {//valid key press
        Serial.printf("/%02d.%02d: 0x%02X \'%c\'", cols[i], rows[j], c, c);
        if(isspace(c) || isprint(c)) {
          Serial.print("<printable>");
          mod_layout[0] = c;
          if(modifier) {
            switch (c) {
              case ',' : mod_layout[0] = '<';  break;
              case '.' : mod_layout[0] = '>';  break;
              case '/' : mod_layout[0] = '?';  break;
              case ';' : mod_layout[0] = ':';  break;
              case '\'': mod_layout[0] = '\"'; break;
              case '\\': mod_layout[0] = '|';  break;
              case '<' : mod_layout[0] = '|';  break;
              default:
                ;
            }
          }
          bleKeyboard.print((const char *)mod_layout);
          //bleKeyboard.press(mod_layout[0]);
        } else if(isModifier(c)) {
          Serial.print("<modifier>");
          if ((c==KEY_RIGHT_SHIFT) || (c==KEY_LEFT_SHIFT)) {
            modifier = 1;//some characters just aren't issued correctly with shift.
          } else if (c==KEY_CAPS_LOCK) {
            led_state++;
            led_state %= 2;
            Serial.printf("%c", led_state?'*':'.');
            digitalWrite(LED_PIN, led_state);
          }
          bleKeyboard.press(c);
        } else { // multimedia keys, enter esc, f keys
          if(layout[j][i][1]==0) {// escape,tab, backspace are one byte
            Serial.print("<special_key>");
            bleKeyboard.write(layout[j][i][0]);
            //bleKeyboard.press(c);
          } else { //multimedia keys are 2 bytes
            Serial.print("<media_key>");
            bleKeyboard.write(layout[j][i]);
          }
        }
        Serial.print("\n");
      } else if(key_release(i,j) && (c || c1)) {
        Serial.printf("\\%02d.%02d: 0x%02X \'%c\'\n", cols[i], rows[j], c, c);
        bleKeyboard.release(c);
        if ((c==KEY_RIGHT_SHIFT) || (c==KEY_LEFT_SHIFT)) {
          modifier = 0;//some characters just aren't issued correctly with shift.
        }
      }
#endif //RAW_TABLE_DEBUG
    }
    pinMode(cols[i], INPUT_PULLDOWN);
  }
}

#ifdef RAW_TABLE_DEBUG
void print_board() {
  for (uint8_t j=0; j<sizeof(rows); j++) {
    for (uint8_t i=0; i<sizeof(cols); i++) {
      Serial.printf("%c ",key_press(i,j)?'*':'.');
    }
    if(j==0) {
      Serial.printf("|%c\n",fn_state?'*':'.');
    } else {
      Serial.printf("|\n");
    }
  }
  Serial.printf("___________________________\n");
}
#endif


uint32_t timeout_cnt_ms;
uint64_t prev_millis;




//this gets called if you hold the wake-up key
void irq_recv() {
  ;
}

void periodic_sleep_check() {
  uint64_t now = millis();

  timeout_cnt_ms += now - prev_millis;
  prev_millis = now;

  if(timeout_cnt_ms > DEEP_SLEEP_TIMEOUT_MS) {
    //trigger deep sleep
    bleKeyboard.releaseAll();

    //wake-up on a gpio (possible gpios 0,2,4,12-15,25-27,32-39)
    pinMode(rows[WAKEUP_KEY_ROW], INPUT_PULLDOWN);
    pinMode(cols[WAKEUP_KEY_COL], OUTPUT);
    digitalWrite(cols[WAKEUP_KEY_COL], HIGH);

    for (int i=0; i<sizeof(cols); i++) {
      pinMode(cols[i], OUTPUT);
      if( i==WAKEUP_KEY_COL ) {
        digitalWrite(cols[i], HIGH);
      } else {
        digitalWrite(cols[i], LOW);
      }
      gpio_hold_en((gpio_num_t)cols[i]);
    }
    gpio_hold_en((gpio_num_t)LED_PIN);
    gpio_deep_sleep_hold_en();

    esp_sleep_enable_ext0_wakeup((gpio_num_t)rows[WAKEUP_KEY_ROW], 1); //1 = Low to High, 0 = High to Low.
    attachInterrupt(digitalPinToInterrupt((gpio_num_t)rows[WAKEUP_KEY_ROW]), irq_recv, RISING);

    esp_deep_sleep_start();
  }
}

void setup() {
  int i,j,k;

  Serial.begin(115200);
  Serial.printf("********** Chip Rev:%d ************\n", ESP.getChipRevision());

#ifndef RAW_TABLE_DEBUG
  bleKeyboard.begin();
  bleKeyboard.setDelay(BLE_DELAY_MS);
#endif

  gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t)LED_PIN);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, led_state);
  //pinMode(FN_PIN, INPUT);

  for (i=0; i<sizeof(cols);i++) {
    gpio_hold_dis((gpio_num_t)cols[i]);
    pinMode(cols[i], INPUT_PULLDOWN);
  }
  for (i=0; i<sizeof(rows);i++) {
    gpio_hold_dis((gpio_num_t)rows[i]);
    pinMode(rows[i], INPUT_PULLDOWN);
  }
  Serial.println(msg1);

  fn_state=0;
  led_state=0;
  connected[1]==0;
  connected[0]==0;
  for (i=0; i<sizeof(cols); i++) {
    for (j=0; j<sizeof(rows); j++) {
      for(k=0; k<BRD_HIST_SZ; k++) {
        board[i][j][k] = 0;
      }
    }
  }
}

void loop() {
int ret;
#ifndef RAW_TABLE_DEBUG // normal bluetooth functionality
  if(connected[0] = bleKeyboard.isConnected()) {
    //once connected switch from using the led for iddle animation to caps lock
    if((connected[1]==0) && (connected[0]==1)) {
      led_state = 0;
      digitalWrite(LED_PIN, led_state);
      timeout_cnt_ms = 0; //we reset on connect and keypress
      setCpuFrequencyMhz(80); //we can now run lower freq for power saving
      delay(1);
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT,ESP_PWR_LVL_N12); //lower tx power -12db
      esp_bt_sleep_enable(); //allow oportunistic radio sleep
    }
    scan_board(); // read keys, send keys on ble
    periodic_sleep_check();//updates counters, timeout to deep_sleep
    delay( SCN_DELAY_MS );
    //esp_delay( 500 );
  } else {
      if((connected[1]==1) && (connected[0]==0)) {
        setCpuFrequencyMhz(160); //we can now run lower freq for power saving
        delay(1);
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P4); //lower tx power -12db
        esp_bt_sleep_disable(); //allow oportunistic radio sleep
      }
      Serial.print(".");
      led_state &=1;
      digitalWrite(LED_PIN, led_state++);
      delay( ADVERTISE_DELAY_MS );
  }
  connected[1] = connected[0];
#else //don't bother with bluetooth, just print the raw buttons on serial
  scan_board();
  display_period%=1;
  if(!display_period)
    print_board();
  display_period++;
  //delay(1);
  esp_delay(2000);
#endif
}