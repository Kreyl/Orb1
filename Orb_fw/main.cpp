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
static void EnterSleepNow();
static void EnterSleep();

State_t State = stateOn;

#define ON_TIME_S       1800
#define FADE_TIME_S     300

#define CLR_L_ON        45
#define CLR_R_ON        117
#define CLR_L_OFF       230
#define CLR_R_OFF       240

// Incrementors
#define CLR_L_INC       (((CLR_L_OFF - CLR_L_ON) / FADE_TIME_S) + 1)
#define CLR_R_INC       (((CLR_R_OFF - CLR_R_ON) / FADE_TIME_S) + 1)

uint32_t TimeCnt = 0;

// Measure battery periodically
static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};

static const NeopixelParams_t NpxParams {NPX_SPI, NPX_DATA_PIN, NPX_DMA, NPX_DMA_MODE(0)};
Neopixels_t Leds{&NpxParams, BAND_CNT, BAND_SETUPS};
#endif

int main(void) {
#if 1 // ==== Get source of wakeup ====
    rccEnablePWRInterface(FALSE);
    State = stateOn;
    if(PWR->CSR & PWR_CSR_WUF) { // Wakeup occured
        // Is it button?
        PinSetupInput(BTN1_PIN, pudPullDown);
        if(PinIsHi(BTN1_PIN)) {
            // Check if pressed long enough
            for(uint32_t i=0; i<270000; i++) {
                // Go sleep if btn released too fast
                if(PinIsLo(BTN1_PIN)) EnterSleepNow();
            }
            // Btn was not released long enough, proceed with powerOn
        }
    }
#endif
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

    OrbRing.ClrHL = CLR_L_ON;
    OrbRing.ClrHR = CLR_R_ON;

    // ==== Leds ====
    Leds.Init();
    // LED pwr pin
    PinSetupOut(NPX_PWR_PIN, omPushPull);
    PinSetHi(NPX_PWR_PIN);

    OrbRing.Init();
    OrbRing.FadeIn();

    // Wait until main button released
    while(PinIsHi(BTN1_PIN)) { chThdSleepMilliseconds(63); }

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
                    case stateOn:
                        Printf("On: %u\r", TimeCnt);
                        if(TimeCnt >= ON_TIME_S) {
                            TimeCnt = 0;
                            State = stateFading;
                        }
                        break;
                    case stateFading:
                        Printf("Fade: %u; %u %u\r", TimeCnt, OrbRing.ClrHL, OrbRing.ClrHR);
                        if(TimeCnt >= FADE_TIME_S) {
                            State = stateOff;
                            OrbRing.FadeOut();
                        }
                        else {
                            chSysLock();
                            OrbRing.ClrHL += CLR_L_INC;
                            OrbRing.ClrHR += CLR_R_INC;
                            if(OrbRing.ClrHL > CLR_L_OFF) OrbRing.ClrHL = CLR_L_OFF;
                            if(OrbRing.ClrHR > CLR_R_OFF) OrbRing.ClrHR = CLR_R_OFF;
                            chSysUnlock();
                        }
                    case stateOff:
                        break;
                }
                break;

            case evtIdLedsDone: EnterSleep(); break;

#if 0 // BUTTONS_ENABLED
            case evtIdButtons:
                Printf("Btn %u\r", Msg.BtnEvtInfo.Type);
                if(State != stateOn) {
                    State = stateOn;
                    OrbRing.ClrHL = CLR_L_ON;
                    OrbRing.ClrHR = CLR_R_ON;
                    OrbRing.FadeIn();
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

void EnterSleepNow() {
    Sleep::EnableWakeup1Pin();
    Sleep::EnableWakeup2Pin();
    Sleep::EnterStandby();
}

void EnterSleep() {
    Printf("Entering sleep\r");
    PinSetLo(NPX_PWR_PIN);
    chThdSleepMilliseconds(45);
    chSysLock();
    EnterSleepNow();
    chSysUnlock();
}

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
