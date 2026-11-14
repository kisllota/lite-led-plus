#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <string>
#include <fstream>

class GuiTest : public tsl::Gui {
public:
    std::string modeString = "unknown";
    GuiTest() { }
    void loadMode() {
        fsdevMountSdmc();
        std::ifstream tf("sdmc:/config/lite-led/type");
        if (tf.good())
            std::getline(tf, modeString);
        else
            modeString = "(not found)";
    }

    virtual tsl::elm::Element* createUI() override {
        loadMode();
        auto frame = new tsl::elm::OverlayFrame("lite-led", "1.0.0-custom");
        auto list = new tsl::elm::List();

        // ===== CUSTOM SMART PROFILE =====
        auto customItem = new tsl::elm::ListItem("Smart Mode (Auto)");
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

        // ===== SOLID =====
        auto solidItem = new tsl::elm::ListItem("Set LED to Solid");
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

        // ===== DIM =====
        auto dimItem = new tsl::elm::ListItem("Set LED to Dim");
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
        
        // ===== FADE =====
        auto fadeItem = new tsl::elm::ListItem("Set LED to Fade");
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

        // ===== OFF =====
        auto offItem = new tsl::elm::ListItem("Set LED to Off");
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

        auto modeDrawer = new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            std::string line = "Current mode: " + modeString;
            renderer->drawString(line.c_str(), false, x + 3, y + 30, 20, renderer->a(0xFFFF));
        });
        list->addItem(modeDrawer, 50);
        frame->setContent(list);
        return frame;
    }

    virtual void update() override { }

    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override {
        return false;
    }
};

class OverlayTest : public tsl::Overlay {
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
        return initially<GuiTest>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<OverlayTest>(argc, argv);
}
