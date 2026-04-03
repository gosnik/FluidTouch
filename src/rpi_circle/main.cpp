#include <circle/cputhrottle.h>
#include <circle/input/rpitouchscreen.h>
#include <circle/serial.h>
#include <circle/startup.h>
#include <circle_stdlib_app.h>
#include <exception>
#include <lvgl/lvgl.h>

#include "core/display_driver.h"
#include "env/platform.h"
#include "env/runtime.h"
#include "ui/ui_common.h"

namespace {

static const char KernelName[] = "fluidtouch";

enum class BootStage {
  StdlibInit = 1,
  TouchInit = 2,
  LvglInit = 3,
  AppInit = 4,
  Running = 5,
  InitFailed = 10,
};

void signalStage(CActLED &led, BootStage stage) {
  led.Blink(static_cast<unsigned>(stage), 220, 600);
  CTimer::SimpleMsDelay(1000);
}

void renderPreAppSmokeScreen(lv_display_t *display) {
  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  lv_obj_t *label = lv_label_create(screen);
  lv_label_set_text(label, "FluidTouch\npre-app smoke test");
  lv_obj_set_style_text_color(label, lv_color_hex(0xF5F7FA), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
  lv_obj_center(label);

  lv_refr_now(display);
}

void renderFatalScreen(lv_display_t *display, const char *title,
                       const char *detail) {
  if (!display) {
    return;
  }

  lv_obj_t *screen = lv_screen_active();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x1F0A0A), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  lv_obj_t *title_label = lv_label_create(screen);
  lv_label_set_text(title_label, title ? title : "FluidTouch fatal error");
  lv_obj_set_style_text_color(title_label, lv_color_hex(0xFEE2E2), 0);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_26, 0);
  lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 64);

  lv_obj_t *detail_label = lv_label_create(screen);
  lv_label_set_long_mode(detail_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(detail_label, 720);
  lv_label_set_text(detail_label,
                    detail && detail[0] != '\0' ? detail : "unknown");
  lv_obj_set_style_text_color(detail_label, lv_color_hex(0xFECACA), 0);
  lv_obj_set_style_text_font(detail_label, &lv_font_montserrat_18, 0);
  lv_obj_align(detail_label, LV_ALIGN_CENTER, 0, 12);

  lv_refr_now(display);
}

class FluidTouchCircleKernel : public CStdlibAppStdio {
public:
  FluidTouchCircleKernel()
      : CStdlibAppStdio(KernelName), display_driver_(), gui_(&mScreen),
        cpu_throttle_(CPUSpeedMaximum) {}

  bool Initialize() override {
    if (!CStdlibAppStdio::Initialize()) {
      signalStage(mActLED, BootStage::InitFailed);
      return false;
    }

    touch_screen_.Initialize();

    if (!gui_.Initialize()) {
      mLogger.Write(GetKernelName(), LogError, "LVGL initialization failed");
      return false;
    }

    display_driver_.attachScreen(&mScreen);
    if (!display_driver_.init()) {
      mLogger.Write(GetKernelName(), LogError,
                    "Display driver initialization failed");
      return false;
    }
    UICommon::setDisplayDriver(&display_driver_);

    try {
      EnvRuntime::initApp(lv_display_get_default());
    } catch (const std::exception &ex) {
      renderFatalScreen(lv_display_get_default(), "Init exception", ex.what());
      mLogger.Write(GetKernelName(), LogPanic, "Init exception: %s", ex.what());
      return false;
    } catch (...) {
      renderFatalScreen(lv_display_get_default(), "Init exception",
                        "unknown exception");
      mLogger.Write(GetKernelName(), LogPanic, "Init exception: unknown");
      return false;
    }
    mLogger.Write(GetKernelName(), LogNotice,
                  "FluidTouch shared UI initialized");
    return true;
  }

  void Cleanup() override { CStdlibAppStdio::Cleanup(); }

  TShutdownMode Run() override {
    signalStage(mActLED, BootStage::Running);

    while (!EnvPlatform::isExitRequested()) {
      try {
        const bool usb_changed = mUSBHCI.UpdatePlugAndPlay();
        mUSBHCI.PollEvents();
        EnvRuntime::tickUi();
        gui_.Update(usb_changed);
        cpu_throttle_.Update();
      } catch (const std::exception &ex) {
        renderFatalScreen(lv_display_get_default(), "Runtime exception",
                          ex.what());
        mLogger.Write(GetKernelName(), LogPanic, "Runtime exception: %s",
                      ex.what());
        for (;;) {
          cpu_throttle_.Update();
        }
      } catch (...) {
        renderFatalScreen(lv_display_get_default(), "Runtime exception",
                          "unknown exception");
        mLogger.Write(GetKernelName(), LogPanic, "Runtime exception: unknown");
        for (;;) {
          cpu_throttle_.Update();
        }
      }
    }

    return ShutdownHalt;
  }

private:
  CRPiTouchScreen touch_screen_;
  DisplayDriver display_driver_;
  CLVGL gui_;
  CCPUThrottle cpu_throttle_;
};

} // namespace

int main() {
  FluidTouchCircleKernel kernel;
  if (!kernel.Initialize()) {
    if (CActLED::Get()) {
      signalStage(*CActLED::Get(), BootStage::InitFailed);
    }
    halt();
    return EXIT_HALT;
  }

  switch (kernel.Run()) {
  case CStdlibApp::ShutdownReboot:
    reboot();
    return EXIT_REBOOT;
  case CStdlibApp::ShutdownHalt:
  case CStdlibApp::ShutdownNone:
  default:
    halt();
    return EXIT_HALT;
  }
}
