#include <8051.h>

#define ADDR_0 P1_0
#define ADDR_1 P1_1
#define ADDR_2 P1_2
#define ADDR_3 P1_3
#define EN_LED P1_4

#define KEY_IN_4 P2_7
#define KEY_IN_3 P2_6
#define KEY_IN_2 P2_5
#define KEY_IN_1 P2_4

#define KEY_OUT_1 P2_3
#define KEY_OUT_2 P2_2
#define KEY_OUT_3 P2_1
#define KEY_OUT_4 P2_0

#define ADD 'A'
#define MINUS 'B'
#define MULTIPLE 'C'
#define DIVIDE 'D'
#define EQUAL 'E'
#define CLEAR 'F'

unsigned long DIGIT = 0;

// unsigned char T0_INTERRUPT_FLAG = 0;

unsigned char LED_CHAR[] = {
    0xff ^ 0b111111,  0xff ^ 0b110,     0xff ^ 0b1011011, 0xff ^ 0b1001111,
    0xff ^ 0b1100110, 0xff ^ 0b1101101, 0xff ^ 0b1111101, 0xff ^ 0b111,
    0xff ^ 0b1111111, 0xff ^ 0b1101111,
};

unsigned char LED_BUFF[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

unsigned char KEY_STATUS[4][4] = {
    {1, 1, 1, 1},
    {1, 1, 1, 1},
    {1, 1, 1, 1},
    {1, 1, 1, 1},
};

unsigned char PREV_KEY_STATUS[4][4] = {
    {1, 1, 1, 1},
    {1, 1, 1, 1},
    {1, 1, 1, 1},
    {1, 1, 1, 1},
};

unsigned short KEY_DOWN_TIMES[4][4] = {
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
    {0, 0, 0, 0},
};

const unsigned short LONG_KEY_MS = 1000;
unsigned short KEY_HIGH_TIMES[4][4] = {
    {1000, 1000, 1000, 1000},
    {1000, 1000, 1000, 1000},
    {1000, 1000, 1000, 1000},
    {1000, 1000, 1000, 1000},
};

void enable_u3_74hc138();

void interrupt_time0() __interrupt(1);
void reload_time0();
void scan_keyboard();
void flush_led_buffer();
void turn_off_all_segs();
void enable_tube(unsigned char i);

unsigned char map_key_digit(unsigned char key_row, unsigned char key_col);
void key_driver(unsigned char input_key);
void long_key_driver(unsigned char row, unsigned char col,
                     unsigned char input_key);

void update_led_buffer(unsigned long digit);

int main() {
  enable_u3_74hc138();

  // config_t0(1, 1);
  EA = 1;   // enable global interrupt
  ET0 = 1;  // enable Timer0 interrupt

  // setup T0_M1 = 0, T0_M0 = 1 (Timer0 mode TH0-TL0 16 bits timer)
  TMOD = 0x01;
  // setup TH0 TL0 initial value
  TH0 = 0xFC;
  TL0 = 0x67;
  TR0 = 1;  // start Timer0

  LED_BUFF[0] = LED_CHAR[0];
  unsigned char i = 0, j = 0, input_key = 0;

  while (1) {
    for (i = 0; i < 4; ++i) {
      for (j = 0; j < 4; ++j) {
        if (PREV_KEY_STATUS[i][j] != KEY_STATUS[i][j]) {
          // 这里结合 i, j 就可以知道是按下了哪个按键
          if (KEY_STATUS[i][j] == 0) {
            input_key = map_key_digit(i, j);
            key_driver(input_key);
          } else {
            // 重置按键长按时间阈值
            KEY_HIGH_TIMES[i][j] = LONG_KEY_MS;
          }
          PREV_KEY_STATUS[i][j] = KEY_STATUS[i][j];
        }
        long_key_driver(i, j, input_key);
      }
    }
  }
}

// T0 中断函数，用来检测按键状态
void interrupt_time0() __interrupt(1) {
  reload_time0();
  // 1.在中断里面检测输入，但是在 main() 里面响应输入的事件
  scan_keyboard();
  // 2.在响应事件里面更新 LED_BUFF，但是刷新确是在每个中断事件里面
  flush_led_buffer();
}

void reload_time0() {
  // setup TH0 TL0 initial value, each interrupt(Timer0 overflow) will pass 1ms
  TH0 = 0xFC;
  TL0 = 0x67;
}

void scan_keyboard() {
  static unsigned char KEY_BUFFER[4][4] = {
      {0xFF, 0xFF, 0xFF, 0xFF},
      {0xFF, 0xFF, 0xFF, 0xFF},
      {0xFF, 0xFF, 0xFF, 0xFF},
      {0xFF, 0xFF, 0xFF, 0xFF},
  };
  static unsigned char keyout = 0;

  KEY_BUFFER[keyout][0] = (KEY_BUFFER[keyout][0] << 1) | KEY_IN_1;
  KEY_BUFFER[keyout][1] = (KEY_BUFFER[keyout][1] << 1) | KEY_IN_2;
  KEY_BUFFER[keyout][2] = (KEY_BUFFER[keyout][2] << 1) | KEY_IN_3;
  KEY_BUFFER[keyout][3] = (KEY_BUFFER[keyout][3] << 1) | KEY_IN_4;

  for (unsigned char i = 0; i < 4; ++i) {
    if (KEY_BUFFER[keyout][i] == 0x00) {
      KEY_STATUS[keyout][i] = 0;
      // 按键按下的时候不断累积按键按下的时间，达到长按时间间隔时触发长按事件
      KEY_DOWN_TIMES[keyout][i] += 4;  // 从代码执行周期估计完成一次扫描需要 4ms
    } else if (KEY_BUFFER[keyout][i] == 0xFF) {
      KEY_STATUS[keyout][i] = 1;
      KEY_DOWN_TIMES[keyout][i] = 0;
    }
  }

  keyout++;
  keyout = keyout & 0x03;

  switch (keyout) {
    case 0:
      KEY_OUT_1 = 0;  // enable current keyout
      KEY_OUT_4 = 1;  // disable previous keyout
      break;
    case 1:
      KEY_OUT_2 = 0;
      KEY_OUT_1 = 1;
      break;
    case 2:
      KEY_OUT_3 = 0;
      KEY_OUT_2 = 1;
      break;
    case 3:
      KEY_OUT_4 = 0;
      KEY_OUT_3 = 1;
      break;
    default:
      break;
  }
}

void flush_led_buffer() {
  turn_off_all_segs();
  static unsigned char TUBE_IDX = 0;
  switch (TUBE_IDX) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
      enable_tube(TUBE_IDX);
      P0 = LED_BUFF[TUBE_IDX];
      TUBE_IDX++;
      break;
    case 5:
      enable_tube(TUBE_IDX);
      P0 = LED_BUFF[TUBE_IDX];
      TUBE_IDX = 0;
      break;
    default:
      break;
  }
}

void turn_off_all_segs() { P0 = 0xff; }

// i: 0 - (TUBE_SIZE-1)
void enable_tube(unsigned char i) {
  // P1_2 P1_1 P1_0
  // TUBE 0 000
  // TUBE 1 001
  // TUBE 2 010
  // TUBE 3 011
  // TUBE 4 100
  // TUBE 5 101
  P1 &= 1 << 3;
  P1 |= i;
}

/**
 * map 4 * 4 input keys to [0-9A-F]
 * keyboard layout:
 *  # # #     #
 *  # # #   #   #
 *  # # #     #
 *  #       #   #
 * @param key_row
 * @param key_col
 * @return
 */
unsigned char map_key_digit(unsigned char key_row, unsigned char key_col) {
  static unsigned char key_code_map[4][4] = {
      {1, 2, 3, ADD},
      {4, 5, 6, MULTIPLE},
      {7, 8, 9, MINUS},
      {0, CLEAR, EQUAL, DIVIDE},
  };
  return key_code_map[key_row][key_col];
}

void key_driver(unsigned char input_key) {
  switch (input_key) {
    case ADD:  // 增加
      DIGIT++;
      update_led_buffer(DIGIT);
      break;
    case MINUS:  // 减少
      DIGIT--;
      update_led_buffer(DIGIT);
      break;
    default:
      break;
  }
}

void long_key_driver(unsigned char row, unsigned char col,
                     unsigned char input_key) {
  // 按键被按下
  if (KEY_DOWN_TIMES[row][col] > 0) {
    // 按键被按下到达长按时间阈值，长按事件触发
    if (KEY_DOWN_TIMES[row][col] > KEY_HIGH_TIMES[row][col]) {
      // 长按事件触发短按同样的事件
      key_driver(input_key);
      // 长按时间阈值每执行一次增加 100，
      // 即进入长按事件后，每间隔 100 个周期再触发一次
      KEY_HIGH_TIMES[row][col] += 100;
    }
  }
}

void update_led_buffer(unsigned long digit) {
  signed char i = 0;
  unsigned char buf[6];
  for (i = 0; i < 6; ++i) {
    buf[i] = digit % 10;
    digit /= 10;
  }

  for (i = 5; i >= 1; i--) {
    if (buf[i] == 0) {
      LED_BUFF[i] = 0xFF;
    } else {
      break;
    }
  }
  for (; i >= 0; i--) {
    LED_BUFF[i] = LED_CHAR[buf[i]];
  }
}

void enable_u3_74hc138() {
  // 74HC138 芯片的使能引脚，G1 高电平 G2 低电平 才能启动74HC138的 3-8 译码电路
  ADDR_3 = 1;  // G1 高电平
  EN_LED = 0;  // G2低电平（G2A, G2B）
}