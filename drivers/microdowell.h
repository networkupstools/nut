#ifndef MICRODOWELL_H
#define MICRODOWELL_H

#ifdef ENTERPRISE_PROTOCOL

#include <ctype.h>
#include "nut_stdint.h"

#define STX_CHAR              '['
#define ERR_COM_NO_CHARS      -999	// nessun carattere dalla porta seriale
#define ERR_MSG_TOO_SHORT     -998	// messaggio troppo breve: non arriva sino al Checksum
#define ERR_MSG_CHECKSUM      -997	// checksum non valido
#define ERR_COM_TIMEOUT       -996	// timeout in lettura
#define ERR_CLR_RX_BUFF       -900	// bisogna cancellare il buffer (non è un errore!)
#define COMMAND_NOT_VALID     -50	// comando non valido
#define PARAMETER_NOT_VALID   -51	// parametro non valido
#define ERR_UPS_NOT_FOUND     -10	// non trovo un UPS
#define ERR_COM_PORT_OPEN     -11	// impossibile aprire la porta di comunicazione
#define ERR_COM_SET_PORT      -12	// impossibile configurare la porta di comunicazione
#define ERR_COM_UNDEF_PORT    -20	// porta non valida o non esistente
#define ERR_USB_DRIVER        -13	// impossibile aprire il driver USB
#define ERR_USB_COMM_KO       -14	// interfaccia USB OK: gruppo spento o guasto
#define ERR_UPS_UNKNOWN       -30	// impossibile identificare l'UPS
#define ERR_PRG_THREAD        -40	// impossibile creare uno thread
#define ERR_PRG_INVALID_DATA  -60	// dati non validi

#define ERR_NO_ERROR          0x00	// no errors
#define ERR_I2C_BUSY          0x01	// I2C bus busy (e2prom)
#define ERR_CMD_CHECKSUM      0x10	// Checksum not valid
#define ERR_CMD_UNRECOG       0x11	// unrecognized command
#define ERR_EEP_NOBLOCK       0x08	// WRITE: eeprom address not multiple of 8
#define ERR_EEP_OOBOUND       0x09	// READ: eeprom address out of bound (with size)
#define ERR_EEP_WADDR1        0x28	// error writing e2prom address
#define ERR_EEP_WSADDR1       0x29	// error writing e2prom subaddress
#define ERR_EEP_RDATA         0x2A	// error reading e2prom data
#define ERR_EEP_WADDR2        0x50	// error writing e2prom address
#define ERR_EEP_WSADDR2       0x51	// error reading e2prom subaddress
#define ERR_EEP_WDATA         0x52	// error writing e2prom data
#define ERR_EEP_WADDRVER      0x53	// error writing e2prom address during data verification
#define ERR_EEP_WDATAVER      0x54	// error verification e2prom data
#define ERR_EEP_VERIFY        0x55	// e2prom data are different from those in the write buffer
#define ERR_EEP_CHECKSUM      0x90	// e2prom checksum error



#define  CMD_ACK              0x06	// ACK
#define  CMD_NACK             0x15	// NACK

#define  CMD_GET_STATUS       0x00	// comando di acquisizione STATUS
#define  CMD_GET_MEASURES     0x01	// comando di acquisizione MISURE
#define  CMD_GET_CONFIG       0x02	// comando di acquisizione CONFIGURAZIONE
#define  CMD_GET_BATT_STAT    0x03	// comando di acquisizione Battery+Load Status
#define  CMD_GET_BAT_LD       0x03	// comando di acquisizione Battery+Load Status
#define  CMD_GET_MASK         0x07	// comando di acquisizione MASCHERA PUNTI
#define  CMD_SET_TIMER        0x20	// comando di CONFIGURAZIONE TIMER
#define  CMD_BATT_TEST        0x21	// comando di CONFIGURAZIONE TEST BATTERIA
#define  CMD_GET_BATT_TEST    0x22	// comando di       LETTURA  TEST BATTERIA
#define  CMD_SD_ONESHOT       0x40	// comando di SCRITTURA SHUTDOWN ONESHOT
#define  CMD_GET_SD_ONESHOT   0x41	// comando di  LETTURA  SHUTDOWN ONESHOT
#define  CMD_SET_SCHEDULE     0x42	// comando di SCRITTURA SCHEDULE
#define  CMD_GET_SCHEDULE     0x43	// comando di  LETTURA  SCHEDULE
#define  CMD_GET_EEP_BLOCK    0x50	// comando di  LETTURA  BLOCCO EEPROM
#define  CMD_SET_EEP_BLOCK    0x51	// comando di SCRITTURA BLOCCO EEPROM
#define  CMD_GET_EEP_SEED     0x52	// comando di acquisizione SEME PROGR. EEPROM
#define  CMD_INIT             0xF0	// comando di REINIZIALIZZAZIONE UPS


#define  LEN_ACK              1	// ACK
#define  LEN_NACK             2	// NACK

#define  LEN_GET_STATUS       1	// comando di acquisizione STATUS
#define  LEN_GET_MEASURES     1	// comando di acquisizione MISURE
#define  LEN_GET_CONFIG       1	// comando di acquisizione CONFIGURAZIONE
#define  LEN_GET_BATT_STAT    1	// comando di acquisizione Battery+Load Status
#define  LEN_GET_BAT_LD       1	// comando di acquisizione Battery+Load Status
#define  LEN_GET_MASK         1	// comando di acquisizione MASCHERA PUNTI
#define  LEN_SET_TIMER        5	// comando di CONFIGURAZIONE TIMER
#define  LEN_BATT_TEST        4	// comando di CONFIGURAZIONE TEST BATTERIA
#define  LEN_GET_BATT_TEST    1	// comando di       LETTURA  TEST BATTERIA
#define  LEN_SD_ONESHOT       8	// comando di SCRITTURA SHUTDOWN ONESHOT
#define  LEN_GET_SD_ONESHOT   1	// comando di  LETTURA  SHUTDOWN ONESHOT
#define  LEN_SET_SCHEDULE     8	// comando di SCRITTURA SCHEDULE
#define  LEN_GET_SCHEDULE     2	// comando di  LETTURA  SCHEDULE
#define  LEN_GET_EEP_BLOCK    3	// comando di SCRITTURA BLOCCO EEPROM
#define  LEN_SET_EEP_BLOCK    12	// comando di SCRITTURA BLOCCO EEPROM
#define  LEN_GET_EEP_SEED     1	// comando di acquisizione SEME PROGR. EEPROM
#define  LEN_INIT             1	// comando di REINIZIALIZZAZIONE UPS


// non completamente definiti!
#define  RET_GET_STATUS       10	// comando di acquisizione STATUS
#define  RET_GET_MEASURES     15	// comando di acquisizione MISURE
#define  RET_GET_CONFIG       11	// comando di acquisizione CONFIGURAZIONE
#define  RET_GET_BATT_STAT    10	// comando di acquisizione Battery+Load Status
#define  RET_GET_BAT_LD       10	// comando di acquisizione Battery+Load Status
#define  RET_GET_MASK         1	// comando di acquisizione MASCHERA PUNTI
#define  RET_SET_TIMER        5	// comando di CONFIGURAZIONE TIMER
#define  RET_BATT_TEST        4	// comando di CONFIGURAZIONE TEST BATTERIA
#define  RET_GET_BATT_TEST    1	// comando di       LETTURA  TEST BATTERIA
#define  RET_SD_ONESHOT       8	// comando di SCRITTURA SHUTDOWN ONESHOT
#define  RET_GET_SD_ONESHOT   1	// comando di  LETTURA  SHUTDOWN ONESHOT
#define  RET_SET_SCHEDULE     8	// comando di SCRITTURA SCHEDULE
#define  RET_GET_SCHEDULE     8	// comando di  LETTURA  SCHEDULE
#define  RET_GET_EEP_BLOCK    11	// comando di SCRITTURA BLOCCO EEPROM
#define  RET_SET_EEP_BLOCK    12	// comando di SCRITTURA BLOCCO EEPROM
#define  RET_GET_EEP_SEED     1	// comando di acquisizione SEME PROGR. EEPROM
#define  RET_INIT             1	// comando di REINIZIALIZZAZIONE UPS


//=======================================================
// Indirizzi delle variabili memorizzate nella EEPROM: //
//=======================================================
#define  EEP_OPT_BYTE_BLK 0x00	// Option Bytes block
#define  EEP_MAGIC        0x00	// magic number: deve essere a 0xAA
#define  EEP_CHECKSUM     0x01	// XOR longitudinale da 0x?? a 0x??
#define  EEP_OUT_CONFIG   0x02	// vedi documentazione
#define  EEP_OPT_BYTE_0   0x02	// vedi documentazione
#define  EEP_OPT_BYTE_1   0x03	// vedi documentazione
#define  EEP_STATUS       0x04	// Status UPS

#define  EEP_THRESHOLD_0  0x08	// Blocco 0 con gli threshold
#define  EEP_MEAN_MIN     0x08	// UPS minimum AC voltage intervention point
#define  EEP_MEAN_MAX     0x0A	// UPS maximum AC voltage intervention point
#define  EEP_AVR_MIN      0x0C	// AVR minimum AC voltage intervention point
#define  EEP_AVR_MAX      0x0E	// AVR maximum AC voltage intervention point

#define  EEP_THRESHOLD_1  0x10	// Blocco 1 con gli threshold
#define  EEP_BLOW_VALUE   0x10	// Battery LOW threshold voltage
#define  EEP_BEND_VALUE   0x12	// Battery END threshold voltage
#define  EEP_VMETER_0     0x14	// Led 0 Voltage indicator threshold
#define  EEP_VMETER_1     0x16	// Led 1 Voltage indicator threshold

#define  EEP_THRESHOLD_2  0x18	// Blocco 2 con gli threshold
#define  EEP_VMETER_2     0x18	// Led 2 Voltage indicator threshold
#define  EEP_VMETER_3     0x1A	// Led 3 Voltage indicator threshold
#define  EEP_VMETER_4     0x1C	// Led 4 Voltage indicator threshold

#define  EEP_FAULT_POINTS 0x20	// number of fault points (in an half sine wave) after which USP enter battery mode
#define  EEP_CHARGER_MIN  0x21	// Threshold voltage for battery charger

#define  EEP_TEMP_MAX     0x28	// Maximum temperature: over this value an alarm will be generated
#define  EEP_TEMP_FAULT   0x2A	// Fault temperature: over this value, UPS will shut down immediately
#define  EEP_FANSPEED_0   0x2C	// Temperature for fan activation (low speed)
#define  EEP_FANSPEED_1   0x2E	// Temperature for fan activation (medium speed)

#define  EEP_FANSPEED_2   0x30	// Temperature for fan activation (high speed)

#define  EEP_IOUT         0x38	//
#define  EEP_IOUT_OVRLD   0x38	// Maximum current: over this value an alarm will be generated
#define  EEP_IOUT_FAULT   0x3A	// Fault current: over this value, UPS will shut down immediately
#define  EEP_PWRMTR_0     0x3C	// Led 0 Power indicator threshold
#define  EEP_PWRMTR_1     0x3E	// Led 1 Power indicator threshold

#define  EEP_PWRMTR_2     0x40	// Led 2 Power indicator threshold
#define  EEP_PWRMTR_3     0x42	// Led 3 Power indicator threshold
#define  EEP_PWRMTR_4     0x44	// Led 4 Power indicator threshold
#define  EEP_PWRMTR_5     0x46	// Between "Full power" and "Overload" Power indicator level

#define  EEP_INPUT_MASK_0 0x48	// Half sine input mask (0-7 of 10 points)
#define  EEP_INPUT_MASK_8 0x50	// Half sine input mask (8-9 of 10 points)

#define  EEP_MIN_VBATT    0x60	// Minimum battery voltaged reached from the last power-on
#define  EEP_RUNTIME_H    0x62	// Working time in minutes (max ~=32 years) (MSB first)
#define  EEP_BAT_TIME_M   0x65	// Time in Battery mode: seconds*2 (max ~=390 days). (MSB first)


#define  EEP_UPS_MODEL    0x80	// Model of the UPS in ASCII. For the Enterprise line, the structure is:
                              	//    ENTxxyyy
                              	// where:	 xx = Power rating in watts/100 (zero padded at left)
                              	//          yyy = model variants (if not defined, space padded to the right)

#define  EEP_NETWRK_ID    0x88	// Network ID in ASCII (not used) - space padded
#define  EEP_NETWRK_NAME  0x90	// Network Name in ASCII (not used) - space padded
#define  EEP_SERIAL_NUM   0x98	// Serial Number in ASCII - zero padded (at left)

#define  EEP_PROD_DATE    0xA0	//
#define  EEP_PROD_YEAR    0xA0	// Year of production (add 2000)
#define  EEP_PROD_WEEK    0xA1	// Week of production [0-51]
#define  EEP_PROD_HW_VER  0xA2	// Hardware version: 2 nibbles ?  Major and Minor number.
#define  EEP_PROD_BRD_VER 0xA3	// Board version: 2 nibbles ?  Major and Minor number.
#define  EEP_PROD_FW_VER  0xA4	// Firmware version: 2 nibbles ?  Major and Minor number.
#define  EEP_PROD_FW_SVER 0xA5	// Firmware SUBversion: 2 nibbles ?  Major and Minor number.
#define  EEP_PROD_BTTRS   0xA6	// number of batteries installed in the UPS


#define  EEP_BATT_SUBST   0xA8	// battery replacement block
#define  EEP_BATT_YEAR_0  0xA8	// Year of 1st battery replacement (add 2000)
#define  EEP_BATT_WEEK_0  0xA9	// Week of 1st battery replacement
#define  EEP_BATT_YEAR_1  0xAA	// Year of 2nd battery replacement (add 2000)
#define  EEP_BATT_WEEK_1  0xAB	// Week of 2nd battery replacement
#define  EEP_BATT_YEAR_2  0xAC	// Year of 3rd battery replacement (add 2000)
#define  EEP_BATT_WEEK_2  0xAD	// Week of 3rd battery replacement
#define  EEP_BATT_YEAR_3  0xAE	// Year of 4th battery replacement (add 2000)
#define  EEP_BATT_WEEK_3  0xAF	// Week of 4th battery replacement


#define  EEP_SCHEDULE     0xC8	// Start of SCHEDULING table
#define  EEP_SCHEDULE_0   0xC8	// 1st Scheduling slot
#define  EEP_SCHEDULE_1   0xD0	// 2nd Scheduling slot
#define  EEP_SCHEDULE_2   0xD8	// 3rd Scheduling slot
#define  EEP_SCHEDULE_3   0xE0	// 4th Scheduling slot
#define  EEP_SCHEDULE_4   0xE8	// 5th Scheduling slot
#define  EEP_SCHEDULE_5   0xF0	// 6th Scheduling slot


#define  BIT_NO_COMM_UPS        0x0001	// Event  2 - communications with the UPS lost
#define  BIT_COMM_OK            0x0002	// Event  3 - communications with the UPS reestablished
#define  BIT_BATTERY_MODE       0x0004	// Event  5 - the UPS is in Battery mode
#define  BIT_BATTERY_LOW        0x0008	// Event  6 - the UPS has a LOW battery level
#define  BIT_MAINS_OK           0x0010	// Event  7 - return of the mains
#define  BIT_OVERLOAD           0x0020	// Event  8 - the UPS is overloaded
#define  BIT_HIGH_TEMPERATURE   0x0040	// Event  9 - the UPS is in overtemperature
#define  BIT_GENERIC_FAULT      0x0080	// Event 11 - there is a generic fault
#define  BIT_SYSTEM_SHUTDOWN    0x0100	// Event 14 - the UPS will shut down IMMEDIATELY
#define  BIT_STANDBY            0x0200	// Event 32 - the UPS went in STANDBY MODE
#define  BIT_BATTERY_END        0x0400	// Event 33 - the UPS went in battery END
#define  BIT_EVENT_0            0x1000	// evento ??
#define  BIT_EVENT_1            0x2000	// evento ??
#define  BIT_FIRST_TIME         0x4000	// se è la prima volta che chiamo la funzione
#define  BIT_POLL_RESTART       0x8000	// se riesco ad identificare l'UPS dopo lo scollegamento, reinvio i dati



typedef struct
{
	int Port ;		// porta a cui è collegato l' UPS:
					//    0 = USB
					//    n = COMn
	char Opened ;	// BOOL flag che identifica se la porta è aperta
	int ErrCode ;	// ultimo codice di errore in fase di lettura
	int ErrCount ;	// conteggio degli errori
	unsigned char PollFlag ;	// identifica se posso accedere all'UPS
										// 0x01: se TRUE, polling abilitato
										// 0x02: se TRUE, la routine di polling dati è attiva: devo attendere
										// 0x04: se TRUE, bisogna rileggere la configurazione del gruppo
	//------------------------------------------------------------------------
	unsigned long Counter ;	// contatore: viene incrementato ad ogni nuovo POLL
	unsigned char CommStatus ;	// stato delle comunicazioni
	//unsigned char FramePointer ;
	size_t FramePointer ;	// puntatore al carattere di START dei dati ricevuti
	//------------------------------------------------------------------------
	char UpsModel[9] ;		// modello UPS (8 caratteri)
	unsigned char ge_2kVA ;	// if more or equal to 2KVA
	char SerialNumber[9] ;	// numero di serie dell'UPS

	unsigned short int YearOfProd ;	// anno di produzione dell'UPS
	unsigned char MonthOfProd ;		// Mese di produzione dell'UPS
	unsigned char DayOfProd ;		// Giorno di produzione dell'UPS
	unsigned char HW_MajorVersion ;	// Hardware: Major version
	unsigned char HW_MinorVersion ;	// Hardware: Minor version
	unsigned char BR_MajorVersion ;	// BoardHardware: Major version
	unsigned char BR_MinorVersion ;	// BoardHardware: Minor version
	unsigned char FW_MajorVersion ;	// Firmware: Major version
	unsigned char FW_MinorVersion ;	// Firmware: Minor version
	unsigned char FW_SubVersion ;		// Firmware: SUBVERSION (special releases
										// for particular customers)

	unsigned char BatteryNumber ;		// number of batteries in UPS
	//------------------------------------------------------------------------
	uint32_t StatusUPS ;	// flag di stato dell'UPS 4 byte): 1=TRUE
									//		bit  0 => BATTERY_MODE
									//		bit  1 => BATTERY_LOW
									//		bit  2 => BATTERY_END
									//		bit  3 => ONLINE (funzionamento in rete)
									//		bit  4 => MAINS_ON (rete di ingresso presente)
									//		bit  5 => STANDBY (UPS in modo Standby)
									//		bit  6 => WAIT_MAINS	(UPS in fase di INIT + attesa rete)
									//		bit  7 => INIT (UPS in fase di inizializzazione)
									//		------
									//		bit  8 => MASK_OK (semionda OK)
									//		bit  9 => MEAN_OK (media tensione di ingresso OK)
									//		bit 10 => SYNC_OK (sincronizzazione semionda OK)
									//		bit 11 => FAULT (generico)
									//		bit 12 => TEMP_MAX (superato livello critico di temperatura)
									//		bit 13 => TEMP_FAULT (Fault da temperatura: UPS si spegne)
									//		bit 14 => LOAD_MAX (soglia overload superata)
									//		bit 15 => LOAD_FAULT (Fault da carico: UPS si spegne)
									//		------
									//		bit 16 => INV_FAULT (Fault dell'inverter)
									//		bit 17 => IINV_MAX (eccessiva corrente sull'inverter)
									//		bit 18 => IINV_FAULT (Fault sulla corrente inverter: l'UPS si spegne)
									//		bit 19 => 50_60Hz (1=60Hz)
									//		bit 20 => EEP_FAULT (problemi con la EEPROM)
									//		bit 21 => VOUT_FAULT (tensione uscita troppo bassa)
									//		bit 22 => - non definito -
									//		bit 23 => - non definito -
									//		------
									//		bit 24 to 31 => - NON DEFINITI -

	uint16_t  ShortStatus ;    // the LSB 2 bytes of the status
	unsigned char OutConfig  ; // stato uscite UPS
	float Vinput     ; // tensione di INPUT in 1/10 di Volt
	float Voutput    ; // tensione di OUTPUT in 1/10 di Volt
	float Temp       ; // temperatura in 1/10 di grado
	float InpFreq    ; // Frequenza di INPUT in 1/10 di Hz
	float OutFreq    ; // Frequenza di OUTPUT in 1/10 di Hz
	float OutCurrent ; // Corrente di Output in 1/10 di A
	unsigned char LoadPerc   ; // Percentuale di carico
	unsigned short int  LoadVA     ; // Carico in VA
	float ChgCurrent ; // corrente carica batterie

	float Vbatt     ; // tensione delle batterie in 1/10 di Volt
	unsigned char PercBatt  ; // percentuale di carica della batteria
	unsigned short int  RtimeEmpty; // minuti alla scarica della batteria
	unsigned char PercEmpty ; // percentuale alla scarica: 0%=scarica, 100%=carica
	unsigned char OutStatus ; // stato delle uscite

	unsigned char Year      ; //
	unsigned char Month     ; //
	unsigned char Day       ; //
	unsigned char WeekDay   ; // Giorno della settimana
	unsigned char Hour      ; // Ora
	unsigned char Min       ; // minuti
	unsigned char Sec       ; // secondi

	unsigned char BattLowPerc ; // percentuale carica batteria quando va in BLOW
	unsigned long Rtime     ; // numero di minuti di accensione dell'UPS
	unsigned long RtimeBatt ; // numero di secondi*2 in modalità batteria dall'accensione
	//------------------------------------------------------------------------
	unsigned int ShutdownDelay ;	// Shutdown delay in seconds
	unsigned int WakeUpDelay ;		// WakeUp delay in seconds
} ENT_STRUCT ;

#endif	// ENTERPRISE_PROTOCOL

#endif
