/************************************************************************************/
// Remoteswitch (Controller) für das ParaLight-Modul an einem RC-Empfänger Ausgang
//
// Version:		0.3-D3012
//
// Autor:		Ruemmler, Elias
//				RC-Art Solutions (Eisenach/Germany)
//
// Copyright:	2011-2014 RC-Art Solutions (Eisenach/Germany)
//				http://www.rc-art.de/
//
// Datum:		29.12.2014
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
#define F_CPU 4800000UL		//4,8MHz
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
#define ReceiverPin			PINB1	// Pin für Eingangssignal RC-Empfänger
#define ReceiverPort		PORTB	// Port für Eingangssignal RC-Empfänger

#define ParaLightPort		PORTB	// Port für Ausgänge
#define ParaLightPortDDR	DDRB	// Datenrichtungsregister für Ausgänge
#define ErrorLed			PB2		// Ausgang für Error LED
#define ParaLightEn			PB3		// Ausgang für ParaLight En-Pin
#define ParaLightSpare1		PB4		// Ausgang für ParaLight Spare1

/************************************************************************************/
// Variablen

static volatile uint8_t RCvalue;		// empfangener Wert von RC-Empfänger -> wird von Timer runtergezählt
static volatile uint8_t PulseCount;		// Zählt die empfangenen Pulse, um ein Blink-Signal zu erzeugen
static volatile uint8_t FlashCount;		// Zählt anhand der empfangenen Pulse alle 160 ms hoch, bis 34
static volatile uint8_t OperationMode;	// OP.MOD 1: AUS, 2: AN, 3: Blink, 4: Flash, 5: S.O.S.

// Merker Flanke

static volatile uint8_t Error;		// Merkerbit für Fehler
static volatile uint8_t Reading;	// Merkerbit zur Sperrung der Hauptroutine während der erneuten Wertermittlung

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
	ReceiverPort |= (1<<ReceiverPin);	// interne Pull-Up-Widerstände aktivieren

	// Initialisierung Ausgänge
	ParaLightPortDDR |= (1<<ErrorLed)|(1<<ParaLightEn)|(1<<ParaLightSpare1); // Datenrichtung
	ParaLightPort |= (1<<ErrorLed)|(1<<ParaLightEn)|(1<<ParaLightSpare1);	 // alle Portausgänge sind jetzt high
	// -> ParaLight wäre AN (En-Pin high)

	// Initialisierung Interrupteingang INT0
	MCUCR |= (1<<ISC00);				// Interrupt wird bei jedem Pegelwechsel an INT0 ausgelöst
	GIMSK |= (1<<INT0);					// Interrupt INT0 aktivieren

	// Initialisierung Timer0
	TIMSK0 |= (1<<TOIE0);				// Timer0 Interrupt aktivieren
	
	// Initialisierung Watchdog auf 500ms
	wdt_enable(WDTO_500MS);

	// Vorbereitung Status-Flags - kein Fehler liegt an, momentan kein Datenempfang
	Reading = 0;
	Error = 0;
	RCvalue = 82;						// Aktiviert OP.MOD1 - AUS
	
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
			ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
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
				OperationMode = 1;
			}
			
			// OP.MOD2 - AN
			if(RCvalue > 90 && RCvalue < 105)
			{
				ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				OperationMode = 2;
			}

			// OP.MOD3 - BLINK (~2,8 Hz)
			if(RCvalue > 105 && RCvalue < 120)
			{
				if ((PulseCount % 16) < 8) // Modulo-Operation um die 272 Pulse in 15 Bereiche einzuteilen
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				else
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				
				OperationMode = 3;
			}
			
			// OP.MOD4 - FLASH (Doppelblitz)
			if(RCvalue > 120 && RCvalue < 135)
			{
				if (OperationMode != 4)
				{
					FlashCount = 0;
				}
				
				if (FlashCount >= 0 && FlashCount < 4)
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				if (FlashCount >= 4 && FlashCount < 7)
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				if (FlashCount >= 7 && FlashCount < 11)
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				if (FlashCount >= 11 && FlashCount <= 34)
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				
				OperationMode = 4;
			}

			// OP.MOD5 - S.O.S. (--- === ---     )
			if(RCvalue > 135 && RCvalue < 150)
			{
				if (OperationMode != 5)
				{
					PulseCount = 0;
				}
				
				// 272 Ticks müssen in 34 Bereiche (á 1T [Dit] = 160 ms -> 8 WpM) eingeteilt werden
				// 272 / 34 = 8 -> 1T [Dit] 8 Ticks
				
				if (PulseCount >= 0 && PulseCount < 8)
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				if (PulseCount >= 8 && PulseCount < 16)
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				if (PulseCount >= 16 && PulseCount < 24)
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				if (PulseCount >= 24 && PulseCount < 32)
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				if (PulseCount >= 32 && PulseCount < 40)
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				if (PulseCount >= 40 && PulseCount < 64)
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				if (PulseCount >= 64 && PulseCount < 88)
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				if (PulseCount >= 88 && PulseCount < 96)
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				if (PulseCount >= 96 && PulseCount < 120)
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				if (PulseCount >= 120 && PulseCount < 128)
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				if (PulseCount >= 128 && PulseCount < 152)
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				if (PulseCount >= 152 && PulseCount < 176)
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				if (PulseCount >= 176 && PulseCount < 184)
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				if (PulseCount >= 184 && PulseCount < 192)
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				if (PulseCount >= 192 && PulseCount < 200)
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				if (PulseCount >= 200 && PulseCount < 208)
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				if (PulseCount >= 208 && PulseCount < 216)
				{
					ParaLightPort |= (1<<ParaLightEn);	// En-Pin high -> ParaLight an
				}
				if (PulseCount >= 216 && PulseCount <= 272)
				{
					ParaLightPort &= ~(1<<ParaLightEn);	// En-Pin low -> ParaLight aus
				}
				
				OperationMode = 5;
			}
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
		Reading = 1;						// Flankenmerker setzen
		if (PulseCount < 272)				// 5440 Millisekunden (erforderliche Zeit für S.O.S. Sequenz)
		{
			PulseCount++;
		}
		else // Rücksetzen
		{
			PulseCount = 0;
		}
		if (FlashCount < 34)				// 5440 Millisekunden (erforderliche Zeit für S.O.S. Sequenz)
		{
			FlashCount++;
		}
		else // Rücksetzen
		{
			FlashCount = 0;
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

	Error = 0;				// Errormerker rücksetzen
}


// ISR für Timer0-Fehlerhandling Timeroverflow
void RC_Error()
{
	TCCR0B = 0x00;			// Stop Timer0
	RCvalue = 97;			// Aktiviert OP.MOD2 (AN)
	TCNT0 = 0x00;			// neuen Startwert für Timer setzen
	Reading = 0;			// Merker Flanke setzten
	Error = 1;				// Errormerker setzen
}