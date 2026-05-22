//imports & constants
  #include <Adafruit_NeoPixel.h>
  #include <ListLib.h>
  #include <LiquidCrystal_I2C.h>
  #include <Wire.h>
  #include <TimerOne.h>

  #define PI 3.14

  #define LEDS 40 // hopefully 64, for checkers
  #define PIN 22

  #define SCREENPIN 0x27
  #define SCREENWIDTH 16
  #define SCREENHEIGHT 2

Adafruit_NeoPixel strip(LEDS, PIN, NEO_GRB + NEO_KHZ800);
LiquidCrystal_I2C screen(SCREENPIN, SCREENWIDTH, SCREENHEIGHT);

//classes & stuff
  class Direction {
    public:
      int x;
      int y;
      Direction(int a, int b) {
        x = a;
        y = b;
      }
      Direction() {
        x = 0;
        y = 0;
      }

      bool operator ^(Direction D) {
        return this->x == -D.x && this->y == -D.y;
      }
  };

  class Segment {
    public:
      int x;
      int y;
      Segment(int a, int b) {
        x = a;
        y = b;
      }
      Segment() {
        x = 0;
        y = 0;
      }

      Segment operator +(Direction D) {
        return Segment(this->x + D.x, this->y + D.y);
      }

      Segment& operator +=(Direction D) {
        this->x += D.x;
        this->y += D.y;
        return *this;
      }

      bool operator ==(Segment S) {
        return this->x == S.x && this->y == S.y;
      }
  };

  class HSV {
    public:
      double h;
      double s;
      double v;
      HSV(double a, double b, double c) {
        h = a;
        s = b;
        v = c;
      }
      HSV() {
        h = 0.0;
        s = 0.0;
        v = 0.0;
      }
  };

  class RGB {
    public:
      double r;
      double g;
      double b;
      RGB(double a, double c, double d) {
        r = a;
        g = c;
        b = d;
      }
      RGB() {
        r = 0.0;
        g = 0.0;
        b = 0.0;
      }
      uint32_t toColor() {
        return strip.Color(round(this->r), round(this->g), round(this->b));
      }
      void normalize() {
        double lowest = min(this->r, min(this->g, this->b))+1.0-1.0;
        this->r -= lowest;
        this->g -= lowest;
        this->b -= lowest;
        double highest = max(this->r, max(this->g, this->b))+1.0-1.0;
        this->r *= 255.0/highest;
        this->g *= 255.0/highest;
        this->b *= 255.0/highest;
      }
  };

  // HSV P1_color = new HSV();
  // HSV P1_head_color = new HSV();
  // HSV P1_win_color = new HSV();
  // HSV P2_color = new HSV();
  // HSV P2_head_color = new HSV();
  // HSV P2_win_color = new HSV();
  // HSV apple_color = new HSV();
  // HSV bg_color = new HSV();
  List<HSV> colors;

  class Menu {
    public:
      List<String> headers;
      int selected;
      Menu() {
        headers.Add("P1B");
        headers.Add("P1H");
        headers.Add("P1W");
        headers.Add("P2B");
        headers.Add("P2H");
        headers.Add("P2W");
        headers.Add("App");
        headers.Add("BG ");
        selected = 0;
      }
      void Draw() {
        screen.clear();
        screen.setCursor(0,0);
        screen.print(this->headers[this->selected]);
        screen.setCursor(5, 0);
        screen.print(int((colors[this->selected].h)/2.0/PI*255.0));
        screen.setCursor(0, 1);
        screen.print("s" + (int((colors[this->selected].s)*255.0)));
        screen.setCursor(5, 1);
        screen.print("v" + (int((colors[this->selected].v)*255.0)));
      }
  };

//snake defs
  const Direction L = Direction( 0, -1);
  const Direction R = Direction( 0,  1);
  const Direction U = Direction(-1,  0);
  const Direction D = Direction( 1,  0);

  int board[4][4];
  List<Segment> P1;
  Direction P1_Dir;
  Direction P1_Next_Dir;
  int P1_Length = 1;
  List<Segment> P2;
  Direction P2_Dir;
  Direction P2_Next_Dir;
  int P2_Length = 1;
  Segment apple;

  bool P1_Moving = true;

  bool dead;
  bool P1_wins;
  bool P2_wins;
  int start_timer;

//input defs
  int P1_X = A15;
  int P1_Y = A14;
  int P2_X = A13;
  int P2_Y = A12;

  int DEADZONE = 200;

  int P1_PUSH = 41;
  int P2_PUSH = 39;

//board stuff
  bool P1_AI = false;
  bool P2_AI = false;
  int P1_AI_counter = 0;
  int P2_AI_counter = 0;

  const int ROWS = 5;
  const int COLS = 8;

  int RAND_PRECISION = 1000;
  int TIMESTEP = 200;

  float deathFlicker = 250.0;
  int deathTime = 2500;
  int AIstart = 1000;

//rotary encoders
  int clkPins[4] = {23, 25, 27, 29};
  int dtPins[4] = {24, 26, 28, 30};
  int lastClkStates[4] = {LOW, LOW, LOW, LOW};
  int rotaryStates[4] = {0, 0, 0, 0};

//7 segment stuff
  unsigned long lastDigitSwitch = millis();
  int lastUpdatedDigit = 0;
  int DIGIT_PINS[4] = {53, 47, 45, 42};
  int SEGMENT_PINS[7] = {51, 43, 46, 50, 52, 49, 44}; //48 is DP
  int SEGMENT_FILTER[11][7] = {
    {1,1,1,1,1,1,0}, // 0
    {0,1,1,0,0,0,0}, // 1
    {1,1,0,1,1,0,1}, // 2
    {1,1,1,1,0,0,1}, // 3
    {0,1,1,0,0,1,1}, // 4
    {1,0,1,1,0,1,1}, // 5
    {1,0,1,1,1,1,1}, // 6
    {1,1,1,0,0,0,0}, // 7
    {1,1,1,1,1,1,1}, // 8
    {1,1,1,1,0,1,1}, // 9
    {0,0,0,0,0,0,0}  // blank
  };
  int current_nums[4] = {10, 10, 10, 10};

int INDEX_TRANSFORMS[ROWS][COLS] = {
  {39, 38, 37, 36, 35, 34, 33, 32},
  { 9,  6,  5,  4,  3,  2,  1,  0},
  {10, 11, 12, 13, 14, 15, 16, 17},
  { 7,  8, 26, 27, 28, 29, 30, 31},
  {25, 24, 23, 22, 21, 20, 19, 18}
};

Menu menu;
unsigned long timeSinceLastMenu;

unsigned long lastUpdate;

void setup() {

  Timer1.initialize(2500);
  Timer1.attachInterrupt(displayNums);

  // pinMode(6, OUTPUT);

  //seed with an unconnected pin, as environmental noise is not consistent
  randomSeed(analogRead(A0));

  if (random(2) == 0) {
    apple = Segment(floor((COLS-1)/2.0), floor((ROWS-1)/2.0));
  } else {
    apple = Segment(ceil ((COLS-1)/2.0), ceil ((ROWS-1)/2.0));
  }

  //intiialize joystick pins
  pinMode(P1_X, INPUT);
  pinMode(P1_Y, INPUT);
  pinMode(P2_X, INPUT);
  pinMode(P2_Y, INPUT);
  pinMode(P1_PUSH, INPUT_PULLUP);
  pinMode(P2_PUSH, INPUT_PULLUP);

  //initialize board pins
  strip.begin(); strip.clear(); strip.show();
  strip.setBrightness(50);
  strip.show();

  //initialize rotary encoders
  for (int i = 0; i < 4; i++) {
    pinMode(clkPins[i], INPUT);
    pinMode(dtPins[i], INPUT);
    lastClkStates[i] = digitalRead(clkPins[i]);
  }

  Serial.begin(9600);

  //initialize colors
  colors.Add(HSV(2/3.0*PI, 1, 1));
  colors.Add(HSV(PI, 1, 1));
  colors.Add(HSV(2/3.0*PI, 1, 0.05));
  colors.Add(HSV(0.0, 1, 1));
  colors.Add(HSV(-1/3.0*PI, 1, 1));
  colors.Add(HSV(0, 1, 0.05));
  colors.Add(HSV(4/3.0*PI, 1, 1));
  colors.Add(HSV(0, 0, 0.03));

  //initialize screen
  screen.begin();
  screen.backlight();

  //initialize snakes
  resetSnakes();

  dead = true;
  P1_wins = false;
  P2_wins = false;
  start_timer = 3;
  P1_Moving = true;

  // Serial.println(pos_to_index(0,0));
  // Serial.println(pos_to_index(3,0));
  // Serial.println(pos_to_index(0,1));
  // Serial.println(pos_to_index(3,1));
  // Serial.println(pos_to_index(0,2));
  // Serial.println(pos_to_index(3,2));
  // Serial.println(pos_to_index(0,3));
  // Serial.println(pos_to_index(3,3));

  for (int i = 0; i < 4; i++) {
    pinMode(DIGIT_PINS[i], OUTPUT);
  }

  for (int i = 0; i < 8; i++) {
    pinMode(SEGMENT_PINS[i], OUTPUT);
  }

  digitalWrite(DIGIT_PINS[0], HIGH);
  digitalWrite(DIGIT_PINS[1], HIGH);
  digitalWrite(DIGIT_PINS[2], HIGH);
  digitalWrite(DIGIT_PINS[3], HIGH);

  menu = Menu();
  screen.setCursor(0, 0);
  screen.print("Hello World!");
}

void loop() {
  // HSV test1 = colors[4];
  // // Serial.print(-1/3.0*PI);
  // // Serial.print("    ");
  // // Serial.print(test1.h);
  // // Serial.print("  ");
  // // Serial.print(test1.s);
  // // Serial.print("  ");
  // // Serial.print(test1.v);
  // // Serial.print("    ");
  // RGB test = rgb(colors[4]);
  // // Serial.print(test.r);
  // // Serial.print("  ");
  // // Serial.print(test.g);
  // // Serial.print("  ");
  // // Serial.println(test.b);

  // digitalWrite(6, HIGH);
  display();
  if (dead) {
    if (digitalRead(P1_PUSH) == LOW && digitalRead(P2_PUSH) == LOW || millis()-lastUpdate >= AIstart) {
      // Serial.println("****");
      dead = false;
      P1_wins = false;
      P2_wins = false;
      P1_AI_counter = 0;
      P2_AI_counter = 0;
      resetSnakes();
      P1_AI = millis()-lastUpdate >= AIstart;
      P2_AI = millis()-lastUpdate >= AIstart;
      start_timer = 3;
      P1_Moving = true;
    }
    displayScreen(P1_Length, P2_Length);
    // readEncoders();
    LCDDisplay();
  } else if (start_timer > 0) {
    // Serial.println("OH ****");
    if (millis()-lastUpdate >= TIMESTEP) {
      lastUpdate = millis();
      if (digitalRead(P1_PUSH) == LOW) {
        P2_AI_counter ++;
      }
      if (digitalRead(P2_PUSH) == LOW) {
        P1_AI_counter ++;
      }
      start_timer -= 1;
    }
    displayScreen(start_timer, start_timer);
  } else {
    if (P1_AI_counter == 3) {
      P1_AI = true;
    }
    if (P2_AI_counter == 3) {
      P2_AI = true;
    }
    responsive_turn();
    if (millis()-lastUpdate >= TIMESTEP) {
      lastUpdate = millis();
      if (P1_Moving) {
        move_P1();
        tick_apple();
      } else {
        move_P2();
        tick_apple();
      }
      P1_Moving = !P1_Moving;
    }
    displayScreen(P1_Length, P2_Length);
  }
}

//useless stuff
  void LCDDisplay() {
    if (timeSinceLastMenu-millis() > 100) {
      menu.Draw();
      timeSinceLastMenu = millis();
    }
  }

  void readEncoders() {
    for (int i = 0; i < 4; i++) {
      int clk = digitalRead(clkPins[i]);
      int dt = digitalRead(dtPins[i]);
      
      if (clk != lastClkStates[i]) {
        // Serial.println("hello");
        if (clk == LOW) {  // only on falling edge
          if (dt == LOW) {
            if (i == 0) {
              menu.selected++;
              if (menu.selected > 7) {
                menu.selected = 0;
              }
            } else if (i == 1) {
              colors[menu.selected].h += 0.1;
              if (colors[menu.selected].h > 2.0*PI) {
                colors[menu.selected].h -= 2.0*PI;
              }
            } else if (i == 2) {
              colors[menu.selected].s += 0.01;
              if (colors[menu.selected].s > 1.0) {
                colors[menu.selected].s = 1;
              }
            } else if (i == 3) {
              colors[menu.selected].v += 0.01;
              if (colors[menu.selected].v > 1.0) {
                colors[menu.selected].v = 1;
              }
            }
          } else {
            if (i == 0) {
              menu.selected--;
              if (menu.selected < 0) {
                menu.selected = 7;
              }
            } else if (i == 1) {
              colors[menu.selected].h -= 0.1;
              if (colors[menu.selected].h < 0.0) {
                colors[menu.selected].h += 2.0*PI;
              }
            } else if (i == 2) {
              colors[menu.selected].s -= 0.01;
              if (colors[menu.selected].s < 0.0) {
                colors[menu.selected].s = 0;
              }
            } else if (i == 3) {
              colors[menu.selected].v -= 0.01;
              if (colors[menu.selected].v < 0.0) {
                colors[menu.selected].v = 0;
              }
            }
          }
        }
      }
      lastClkStates[i] = clk;
    }
  }

//snake stuff
  void resetSnakes() {
    P1_Length = 1;
    P1.Clear();
    P1_Dir = D;
    P1_Next_Dir = D;
    P1.Add(Segment(     0, floor((ROWS-1)/2.0)) );
    P1_AI = false;
    P1_AI_counter = 0;
    P2_Length = 1;
    P2.Clear();
    P2_Dir = U;
    P2_Next_Dir = U;
    P2.Add(Segment(COLS-1, ceil ((ROWS-1)/2.0)) );
    P2_AI = false;
    P2_AI_counter = 0;
    if (random(2) == 0) {
      apple = Segment(floor((COLS-1)/2.0), floor((ROWS-1)/2.0));
    } else {
      apple = Segment(ceil ((COLS-1)/2.0), ceil ((ROWS-1)/2.0));
    }
  }

  void tick_apple() {
    if (within_P1(apple)) {
      move_apple();
      P1_Length ++;
    }
    if (within_P2(apple)) {
      move_apple();
      P2_Length ++;
    }
  }

  void move_apple() {
    while (within_P1(apple) || within_P2(apple)) {
      apple.x = random(COLS);
      apple.y = random(ROWS);
    }
  }

  bool within_P1(Segment loc) {
    for (int i = 0; i < P1.Count(); i++) {
      if (P1[i] == loc) {
        return true;
      }
    }
    return false;
  }

  bool within_P2(Segment loc) {
    for (int i = 0; i < P2.Count(); i++) {
      if (P2[i] == loc) {
        return true;
      }
    }
    return false;
  }

  void responsive_turn() {
    int x_read = analogRead(P1_X)-511;
    int y_read = analogRead(P1_Y)-511;

    if (abs(x_read) < DEADZONE) {
      x_read = 0;
    }

    if (abs(y_read) < DEADZONE) {
      y_read = 0;
    }

    

    if (abs(x_read) > 0 || abs(y_read) > 0) {
      P1_AI = false;
      if (abs(y_read) >= abs(x_read)) {
        if (y_read > 0) {
          turn_P1(D);
        } else {
          turn_P1(U);
        }
      } else {
        if (x_read > 0) {
          turn_P1(L);
        } else {
          turn_P1(R);
        }
      }
    }

    x_read = analogRead(P2_X)-511;
    y_read = analogRead(P2_Y)-511;

    if (abs(x_read) < DEADZONE) {
      x_read = 0;
    }

    if (abs(y_read) < DEADZONE) {
      y_read = 0;
    }

    

    if (abs(x_read) > 0 || abs(y_read) > 0) {
      P2_AI = false;
      if (abs(y_read) >= abs(x_read)) {
        if (y_read > 0) {
          turn_P2(D);
        } else {
          turn_P2(U);
        }
      } else {
        if (x_read > 0) {
          turn_P2(L);
        } else {
          turn_P2(R);
        }
      }
    }

  }

  void move_P1() {

    if (P1_AI) {
      P1_AI_input();
    }

    P1_Dir = P1_Next_Dir;

    Segment P1_Head_Pos = P1[P1.Count()-1];
    P1_Head_Pos = P1_Head_Pos + P1_Dir;

    if (P1.Count() >= P1_Length) {
      P1.Remove(0);
    }

    if (
      within_P2(P1_Head_Pos)
      || within_P1(P1_Head_Pos)
      || P1_Head_Pos.x < 0
      || P1_Head_Pos.y < 0
      || P1_Head_Pos.x >= COLS
      || P1_Head_Pos.y >= ROWS
      )
    {
      dead = true;
      P1_wins = false;
      P2_wins = true;
    }

    P1.Add(P1_Head_Pos);

  }

  void turn_P1(Direction D) {
    if ((D.x != 0) == (P1_Dir.x == 0)) {
      P1_Next_Dir = D;
    }
  }

  void move_P2() {

    if (P2_AI) {
      P2_AI_input();
    }

    P2_Dir = P2_Next_Dir;

    Segment P2_Head_Pos = P2[P2.Count()-1];
    P2_Head_Pos = P2_Head_Pos + P2_Dir;

    if (P2.Count() >= P2_Length) {
      P2.Remove(0);
    }

    if (
      within_P1(P2_Head_Pos)
      || within_P2(P2_Head_Pos)
      || P2_Head_Pos.x < 0
      || P2_Head_Pos.y < 0
      || P2_Head_Pos.x >= COLS
      || P2_Head_Pos.y >= ROWS
      )
    {
      dead = true;
      P1_wins = true;
      P2_wins = false;
    }

    P2.Add(P2_Head_Pos);

  }

  void turn_P2(Direction D) {
    if ((D.x != 0) == (P2_Dir.x == 0)) {
      P2_Next_Dir = D;
    }
  }

//AI stuff
  void P1_AI_input() {
    int L_score = (L ^ P1_Dir) ? -100 : AI_score(P1[P1.Count()-1], L);
    int R_score = (R ^ P1_Dir) ? -100 : AI_score(P1[P1.Count()-1], R);
    int U_score = (U ^ P1_Dir) ? -100 : AI_score(P1[P1.Count()-1], U);
    int D_score = (D ^ P1_Dir) ? -100 : AI_score(P1[P1.Count()-1], D);
    int best = max(max(L_score, R_score), max(U_score, D_score));

    if (L_score == best) {
      P1_Next_Dir = L;
    }
    if (R_score == best) {
      P1_Next_Dir = R;
    }
    if (U_score == best) {
      P1_Next_Dir = U;
    }
    if (D_score == best) {
      P1_Next_Dir = D;
    }
  }

  void P2_AI_input() {
    int L_score = (L ^ P2_Dir) ? -100 : AI_score(P2[P2.Count()-1], L);
    int R_score = (R ^ P2_Dir) ? -100 : AI_score(P2[P2.Count()-1], R);
    int U_score = (U ^ P2_Dir) ? -100 : AI_score(P2[P2.Count()-1], U);
    int D_score = (D ^ P2_Dir) ? -100 : AI_score(P2[P2.Count()-1], D);
    int best = max(max(L_score, R_score), max(U_score, D_score));

    if (L_score == best) {
      P2_Next_Dir = L;
    }
    if (R_score == best) {
      P2_Next_Dir = R;
    }
    if (U_score == best) {
      P2_Next_Dir = U;
    }
    if (D_score == best) {
      P2_Next_Dir = D;
    }
  }

  int AI_score(Segment pos, Direction dir) {
    Segment lookpos = pos+dir;
    if (lookpos.x < 0 || lookpos.y < 0 || lookpos.x >= COLS || lookpos.y >= ROWS) {
      return 0;
    } else if (within_P1(lookpos) || within_P2(lookpos)) {
      return 10;
    } else {
      return 111-(abs(apple.x-lookpos.x)+abs(apple.y-lookpos.y));
    }
  }

//display stuff
  void display() {
    uint32_t bgColor;
    if (P1_wins && !P2_wins) {
      bgColor = rgb(colors[2]).toColor();
    } else if (!P1_wins && P2_wins) {
      bgColor = rgb(colors[5]).toColor();
    } else {
      bgColor = rgb(colors[7]).toColor();
    }
    for (int i = 0; i < LEDS; i++) {
      if (dead) {
        if (P1_wins && !P2_wins) {
          strip.setPixelColor(i, rgb(colors[2]).toColor());
        } else if (!P1_wins && P2_wins) {
          strip.setPixelColor(i, bgColor);
        } else {
          strip.setPixelColor(i, rgb(colors[7]).toColor());
        }
      } else {
        strip.setPixelColor(i, strip.Color( 0,  0,  0));
      }
    }
    // strip.clear();

    for (int i = 0; i < P1.Count(); i++) {
      int index = pos_to_index(P1[i].x, P1[i].y);
      if (index != -1) {
        if (((round(pow(2.5, ((millis()-lastUpdate)/deathFlicker))))%2-1.0 >= 0 && (millis()-lastUpdate < deathTime)) || P1_wins || !P2_wins || !dead) {
          if (i == P1.Count()-1) {
            strip.setPixelColor(index, rgb(colors[1]).toColor());
          } else {
            strip.setPixelColor(index, rgb(colors[0]).toColor());
          }
        }
      }
    }

    for (int i = 0; i < P2.Count(); i++) {
      int index = pos_to_index(P2[i].x, P2[i].y);
      if (index != -1) {
        if (((round(pow(2.5, ((millis()-lastUpdate)/deathFlicker))))%2-1.0 >= 0 && (millis()-lastUpdate < deathTime)) || !P1_wins || P2_wins || !dead) {
          if (i == P2.Count()-1) {
            strip.setPixelColor(index, rgb(colors[4]).toColor());
          } else {
            strip.setPixelColor(index, rgb(colors[3]).toColor());
          }
        }
      }
    }

    int index = pos_to_index(apple.x, apple.y);
    if (index != -1) {
      strip.setPixelColor(index, rgb(colors[6]).toColor());
    }

    strip.show();
  }

  int pos_to_index(int x, int y) { // convert coordinates to strip indices
    // if ( x < 0
    //   || y < 0
    //   || x >= COLS
    //   || y >= ROWS
    // ) {
    //   return -1;
    // }
    // int y_total = COLS*y;
    // int x_total = (COLS-1)*(y%2) + x*( -2*(y%2)+1 );
    // //COLS*y+COLS*(y%2)-(y%2)+x-2x*(y%2)
    if (x < 0 || x >= COLS || y < 0 || y >= ROWS) {
      return -1;
    }

    // return x_total + y_total;
    return INDEX_TRANSFORMS[y][x];
  }

  void displayScreen(int num1, int num2) {
    current_nums[0] = 10;
    current_nums[1] = 10;
    current_nums[2] = 10;
    current_nums[3] = 10;
    if (num1 < 10) {
      current_nums[0] = num1;
    } else {
      current_nums[0] = num1/10;
      current_nums[1] = num1%10;
    }
    if (num2 < 10) {
      current_nums[3] = num2;
    } else {
      current_nums[2] = num2/10;
      current_nums[3] = num2%10;
    }

    // displayNums(nums);

  }

  void displayNums() {
    digitalWrite(DIGIT_PINS[lastUpdatedDigit], HIGH);
    lastUpdatedDigit = (lastUpdatedDigit+1)%4;
    digitalWrite(DIGIT_PINS[lastUpdatedDigit], LOW);
    displayNum(current_nums[lastUpdatedDigit]);
  }

  void displayNum(int num) {
    for (int i = 0; i < 7; i++) {
      digitalWrite(SEGMENT_PINS[i], SEGMENT_FILTER[num][i]);
    }
  }

RGB rgb(HSV startColor) {
  RGB endColor = RGB();
  endColor.r = cos(startColor.h);
  endColor.g = cos(startColor.h)*cos(2/3.0*PI) + sin(startColor.h)*sin(2/3.0*PI);
  endColor.b = cos(startColor.h)*cos(4/3.0*PI) + sin(startColor.h)*sin(4/3.0*PI);

  endColor.normalize();

  endColor.r = 255.0*(1-startColor.s) + endColor.r*startColor.s;
  endColor.g = 255.0*(1-startColor.s) + endColor.g*startColor.s;
  endColor.b = 255.0*(1-startColor.s) + endColor.b*startColor.s;

  endColor.r *= startColor.v;
  endColor.g *= startColor.v;
  endColor.b *= startColor.v;
  
  return endColor;
}
