#include <BleKeyboard.h>
#include <ctype.h>

BleKeyboard bleKeyboard("Sculpt ESP32", "Microsoft", 100);
const uint8_t cols[]={       23,   22,    13,   21,     5,    16,    17,  0,   4,   15,  2,  27    };
const uint8_t rows[]={19, /* 9     paus   del   0       8     Bspc   7    tab  Q    2    1         */
                      12, /* mins  pgup   F12  lbrc     rbrc  ins    Y    F5   F3   W    4    F6   */
                      18, /* O     home   calc  P       I            U    R    E    caps 3    T    */
                      14, /* L     slck   ent   scln    K     bsls   J    F    D         A    lgui */
                      32, /* quot         app   slsh    ralt  left   H    G    F4   S    esc  lalt */
                      33, /* dot   end    pgdn  rshift  comm         M    V    C    X    Z    lsft */
                      25, /* rctl  right  up    down          rspc   N    B    lspc           lctl */
                      26};/* F9    pscr   F11   eql     F8    F10    F7   5    F2   F1   grv`  6    */
/*
TBD - might not the right ones:
KEY_PAUSE
KEY_SCROLL_LOCK
app
scln
KEY_MEDIA_CALCULATOR-> -*
KEY_MEDIA_PLAY_PAUSE
BleKEyboard doesn't match codes to any of these:
https://cdn.sparkfun.com/datasheets/Wireless/Bluetooth/RN-HID-User-Guide-v1.0r.pdf
https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf
*/
#define KEY_PAUSE 0x13
#define KEY_SCROLL_LOCK 0x47
// 0x00 0x00 is a placeholder for no key

uint8_t layout[sizeof(rows)][sizeof(cols)][2] = {
  {{'9',            0x00}, {KEY_PAUSE,      0x00}, {KEY_DELETE,   0x00}, {'0',             0x00}, {'8',           0x00}, {KEY_BACKSPACE,  0x00}, {'7',    0x00}, {KEY_TAB, 0x00}, {'q',    0x00}, {'2',           0x00}, {'1',     0x00}, {0x00,           0x00}},
  {{'-',            0x00}, {KEY_PAGE_UP,    0x00}, {KEY_F12,      0x00}, {'[',             0x00}, {']',           0x00}, {KEY_INSERT,     0x00}, {'y',    0x00}, {KEY_F5,  0x00}, {KEY_F3, 0x00}, {'w',           0x00}, {'4',     0x00}, {KEY_F6,         0x00}},
  {{'o',            0x00}, {KEY_HOME,       0x00}, {0x00,         0x02}, {'p',             0x00}, {'i',           0x00}, {0x00,           0x00}, {'u',    0x00}, {'r',     0x00}, {'e',    0x00}, {KEY_CAPS_LOCK, 0x00}, {'3',     0x00}, {'t',            0x00}},
  {{'l',            0x00}, {KEY_SCROLL_LOCK,0x00}, {KEY_RETURN,   0x00}, {';',             0x00}, {'k',           0x00}, {'\\',           0x00}, {'j',    0x00}, {'f',     0x00}, {'d',    0x00}, {0x00,          0x00}, {'a',     0x00}, {KEY_LEFT_GUI,   0x00}},
  {{'\'',           0x00}, {0x00,           0x00}, {0x00,         0x80}, {'/',             0x00}, {KEY_RIGHT_ALT, 0x00}, {KEY_LEFT_ARROW, 0x00}, {'h',    0x00}, {'g',     0x00}, {KEY_F4, 0x00}, {'s',           0x00}, {KEY_ESC, 0x00}, {KEY_LEFT_ALT,   0x00}},
  {{'.',            0x00}, {KEY_END,        0x00}, {KEY_PAGE_DOWN,0x00}, {KEY_RIGHT_SHIFT, 0x00}, {';',           0x00}, {0x00,           0x00}, {'m',    0x00}, {'v',     0x00}, {'c',    0x00}, {'x',           0x00}, {'z',     0x00}, {KEY_LEFT_SHIFT, 0x00}},
  {{KEY_RIGHT_CTRL, 0x00}, {KEY_RIGHT_ARROW,0x00}, {KEY_UP_ARROW, 0x00}, {KEY_DOWN_ARROW,  0x00}, {0x00,          0x00}, {' ',            0x00}, {'n',    0x00}, {'b',     0x00}, {' ',    0x00}, {0x00,          0x00}, {0x00,    0x00}, {KEY_LEFT_CTRL,  0x00}},
  {{KEY_F9,         0x00}, {KEY_PRTSC,      0x00}, {KEY_F11,      0x00}, {'=',             0x00}, {KEY_F8,        0x00}, {KEY_F10,        0x00}, {KEY_F7, 0x00}, {'5',     0x00}, {KEY_F2, 0x00}, {KEY_F1,        0x00}, {'`',     0x00}, {'6',            0x00}}
};
#define FN_PIN      34
#define BRD_HIST_SZ 5
uint8_t board[sizeof(cols)][sizeof(rows)][BRD_HIST_SZ];
uint8_t fn_state;

void setup() {
  int i,j,k;
  Serial.begin(115200);
  Serial.println("**********************");

  bleKeyboard.begin();

  for (i=0; i<sizeof(cols);i++) {
    pinMode(cols[i], OUTPUT);
    digitalWrite(cols[i], LOW);
  }
  for (i=0; i<sizeof(rows);i++) {
    pinMode(rows[i], INPUT_PULLDOWN);
  }
  //pinMode(FN_PIN, INPUT);

  Serial.println("Configured GPIOS");

  for (i=0; i<sizeof(cols); i++) {
    for (j=0; j<sizeof(rows); j++) {
      for(k=0; k<BRD_HIST_SZ; k++) {
        board[i][j][k] = 0;
      }
    }
  }
  fn_state=0;
}

/* this takes in a position on the button matrix
 * returning the average of historical values
*/ 
int key_press(int column, int row) {
  int tmp, i;

#if LEVEL_TRIGGER
  for( i=0, tmp=0; i<BRD_HIST_SZ; i++) {
    tmp+=board[column][row][i];
  }
  //remember there's BRD_HIST_SZ values of 0 or 1
  //as far as we're above average it's probably a stable high
  return tmp > (BRD_HIST_SZ/2);
#else
  return (board[column][row][3]==0 &&
          board[column][row][2]==0 &&
          board[column][row][1]==1 &&
          board[column][row][0]==1);
#endif
}
int key_release(int column, int row) {
  int tmp, i;

#if LEVEL_TRIGGER
  for( i=0, tmp=0; i<BRD_HIST_SZ; i++) {
    tmp+=board[column][row][i];
  }
  //remember there's BRD_HIST_SZ values of 0 or 1
  //as far as we're above average it's probably a stable high
  return tmp < (BRD_HIST_SZ/2);
#else
  return (board[column][row][3]==1 &&
          board[column][row][2]==1 &&
          board[column][row][1]==0 &&
          board[column][row][0]==0);
#endif
}
int isModifier(uint8_t c) {
  switch (c) {
//  case KEY_CAPS_LOCK  :
  case KEY_LEFT_GUI   :
  case KEY_RIGHT_ALT  :
  case KEY_LEFT_ALT   :
  case KEY_RIGHT_SHIFT:
  case KEY_LEFT_SHIFT :
  case KEY_RIGHT_CTRL :
  case KEY_LEFT_CTRL  :
    return 1;
  break;
  default:
    return 0;
  }
  return 0;
}

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

void scan_board() {
  uint8_t tmp, c, c1;

  
  for (uint8_t i=0; i<sizeof(cols); i++) {
    digitalWrite(cols[i],   HIGH);
    //delay(1);
    fn_state = digitalRead(FN_PIN);
    //if (tmp == 1) {
    //  fn_state=1;
    //  Serial.printf("==%d==\n",cols[i]);
    //}

    for (uint8_t j=0; j<sizeof(rows); j++) {
      board[i][j][4]=board[i][j][3];
      board[i][j][3]=board[i][j][2];
      board[i][j][2]=board[i][j][1];
      board[i][j][1]=board[i][j][0];
      board[i][j][0]=digitalRead(rows[j]);
      c = layout[j][i][0];
      c1= layout[j][i][1];
      if(key_press(i,j) && (c || c1)) {
        Serial.printf("%02d.%02d: 0x%02X \'%c\'", cols[i], rows[j], c, c);
        if(isspace(c) || isprint(c)) {
          Serial.print("<alphanum>");
          bleKeyboard.print((const char *)layout[j][i]);
        } else if(isModifier(c)) {
          Serial.print("<modifier>");
          bleKeyboard.press(c);
        } else { // multimedia keys, enter esc, f keys
          if(layout[j][i][1]==0) {// escape and calculator behave differently
            Serial.print("<special_key>");
            bleKeyboard.write(layout[j][i][0]);
          } else {
            Serial.print("<media_key>");
            bleKeyboard.write(layout[j][i]);
          }
        }
        Serial.print("\n");
      }
      if(key_release(i,j) && isModifier(c)) {
          bleKeyboard.release(c);
      }
    }
    digitalWrite(cols[i],   LOW);
  }
}

unsigned display_period=0;

void loop() {
  
  if(bleKeyboard.isConnected()) {
    scan_board();
    delay(10);
  } else {
      Serial.print(".");
      delay(5000);
  }
  /*
  display_period%=200;
  if(!display_period)
    print_board();
  display_period++;
  */
}