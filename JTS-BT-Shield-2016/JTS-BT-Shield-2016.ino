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

// Setzen von Bluetooth Parametern
#define BTSET   // wenn definiert, werden die Parameter an das Modul gesendet
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
char name[][6] = { "Rot", "Gruen", "Blau" };

// Werte für die Helligkeit der drei LEDs 0=aus bis 15=max
uint8_t pwm[3];

// Die Auswertung der Steuerbefehle, die per Bluetooth eintreffen, erfolgt
// durch eine einfache Statemachine
#define S_START  1
#define S_VALUE  2

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
    
// Starten des Timers 1, konfigurieren als 100µs Taktgeber
  Timer1.initialize(100);           // Setze den Timer so, dass er alle 100µs auslöst
  Timer1.disablePwm(9);             // Ausgang 9 wird nicht direkt vom Timer gesteuert
  Timer1.disablePwm(10);            // Ausgang 10 wird nicht direkt vom Timer gesteuert
  Timer1.attachInterrupt ( count ); // Rufe die Funktion count auf, wenn der Timer auslöst
}

/*
 * Die Funktion count wird durch den Timer 10.000 Mal pro Sekunde aufgerufen
 * Sie schaltet die Ausgänge, an denen die LEDs angeschlossen sind, aus oder ein
 * Sie zählt einen Zähler pwm_count von 1 bis 15 hoch, setzt ihn dann wieder auf 1
 * Durch einen Vergleich mit dem Helligkeitswert jeder Farbe wird der zugegehörige Ausgang aus-
 * oder eingeschaltet. Je länger der Ausgang eingeschaltet ist, umso heller leuchtet die LED
 */
void count ( void )
{
  static uint8_t pwm_count = 1;
  
  if (++pwm_count > 15) pwm_count = 1;
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
  static uint8_t val = 0;
  static uint8_t state = S_START;
  static uint8_t channel;
  
  if ( Serial.available() ) {   // wenn ein Zeichen vom Bluetooth empfangen wurde
    char c = Serial.read();     // lies es aus und speichere es in c
    switch ( state ) {          // arbeite weiter in Abhängigkeit vom Empfangszustand:
      case S_START:   // Warten auf den Buchstaben, der die einzustellende Farbe enthält
        if ( c == 'R' || c == 'G' || c == 'B' ) { // wurde ein Farb-Buchstabe empfangen?
          val = 0;        // der einzustellende Wert wird auf 0 gesetzt
          switch ( c ) {  // je nach Buchstabe=Farbe wird der Feldindex gesetzt
            case 'R': channel = IDX_R; break;
            case 'G': channel = IDX_G; break;
            case 'B': channel = IDX_B; break;
          }
          state = S_VALUE; // neuer Empfangszustand: die nächsten Zeichen werden als Wert ausgewertet
        }
        break;
        
      case S_VALUE: // Lesen des Zahlenwertes
        if ( c == 0x0d || c == 0x0a || c == '#' ) { // Befehlsende erkannt?
          pwm[channel] = val;         // Übertrage den Helligkeitswert für die Farbe
          Serial.print ( name[channel] );   // Debug-Ausgabe
          Serial.print ( ": " );
          Serial.println ( val );
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
  
}  

