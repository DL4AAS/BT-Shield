#include <TimerOne.h>

#define  LED_B  A3
#define  LED_R  A2
#define  LED_G  A1

#define  IDX_R  0
#define  IDX_G  1
#define  IDX_B  2

#define  POTI   A4
#define  KEYS   A0

#define KEY_NO  0
#define KEY_S1  1
#define KEY_S2  2
#define KEY_S3  3

#define KEY_S1_VAL  0
#define KEY_S2_VAL  361
#define KEY_S3_VAL  536
#define KEY_NO_VAL  1023

#define KEY_S1_MAX  ((KEY_S1_VAL+KEY_S2_VAL)>>1)
#define KEY_S2_MAX  ((KEY_S2_VAL+KEY_S3_VAL)>>1)
#define KEY_S3_MAX  ((KEY_S3_VAL+KEY_NO_VAL)>>1)

#define S_START  1
#define S_VALUE  2
#define S_READY  3

uint8_t pwm[3];


uint8_t  keypressed ()
{
  static uint8_t ispressed = 0;
  uint16_t a = analogRead(KEYS);

  if ( a > KEY_S3_MAX ) {
    ispressed = 0;
    return KEY_NO;
  }
  
  if ( !ispressed ) {
    ispressed = 1;
    delay (10);
    if ( a < KEY_S1_MAX ) return KEY_S1;
    if ( a < KEY_S2_MAX ) return KEY_S2;
    if ( a < KEY_S3_MAX ) return KEY_S3;
  }
  return KEY_NO;
}
  
void setup()
{
  digitalWrite(LED_R,LOW);
  pinMode(LED_R, OUTPUT);
  digitalWrite(LED_G,LOW);
  pinMode(LED_G, OUTPUT);
  digitalWrite(LED_B,LOW);
  pinMode(LED_B, OUTPUT);

  Serial.begin(9600);
  
  pwm[IDX_R] = 0;
  pwm[IDX_G] = 0;
  pwm[IDX_B] = 0;

  Timer1.initialize(100);
  Timer1.disablePwm(9);
  Timer1.disablePwm(10);
  Timer1.attachInterrupt ( count );
}
 
uint8_t pwm_count;

void count ( void )
{
  if (++pwm_count > 15) pwm_count = 1;
  if ( pwm[IDX_R] >= pwm_count ) digitalWrite ( LED_R, HIGH ); else digitalWrite ( LED_R, LOW ); 
  if ( pwm[IDX_G] >= pwm_count ) digitalWrite ( LED_G, HIGH ); else digitalWrite ( LED_G, LOW ); 
  if ( pwm[IDX_B] >= pwm_count ) digitalWrite ( LED_B, HIGH ); else digitalWrite ( LED_B, LOW ); 
}

void loop ()
{
  switch ( keypressed() ){
    case KEY_S1: if (pwm[IDX_R]) pwm[IDX_R] =  0; else pwm[IDX_R] =  15; break;
    case KEY_S2: if (pwm[IDX_G]) pwm[IDX_G] =  0; else pwm[IDX_G] =  15; break;
    case KEY_S3: if (pwm[IDX_B]) pwm[IDX_B] =  0; else pwm[IDX_B] =  15; break;
  }
  
  static int val = 0;
  static char state = S_START;
  static char channel;
  
  if ( Serial.available() ) {
    char c = Serial.read();
    switch ( state ) {
      case S_START:
        val = 0;
        switch ( c ) {
          case 'R': channel = IDX_R; state = S_VALUE; break;
          case 'G': channel = IDX_G; state = S_VALUE; break;
          case 'B': channel = IDX_B; state = S_VALUE; break;
        }
        break;
        
      case S_VALUE:
        if ( c == 0x0d || c == 0x0a || c == '#' ) {
          state = S_READY;
        } else {
          if ( isdigit(c) )
            val = val * 10 + (c-'0');
        }    
        break;
      default:
        state = S_READY;      
    }
  } // Serial.available
  
  if ( state == S_READY ) {
    pwm[channel] = val;
    state = S_START;
    Serial.printf ( "%d:%d\n", channel, val );
  }
}  

