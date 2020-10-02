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
bool IsEnteringSleep = false;

State_t State = stateFlaring;

#define EE_ADDR_CLR_L   18
#define EE_ADDR_CLR_R   36

// Measure battery periodically
static TmrKL_t TmrOneSecond {TIME_MS2I(999), evtIdEverySecond, tktPeriodic};
static void OnMeasurementDone();

bool IsUsbConnected() { return PinIsHi(PIN_5V_USB); }
bool IsCharging()     { return PinIsLo(IS_CHARGING_PIN); }

void SaveSettings();

// LEDs
TmrKL_t TmrSave {TIME_MS2I(3600), evtIdTimeToSave, tktOneShot};
TmrKL_t TmrEndShowBounds {TIME_MS2I(1530), evtIdTimeToFlare, tktOneShot};
static const NeopixelParams_t NpxParams {NPX_SPI, NPX_DATA_PIN, NPX_DMA, NPX_DMA_MODE(0)};
Neopixels_t Leds{&NpxParams, BAND_CNT, BAND_SETUPS};
#endif

int main(void) {
#if 1 // ==== Get source of wakeup ====
    rccEnablePWRInterface(FALSE);
    State = stateFlaring;
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
        else State = stateChargingStatus;
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

    // Load and check color
    OrbRing.ClrHL = EE::Read32(EE_ADDR_CLR_L);
    OrbRing.ClrHR = EE::Read32(EE_ADDR_CLR_R);
    if(OrbRing.ClrHL < 0 or OrbRing.ClrHL > 360 or OrbRing.ClrHR < 0 or OrbRing.ClrHR > 360) {
        OrbRing.ClrHL = CLR_H_L;
        OrbRing.ClrHR = CLR_H_R;
    }
    Printf("L: %d; R: %d\r", OrbRing.ClrHL, OrbRing.ClrHR);

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
    // Charging pin
    PinSetupInput(IS_CHARGING_PIN, pudPullUp);
    // ADC
    PinSetupOut(ADC_BAT_EN, omPushPull);
    PinSetHi(ADC_BAT_EN); // Enable it forever, as 200k produces ignorable current
    PinSetupAnalog(ADC_BAT_PIN);
    Adc.Init();
    TmrOneSecond.StartOrRestart();

    // Main cycle
    ITask();
}

__noreturn
void ITask() {
    while(true) {
        EvtMsg_t Msg = EvtQMain.Fetch(TIME_INFINITE);
        switch(Msg.ID) {
#if BUTTONS_ENABLED
            case evtIdButtons:
//                Printf("Btn %u\r", Msg.BtnEvtInfo.Type);
                // Main button
                if(Msg.BtnEvtInfo.BtnID == 0) {
                    if(Msg.BtnEvtInfo.Type == beLongPress) {
                        IsEnteringSleep = !IsEnteringSleep;
                        if(IsEnteringSleep) OrbRing.FadeOut();
                        else OrbRing.FadeIn();
                    }
                    else if(Msg.BtnEvtInfo.Type == beShortPress) {
                        if(IsUsbConnected()) { // Switch between flaring and Charging status
                            State = (State == stateChargingStatus)? stateFlaring : stateChargingStatus;
                        }
                        else State = stateFlaring;
                    }
                }
                // Right / Left buttons
                if(Msg.BtnEvtInfo.BtnID == 1 or Msg.BtnEvtInfo.BtnID == 2) {
                    if(State == stateFlaring) {
                        State = stateShowBounds;
                        OrbRing.StartShowBounds();
                        TmrEndShowBounds.StartOrRestart();
                    }
                    else if(State == stateShowBounds) {
                        if(Msg.BtnEvtInfo.Type == beShortPress or Msg.BtnEvtInfo.Type == beRepeat) {
                            if     (Msg.BtnEvtInfo.BtnID == 1) OrbRing.IncreaseColorBounds();
                            else if(Msg.BtnEvtInfo.BtnID == 2) OrbRing.DecreaseColorBounds();
                            TmrSave.StartOrRestart(); // Prepare to save
                            TmrEndShowBounds.StartOrRestart();
                        }
                    }
                } // btn 1 or 2
                break;
#endif
            case evtIdTimeToFlare:
                if(State == stateShowBounds) {
                    State = stateFlaring;
                    OrbRing.StopShowBounds();
                }
                break;

            case evtIdTimeToSave:
                SaveSettings();
                break;

            case evtIdLedsDone: EnterSleep(); break;

            case evtIdEverySecond: Adc.StartMeasurement(); break;
            case evtIdAdcRslt: OnMeasurementDone(); break;

            case evtIdShellCmd:
                OnCmd((Shell_t*)Msg.Ptr);
                ((Shell_t*)Msg.Ptr)->SignalCmdProcessed();
                break;
            default: Printf("Unhandled Msg %u\r", Msg.ID); break;
        } // Switch
    } // while true
} // ITask()

void ProcessUSB(PinSnsState_t *PState, uint32_t Len) {
    if(*PState == pssRising) {
        if(State == stateFlaring) State = stateChargingStatus;
    }
    else if(*PState == pssFalling) {
        State = stateFlaring;
    }
}

//void ProcessIsCharging(PinSnsState_t *PState, uint32_t Len) {
//    if(*PState == pssFalling) {
//        Printf("ChargingStart\r");
//        OrbRing.ShowMode = OrbRing_t::showCharging;
//    }
//    else if(*PState == pssRising) {
//        Printf("ChargingEnd\r");
//        OrbRing.ShowMode = OrbRing_t::showChargingDone;
//    }
//}

void OnMeasurementDone() {
//    Printf("AdcDone\r");
    if(Adc.FirstConversion) Adc.FirstConversion = false;
    else {
        uint32_t VRef_adc = Adc.GetResult(ADC_VREFINT_CHNL);
        uint32_t Vadc = Adc.GetResult(BAT_CHNL);
        uint32_t Vmv = Adc.Adc2mV(Vadc, VRef_adc);
//        Printf("VrefAdc=%u; Vadc=%u; Vmv=%u\r", VRef_adc, Vadc, Vmv);
        uint32_t Battery_mV = Vmv * 2; // Resistor divider
//        Printf("Vbat=%u\r", Battery_mV);
        if(Battery_mV < 3000) {
            Printf("Discharged\r");
            State = stateDischarged;
        }
    }
}

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

void SaveSettings() {
    EE::Write32(EE_ADDR_CLR_L, OrbRing.ClrHL);
    EE::Write32(EE_ADDR_CLR_R, OrbRing.ClrHR);
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
        SaveSettings();
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

    else if(PCmd->NameIs("Tail")) {
        int32_t Len, TailLen, k1;
        if(PCmd->GetNext<int32_t>(&Len) == retvOk) {
            if(PCmd->GetNext<int32_t>(&TailLen) == retvOk) {
                if(PCmd->GetNext<int32_t>(&k1) == retvOk) {
                    OrbRing.SetLen(Len, TailLen, k1);
                }
            }
        }
        else PShell->CmdError();
    }
    else if(PCmd->NameIs("Per")) {
        uint32_t V;
        if(PCmd->GetNext<uint32_t>(&V) == retvOk) {
            OrbRing.SetPeriod(V);
        }
        else PShell->CmdError();
    }

    else if(PCmd->NameIs("strt")) {
        uint32_t V;
        if(PCmd->GetNext<uint32_t>(&V) == retvOk) {
            OrbRing.Start(V);
        }
        else PShell->CmdError();
    }

    else PShell->CmdUnknown();
}
#endif
