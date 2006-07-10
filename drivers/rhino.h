/* rhino.h */
#ifndef INCLUDED_RHINO_H
#define INCLUDED_RHINO_H

typedef int bool;

#define false 0
#define true 1

/* rhino commands */
#define CMD_INON    0x0001
#define CMD_INOFF   0x0002
#define CMD_SHUT    0x0004
#define CMD_OUTON   0x0003
#define CMD_OUTOFF  0x0004
#define CMD_PASSON  0x0005
#define CMD_PASSOFF 0x0006
#define CMD_UPSCONT 0x0053

/* xoff - xon protocol
#define _SOH = 0x01; // start of header
#define _EOT = 0x04; // end of transmission
#define _ACK = 0x06; // acknoledge (positive)
#define _DLE = 0x10; // data link escape
#define _XOn = 0x11; // transmit on
#define _XOff = 0x13; // transmit off
#define _NAK = 0x15; // negative acknoledge
#define _SYN = 0x16; // synchronous idle
#define _CAN = 0x18; // cancel
*/

static int const pacsize = 37; /* size of receive data package */

/* autonomy calcule */
static double  const AmpH = 40;       // Amperes-hora da bateria
static double  const VbatMin = 126;   // Tensão mínina das baterias
static double  const VbatNom = 144;   // Tensão nominal das baterias
static double  const FM = 0.32;       // Fator multiplicativo de correção da autonomia
static double  const FA = -2;         // Fator aditivo de correção da autonomia
static double  const ConstInt = 250;  // Consumo interno sem o carregador
static double  const Vin = 220;       // Tensão de entrada

int Day, Month, Year;
int dian=0, mesn=0, anon=0, weekn=0;
int ihour,imin, isec;
/* unsigned char DaysOnWeek; */
/* char seman[4]; */

/* int FExpansaoBateria; */
// internal variables
// package handshake ariables
/* int ContadorEstouro; */
bool detected;
bool SourceFail, Out110, RedeAnterior, OcorrenciaDeFalha;
bool RetornoDaRede, SuperAquecimento, SuperAquecimentoAnterior;
bool OverCharge, OldOverCharge, CriticBatt, OldCritBatt;
bool Flag_inversor, BypassOn, InputOn, OutputOn;
bool LowBatt, oldInversorOn;
/* data vetor from received and configuration data package - not used yet
unsigned char Dados[ 161 ]; */
/* identification group */
char Model[12];
int RhinoModel; /*, imodel; */
int PotenciaNominal, PowerFactor;
/* input group */
double AppPowerIn, UtilPowerIn, InFreq, InCurrent;
double LimInfEntrada, LimSupEntrada, ValorNominalEntrada;
int FatorPotEntrada;
/* output group */
double OutVoltage, InVoltage, OutCurrent, AppPowerOut;
double UtilPowerOut, OutFreq, LimInfSaida, LimSupSaida, ValorNominalSaida;
int FatorPotSaida;
/* battery group */
int Autonomy, Waiting;
double BattVoltage, Temperature, LimInfBattSrc, LimSupBattSrc;
double LimInfBattInv, LimSupBattInv, BattNonValue;
/* general group */
int BoostVolt, Rendimento, ContadorBateriaCritica;
/* status group */
unsigned char StatusEntrada, StatusSaida, StatusBateria, StatusGeral;
/* events group */
unsigned char EventosRede, EventosSaida, EventosBateria, EventosGeral;
// Grupo de Programação
unsigned char DiasDaSemana;
bool HabComandos;

/* Methods */
static void ScanReceivePack();
static int AutonomyCalc( int );
static void CommReceive(const char*, int );
static void getbaseinfo();
static void getupdateinfo();
  
unsigned char PacoteConf[51];
unsigned char RecPack[37];
int IndiceRecepcao;
bool flag_Recepcao;
bool flag_ReceberDados;
unsigned char Estado_Serial;

int Aux_NumEsperado;
int DadosIndex = 0;

int ContaErro = 0;
int ContaTimeOut = 0;
bool flg_XModemInit = false;
unsigned char Pac132Index = 0;
int Pac132[132];
bool flg_Pac132Strat = false;
bool flg_Cancel = false;
bool flag_pac132 = true;
int Cont_Global = 0;
int LastPacote = 0;
int Comando;
int Err_Comando = 0;

int Numero_Eventos;
int Numero_Pacotes_Restantes;

#endif //INCLUDED_RHINO_H
