#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <string>
#include <fstream>

class GuiCyber : public tsl::Gui {
public:
    std::string modeString = "unknown";
    GuiCyber() { }

    void loadMode() {
        fsdevMountSdmc();
        std::ifstream tf("sdmc:/config/lite-led/type");
        if (tf.good())
            std::getline(tf, modeString);
        else
            modeString = "NULL_PTR";
    }

    std::string getProtocolAlias(const std::string& raw) {
        if (raw == "custom")  return "SMART_DAEMON.HEX [SYNCED]";
        if (raw == "solid")   return "OVERRIDE_HIGH.RAW [100%]";
        if (raw == "dim")     return "OVERRIDE_LOW.RAW  [33%]";
        if (raw == "fade")    return "OSCILLATOR.WAV    [PULSE]";
        if (raw == "off")     return "KILLSWITCH.EXE    [DARK]";
        return raw;
    }

    virtual tsl::elm::Element* createUI() override {
        loadMode();
        auto frame = new tsl::elm::OverlayFrame("LITE-LED+", "REV.2077 // DAEMON");
        auto list = new tsl::elm::List();

        // 0x01 SMART DAEMON
        auto customItem = new tsl::elm::ListItem("0x01 // SMART_DAEMON (AUTO)");
        customItem->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                fsdevMountSdmc();
                std::ofstream("sdmc:/config/lite-led/reset");
                std::ofstream("sdmc:/config/lite-led/type") << "custom";
                loadMode();
                return true;
            }
            return false;
        });
        list->addItem(customItem);

        // 0x02 SOLID HIGH
        auto solidItem = new tsl::elm::ListItem("0x02 // OVERRIDE_HIGH [100%]");
        solidItem->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                fsdevMountSdmc();
                std::ofstream("sdmc:/config/lite-led/reset");
                std::ofstream("sdmc:/config/lite-led/type") << "solid";
                loadMode();
                return true;
            }
            return false;
        });
        list->addItem(solidItem);

        // 0x03 DIM LOW
        auto dimItem = new tsl::elm::ListItem("0x03 // OVERRIDE_LOW  [33%]");
        dimItem->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                fsdevMountSdmc();
                std::ofstream("sdmc:/config/lite-led/reset");
                std::ofstream("sdmc:/config/lite-led/type") << "dim";
                loadMode();
                return true;
            }
            return false;
        });
        list->addItem(dimItem);
        
        // 0x04 FADE OSCILLATE
        auto fadeItem = new tsl::elm::ListItem("0x04 // OSCILLATE_BREATHE");
        fadeItem->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                fsdevMountSdmc();
                std::ofstream("sdmc:/config/lite-led/reset");
                std::ofstream("sdmc:/config/lite-led/type") << "fade";
                loadMode();
                return true;
            }
            return false;
        });
        list->addItem(fadeItem);

        // 0x00 KILLSWITCH
        auto offItem = new tsl::elm::ListItem("0x00 // KILLSWITCH_OFF");
        offItem->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                fsdevMountSdmc();
                std::ofstream("sdmc:/config/lite-led/reset");
                std::ofstream("sdmc:/config/lite-led/type") << "off";
                loadMode();
                return true;
            }
            return false;
        });
        list->addItem(offItem);

        // Cyberpunk terminal status telemetry
        auto modeDrawer = new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            std::string line1 = "// ICE_STATUS: BYPASSED | HW_GPIO: 0x35000065";
            std::string line2 = ">> ACTIVE: " + getProtocolAlias(modeString);
            renderer->drawString(line1.c_str(), false, x + 3, y + 16, 15, renderer->a(0x7FFF));
            renderer->drawString(line2.c_str(), false, x + 3, y + 42, 17, renderer->a(0xFFFF));
        });
        list->addItem(modeDrawer, 70);

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
