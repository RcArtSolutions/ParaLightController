/************************************************************************************/
// Schalterfunktionalit�t an einem RC-Empf�nger Ausgang
//
// Version: 0.2
//
// Autor: Ruemmler, Elias
//		  RC-Art Solutions (Eisenach/Germany)
//
// Copyright: 2011-2014 RC-Art Solutions (Eisenach/Germany)
//
// Datum: 29.12.2014
//
// Hardware an ATtiny13A (Taktfrequenz (4,8 Mhz; CKDIV8 Vorteiler Fuse NICHT gesetzt):
// RC-Empf�nger an PB1 (INT0)
// Error LED an	PB2
// ParaLight En-Pin an PB3
// ParaLight Spare1	an PB4
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
#include <avr/wdt.h>		// f�r Watchdog- Funktionen
#include <stdint.h>

/************************************************************************************/
// Pin- Belegung

#define ReceiverInput		PINB	// Eingangssignal RC-Empf�nger
#define ReceiverPin			PINB1	// Pin f�r Eingangssignal RC-Empf�nger
#define ReceiverPort		PORTB	// Port f�r Eingangssignal RC-Empf�nger

#define ParaLightPort		PORTB	// Port f�r Ausg�nge
#define ParaLightPortDDR	DDRB	// Datenrichtungsregister f�r Ausg�nge
#define ErrorLed			PB2		// Ausgang f�r Error LED
#define ParaLightEn			PB3		// Ausgang f�r ParaLight En-Pin
#define ParaLightSpare1		PB4		// Ausgang f�r ParaLight Spare1

/************************************************************************************/
// Variablen

static volatile uint8_t Reading;	// Bit- Merker zur Sperrung der Hauptroutine w�hrend der erneuten Wertermittlung
// Merker Flanke

static volatile uint8_t Error;		// Bit- Merker f�r Fehler
static volatile uint8_t RCvalue;	// empfangener Wert von R/C- Empf�nger -> wird von Timer runtergez�hlt
static volatile uint8_t PulseCount;	// Z�hlt die empfangenen Pulse, um ein Blink-Signal zu erzeugen
static volatile uint8_t PulseToggle;// L�st alle 5440ms aus (5440ms Zeitdauer f�r S.O.S. Sequenz)

/************************************************************************************/
//Header

void RC_Read();
void RC_Error();

/************************************************************************************/
// Interruptroutinen

/* ISR f�r INT0 - RC-Signal mit Timer lesen */
ISR(INT0_vect)
{
	RC_Read();
}

/* Fehlerbehandlung bei Timer�berlauf -> Fehler generieren */
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
	ReceiverPort |= (1<<ReceiverPin); // interne Pull-Up-Widerst�nde aktivieren

	// Initialisierung Ausg�nge
	ParaLightPortDDR |= (1<<ErrorLed)|(1<<ParaLightEn)|(1<<ParaLightSpare1); // Datenrichtung
	ParaLightPort |= (1<<ErrorLed)|(1<<ParaLightEn)|(1<<ParaLightSpare1);	 // alle Portausg�nge sind jetzt high
	// -> ParaLight w�re AN (En-Pin high)

	// Initialisierung Interrupteingang INT0
	MCUCR |= (1<<ISC00);	// Interrupt wird bei jedem Pegelwechsel an INT0 ausgel�st
	GIMSK |= (1<<INT0);		// Interrupt INT0 aktivieren

	// Initialisierung Timer0
	TIMSK0 |= (1<<TOIE0);	// Timer0 Interrupt aktivieren
	
	// Initialisierung Watchdog auf 500ms
	wdt_enable(WDTO_500MS);

	// Vorbereitung Status- Flags - kein Fehler liegt an, momentan kein Datenenpfang
	Reading = 0;
	Error = 0;
	
	// globale Interrupptfreigabe
	sei();

	/***********************************************************************************/
	while(1)
	{
		// Watchdog zur�cksetzen
		wdt_reset();

		// Error-LED an, wenn Timer-�berlauf war
		if(Error == 1)
		{
			ParaLightPort &= ~(1<<ErrorLed);	// Portausgang Error-LED low -> LED an
		}
		else
		{
			ParaLightPort |= (1<<ErrorLed);	// Portausgang Error-LED high -> LED aus
		}
		
		// ParaLight-Ansteuerung w�hrend der langen Lesepause...
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
				if ((PulseCount % 34) < 17) // Modulo-Operation um die 272 Pulse in 8 Bereiche (� 680ms) einzuteilen
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
				//TODO: FlashLight hier erzeugen
				ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
			}

			// OP.MOD5 - S.O.S.
			if(RCvalue > 135 && RCvalue < 150)
			{
				//TODO: S.O.S. Sequenz hier erzeugen
				ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
			}

			//DEBUG: Testsequenz zur Zeitpr�fung
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

// ISR f�r INT0 - RC-Signal mit Timer lesen
void RC_Read()
{
	// Timer starten mit steigender Flanke
	if(Reading == 0)
	{
		TCCR0B |= (1<<CS00)|(1<<CS01);	// Start Timer0 mit Vorteiler 64 -> 75kHz
		Reading = 1;					// Merker Flanke setzten
		if (PulseCount < 272)			// 5440 Millisekunden
		{
			PulseCount++;
		}
		else // R�cksetzen
		{
			PulseCount = 0;
			PulseToggle = !PulseToggle;
		}
	}
	// Timer stoppen mit fallender Flanke
	else
	{
		TCCR0B = 0x00;		// Stop Timer0
		RCvalue = TCNT0;	// Wert von Timer lesen
		TCNT0 = 0x00;		// neuen Startwert f�r Timer laden
		Reading = 0;		// Merker Flanke setzten
	}

	Error = 0;				// Error-Merker zur�cksetzen
}


// ISR f�r Timer0-Fehlerhandling Timeroverflow
void RC_Error()
{
	TCCR0B = 0x00;			// Stop Timer0
	RCvalue = 0;			// Wert f�r Ausg�nge annehmen
	TCNT0 = 0x00;			// neuen Startwert f�r Timer zur�cksetzen
	Reading = 0;			// Merker Flanke setzten
	Error = 1;				// Error-Merker setzen
}

