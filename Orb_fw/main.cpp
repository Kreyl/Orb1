#include "board.h"
#include "led.h"
#include "Sequences.h"
#include "kl_lib.h"
#include "MsgQ.h"
#include "SimpleSensors.h"
#include "buttons.h"
#include "ws2812b.h"
#include "color.h"
#include "OrbRing.h"
#include "kl_adc.h"
#include "States.h"

#if 1 // ======================== Variables and defines ========================
// Forever
EvtMsgQ_t<EvtMsg_t, MAIN_EVT_Q_LEN> EvtQMain;
static const UartParams_t CmdUartParams(115200, CMD_UART_PARAMS);
CmdUart_t Uart{&CmdUartParams};
static void ITask();
static void OnCmd(Shell_t *PShell);
//static void EnterSleepNow();
//static void EnterSleep();

State_t State = stateIdle;

// Colors
#define CLR_L_IDLE      0
#define CLR_R_IDLE      9
#define CLR_L_3M        297
#define CLR_R_3M        306
#define CLR_L_5M        234
#define CLR_R_5M        243
#define CLR_L_7M        117
#define CLR_R_7M        126
#define CLR_L_YELLOW    50
#define CLR_R_YELLOW    60
#define CLR_L_ORANGE    15
#define CLR_R_ORANGE    19

uint32_t TimeCnt = 0;

// Measure battery periodically
static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};

static const NeopixelParams_t NpxParams {NPX_SPI, NPX_DATA_PIN, NPX_DMA, NPX_DMA_MODE(0)};
Neopixels_t Leds{&NpxParams, BAND_CNT, BAND_SETUPS};
#endif

void SetState(State_t NewState) {
    State = NewState;
    TimeCnt = 0;
    switch(State) {
        case stateIdle:
            OrbRing.ClrHL = CLR_L_IDLE;
            OrbRing.ClrHR = CLR_R_IDLE;
            break;
        case stateStart3min:
            OrbRing.ClrHL = CLR_L_3M;
            OrbRing.ClrHR = CLR_R_3M;
            break;
        case stateStart5min:
            OrbRing.ClrHL = CLR_L_5M;
            OrbRing.ClrHR = CLR_R_5M;
            break;
        case stateStart7min:
            OrbRing.ClrHL = CLR_L_7M;
            OrbRing.ClrHR = CLR_R_7M;
            break;
        case stateYellow:
            OrbRing.ClrHL = CLR_L_YELLOW;
            OrbRing.ClrHR = CLR_R_YELLOW;
            break;
        case stateOrange:
            OrbRing.ClrHL = CLR_L_ORANGE;
            OrbRing.ClrHR = CLR_R_ORANGE;
            break;
    }
}

int main(void) {
    rccEnablePWRInterface(FALSE);
#if 1 // ==== Init Vcore & clock system ====
    SetupVCore(vcore1V8);
#if WS2812B_V2
    // PLL fed by HSI
    if(Clk.EnableHSI() == retvOk) {
        Clk.SetupFlashLatency(16);
        Clk.SetupPLLSrc(pllSrcHSI16);
        Clk.SetupPLLDividers(pllMul4, pllDiv3);
        Clk.SetupBusDividers(ahbDiv2, apbDiv1, apbDiv1);
        Clk.SwitchToPLL();
    }
#else
    if(Clk.EnableHSE() == retvOk) {
        Clk.SetupFlashLatency(12);
        Clk.SetupPLLSrc(pllSrcHSE);
        Clk.SetupPLLDividers(pllMul8, pllDiv4);
        Clk.SetupBusDividers(ahbDiv2, apbDiv1, apbDiv1);
        Clk.SwitchToPLL();
    }
    else { // PLL fed by HSI
        if(Clk.EnableHSI() == retvOk) {
            Clk.SetupFlashLatency(16);
            Clk.SetupPLLSrc(pllSrcHSI16);
            Clk.SetupPLLDividers(pllMul4, pllDiv3);
            Clk.SetupBusDividers(ahbDiv2, apbDiv1, apbDiv1);
            Clk.SwitchToPLL();
        }
    }
#endif
    Clk.UpdateFreqValues();
#endif
#if 1 // ==== Init OS and UART ====
    halInit();
    chSysInit();
    EvtQMain.Init();
    Uart.Init();
    Printf("\r%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));
    Clk.PrintFreqs();
    Printf("State: %u\r", State);
#endif

    SetState(stateIdle);

    // ==== Leds ====
    Leds.Init();
    // LED pwr pin
    PinSetupOut(NPX_PWR_PIN, omPushPull);
    PinSetHi(NPX_PWR_PIN);

    OrbRing.Init();
    OrbRing.FadeIn();

    SimpleSensors::Init();
    TmrOneSecond.StartOrRestart();

    // Main cycle
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
            case evtIdEverySecond:
                TimeCnt++;
                switch(State) {
                    case stateIdle: break;
                    case stateStart3min: if(TimeCnt >= 60)  SetState(stateYellow);  break;
                    case stateStart5min: if(TimeCnt >= 180) SetState(stateYellow);  break;
                    case stateStart7min: if(TimeCnt >= 300) SetState(stateYellow);  break;
                    case stateYellow:    if(TimeCnt >= 60)  SetState(stateOrange);  break;
                    case stateOrange:    if(TimeCnt >= 60)  SetState(stateIdle);    break;
                }
                Printf("t=%u; State=%u; Clr: %u %u\r", TimeCnt, State, OrbRing.ClrHL, OrbRing.ClrHR);
                break;

//            case evtIdLedsDone: EnterSleep(); break;

#if 1 // BUTTONS_ENABLED
            case evtIdButtons:
                Printf("Btn %u\r", Msg.BtnEvtInfo.BtnID);
                switch(Msg.BtnEvtInfo.BtnID) {
                    case 0: SetState(stateStart3min); break;
                    case 1: SetState(stateStart5min); break;
                    case 2: SetState(stateStart7min); break;
                    default: break;
                }
                break;
#endif
            case evtIdShellCmd:
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;
            default: Printf("Unhandled Msg %u\r", Msg.ID); break;
        } // Switch
    } // while true
} // ITask()

//void EnterSleepNow() {
//    Sleep::EnableWakeup1Pin();
//    Sleep::EnableWakeup2Pin();
//    Sleep::EnterStandby();
//}
//
//void EnterSleep() {
//    Printf("Entering sleep\r");
//    PinSetLo(NPX_PWR_PIN);
//    chThdSleepMilliseconds(45);
//    chSysLock();
//    EnterSleepNow();
//    chSysUnlock();
//}

#if 1 // ================= Command processing ====================
void OnCmd(Shell_t *PShell) {
	Cmd_t *PCmd = &PShell->Cmd;
    __attribute__((unused)) int32_t dw32 = 0;  // May be unused in some configurations
//    Printf("%S%S\r", PCmd->IString, PCmd->Remainer? PCmd->Remainer : " empty");
    // Handle command
    if(PCmd->NameIs("Ping")) {
        PShell->Ok();
    }
    else if(PCmd->NameIs("Version")) PShell->Print("%S %S\r", APP_NAME, XSTRINGIFY(BUILD_TIME));

    else if(PCmd->NameIs("SetClrLR")) {
        int32_t L, R;
        if(PCmd->GetNext<int32_t>(&L) != retvOk) {  PShell->BadParam();  return; }
        if(PCmd->GetNext<int32_t>(&R) != retvOk) {  PShell->BadParam();  return; }
        if(L < 0 or L > 360 or R < 0 or R > 360 or L > R) {  PShell->BadParam();  return; }
        OrbRing.ClrHL = L;
        OrbRing.ClrHR = R;
        PShell->Ok();
    }

    else if(PCmd->NameIs("State")) {
        uint8_t St;
        if(PCmd->GetNext<uint8_t>(&St) != retvOk) {  PShell->BadParam();  return; }
        SetState((State_t)St);
        PShell->Ok();
    }

    else if(PCmd->NameIs("Clr")) {
        Color_t Clr;
        if(PCmd->GetClrRGB(&Clr) == retvOk) {
//            Clr.Print();
//            PrintfEOL();
            if(Leds.TransmitDone) {
                Leds.SetAll(Clr);
                Leds.SetCurrentColors();
                PShell->Ok();
            }
            else Printf("Busy\r");
//            Eff::SetColor(Color_t(FClr[0], FClr[1], FClr[2]));

        }
        else {
            PShell->CmdError();
        }
    }

    else if(PCmd->NameIs("hsv")) {
        ColorHSV_t fhsv;
        if(PCmd->GetNext<uint16_t>(&fhsv.H) != retvOk) {  PShell->BadParam();  return; }
        if(PCmd->GetNext<uint8_t>(&fhsv.S) != retvOk) {  PShell->BadParam();  return; }
        if(PCmd->GetNext<uint8_t>(&fhsv.V) != retvOk) {  PShell->BadParam();  return; }
        OrbRing.SetColor(fhsv);
    }

    else PShell->CmdUnknown();
}
#endif
