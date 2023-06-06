const uint8_t cols[]={       23,   22,    13,   21,     5,    16,    17,  0,   4,   15,  2,  27,  };
const uint8_t rows[]={19, /* 9     paus   del   0       8     Bspc   7    tab  Q    2    1         */
                      12, /* mins  pgup   F12  lbrc     rbrc  ins    Y    F5   F3   W    4    F6   */
                      18, /* O     home   calc  P       I            U    R    E    caps 3    T    */
                      14, /* L     slck   ent   scln    K     bsls   J    F    D         A    lgui */
                      32, /* quot         app   slsh    ralt  left   H    G    F4   S    esc  lalt */
                      33, /* dot   end    pgdn  rshift  comm         M    V    C    X    Z    lsft */
                      25, /* rctl  right  up    down          rspc   N    B    lspc           lctl */
                      26};/* F9    pscr   F11   eql     F8    F10    F7   5    F2   F1   grv  6    */
#define FN_PIN      34
#define BRD_HIST_SZ 5
uint8_t board[sizeof(cols)][sizeof(rows)][BRD_HIST_SZ];
uint8_t fn_state;

void setup() {
  int i,j,k;
  Serial.begin(115200);
  Serial.println("**********************");
  for (i=0; i<sizeof(cols)/sizeof(uint8_t);i++) {
    pinMode(cols[i], OUTPUT);
    digitalWrite(cols[i], LOW);
  }
  for (i=0; i<sizeof(rows)/sizeof(uint8_t);i++) {
    pinMode(rows[i], INPUT_PULLDOWN);
  }
  pinMode(FN_PIN, INPUT);
  
  Serial.println("Configured GPIOS");

  for (i=0; i<sizeof(cols)/sizeof(uint8_t); i++) {
    for (j=0; j<sizeof(rows)/sizeof(uint8_t); j++) {
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
bool stabilized_board(int column, int row) {
  int tmp, i;
  for( i=0, tmp=0; i<BRD_HIST_SZ; i++) {
    tmp+=board[column][row][i];
  }
  //remember there's BRD_HIST_SZ values of 0 or 1
  //as far as we're above average it's probably a stable high
  return tmp > (BRD_HIST_SZ/2);
}

void print_board() {
  for (uint8_t j=0; j<sizeof(rows)/sizeof(uint8_t); j++) {
    for (uint8_t i=0; i<sizeof(cols)/sizeof(uint8_t); i++) {
      Serial.printf("%c ",stabilized_board(i,j)?'*':'.');
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
  uint8_t tmp;
  
  for (uint8_t i=0; i<(sizeof(cols)/sizeof(uint8_t)); i++) {
    digitalWrite(cols[i],   HIGH);
    //delay(1);
    fn_state = digitalRead(FN_PIN);
    //if (tmp == 1) {
    //  fn_state=1;
    //  Serial.printf("==%d==\n",cols[i]);
    //}

    for (uint8_t j=0; j<(sizeof(rows)/sizeof(uint8_t)); j++) {
      board[i][j][4]=board[i][j][3];
      board[i][j][3]=board[i][j][2];
      board[i][j][2]=board[i][j][1];
      board[i][j][1]=board[i][j][0];
      board[i][j][0]=digitalRead(rows[j]);
    }
    digitalWrite(cols[i],   LOW);
  }
}

unsigned display_period=0;

void loop() {
  //Serial.println("scan loop\n");
  scan_board();
  display_period%=200;
  if(!display_period)
    print_board();
  delay(1);
  display_period++;
}