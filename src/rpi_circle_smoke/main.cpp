#include <circle/cputhrottle.h>
#include <circle/input/rpitouchscreen.h>
#include <circle/startup.h>
#include <circle_stdlib_app.h>
#include <lvgl/lvgl.h>

namespace {

static const char KernelName[] = "fluidtouch-smoke";

class SmokeKernel : public CStdlibAppStdio {
  public:
    SmokeKernel()
        : CStdlibAppStdio(KernelName),
          gui_(&mScreen),
          cpu_throttle_(CPUSpeedMaximum) {
        mActLED.Blink(3, 250, 250);
    }

    bool Initialize() override {
        if (!CStdlibAppStdio::Initialize()) {
            return false;
        }

        touch_screen_.Initialize();

        if (!gui_.Initialize()) {
            mLogger.Write(GetKernelName(), LogError, "LVGL initialization failed");
            return false;
        }

        renderSmokeScreen();
        mLogger.Write(GetKernelName(), LogNotice, "Smoke test screen ready");
        return true;
    }

    TShutdownMode Run() override {
        while (1) {
            const bool usb_changed = mUSBHCI.UpdatePlugAndPlay();
            gui_.Update(usb_changed);
            cpu_throttle_.Update();
        }

        return ShutdownHalt;
    }

  private:
    void renderSmokeScreen() {
        lv_obj_t *screen = lv_screen_active();
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x0C1B2A), 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

        lv_obj_t *title = lv_label_create(screen);
        lv_label_set_text(title, "FluidTouch");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xF6F7EB), 0);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -24);

        lv_obj_t *subtitle = lv_label_create(screen);
        lv_label_set_text(subtitle, "rpi_circle_smoke");
        lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(subtitle, lv_color_hex(0x7FDBFF), 0);
        lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 22);

        lv_refr_now(lv_display_get_default());
    }

    CRPiTouchScreen touch_screen_;
    CLVGL gui_;
    CCPUThrottle cpu_throttle_;
};

}  // namespace

int main(void) {
    SmokeKernel kernel;
    if (!kernel.Initialize()) {
        halt();
        return EXIT_HALT;
    }

    CStdlibApp::TShutdownMode shutdown_mode = kernel.Run();
    kernel.Cleanup();

    switch (shutdown_mode) {
        case CStdlibApp::ShutdownReboot:
            reboot();
            return EXIT_REBOOT;

        case CStdlibApp::ShutdownHalt:
        default:
            halt();
            return EXIT_HALT;
    }
}
