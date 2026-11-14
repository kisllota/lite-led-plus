#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <switch.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define INNER_HEAP_SIZE 0x80000

#define LITE_DEVICE_CODE 0x35000065U
#define LITE_GPIO_ACCESS_MODE 3U

#define LITE_THREAD_STACK_SIZE 0x4000
#define LITE_TICK_NS 2000000ULL          // 2 ms per PWM delta-sigma tick (500 Hz)
#define LITE_BREATHE_PERIOD_TICKS 1000U  // 2.0 seconds at 2 ms/tick
#define LITE_BLINK_PERIOD_TICKS 500U    // 1.0 second at 2 ms/tick
#define LITE_DIM_BRIGHTNESS 85U
#define LITE_FULL_BRIGHTNESS 255U

typedef enum {
    SelectedMode_Custom = 0,
    SelectedMode_Dim,
    SelectedMode_Solid,
    SelectedMode_Fade,
    SelectedMode_Off,
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

// Light Control / Backlight Service (lbl)
static bool g_lblReady = false;

// Hardware IPC: nn::led::detail::IManager & IController (PMC hardware driver for SC7 deep sleep)
typedef struct {
    Service s;
} LedManager;

typedef struct {
    Service s;
} LedController;

static LedManager g_ledManager;
static LedController g_ledController;
static bool g_hwLedReady = false;
static bool g_hwLedActive = false;
static int g_lastHwMode = 0;

// Unified GPIO Hardware Backend
static GpioPadSession liteGpioSession;
static bool liteGpioServiceReady = false;
static bool liteBackendReady = false;

static Thread liteThread;
static bool liteThreadStarted = false;
static volatile bool liteThreadRunning = false;
static volatile u32 liteAnimMode = LiteAnimMode_Off;
static volatile u8 liteTargetBrightness = 0;

static Event homeButtonEvent;
static bool homeEventReady = false;

static void logDebug(const char* format, ...) {
    FILE* logFile = fopen("sdmc:/config/lite-led/daemon.log", "a");
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

// Low-level IPC commands for nn::led (hardware PMC execution during deep sleep)
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

static Result initHwLed(void) {
    if (!isLite) return -1;
    Result rc = smGetService(&g_ledManager.s, "led");
    if (R_FAILED(rc)) {
        logDebug("smGetService('led') failed: 0x%X", rc);
        return rc;
    }

    (void)serviceConvertToDomain(&g_ledManager.s);

    u32 deviceCode = LITE_DEVICE_CODE;
    rc = serviceDispatchIn(&g_ledManager.s, 0, deviceCode,
        .out_num_objects = 1,
        .out_objects = &g_ledController.s,
    );
    if (R_FAILED(rc)) {
        logDebug("OpenController(0x35000065) failed: 0x%X", rc);
        serviceClose(&g_ledManager.s);
        return rc;
    }

    g_hwLedReady = true;
    logDebug("SUCCESS: nn::led hardware PMC controller hooked for deep sleep!");
    return 0;
}

static void exitHwLed(void) {
    if (g_hwLedReady) {
        hwLedStop();
        serviceClose(&g_ledController.s);
        serviceClose(&g_ledManager.s);
        g_hwLedReady = false;
        g_hwLedActive = false;
        g_lastHwMode = 0;
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

    rc = lblInitialize();
    if (R_SUCCEEDED(rc)) {
        g_lblReady = true;
    }

    if (isLite) {
        gpioInitialize();
    }

    fsdevMountSdmc();
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

    if (liteGpioServiceReady) {
        gpioPadSetValue(&liteGpioSession, GpioValue_Low);
        gpioPadClose(&liteGpioSession);
        liteGpioServiceReady = false;
        liteBackendReady = false;
    }

    if (g_lblReady) {
        lblExit();
        g_lblReady = false;
    }

    fsdevUnmountAll();
    if (isLite) gpioExit();
    hidsysExit();
    hidExit();
    fsExit();
    psmExit();
    smExit();
}

#ifdef __cplusplus
}
#endif

// Robust Self-Healing GPIO Session Backend (reclaims hardware Pinmux)
static Result initLiteBackend(void) {
    if (liteGpioServiceReady) {
        gpioPadClose(&liteGpioSession);
        liteGpioServiceReady = false;
        liteBackendReady = false;
    }

    Result rc = gpioOpenSession2(&liteGpioSession, LITE_DEVICE_CODE, LITE_GPIO_ACCESS_MODE);
    if (R_FAILED(rc)) {
        logDebug("gpioOpenSession2 failed: 0x%X", rc);
        return rc;
    }

    rc = gpioPadSetDirection(&liteGpioSession, GpioDirection_Output);
    if (R_FAILED(rc)) {
        logDebug("gpioPadSetDirection failed: 0x%X", rc);
        gpioPadClose(&liteGpioSession);
        return rc;
    }

    rc = gpioPadSetValue(&liteGpioSession, GpioValue_Low);
    if (R_FAILED(rc)) {
        logDebug("gpioPadSetValue failed: 0x%X", rc);
        gpioPadClose(&liteGpioSession);
        return rc;
    }

    liteGpioServiceReady = true;
    liteBackendReady = true;
    return 0;
}

static Result liteSetGpioLevel(bool on) {
    if (!liteBackendReady || !liteGpioServiceReady) {
        Result rc = initLiteBackend();
        if (R_FAILED(rc)) return rc;
    }

    Result rc = gpioPadSetValue(&liteGpioSession, on ? GpioValue_High : GpioValue_Low);
    if (R_FAILED(rc)) {
        // Auto-recovery: if session drops during USB power state transition, reconnect
        logDebug("GPIO write failed (0x%X), attempting session recovery...", rc);
        initLiteBackend();
        rc = gpioPadSetValue(&liteGpioSession, on ? GpioValue_High : GpioValue_Low);
    }
    return rc;
}

// Flicker-Free First-Order Delta-Sigma Software PWM Engine (Active Screen)
static void liteThreadMain(void* arg) {
    (void)arg;
    u32 phase = 0;
    u32 accumulator = 0;
    bool lastGpioLevel = false;

    while (liteThreadRunning) {
        u32 mode = liteAnimMode;
        u8 targetBrightness = liteTargetBrightness;
        u8 currentBrightness = 0;

        switch ((LiteAnimMode)mode) {
            case LiteAnimMode_Off:
                currentBrightness = 0;
                phase = 0;
                break;

            case LiteAnimMode_Solid:
                currentBrightness = targetBrightness;
                phase = 0;
                break;

            case LiteAnimMode_Breathe: {
                u32 currentPhase = phase % LITE_BREATHE_PERIOD_TICKS;
                u32 halfPeriod = LITE_BREATHE_PERIOD_TICKS / 2;
                if (currentPhase < halfPeriod) {
                    currentBrightness = (u8)((targetBrightness * currentPhase) / halfPeriod);
                } else {
                    currentBrightness = (u8)((targetBrightness * (LITE_BREATHE_PERIOD_TICKS - currentPhase)) / halfPeriod);
                }
                phase = (phase + 1) % LITE_BREATHE_PERIOD_TICKS;
                break;
            }

            case LiteAnimMode_Blink: {
                u32 currentPhase = phase % LITE_BLINK_PERIOD_TICKS;
                currentBrightness = (currentPhase < (LITE_BLINK_PERIOD_TICKS / 2)) ? targetBrightness : 0;
                phase = (phase + 1) % LITE_BLINK_PERIOD_TICKS;
                break;
            }

            default:
                currentBrightness = 0;
                phase = 0;
                break;
        }

        // Delta-Sigma Modulation
        bool nextGpioLevel = false;
        if (currentBrightness == 0) {
            nextGpioLevel = false;
            accumulator = 0;
        } else if (currentBrightness >= 255) {
            nextGpioLevel = true;
            accumulator = 0;
        } else {
            accumulator += currentBrightness;
            if (accumulator >= 255) {
                accumulator -= 255;
                nextGpioLevel = true;
            } else {
                nextGpioLevel = false;
            }
        }

        // Minimal IPC calls: only update hardware pin if logical value actually flipped
        if (nextGpioLevel != lastGpioLevel) {
            liteSetGpioLevel(nextGpioLevel);
            lastGpioLevel = nextGpioLevel;
        }

        svcSleepThread(LITE_TICK_NS);
    }

    liteSetGpioLevel(false);
    threadExit();
}

static void setLiteAnimation(LiteAnimMode mode, u8 brightness) {
    liteAnimMode = (u32)mode;
    liteTargetBrightness = brightness;
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

    remove("sdmc:/config/lite-led/daemon.log");
    logDebug("=== Lite-LED+ Daemon Boot: model=%s ===", isLite ? "Switch Lite (Hoag)" : "Other");

    // Инициализируем аппаратный PMC контроллер (для работы в глубоком сне SC7)
    initHwLed();

    // Запускаем программный контур (для активного режима с включенным экраном)
    startLiteThreadIfNeeded();
    loadPatternFromFile();

    int homeOverrideTicks = 0;
    int wakeStep = 0;
    int wakeStepTicks = 0;
    u64 lastTick = armGetSystemTick();
    bool wasSleeping = false;

    while (sysmoduleRunning) {
        // 1. Проверяем сброс / смену профиля от Tesla оверлея
        FILE* resetFile = fopen("sdmc:/config/lite-led/reset", "r");
        if (resetFile) {
            fclose(resetFile);
            remove("sdmc:/config/lite-led/reset");
            loadPatternFromFile();
        }

        // 2. Проверяем статус подсветки экрана (LBL)
        bool screenIsOff = false;
        if (g_lblReady) {
            LblBacklightSwitchStatus blStatus = LblBacklightSwitchStatus_Enabled;
            if (R_SUCCEEDED(lblGetBacklightSwitchStatus(&blStatus))) {
                if (blStatus == LblBacklightSwitchStatus_Disabled || blStatus == LblBacklightSwitchStatus_Disabling) {
                    screenIsOff = true;
                }
            }
        }

        // 3. Детектируем пробуждение по дельте ARM-таймера (если процессор спал в SC7)
        u64 currentTick = armGetSystemTick();
        u64 elapsedNs = armTicksToNs(currentTick - lastTick);
        lastTick = currentTick;

        if (elapsedNs > 1500000000ULL) {
            logDebug("Deep sleep SC7 wake detected: delta=%llu ms", (unsigned long long)(elapsedNs / 1000000ULL));
            wasSleeping = true;
        }

        // 4. Логика перехода в сон и выхода из сна
        if (screenIsOff) {
            wasSleeping = true;

            // Гасим софтверный PWM поток (ядра CPU в SC7 всё равно обесточены)
            setLiteAnimation(LiteAnimMode_Off, 0);

            PsmChargerType chargerType = PsmChargerType_Unconnected;
            psmGetChargerType(&chargerType);
            u32 batteryCharge = 0;
            psmGetBatteryChargePercentage(&batteryCharge);

            if (chargerType != PsmChargerType_Unconnected) {
                // Идет зарядка при погашенном экране: запускаем аппаратный PMC контроллер
                if (g_hwLedReady) {
                    // В аппаратном драйвере nn::led на Switch Lite (Hoag):
                    // Mode 1: Аппаратная анимация («дыхание» PMC Breathing)
                    // Mode 2: Постоянный статический свет (PMC Solid)
                    int desiredMode = (batteryCharge >= 100) ? 2 : 1;
                    if (!g_hwLedActive || g_lastHwMode != desiredMode) {
                        Result rcMode = hwLedSetMode(desiredMode);
                        Result rcStart = hwLedStart();
                        g_hwLedActive = (rcStart == 0);
                        g_lastHwMode = desiredMode;
                        logDebug("Sleep charging: mode=%d rcMode=0x%X rcStart=0x%X batt=%d%%", 
                                 desiredMode, rcMode, rcStart, batteryCharge);
                    }
                }
            } else {
                // Без зарядки во сне диод полностью обесточен
                if (g_hwLedReady && g_hwLedActive) {
                    hwLedStop();
                    g_hwLedActive = false;
                    g_lastHwMode = 0;
                    logDebug("Sleep charging stopped");
                }
            }

            svcSleepThread(100000000ULL);
            continue;
        } else if (wasSleeping) {
            // ЭКРАН ВКЛЮЧИЛСЯ: выход из сна
            wasSleeping = false;
            logDebug("Screen/System woke up -> disarming HW LED and rearming GPIO");

            if (g_hwLedReady && g_hwLedActive) {
                hwLedStop();
                g_hwLedActive = false;
                g_lastHwMode = 0;
            }

            // КРИТИЧЕСКИЙ ШАГ: принудительно возвращаем владение Pinmux шиной GPIO!
            initLiteBackend();

            if (wakeStep == 0) {
                wakeStep = 1;
                wakeStepTicks = 10; // 1.0 сек первый импульс
            }
        }

        // 5. Перехват нажатия кнопки Home
        bool homePressed = false;
        if (homeEventReady && R_SUCCEEDED(eventWait(&homeButtonEvent, 0))) {
            homePressed = true;
        }
        HidHomeButtonState homeState;
        if (hidGetHomeButtonStates(&homeState, 1) > 0 && homeState.buttons != 0) {
            homePressed = true;
        }
        if (homePressed) {
            homeOverrideTicks = 10; // 1.0 сек удержание
        }

        // 6. Исполнение матрицы приоритетов профиля
        if (selectedMode == SelectedMode_Custom) {
            if (homeOverrideTicks > 0) {
                // Приоритет 1 (P1): Нажатие кнопки Home — немедленный мягкий отклик
                homeOverrideTicks--;
                wakeStep = 0; // Нажатие сбрасывает анимацию приветствия
                setLiteAnimation(LiteAnimMode_Solid, LITE_DIM_BRIGHTNESS);
            } else if (wakeStep > 0) {
                // Приоритет 2 (P2): Двойной приветственный импульс при выходе из сна
                wakeStepTicks--;
                if (wakeStep == 1) {
                    setLiteAnimation(LiteAnimMode_Solid, LITE_DIM_BRIGHTNESS);
                    if (wakeStepTicks <= 0) { wakeStep = 2; wakeStepTicks = 4; } // 0.4 сек пауза
                } else if (wakeStep == 2) {
                    setLiteAnimation(LiteAnimMode_Off, 0);
                    if (wakeStepTicks <= 0) { wakeStep = 3; wakeStepTicks = 10; } // 1.0 сек импульс 2
                } else if (wakeStep == 3) {
                    setLiteAnimation(LiteAnimMode_Solid, LITE_DIM_BRIGHTNESS);
                    if (wakeStepTicks <= 0) { wakeStep = 0; }
                }
            } else {
                // Приоритет 3 (P3): Индикация подключения зарядного устройства
                PsmChargerType chargerType = PsmChargerType_Unconnected;
                psmGetChargerType(&chargerType);
                if (chargerType != PsmChargerType_Unconnected) {
                    u32 batteryCharge = 0;
                    psmGetBatteryChargePercentage(&batteryCharge);
                    if (batteryCharge >= 100) {
                        // 100% заряд батареи: ровный тусклый свет
                        setLiteAnimation(LiteAnimMode_Solid, LITE_DIM_BRIGHTNESS);
                    } else {
                        // Идет процесс зарядки: мягкое синусоидальное «дыхание»
                        setLiteAnimation(LiteAnimMode_Breathe, LITE_DIM_BRIGHTNESS);
                    }
                } else {
                    // Базовое состояние: без зарядки диод полностью выключен
                    setLiteAnimation(LiteAnimMode_Off, 0);
                }
            }
        } else if (selectedMode == SelectedMode_Solid) {
            setLiteAnimation(LiteAnimMode_Solid, LITE_FULL_BRIGHTNESS);
        } else if (selectedMode == SelectedMode_Dim) {
            setLiteAnimation(LiteAnimMode_Solid, LITE_DIM_BRIGHTNESS);
        } else if (selectedMode == SelectedMode_Fade) {
            setLiteAnimation(LiteAnimMode_Breathe, LITE_FULL_BRIGHTNESS);
        } else {
            setLiteAnimation(LiteAnimMode_Off, 0);
        }

        svcSleepThread(100000000ULL); // 100 ms
    }

    return 0;
}
