#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <switch.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define INNER_HEAP_SIZE 0x80000
#define MAX_PADS 8

#define LITE_DEVICE_CODE 0x35000065U
#define LITE_GPIO_ACCESS_MODE 3U

#define LITE_PWM_PERIOD_NS 200000ULL
#define LITE_THREAD_STACK_SIZE 0x4000
#define LITE_BREATHE_PERIOD_TICKS 10000U
#define LITE_BLINK_PERIOD_TICKS 5000U
#define LITE_DIM_BRIGHTNESS 85U
#define LITE_FULL_BRIGHTNESS 255U

typedef enum {
    SelectedMode_Custom = 0,
    SelectedMode_Dim,
    SelectedMode_Solid,
    SelectedMode_Fade,
    SelectedMode_Off,
    SelectedMode_Charge,
    SelectedMode_Battery,
} SelectedMode;

typedef enum {
    LiteAnimMode_Off = 0,
    LiteAnimMode_Solid,
    LiteAnimMode_Breathe,
    LiteAnimMode_Blink,
} LiteAnimMode;

static bool sysmoduleRunning = true;
static bool isLite = false;
static SelectedMode selectedMode = SelectedMode_Custom;

// Hardware IPC: nn::led::detail::IManager & IController
typedef struct {
    Service s;
} LedManager;

typedef struct {
    Service s;
} LedController;

static LedManager g_ledManager;
static LedController g_ledController;
static bool g_hwLedReady = false;

// Power State Coordinator (psc:m)
static PscPmModule g_pmModule;
static bool g_pmModuleReady = false;

// Fallback GPIO Session
static GpioPadSession liteGpioSession;
static bool liteGpioServiceReady = false;
static bool liteBackendReady = false;

static Thread liteThread;
static bool liteThreadStarted = false;
static volatile bool liteThreadRunning = false;
static volatile u32 liteAnimMode = LiteAnimMode_Off;
static volatile u8 liteBrightness = 0;
static volatile u32 liteAnimPeriodTicks = LITE_BREATHE_PERIOD_TICKS;

static Event homeButtonEvent;
static bool homeEventReady = false;

static void logHw(const char* format, ...) {
    FILE* logFile = fopen("sdmc:/config/lite-led/hw_debug.log", "a");
    if (logFile) {
        time_t now = time(NULL);
        fprintf(logFile, "[%lu] ", (unsigned long)now);
        va_list args;
        va_start(args, format);
        vfprintf(logFile, format, args);
        va_end(args);
        fputc('\n', logFile);
        fclose(logFile);
    }
}

// Low-level IPC commands for nn::led::detail::IController
static Result hwLedStart(void) {
    if (!g_hwLedReady) return -1;
    return serviceDispatch(&g_ledController.s, 0);
}

static Result hwLedStop(void) {
    if (!g_hwLedReady) return -1;
    return serviceDispatch(&g_ledController.s, 1);
}

static Result hwLedSetMode(u32 mode) {
    if (!g_hwLedReady) return -1;
    return serviceDispatchIn(&g_ledController.s, 2, mode);
}

static Result hwLedSetConstantBrightness(u32 brightness) {
    if (!g_hwLedReady) return -1;
    return serviceDispatchIn(&g_ledController.s, 18, brightness);
}

static Result initHwLed(void) {
    Result rc = smGetService(&g_ledManager.s, "led");
    if (R_FAILED(rc)) {
        logHw("smGetService('led') failed: 0x%X", rc);
        return rc;
    }

    rc = serviceDispatchIn(&g_ledManager.s, 0, (u32)LITE_DEVICE_CODE,
        .out_num_objects = 1,
        .out_objects = &g_ledController.s,
    );
    if (R_FAILED(rc)) {
        logHw("OpenController(0x35000065) failed: 0x%X", rc);
        serviceClose(&g_ledManager.s);
        return rc;
    }

    g_hwLedReady = true;
    logHw("SUCCESS: nn::led hardware controller hooked!");
    return 0;
}

static void exitHwLed(void) {
    if (g_hwLedReady) {
        hwLedStop();
        serviceClose(&g_ledController.s);
        serviceClose(&g_ledManager.s);
        g_hwLedReady = false;
    }
}

static Result initPscMonitoring(void) {
    Result rc = pscmInitialize();
    if (R_FAILED(rc)) {
        logHw("pscmInitialize failed: 0x%X", rc);
        return rc;
    }

    rc = pscmGetPmModule(&g_pmModule, (PscPmModuleId)120, NULL, 0, true);
    if (R_FAILED(rc)) {
        rc = pscmGetPmModule(&g_pmModule, (PscPmModuleId)62, NULL, 0, true);
    }

    if (R_SUCCEEDED(rc)) {
        g_pmModuleReady = true;
        logHw("SUCCESS: psc:m sleep lifecycle listener registered!");
    } else {
        logHw("pscmGetPmModule failed: 0x%X", rc);
    }
    return rc;
}

static void exitPscMonitoring(void) {
    if (g_pmModuleReady) {
        pscPmModuleClose(&g_pmModule);
        pscmExit();
        g_pmModuleReady = false;
    }
}

#ifdef __cplusplus
extern "C" {
#endif

u32 __nx_applet_type = AppletType_None;
u32 __nx_fs_num_sessions = 1;

void __libnx_initheap(void) {
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    fake_heap_start = inner_heap;
    fake_heap_end = inner_heap + sizeof(inner_heap);
}

void __appInit(void) {
    Result rc = smInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw))) {
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        }
        SetSysProductModel model;
        if (R_SUCCEEDED(setsysGetProductModel(&model))) {
            isLite = (model == SetSysProductModel_Hoag);
        }
        setsysExit();
    }

    rc = hidInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));

    rc = hidsysInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));

    if (R_SUCCEEDED(hidsysActivateHomeButton())) {
        if (R_SUCCEEDED(hidsysAcquireHomeButtonEventHandle(&homeButtonEvent, true))) {
            homeEventReady = true;
        }
    }

    rc = fsInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));

    rc = psmInitialize();
    if (R_FAILED(rc)) diagAbortWithResult(rc);

    if (isLite) {
        gpioInitialize();
    }

    fsdevMountSdmc();
    smExit();
}

void __appExit(void) {
    sysmoduleRunning = false;

    if (homeEventReady) {
        eventClose(&homeButtonEvent);
        homeEventReady = false;
    }

    if (liteThreadStarted) {
        liteThreadRunning = false;
        threadWaitForExit(&liteThread);
        threadClose(&liteThread);
        liteThreadStarted = false;
    }

    exitHwLed();
    exitPscMonitoring();

    if (liteGpioServiceReady) {
        gpioPadSetValue(&liteGpioSession, GpioValue_Low);
        gpioPadClose(&liteGpioSession);
        liteGpioServiceReady = false;
    }

    fsdevUnmountAll();
    if (isLite) gpioExit();
    hidsysExit();
    hidExit();
    fsExit();
    psmExit();
}

#ifdef __cplusplus
}
#endif

// Fallback Software PWM Implementation
static Result liteSetGpioLevel(bool on) {
    if (!liteBackendReady || !liteGpioServiceReady) return KERNELRESULT(InvalidState);
    return gpioPadSetValue(&liteGpioSession, on ? GpioValue_High : GpioValue_Low);
}

static Result initLiteBackend(void) {
    Result rc = gpioOpenSession2(&liteGpioSession, LITE_DEVICE_CODE, LITE_GPIO_ACCESS_MODE);
    if (R_FAILED(rc)) return rc;

    rc = gpioPadSetDirection(&liteGpioSession, GpioDirection_Output);
    if (R_FAILED(rc)) {
        gpioPadClose(&liteGpioSession);
        return rc;
    }

    rc = gpioPadSetValue(&liteGpioSession, GpioValue_Low);
    if (R_FAILED(rc)) {
        gpioPadClose(&liteGpioSession);
        return rc;
    }

    liteGpioServiceReady = true;
    liteBackendReady = true;
    return 0;
}

static void litePwmTick(u8 brightness) {
    if (!liteBackendReady) {
        svcSleepThread(LITE_PWM_PERIOD_NS);
        return;
    }

    if (brightness == 0) {
        liteSetGpioLevel(false);
        svcSleepThread(LITE_PWM_PERIOD_NS);
        return;
    }

    if (brightness >= LITE_FULL_BRIGHTNESS) {
        liteSetGpioLevel(true);
        svcSleepThread(LITE_PWM_PERIOD_NS);
        return;
    }

    u64 onNs = (LITE_PWM_PERIOD_NS * brightness) / LITE_FULL_BRIGHTNESS;
    u64 offNs = LITE_PWM_PERIOD_NS - onNs;

    liteSetGpioLevel(true);
    svcSleepThread(onNs);
    liteSetGpioLevel(false);
    svcSleepThread(offNs);
}

static u8 computeBreatheBrightness(u8 peakBrightness, u32 phase, u32 periodTicks) {
    if (periodTicks < 2) return peakBrightness;
    u32 halfPeriod = periodTicks / 2;
    if (halfPeriod == 0) return peakBrightness;

    if (phase < halfPeriod) {
        return (u8)((peakBrightness * phase) / halfPeriod);
    }
    return (u8)((peakBrightness * (periodTicks - phase)) / halfPeriod);
}

static void liteThreadMain(void* arg) {
    (void)arg;
    u32 phase = 0;
    u32 lastMode = UINT32_MAX;
    u8 lastBrightness = UINT8_MAX;
    u32 lastPeriodTicks = 0;

    while (liteThreadRunning) {
        u32 mode = liteAnimMode;
        u8 brightness = liteBrightness;
        u32 periodTicks = liteAnimPeriodTicks;
        u8 pwmBrightness = 0;

        if (mode != lastMode || brightness != lastBrightness || periodTicks != lastPeriodTicks) {
            phase = 0;
            lastMode = mode;
            lastBrightness = brightness;
            lastPeriodTicks = periodTicks;
        }

        switch ((LiteAnimMode)mode) {
            case LiteAnimMode_Off:
                pwmBrightness = 0;
                break;
            case LiteAnimMode_Solid:
                pwmBrightness = brightness;
                break;
            case LiteAnimMode_Breathe:
                if (periodTicks < 2) periodTicks = LITE_BREATHE_PERIOD_TICKS;
                pwmBrightness = computeBreatheBrightness(brightness, phase, periodTicks);
                phase = (phase + 1) % periodTicks;
                break;
            case LiteAnimMode_Blink:
                if (periodTicks < 2) periodTicks = LITE_BLINK_PERIOD_TICKS;
                pwmBrightness = (phase < (periodTicks / 2)) ? brightness : 0;
                phase = (phase + 1) % periodTicks;
                break;
            default:
                pwmBrightness = 0;
                break;
        }

        litePwmTick(pwmBrightness);
    }

    liteSetGpioLevel(false);
    threadExit();
}

static void setLiteAnimation(LiteAnimMode mode, u8 brightness, u32 periodTicks) {
    liteAnimMode = (u32)mode;
    liteBrightness = brightness;
    liteAnimPeriodTicks = periodTicks;
}

static void startLiteThreadIfNeeded(void) {
    if (!isLite || liteThreadStarted) return;
    if (R_FAILED(initLiteBackend())) return;

    Result rc = threadCreate(&liteThread, liteThreadMain, NULL, NULL, LITE_THREAD_STACK_SIZE, 0x26, 3);
    if (R_FAILED(rc)) return;

    liteThreadRunning = true;
    rc = threadStart(&liteThread);
    if (R_FAILED(rc)) {
        liteThreadRunning = false;
        threadClose(&liteThread);
        return;
    }

    liteThreadStarted = true;
}

static void loadPatternFromFile(void) {
    FILE* file = fopen("sdmc:/config/lite-led/type", "r");
    if (!file) {
        FILE* newFile = fopen("sdmc:/config/lite-led/type", "w");
        if (newFile) {
            fputs("custom", newFile);
            fclose(newFile);
        }
        file = fopen("sdmc:/config/lite-led/type", "r");
    }

    char buffer[256];
    if (file && fgets(buffer, sizeof(buffer), file) != NULL) {
        buffer[strcspn(buffer, "\r\n")] = 0;
        if (strcmp(buffer, "custom") == 0) selectedMode = SelectedMode_Custom;
        else if (strcmp(buffer, "solid") == 0) selectedMode = SelectedMode_Solid;
        else if (strcmp(buffer, "dim") == 0) selectedMode = SelectedMode_Dim;
        else if (strcmp(buffer, "fade") == 0) selectedMode = SelectedMode_Fade;
        else if (strcmp(buffer, "off") == 0) selectedMode = SelectedMode_Off;
        else selectedMode = SelectedMode_Custom;
        fclose(file);
    } else {
        selectedMode = SelectedMode_Custom;
        if (file) fclose(file);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    DIR* dir = opendir("sdmc:/config/lite-led");
    if (dir) closedir(dir);
    else mkdir("sdmc:/config/lite-led", 0777);

    remove("sdmc:/config/lite-led/hw_debug.log");
    logHw("=== Lite-LED+ Hardware Daemon Boot ===");

    // 1. Инициализируем нативный аппаратный драйвер nn::led
    initHwLed();

    // 2. Инициализируем перехватчик сна psc:m
    initPscMonitoring();

    // 3. Поднимаем софтверный контур (для активной работы)
    startLiteThreadIfNeeded();
    loadPatternFromFile();

    int homeOverrideTicks = 0;
    int wakeStep = 0;
    int wakeStepTicks = 0;
    u64 lastTick = armGetSystemTick();

    while (sysmoduleRunning) {
        // Проверяем сброс от оверлея
        FILE* resetFile = fopen("sdmc:/config/lite-led/reset", "r");
        if (resetFile) {
            fclose(resetFile);
            remove("sdmc:/config/lite-led/reset");
            loadPatternFromFile();
        }

        // ОБРАБОТКА ЦИКЛА СНА И ПРОБУЖДЕНИЯ ЧЕРЕЗ PSC:M
        if (g_pmModuleReady) {
            PscPmState pscState;
            u32 pscFlags = 0;
            if (R_SUCCEEDED(pscPmModuleGetRequest(&g_pmModule, &pscState, &pscFlags))) {
                logHw("PSC Signal received: state=%d", pscState);

                if (pscState == PscPmState_ReadySleep || pscState == PscPmState_ReadySleepCritical) {
                    // КОНСОЛЬ УХОДИТ В ГЛУБОКИЙ СОН (SC7)
                    PsmChargerType chargerType = PsmChargerType_Unconnected;
                    psmGetChargerType(&chargerType);
                    u32 batteryCharge = 0;
                    psmGetBatteryChargePercentage(&batteryCharge);

                    logHw("Sleep Transition: Charging=%d, Battery=%d%%", (chargerType != PsmChargerType_Unconnected), batteryCharge);

                    if (chargerType != PsmChargerType_Unconnected) {
                        if (g_hwLedReady) {
                            if (batteryCharge >= 100) {
                                // 100% на зарядке во сне: аппаратный постоянный свет PMC
                                hwLedSetMode(1);
                                hwLedSetConstantBrightness(LITE_DIM_BRIGHTNESS);
                                hwLedStart();
                                logHw("PMC Hardware LED armed: SOLID_DIM");
                            } else {
                                // Зарядка во сне: аппаратное дыхание PMC
                                hwLedSetMode(2);
                                hwLedStart();
                                logHw("PMC Hardware LED armed: BREATHE");
                            }
                        }
                    } else {
                        // Без зарядки: гасим диод
                        if (g_hwLedReady) hwLedStop();
                    }

                    // Подтверждаем переход ядру ОС (предотвращаем дедлок)
                    pscPmModuleAcknowledge(&g_pmModule, pscState);
                }
                else if (pscState == PscPmState_ReadyAwaken || pscState == PscPmState_ReadyAwakenCritical) {
                    // КОНСОЛЬ ПРОСНУЛАСЬ
                    logHw("Wakeup Transition: restoring active state");
                    if (g_hwLedReady) hwLedStop();
                    pscPmModuleAcknowledge(&g_pmModule, pscState);
                }
            }
        }

        // Детектируем пробуждение по дельте таймера ARM
        u64 currentTick = armGetSystemTick();
        u64 elapsedNs = armTicksToNs(currentTick - lastTick);
        lastTick = currentTick;

        if (elapsedNs > 1800000000ULL) {
            wakeStep = 1;
            wakeStepTicks = 10; // 1.0 сек
        }

        // Перехват нажатия кнопки Home
        bool homePressed = false;
        if (homeEventReady && R_SUCCEEDED(eventWait(&homeButtonEvent, 0))) {
            homePressed = true;
        }
        HidHomeButtonState homeState;
        if (hidGetHomeButtonStates(&homeState, 1) > 0 && homeState.buttons != 0) {
            homePressed = true;
        }
        if (homePressed) {
            homeOverrideTicks = 10;
        }

        // Активный режим работы (экран включен)
        if (selectedMode == SelectedMode_Custom) {
            if (homeOverrideTicks > 0) {
                homeOverrideTicks--;
                wakeStep = 0;
                setLiteAnimation(LiteAnimMode_Solid, LITE_DIM_BRIGHTNESS, 0);
            } else if (wakeStep > 0) {
                wakeStepTicks--;
                if (wakeStep == 1) {
                    setLiteAnimation(LiteAnimMode_Solid, LITE_DIM_BRIGHTNESS, 0);
                    if (wakeStepTicks <= 0) { wakeStep = 2; wakeStepTicks = 4; }
                } else if (wakeStep == 2) {
                    setLiteAnimation(LiteAnimMode_Off, 0, 0);
                    if (wakeStepTicks <= 0) { wakeStep = 3; wakeStepTicks = 10; }
                } else if (wakeStep == 3) {
                    setLiteAnimation(LiteAnimMode_Solid, LITE_DIM_BRIGHTNESS, 0);
                    if (wakeStepTicks <= 0) wakeStep = 0;
                }
            } else {
                PsmChargerType chargerType = PsmChargerType_Unconnected;
                psmGetChargerType(&chargerType);
                if (chargerType != PsmChargerType_Unconnected) {
                    u32 batteryCharge = 0;
                    psmGetBatteryChargePercentage(&batteryCharge);
                    if (batteryCharge >= 100) {
                        setLiteAnimation(LiteAnimMode_Solid, LITE_DIM_BRIGHTNESS, 0);
                    } else {
                        setLiteAnimation(LiteAnimMode_Breathe, LITE_DIM_BRIGHTNESS, LITE_BREATHE_PERIOD_TICKS);
                    }
                } else {
                    setLiteAnimation(LiteAnimMode_Off, 0, 0);
                }
            }
        } else if (selectedMode == SelectedMode_Solid) {
            setLiteAnimation(LiteAnimMode_Solid, LITE_FULL_BRIGHTNESS, 0);
        } else if (selectedMode == SelectedMode_Dim) {
            setLiteAnimation(LiteAnimMode_Solid, LITE_DIM_BRIGHTNESS, 0);
        } else if (selectedMode == SelectedMode_Fade) {
            setLiteAnimation(LiteAnimMode_Breathe, LITE_FULL_BRIGHTNESS, LITE_BREATHE_PERIOD_TICKS);
        } else {
            setLiteAnimation(LiteAnimMode_Off, 0, 0);
        }

        svcSleepThread(100000000ULL); // 100 ms
    }

    return 0;
}
