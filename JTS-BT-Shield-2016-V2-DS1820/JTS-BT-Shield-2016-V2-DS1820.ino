/*
 * Software für das JTS-Bluetooth-Shield Version 2016
 * 
 * Die RGB-Led wird über ein per Bluetooth angekoppeltes Smartphone 
 * gesteuert. Dazu überträgt das Smartphone Befehle der Art
 * R0 oder G15, jeweils mit CR (Zeilenende) oder LF (Zeilenvorschub) beendet
 * R, G oder B stehen für die einzustellende Farbe
 * 0..15 ist der Helligkeitswert (0 = aus, 15 = maximal)
 */
 
#include <TimerOne.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Setzen von Bluetooth Parametern
//#define BTSET   // wenn definiert, werden die Parameter an das Modul gesendet
                // funktioniert aber nur, wenn das Modul noch nicht gekoppelt ist.
#define BTNAME "JTS-BT-Shield"
#define BTPIN  "1234"

// Festlegen der Ausgangsleitungen, an denen die 3 LEDs angeschlossen sind
#define  LED_R  A1
#define  LED_G  A2
#define  LED_B  A3

// Die Helligkeitswerte der LEDs werden in einem Feld gespeichert
// diese Konstanten legen fest, welcher Feldeintrag (0..2) zu welcher Farbe gehört
#define  IDX_R  0
#define  IDX_G  1
#define  IDX_B  2

// Namen der Farben, um sie bei Debug-Ausgaben im Klartext anzeigen zu können
//char name[][6] = { "Rot", "Gruen", "Blau" };
char name[][6] = { "R", "G", "B" };

// Werte für die Helligkeit der drei LEDs 0=aus bis 15=max
uint8_t pwm[3];

#define PWM 100

uint8_t autoon;       // wenn 1, wird die Farbe automatisch geändert
uint16_t autodelay;   // Verzoergung der Farbänderung
uint8_t autoprint;    // wenn 1, aktuelle Autoi-Werte ausgeben, um Anzeige zu aktualisieren

// Die Auswertung der Steuerbefehle, die per Bluetooth eintreffen, erfolgt
// durch eine einfache Statemachine
#define S_START  1
#define S_VALUE  2

// Anschluss eines Temperatursensors DS1820 (OneWire)
// Festlegen der Pins, wenn GND und VCC direkt angeschlossen sind, einfach auskommentieren
// Wird der Sensor in der Startphase nicht erkannt, entfällt die Funktion automatisch
#define DS1820_DQ   6

#ifdef DS1820_DQ
 #define DS1820_GND  7
 #define DS1820_VCC  5
 OneWire ds1820(DS1820_DQ);
 DallasTemperature temp(&ds1820);
 uint8_t ds1820_addr[8];
 uint8_t ds1820_data[12];
 uint8_t ds1820_found;

 float tempvalue;
 uint8_t tempnew = 0;
#endif


/*
 * Die Funktion setup wird nach dem Einschalten des Arduinos einmal ausgeführt,
 * in ihr werden grundlegende Einstellung vorgenommen, die den späteren Betrieb
 * vorbereiten
 * - Einstellen und Starten der seriellen Schnittstelle für Bluetooth
 * - Setzen der Ausgänge für die LEDs
 * - Setzen der Start-Helligkeiten der LEDs
 * - Konfigurieren des Bluetooth-Moduls (wenn BTSET definiert ist)
 * - Starten des Taktgebers
 */
void setup()
{
  Serial.begin(9600);               // das Bluetooth-Modul ist an der seriellen Schnittstelle angeschlossen
  
  digitalWrite(LED_R,LOW);          // Ausgang für die rote LED auf aus
  pinMode(LED_R, OUTPUT);           // Leitung für die rote LED als Ausgang schalten
  digitalWrite(LED_G,LOW);
  pinMode(LED_G, OUTPUT);
  digitalWrite(LED_B,LOW);
  pinMode(LED_B, OUTPUT);

  pwm[IDX_R] = 0;                   // nach dem Einschalten sind alle LEDs aus
  pwm[IDX_G] = 0;
  pwm[IDX_B] = 0;

  autoon = 1;                       // Automatischer Betrieb als Start-Einstellung
  autodelay = 300;

#ifdef BTSET
  Serial.println();
// Setzen des Names des Bluetooth-Moduls
  Serial.print ( "AT+NAME" );
  Serial.print ( BTNAME );
  delay ( 1000 );
  Serial.println();
  while ( Serial.available() )
    Serial.print ( (char)Serial.read() );
  Serial.println();
  delay ( 1000 );
  
// Setzen des Pins des Bluetooth-Moduls
  Serial.print ( "AT+PIN" );
  Serial.print ( BTPIN );
  delay ( 1000 );
  Serial.println();
  while ( Serial.available() )
    Serial.print ( (char)Serial.read() );
  Serial.println();
#endif  
    
// Vorbereiten eines DS1820-Sensors
#ifdef DS1820_DQ
 #ifdef DS1820_GND
  digitalWrite ( DS1820_GND, LOW );
  pinMode ( DS1820_GND, OUTPUT );
  Serial.println ( "GND" );
 #endif
 #ifdef DS1820_VCC
  digitalWrite ( DS1820_VCC, HIGH );
  pinMode ( DS1820_VCC, OUTPUT );
  Serial.println ( "Vcc" );
 #endif
//  digitalWrite ( DS1820_DQ, HIGH );  // Pullup
//  delay(1000);
//  ds1820.reset_search();
//  ds1820.search(ds1820_addr);
  if ( !(ds1820_found = ds1820.search(ds1820_addr))) {
    Serial.println ( "Keinen Sensor DS1820 gefunden" );
  } else {
//    ds1820.reset_search();
    temp.begin();
    temp.requestTemperatures();
    Serial.print ( temp.getTempCByIndex(0) );
    Serial.println ( " GradC" );
    temp.setWaitForConversion(FALSE);
  }  
#endif 

// Starten des Timers 1, konfigurieren als 100µs Taktgeber
  Timer1.initialize(100);           // Setze den Timer so, dass er alle 100µs auslöst
  Timer1.disablePwm(9);             // Ausgang 9 wird nicht direkt vom Timer gesteuert
  Timer1.disablePwm(10);            // Ausgang 10 wird nicht direkt vom Timer gesteuert
  Timer1.attachInterrupt ( tick );  // Rufe die Funktion tick auf, wenn der Timer auslöst

  Serial.print ( "A " );   // Debug-Ausgabe
  Serial.println ( autodelay/10 );
}

/*
 * tick Funktion - ruft die verschiedenen Interruptptogramme auf
 */
void tick ( void )
{
  if ( autoon )  // Funktion nur ausführen, wenn Automode aktiv ist
    automode();
  pwmcount();
  tempread();
}

/*
 * tempread - Timer, um die Messung im Hauptprogramm zu starten
 */
void tempread() {
  static uint16_t tempdelay = 1;
  if ( !--tempdelay ) {
    tempdelay = 10000;
    tempnew = 1;
  } else if ( tempdelay == 1000 )
    tempnew = 2;
}


/*
 * Die Funktion automode berechnet die neuen Helligkeitswerte, wenn die
 * Auto-Betriebsart aktiv ist ( Variable autoon ist 1 )
 */
void automode ( void )
{
  static uint16_t autotimer = 0;
  static uint16_t autovalue = 0;
  static uint16_t autoprinttimer = 0;
  
  if ( ++autotimer > autodelay ) {      // Zeit für nächste Veränderung der Werte erreicht?
    autotimer = 0;
    if ( ++autovalue > (3*PWM) ) autovalue = 0;    // autovalue ist die Berechnungsbasis
    if ( autovalue <= PWM ) {
      pwm[IDX_R] = PWM - autovalue;
      pwm[IDX_G] = autovalue;
    } else if ( autovalue <= (2*PWM) ) {
      pwm[IDX_R] = 0;
      pwm[IDX_G] = (2*PWM) - autovalue;
    } else {
      pwm[IDX_R] = autovalue - (2*PWM);
      pwm[IDX_G] = 0;
    }
    pwm[IDX_B] = PWM - pwm[IDX_R] - pwm[IDX_G];
  }
  if ( ++autoprinttimer > 1000 ) {
    autoprinttimer = 0;
    autoprint = 1;
  }
}

/*
 * Die Funktion pwmcount wird durch den Timer 10.000 Mal pro Sekunde aufgerufen
 * Sie schaltet die Ausgänge, an denen die LEDs angeschlossen sind, aus oder ein
 * Sie zählt einen Zähler pwm_count von 1 bis PWM hoch, setzt ihn dann wieder auf 1
 * Durch einen Vergleich mit dem Helligkeitswert jeder Farbe wird der zugegehörige Ausgang aus-
 * oder eingeschaltet. Je länger der Ausgang eingeschaltet ist, umso heller leuchtet die LED
 */
void pwmcount ( void )
{
  static uint8_t pwm_count = 1;
  
  if (++pwm_count > PWM) pwm_count = 1;
  if ( pwm[IDX_R] >= pwm_count ) digitalWrite ( LED_R, HIGH ); else digitalWrite ( LED_R, LOW ); 
  if ( pwm[IDX_G] >= pwm_count ) digitalWrite ( LED_G, HIGH ); else digitalWrite ( LED_G, LOW ); 
  if ( pwm[IDX_B] >= pwm_count ) digitalWrite ( LED_B, HIGH ); else digitalWrite ( LED_B, LOW ); 
}

/*
 * Die Funktion loop wird nach der Initialisierung setup() immer wieder aufgerufen
 * 
 * Sie wertet die Befehle, die per Bluetooth kommen, aus und setzt die Farbwerte der drei LEDs
 */
void loop ()
{
  static uint16_t val = 0;
  static uint8_t state = S_START;
  static uint8_t channel;

  if ( Serial.available() ) {   // wenn ein Zeichen vom Bluetooth empfangen wurde
    char c = Serial.read();     // lies es aus und speichere es in c
    switch ( state ) {          // arbeite weiter in Abhängigkeit vom Empfangszustand:
      case S_START:   // Warten auf den Buchstaben, der die einzustellende Farbe enthält
        if ( c == 'R' || c == 'G' || c == 'B' ) { // wurde ein Farb-Buchstabe empfangen?
          val = 0;        // der einzustellende Wert wird auf 0 gesetzt
          autoon = 0;     // Automatikbetrieb wird deaktiviert
          switch ( c ) {  // je nach Buchstabe=Farbe wird der Feldindex gesetzt
            case 'R': channel = IDX_R; break;
            case 'G': channel = IDX_G; break;
            case 'B': channel = IDX_B; break;
          }
          state = S_VALUE; // neuer Empfangszustand: die nächsten Zeichen werden als Wert ausgewertet
        } else if ( c == 'A' ) {
          val = 0;
          autoon = 1;
          state = S_VALUE; // neuer Empfangszustand: die nächsten Zeichen werden als Wert ausgewertet
        }
        break;
        
      case S_VALUE: // Lesen des Zahlenwertes
        if ( c == 0x0d || c == 0x0a || c == '#' ) { // Befehlsende erkannt?
          if ( autoon ) {
            autodelay = val * 10;
            Serial.print ( "A " );   // Debug-Ausgabe
            Serial.println ( val );
          } else {
            pwm[channel] = val;         // Übertrage den Helligkeitswert für die Farbe
            Serial.print ( name[channel] );   // Debug-Ausgabe
            Serial.print ( " " );
            Serial.println ( val );
          }
          state = S_START;            // neuer Empfangszustand: Beginne die Auswertung des nächsten Befehls
        } else {  // noch kein Befehlsende, also noch Zahl
          if ( isdigit(c) )           // wenn das Zeichen eine Ziffer ist
            val = val * 10 + (c-'0'); // erweitere die Zahl um die neue Stelle
        }
        break;
        
      default:      // Der Empfangsstatus war nicht definiert
        state = S_START;      // dann beginne mit einem neuen Befehl
    } // switch (state)
  } // Serial.available


  if ( tempnew == 1 ) {
    temp.requestTemperatures();
    tempnew = 0;
  } else if ( tempnew == 2 ) {
    Serial.print ( "T " );
    Serial.println ( (int)(temp.getTempCByIndex(0)*100) );
    tempnew = 0;
  }
  
  if ( autoprint ) {
    autoprint = 0;
    for ( uint8_t i = 0; i < 3; i++ ) {
      Serial.print ( name[i] );
      Serial.print ( " " );
      Serial.println ( pwm[i] ); 
    }
  }
} // loop  


