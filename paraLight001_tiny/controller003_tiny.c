/************************************************************************************/
// Schalterfunktionalität an einem RC-Empfänger Ausgang
//
// Version: 0.2
//
// Autor: Ruemmler, Elias
//        RC-Art Solutions (Eisenach/Germany)
//
// Copyright: 2011-2014 RC-Art Solutions (Eisenach/Germany)
//	      http://www.rc-art.de/
//
// Datum: 29.12.2014
//
// Hardware an ATtiny13A (Taktfrequenz (4,8 Mhz; CKDIV8 Vorteiler Fuse NICHT gesetzt):
// RC-Empfänger an PB1 (INT0)
// Error LED an	PB2
// ParaLight En-Pin an PB3
// ParaLight Spare1 an PB4
/************************************************************************************/
// Compiler- Warnungen

#ifndef F_CPU
#warning "F_CPU not defined"
#define F_CPU 4800000UL //4,8MHz
#endif

/************************************************************************************/
// Libraries

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>		// Watchdog Funktionalität
#include <stdint.h>

/************************************************************************************/
// Pin-Belegung

#define ReceiverInput		PINB	// Eingangssignal RC-Empfänger
#define ReceiverPin		PINB1	// Pin für Eingangssignal RC-Empfänger
#define ReceiverPort		PORTB	// Port für Eingangssignal RC-Empfänger

#define ParaLightPort		PORTB	// Port für Ausgänge
#define ParaLightPortDDR	DDRB	// Datenrichtungsregister für Ausgänge
#define ErrorLed		PB2	// Ausgang für Error LED
#define ParaLightEn		PB3	// Ausgang für ParaLight En-Pin
#define ParaLightSpare1		PB4	// Ausgang für ParaLight Spare1

/************************************************************************************/
// Variablen

static volatile uint8_t Reading;	// Bit- Merker zur Sperrung der Hauptroutine während der erneuten Wertermittlung
// Merker Flanke

static volatile uint8_t Error;		// Bit- Merker für Fehler
static volatile uint8_t RCvalue;	// empfangener Wert von RC-Empfänger -> wird von Timer runtergezählt
static volatile uint8_t PulseCount;	// Zählt die empfangenen Pulse, um ein Blink-Signal zu erzeugen
static volatile uint8_t PulseToggle;	// Löst alle 5440ms aus (5440ms Zeitdauer für S.O.S. Sequenz)

/************************************************************************************/
//Header

void RC_Read();
void RC_Error();

/************************************************************************************/
// Interruptroutinen

/* ISR für INT0 - RC-Signal mit Timer lesen */
ISR(INT0_vect)
{
	RC_Read();
}

/* Fehlerbehandlung bei Timerüberlauf -> Fehler generieren */
ISR(TIM0_OVF_vect)
{
	RC_Error();
}

/************************************************************************************/
// Hauptprogramm

int main(void)
{
	// Vorbereitung des RC-Eingangs
	// RC-Eingang ist schon nach Initialisierung des AVR ein Eingang
	ReceiverPort |= (1<<ReceiverPin); // interne Pull-Up-Widerstände aktivieren

	// Initialisierung Ausgänge
	ParaLightPortDDR |= (1<<ErrorLed)|(1<<ParaLightEn)|(1<<ParaLightSpare1); // Datenrichtung
	ParaLightPort |= (1<<ErrorLed)|(1<<ParaLightEn)|(1<<ParaLightSpare1);	 // alle Portausgänge sind jetzt high
	// -> ParaLight wäre AN (En-Pin high)

	// Initialisierung Interrupteingang INT0
	MCUCR |= (1<<ISC00);	// Interrupt wird bei jedem Pegelwechsel an INT0 ausgelöst
	GIMSK |= (1<<INT0);	// Interrupt INT0 aktivieren

	// Initialisierung Timer0
	TIMSK0 |= (1<<TOIE0);	// Timer0 Interrupt aktivieren
	
	// Initialisierung Watchdog auf 500ms
	wdt_enable(WDTO_500MS);

	// Vorbereitung Status-Flags - kein Fehler liegt an, momentan kein Datenempfang
	Reading = 0;
	Error = 0;
	
	// globale Interrupptfreigabe
	sei();

	/***********************************************************************************/
	while(1)
	{
		// Watchdog zurücksetzen
		wdt_reset();

		// Error-LED an, wenn Timer-Überlauf war
		if(Error == 1)
		{
			ParaLightPort &= ~(1<<ErrorLed);	// Portausgang Error-LED low -> LED an
		}
		else
		{
			ParaLightPort |= (1<<ErrorLed);	// Portausgang Error-LED high -> LED aus
		}
		
		// ParaLight-Ansteuerung während der langen Lesepause...
		if(Reading == 0)
		{			
			// OP.MOD1 - AUS
			if(RCvalue > 75 && RCvalue < 90)
			{
				ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
			}
			
			// OP.MOD2 - AN
			if(RCvalue > 90 && RCvalue < 105)
			{
				ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
			}

			// OP.MOD3 - BLINK (~1,5 Hz)
			if(RCvalue > 105 && RCvalue < 120)
			{
				if ((PulseCount % 34) < 17) // Modulo-Operation um die 272 Pulse in 8 Bereiche (á 680ms) einzuteilen
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				else
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
			}
			
			// OP.MOD4 - FLASH
			if(RCvalue > 120 && RCvalue < 135)
			{
				//TODO: Flashlight hier erzeugen
				ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
			}

			// OP.MOD5 - S.O.S.
			if(RCvalue > 135 && RCvalue < 150)
			{
				//TODO: S.O.S. Sequenz hier erzeugen
				ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
			}

			//DEBUG: Testsequenz zur Zeitprüfung
			//if(RCvalue > 112 && PulseToggle)
			//{
				//LED_Port &= ~(1<<SchaltLED1);	// En-Pin low -> ParaLight aus
			//}
			//else
			//{
				//LED_Port |= (1<<SchaltLED1);	// En-Pin high -> ParaLight an
			//}
		}
	}
}

/***********************************************************************************/

// ISR für INT0 - RC-Signal mit Timer lesen
void RC_Read()
{
	// Timer starten mit steigender Flanke
	if(Reading == 0)
	{
		TCCR0B |= (1<<CS00)|(1<<CS01);	 	// Start Timer0 mit Vorteiler 64 -> 75kHz
		Reading = 1;				// Flankenmerker setzen
		if (PulseCount < 272)			// 5440 Millisekunden (erforderliche Zeit für S.O.S. Sequenz)
		{
			PulseCount++;
		}
		else // Rücksetzen
		{
			PulseCount = 0;
			PulseToggle = !PulseToggle;
		}
	}
	// Timer stoppen mit fallender Flanke
	else
	{
		TCCR0B = 0x00;		// Timer0 anhalten
		RCvalue = TCNT0;	// Wert von Timer lesen
		TCNT0 = 0x00;		// Timer rücksetzen
		Reading = 0;		// Flankenmerker rücksetzen
	}

	Error = 0;			// Errormerker rücksetzen
}


// ISR für Timer0-Fehlerhandling Timeroverflow
void RC_Error()
{
	TCCR0B = 0x00;			// Stop Timer0
	RCvalue = 0;			// Wert für Ausgänge annehmen
	TCNT0 = 0x00;			// neuen Startwert für Timer zurücksetzen
	Reading = 0;			// Merker Flanke setzten
	Error = 1;			// Errormerker setzen
}

