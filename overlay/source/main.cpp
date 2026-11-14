#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <string>
#include <fstream>

class GuiCyber : public tsl::Gui {
public:
    std::string modeString = "custom";
    GuiCyber() { }

    void loadMode() {
        fsdevMountSdmc();
        std::ifstream tf("sdmc:/config/lite-led/type");
        if (tf.good())
            std::getline(tf, modeString);
        else
            modeString = "custom";
    }

    void applyMode(const std::string& newMode) {
        fsdevMountSdmc();
        std::ofstream("sdmc:/config/lite-led/reset");
        std::ofstream("sdmc:/config/lite-led/type") << newMode;
        loadMode();
        tsl::changeTo<GuiCyber>();
    }

    virtual tsl::elm::Element* createUI() override {
        loadMode();
        auto frame = new tsl::elm::OverlayFrame("LITE-LED+", "v1.1.0 // CYBERDECK");
        auto list = new tsl::elm::List();

        // 1. АВТОМАТИЧЕСКИЙ ПРОФИЛЬ
        list->addItem(new tsl::elm::CategoryHeader("SYSTEM PROFILE", true));

        std::string smartBadge = (modeString == "custom" || modeString.empty()) ? "[ACTIVE]" : "AUTO";
        auto smartItem = new tsl::elm::ListItem("Smart Daemon", smartBadge);
        smartItem->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                applyMode("custom");
                return true;
            }
            return false;
        });
        list->addItem(smartItem);

        // 2. РУЧНОЕ УПРАВЛЕНИЕ
        list->addItem(new tsl::elm::CategoryHeader("MANUAL OVERRIDE", true));

        std::string solidBadge = (modeString == "solid") ? "[ACTIVE]" : "100%";
        auto solidItem = new tsl::elm::ListItem("Solid High", solidBadge);
        solidItem->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                applyMode("solid");
                return true;
            }
            return false;
        });
        list->addItem(solidItem);

        std::string dimBadge = (modeString == "dim") ? "[ACTIVE]" : "33%";
        auto dimItem = new tsl::elm::ListItem("Solid Low", dimBadge);
        dimItem->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                applyMode("dim");
                return true;
            }
            return false;
        });
        list->addItem(dimItem);

        std::string fadeBadge = (modeString == "fade") ? "[ACTIVE]" : "PULSE";
        auto fadeItem = new tsl::elm::ListItem("Breathing", fadeBadge);
        fadeItem->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                applyMode("fade");
                return true;
            }
            return false;
        });
        list->addItem(fadeItem);

        std::string offBadge = (modeString == "off") ? "[ACTIVE]" : "OFF";
        auto offItem = new tsl::elm::ListItem("LED Output", offBadge);
        offItem->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                applyMode("off");
                return true;
            }
            return false;
        });
        list->addItem(offItem);

        // 3. ТЕЛЕМЕТРИЯ (БЕЗОПАСНАЯ ШИРИНА)
        list->addItem(new tsl::elm::CategoryHeader("TELEMETRY", true));

        auto telemetryDrawer = new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            std::string line1 = "SYS: ONLINE // HW_GPIO: 0x65";
            std::string line2 = "DAEMON: " + (modeString == "custom" ? "SMART_MODE.HEX" : modeString);
            renderer->drawString(line1.c_str(), false, x + 8, y + 14, 15, renderer->a(0x8FFF));
            renderer->drawString(line2.c_str(), false, x + 8, y + 36, 17, renderer->a(0xFFFF));
        });
        list->addItem(telemetryDrawer, 60);

        frame->setContent(list);
        return frame;
    }

    virtual void update() override { }

    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;
    }
};

class OverlayCyber : public tsl::Overlay {
public:
    virtual void initServices() override {
        fsdevMountSdmc();
    }

    virtual void exitServices() override {
        fsdevUnmountDevice("sdmc");
    }

    virtual void onShow() override {}
    virtual void onHide() override {}

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<GuiCyber>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<OverlayCyber>(argc, argv);
}
