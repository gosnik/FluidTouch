#ifndef QTDIAL_HID_PROTOCOL_H
#define QTDIAL_HID_PROTOCOL_H

#include <cstddef>
#include <cstdint>

namespace QtdialHidProtocol {

// TODO: Replace with assigned VID/PID once available.
constexpr uint16_t kUsbVendorId = 0x303A;  // Espressif default VID (dev placeholder)
constexpr uint16_t kUsbProductId = 0xF1D1; // qtdial dev PID placeholder

// HID report ID used by the USBHIDVendor descriptor.
constexpr uint8_t kHidReportId = 6; // HID_REPORT_ID_VENDOR

// App-level report IDs (byte 0 of payload).
constexpr uint8_t kAppReportInputStatus = 0x01;
constexpr uint8_t kAppReportOutputDisplay = 0x02;
constexpr uint8_t kAppReportFeatureConfig = 0x03;
constexpr uint8_t kAppReportOutputStatus = 0x04;

constexpr uint8_t kProtocolVersion = 1;
constexpr size_t kReportSizeBytes = 63; // USBHIDVendor payload size (bytes)

// Input status report layout (device -> host), little-endian fields:
// [0]  app_report_id = kAppReportInputStatus
// [1]  protocol_version
// [2]  flags (bit0=encoder_ready, bit1=has_delta, bit2=has_rate)
// [3]  buttons bitfield (bit0=btn0, bit1=btn1, bit2=btn2, bit3=btn3)
// [4]  delta_detents_le (int16)
// [6]  rate_detents_per_s_le (int16)
// [8]  uptime_ms_le (uint32)
// [12] role_id (uint8)
// [13] reserved
// [14] seq_le (uint16)

// Output display report layout (host -> device), little-endian fields:
// [0]  app_report_id = kAppReportOutputDisplay
// [1]  protocol_version
// [2]  command
// [3]  flags (bit0=clear_before_load)
// [4]  key_len
// [5]  val_len
// [6]  key bytes (UTF-8)
// [6+key_len] value bytes (UTF-8)
// Commands:
//  0x01 select_screen: key=xml screen name, val_len=0
//  0x02 set_field: key=field name, value=text
//  0x03 clear_fields: key_len=0, val_len=0
//  0x04 set_brightness: val_len=1, value=brightness (0-255)

constexpr uint8_t kDisplayCmdSelectScreen = 0x01;
constexpr uint8_t kDisplayCmdSetField = 0x02;
constexpr uint8_t kDisplayCmdClearFields = 0x03;
constexpr uint8_t kDisplayCmdSetBrightness = 0x04;

constexpr uint8_t kDisplayFlagClearBeforeLoad = 0x01;

// Role IDs for feature/config report.
constexpr uint8_t kRoleX = 0;
constexpr uint8_t kRoleY = 1;
constexpr uint8_t kRoleZ = 2;

// Feature/config report layout (host -> device):
// [0]  app_report_id = kAppReportFeatureConfig
// [1]  protocol_version
// [2]  role_id (0-15)
// [3]  sensitivity (1-10)
// [4]  detent_div (1-8)
// [5]  report_interval_ms (0=event-driven, otherwise periodic/10ms units)

// CNC status report layout (host -> device):
// [0]  app_report_id = kAppReportOutputStatus
// [1]  protocol_version
// [2]  state (MachineState enum value)
// [3]  flags (bit0=connected, bit1=has_wpos, bit2=has_mpos)
// [4]  feed_rate_x100 (int32)
// [8]  spindle_speed (int32)
// [12] wpos_x_x1000 (int32)
// [16] wpos_y_x1000 (int32)
// [20] wpos_z_x1000 (int32)
// [24] mpos_x_x1000 (int32)
// [28] mpos_y_x1000 (int32)
// [32] mpos_z_x1000 (int32)

constexpr int32_t kStatusFeedScale = 100;   // feed_rate = value / 100.0
constexpr int32_t kStatusPosScale = 1000;   // positions = value / 1000.0

} // namespace QtdialHidProtocol

#endif // QTDIAL_HID_PROTOCOL_H
