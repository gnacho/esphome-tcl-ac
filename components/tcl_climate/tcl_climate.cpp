#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "tcl_climate.h"
#include <map>

namespace esphome {
namespace tcl_climate {

static constexpr uint8_t REQ_CMD[] = {0xBB, 0x00, 0x01, 0x04, 0x02, 0x01, 0x00, 0xBD};
static constexpr int MAX_LINE_LENGTH = 100;
static constexpr int UPDATE_INTERVAL_MS = 450;

void TCLClimate::set_current_temperature(float current_temperature) {
  if (std::abs(this->current_temperature - current_temperature) < 0.1f) return;
  ESP_LOGD("TCL", "Current temperature updated to: %.1f°C", current_temperature);
  this->is_changed = true;
  this->current_temperature = current_temperature;
}

void TCLClimate::set_custom_fan_mode(StringRef fan_mode) {
  StringRef current(this->get_custom_fan_mode());
  if (!current.empty() && fan_mode == current.c_str()) {
    return;
  }
  ESP_LOGI("TCL", "Fan mode changed to: %s", fan_mode.c_str());
  this->is_changed = true;
  this->set_custom_fan_mode_(fan_mode.c_str());
}

void TCLClimate::set_mode(climate::ClimateMode mode) {
  if (this->mode == mode) return;
  const char* mode_str = "";
  switch (mode) {
    case climate::CLIMATE_MODE_OFF: mode_str = "OFF"; break;
    case climate::CLIMATE_MODE_COOL: mode_str = "COOL"; break;
    case climate::CLIMATE_MODE_HEAT: mode_str = "HEAT"; break;
    case climate::CLIMATE_MODE_FAN_ONLY: mode_str = "FAN ONLY"; break;
    case climate::CLIMATE_MODE_DRY: mode_str = "DRY"; break;
    case climate::CLIMATE_MODE_AUTO: mode_str = "AUTO"; break;
    default: mode_str = "UNKNOWN"; break;
  }
  ESP_LOGI("TCL", "Climate mode changed to: %s", mode_str);
  this->is_changed = true;
  this->mode = mode;
}

void TCLClimate::set_swing_mode(climate::ClimateSwingMode swing_mode) {
  if (this->swing_mode == swing_mode) return;
  const char* swing_str = "";
  switch (swing_mode) {
    case climate::CLIMATE_SWING_OFF: swing_str = "OFF"; break;
    case climate::CLIMATE_SWING_BOTH: swing_str = "BOTH"; break;
    case climate::CLIMATE_SWING_VERTICAL: swing_str = "VERTICAL"; break;
    case climate::CLIMATE_SWING_HORIZONTAL: swing_str = "HORIZONTAL"; break;
    default: swing_str = "UNKNOWN"; break;
  }
  ESP_LOGI("TCL", "Swing mode changed to: %s", swing_str);
  this->is_changed = true;
  this->swing_mode = swing_mode;
}

void TCLClimate::set_hswing_pos(const std::string &hswing_pos) {
  if (this->hswing_pos == hswing_pos) return;
  ESP_LOGI("TCL", "Horizontal swing position: %s", hswing_pos.c_str());
  this->hswing_pos = hswing_pos;
}

void TCLClimate::set_vswing_pos(const std::string &vswing_pos) {
  if (this->vswing_pos == vswing_pos) return;
  ESP_LOGI("TCL", "Vertical swing position: %s", vswing_pos.c_str());
  this->vswing_pos = vswing_pos;
}

void TCLClimate::set_target_temperature(float target_temperature) {
  if (std::abs(this->target_temperature - target_temperature) < 0.1f) return;
  ESP_LOGI("TCL", "Target temperature changed to: %.1f°C", target_temperature);
  this->is_changed = true;
  this->target_temperature = target_temperature;
}

climate::ClimateMode TCLClimate::mode_from_raw_(uint8_t mode_raw) {
  switch (mode_raw) {
    case 0x01: return climate::CLIMATE_MODE_COOL;
    case 0x02: return climate::CLIMATE_MODE_FAN_ONLY;
    case 0x03: return climate::CLIMATE_MODE_DRY;
    case 0x04: return climate::CLIMATE_MODE_HEAT;
    case 0x05: return climate::CLIMATE_MODE_AUTO;
    default: return climate::CLIMATE_MODE_COOL;
  }
}

climate::ClimateFanMode TCLClimate::fan_from_raw_(uint8_t fan_raw) {
  switch (fan_raw) {
    case 0x01: return climate::CLIMATE_FAN_LOW;
    case 0x02: return climate::CLIMATE_FAN_MEDIUM;
    case 0x03: return climate::CLIMATE_FAN_HIGH;
    default: return climate::CLIMATE_FAN_AUTO;
  }
}

climate::ClimatePreset TCLClimate::preset_from_code_(uint8_t preset_code) {
  switch (preset_code) {
    case 1: return climate::CLIMATE_PRESET_ECO;
    case 2: return climate::CLIMATE_PRESET_SLEEP;
    case 3: return climate::CLIMATE_PRESET_BOOST;
    default: return climate::CLIMATE_PRESET_NONE;
  }
}

void TCLClimate::load_last_state_() {
  this->last_state_pref_ = global_preferences->make_preference<uint32_t>(0xB105F00D);
  uint32_t encoded = 0;
  if (this->last_state_pref_.load(&encoded)) {
    last_state_.valid = (encoded & 0x80000000UL) != 0;
    last_state_.mode_raw = encoded & 0xFF;
    last_state_.temp_raw = (encoded >> 8) & 0xFF;
    last_state_.fan_raw = (encoded >> 16) & 0xFF;
    last_state_.preset_code = (encoded >> 24) & 0x7F;
    if (last_state_.mode_raw == 0 || last_state_.mode_raw > 0x05) last_state_.valid = false;
    if (last_state_.preset_code > 3) last_state_.valid = false;
  } else {
    last_state_.valid = false;
  }
}

void TCLClimate::save_last_state_(const get_cmd_resp_t &state) {
  last_state_.mode_raw = state.data.mode;
  last_state_.temp_raw = state.data.temp;
  last_state_.fan_raw = state.data.fan;
  if (this->preset.has_value() && this->preset.value() == climate::CLIMATE_PRESET_ECO) {
    last_state_.preset_code = 1;
  } else if (sleep_mode_ != 0x00) {
    last_state_.preset_code = 2;
  } else if (this->preset.has_value() && this->preset.value() == climate::CLIMATE_PRESET_BOOST) {
    last_state_.preset_code = 3;
  } else {
    last_state_.preset_code = 0;
  }
  last_state_.valid = true;
  uint32_t encoded = (static_cast<uint32_t>(last_state_.mode_raw) & 0xFF) |
                     ((static_cast<uint32_t>(last_state_.temp_raw) & 0xFF) << 8) |
                     ((static_cast<uint32_t>(last_state_.fan_raw) & 0xFF) << 16) |
                     ((static_cast<uint32_t>(last_state_.preset_code) & 0x7F) << 24) |
                     0x80000000UL;
  this->last_state_pref_.save(&encoded);
  ESP_LOGI("TCL", "Saved last active state: mode_raw=0x%02X temp_raw=%u fan_raw=%u preset=%u",
           last_state_.mode_raw, last_state_.temp_raw, last_state_.fan_raw, last_state_.preset_code);
}

void TCLClimate::restore_last_state_(get_cmd_resp_t *state) {
  if (!last_state_.valid) return;
  state->data.mode = last_state_.mode_raw;
  state->data.temp = last_state_.temp_raw;
  state->data.fan = last_state_.fan_raw;
  state->data.eco = (last_state_.preset_code == 1);
  state->data.turbo = (last_state_.preset_code == 3);
  sleep_mode_ = (last_state_.preset_code == 2) ? 0x01 : 0x00;

  this->mode = mode_from_raw_(last_state_.mode_raw);
  this->target_temperature = static_cast<float>(last_state_.temp_raw + 16);
  this->fan_mode = fan_from_raw_(last_state_.fan_raw);
  this->preset = preset_from_code_(last_state_.preset_code);
  this->is_changed = true;
  ESP_LOGI("TCL", "Restored last active state: mode=%d temp=%.0f fan=%d preset=%d",
           static_cast<int>(this->mode), this->target_temperature,
           static_cast<int>(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)), last_state_.preset_code);
}

void TCLClimate::build_set_cmd(get_cmd_resp_t *get_cmd_resp) {
    memcpy(m_set_cmd.raw, set_cmd_base, sizeof(m_set_cmd.raw));

    m_set_cmd.data.power = get_cmd_resp->data.power;
    m_set_cmd.data.off_timer_en = 0;
    m_set_cmd.data.on_timer_en = 0;
    m_set_cmd.data.beep = beep_ ? 1 : 0;
    m_set_cmd.data.disp = 1;
    m_set_cmd.data.eco = get_cmd_resp->data.eco;
    m_set_cmd.data.turbo = get_cmd_resp->data.turbo;
    m_set_cmd.data.mute = get_cmd_resp->data.mute;
    m_set_cmd.data.sleep_mode = sleep_mode_;

    // Map internal raw mode -> command mode byte, aligned with the reference
    // TCL protocol (cool=0x03, dry=0x02, fan=0x07, heat=0x01, auto=0x08).
    static constexpr uint8_t MODE_MAP[] = {
        0x00,  // 0 (unused)
        0x03,  // COOL
        0x07,  // FAN_ONLY
        0x02,  // DRY
        0x01,  // HEAT
        0x08   // AUTO
    };

    if (get_cmd_resp->data.mode < sizeof(MODE_MAP)) {
        m_set_cmd.data.mode = MODE_MAP[get_cmd_resp->data.mode];
    }

    m_set_cmd.data.temp = 15 - get_cmd_resp->data.temp;

    static constexpr uint8_t FAN_MAP[] = {
        0x00,
        0x02,
        0x03,
        0x05,
        0x06,
        0x07
    };

    if (get_cmd_resp->data.fan < sizeof(FAN_MAP)) {
        m_set_cmd.data.fan = FAN_MAP[get_cmd_resp->data.fan];
    }

    // Swing on/off: the GET response reports vertical in byte10 bit6 and
    // horizontal in byte10 bit5 (confirmed by sniffing the UART bus while
    // toggling the physical remote, 17-Jul-2026). The SET frame carries
    // vertical in byte10 bits3-5 (0x07 = on) and horizontal in byte11 bit3.
    // This unit never reports fix/move positions (bytes 51/52 stay 0xFF),
    // so plain on/off is the correct mapping for normal commands. Explicit
    // position commands from the selects override these fields afterwards.
    m_set_cmd.data.vswing = get_cmd_resp->data.vswing ? 0x07 : 0x00;
    m_set_cmd.data.vswing_fix = 0;
    m_set_cmd.data.vswing_mv = 0;
    m_set_cmd.data.hswing = get_cmd_resp->data.hswing ? 0x01 : 0x00;
    m_set_cmd.data.hswing_fix = 0;
    m_set_cmd.data.hswing_mv = 0;

    m_set_cmd.data.half_degree = 0;

    finalize_set_cmd_();
}

void TCLClimate::finalize_set_cmd_() {
    uint8_t xor_byte = 0;
    for (size_t i = 0; i < sizeof(m_set_cmd.raw) - 1; i++) {
        xor_byte ^= m_set_cmd.raw[i];
    }
    m_set_cmd.raw[sizeof(m_set_cmd.raw) - 1] = xor_byte;
}

void TCLClimate::setup() {
  set_update_interval(UPDATE_INTERVAL_MS);
  load_last_state_();
}

void TCLClimate::control_vertical_swing(const std::string &swing_mode) {
  get_cmd_resp_t get_cmd_resp = {0};
  memcpy(get_cmd_resp.raw, m_get_cmd_resp.raw, sizeof(get_cmd_resp.raw));

  build_set_cmd(&get_cmd_resp);

  uint8_t vswing = 0, fix = 0, mv = 0;
  if (swing_mode == "Move full") { vswing = 0x07; mv = 0x01; }
  else if (swing_mode == "Move upper") { vswing = 0x07; mv = 0x02; }
  else if (swing_mode == "Move lower") { vswing = 0x07; mv = 0x03; }
  else if (swing_mode == "Fix top") { fix = 0x01; }
  else if (swing_mode == "Fix upper") { fix = 0x02; }
  else if (swing_mode == "Fix mid") { fix = 0x03; }
  else if (swing_mode == "Fix lower") { fix = 0x04; }
  else if (swing_mode == "Fix bottom") { fix = 0x05; }

  m_set_cmd.data.vswing = vswing;
  m_set_cmd.data.vswing_fix = fix;
  m_set_cmd.data.vswing_mv = mv;
  finalize_set_cmd_();
  ready_to_send_set_cmd_flag = true;
}

void TCLClimate::control_horizontal_swing(const std::string &swing_mode) {
  get_cmd_resp_t get_cmd_resp = {0};
  memcpy(get_cmd_resp.raw, m_get_cmd_resp.raw, sizeof(get_cmd_resp.raw));

  build_set_cmd(&get_cmd_resp);

  uint8_t hswing = 0, fix = 0, mv = 0;
  if (swing_mode == "Move full") { hswing = 0x01; mv = 0x01; }
  else if (swing_mode == "Move left") { hswing = 0x01; mv = 0x02; }
  else if (swing_mode == "Move mid") { hswing = 0x01; mv = 0x03; }
  else if (swing_mode == "Move right") { hswing = 0x01; mv = 0x04; }
  else if (swing_mode == "Fix left") { fix = 0x01; }
  else if (swing_mode == "Fix mid left") { fix = 0x02; }
  else if (swing_mode == "Fix mid") { fix = 0x03; }
  else if (swing_mode == "Fix mid right") { fix = 0x04; }
  else if (swing_mode == "Fix right") { fix = 0x05; }

  m_set_cmd.data.hswing = hswing;
  m_set_cmd.data.hswing_fix = fix;
  m_set_cmd.data.hswing_mv = mv;
  finalize_set_cmd_();
  ready_to_send_set_cmd_flag = true;
}

void TCLClimate::set_mute(bool mute) {
  get_cmd_resp_t gcr = {0};
  memcpy(gcr.raw, m_get_cmd_resp.raw, sizeof(gcr.raw));
  if (mute) {
    gcr.data.fan = 0x01;
    gcr.data.mute = 0x01;
  } else {
    gcr.data.mute = 0x00;
  }
  build_set_cmd(&gcr);
  ready_to_send_set_cmd_flag = true;
}

void TCLClimate::set_beep(bool beep) {
  get_cmd_resp_t gcr = {0};
  memcpy(gcr.raw, m_get_cmd_resp.raw, sizeof(gcr.raw));
  build_set_cmd(&gcr);

  uint8_t mask = 0x20;
  if (beep) {
    m_set_cmd.raw[7] |= mask;
  } else {
    m_set_cmd.raw[7] &= ~mask;
  }

  finalize_set_cmd_();

  ready_to_send_set_cmd_flag = true;
  beep_ = beep;
  ESP_LOGI("TCL", "Beep set to: %s", beep ? "ON" : "OFF");
}

void TCLClimate::control(const climate::ClimateCall &call) {
    get_cmd_resp_t get_cmd_resp = {0};
    memcpy(get_cmd_resp.raw, m_get_cmd_resp.raw, sizeof(get_cmd_resp.raw));
    bool should_build_cmd = false;
    bool restored = false;

    if (call.get_mode().has_value()) {
        climate::ClimateMode climate_mode = *call.get_mode();
        ESP_LOGI("TCL", "Received mode control command: %d", static_cast<int>(climate_mode));

        if (climate_mode == climate::CLIMATE_MODE_OFF) {
            if (this->mode != climate::CLIMATE_MODE_OFF) {
                save_last_state_(get_cmd_resp);
            }
            get_cmd_resp.data.power = 0x00;
        } else {
            get_cmd_resp.data.power = 0x01;
            if ((this->mode == climate::CLIMATE_MODE_OFF || m_get_cmd_resp.data.power == 0x00) && last_state_.valid) {
                restore_last_state_(&get_cmd_resp);
                restored = true;
            } else {
                switch (climate_mode) {
                    case climate::CLIMATE_MODE_COOL:    get_cmd_resp.data.mode = 0x01; break;
                    case climate::CLIMATE_MODE_DRY:     get_cmd_resp.data.mode = 0x03; break;
                    case climate::CLIMATE_MODE_FAN_ONLY:get_cmd_resp.data.mode = 0x02; break;
                    case climate::CLIMATE_MODE_HEAT:
                    case climate::CLIMATE_MODE_HEAT_COOL:get_cmd_resp.data.mode = 0x04; break;
                    case climate::CLIMATE_MODE_AUTO:    get_cmd_resp.data.mode = 0x05; break;
                    default: break;
                }
            }
        }
        should_build_cmd = true;
    }

    if (call.get_preset().has_value()) {
        climate::ClimatePreset preset = *call.get_preset();
        ESP_LOGI("TCL", "Received preset control command: %d", static_cast<int>(preset));
        get_cmd_resp.data.turbo = 0;
        get_cmd_resp.data.mute = 0;
        get_cmd_resp.data.eco = 0;
        this->eco_ = false;
        sleep_mode_ = 0x00;
        if (preset == climate::CLIMATE_PRESET_ECO) {
            get_cmd_resp.data.eco = 1;
            this->eco_ = true;
        } else if (preset == climate::CLIMATE_PRESET_SLEEP) {
            sleep_mode_ = 0x01;
        } else if (preset == climate::CLIMATE_PRESET_BOOST) {
            get_cmd_resp.data.turbo = 1;
        }
        should_build_cmd = true;
    }

    if (call.get_target_temperature().has_value()) {
        float temp = *call.get_target_temperature();
        ESP_LOGI("TCL", "Received temperature control command: %.1f°C", temp);
        get_cmd_resp.data.temp = static_cast<uint8_t>(temp) - 16;
        should_build_cmd = true;
    }

    if (call.get_swing_mode().has_value()) {
        climate::ClimateSwingMode swing_mode = *call.get_swing_mode();
        switch(swing_mode) {
            case climate::CLIMATE_SWING_OFF:
                get_cmd_resp.data.hswing = 0;
                get_cmd_resp.data.vswing = 0;
                break;
            case climate::CLIMATE_SWING_BOTH:
                get_cmd_resp.data.hswing = 1;
                get_cmd_resp.data.vswing = 1;
                break;
            case climate::CLIMATE_SWING_VERTICAL:
                get_cmd_resp.data.hswing = 0;
                get_cmd_resp.data.vswing = 1;
                break;
            case climate::CLIMATE_SWING_HORIZONTAL:
                get_cmd_resp.data.hswing = 1;
                get_cmd_resp.data.vswing = 0;
                break;
        }
        should_build_cmd = true;
    }

    if (call.get_fan_mode().has_value()) {
        climate::ClimateFanMode fan = *call.get_fan_mode();
        ESP_LOGI("TCL", "Received standard fan mode control command: %d", static_cast<int>(fan));
        get_cmd_resp.data.mute = 0;
        switch (fan) {
            case climate::CLIMATE_FAN_AUTO:   get_cmd_resp.data.fan = 0x00; break;
            case climate::CLIMATE_FAN_LOW:      get_cmd_resp.data.fan = 0x01; break;
            case climate::CLIMATE_FAN_MEDIUM:   get_cmd_resp.data.fan = 0x02; break;
            case climate::CLIMATE_FAN_HIGH:     get_cmd_resp.data.fan = 0x03; break;
            default: break;
        }
        should_build_cmd = true;
    }

    StringRef custom_fan_mode(call.get_custom_fan_mode());
    if (!custom_fan_mode.empty()) {
        std::string fan_mode(custom_fan_mode.c_str());
        ESP_LOGI("TCL", "Received custom fan mode control command: %s", fan_mode.c_str());
        if (fan_mode == "Silencio") {
            get_cmd_resp.data.fan = 0x01;
            get_cmd_resp.data.mute = 0x01;
        }
        should_build_cmd = true;
    }

    if (should_build_cmd) {
        ESP_LOGI("TCL", "Building and sending command to AC unit");
        build_set_cmd(&get_cmd_resp);
        ready_to_send_set_cmd_flag = true;
        if (restored) {
            this->publish_state();
        }
    }
}

climate::ClimateTraits TCLClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_FAN_ONLY,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_AUTO
  });
  traits.set_supported_swing_modes({
    climate::CLIMATE_SWING_OFF,
    climate::CLIMATE_SWING_BOTH,
    climate::CLIMATE_SWING_VERTICAL,
    climate::CLIMATE_SWING_HORIZONTAL
  });
  traits.set_supported_fan_modes({
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH
  });
  traits.set_supported_presets({
    climate::CLIMATE_PRESET_NONE,
    climate::CLIMATE_PRESET_ECO,
    climate::CLIMATE_PRESET_SLEEP,
    climate::CLIMATE_PRESET_BOOST
  });
  traits.set_visual_min_temperature(16.0);
  traits.set_visual_max_temperature(31.0);
  traits.set_visual_target_temperature_step(1.0);
  return traits;
}

void TCLClimate::update() {
    if (ready_to_send_set_cmd_flag) {
        ready_to_send_set_cmd_flag = false;
        ESP_LOGD("TCL", "Sending SET: %s", format_hex_pretty(m_set_cmd.raw, sizeof(m_set_cmd.raw)).c_str());
        write_array(m_set_cmd.raw, sizeof(m_set_cmd.raw));
    } else {
        write_array(REQ_CMD, sizeof(REQ_CMD));
    }
}

int TCLClimate::read_data_line(int readch, uint8_t *buffer, int len) {
    static int pos = 0;
    static bool wait_len = false;
    static int skipch = 0;

    if (readch < 0) return -1;

    if (readch == 0xBB && skipch == 0 && !wait_len) {
        pos = 0;
        skipch = 3;
        wait_len = true;
        if (pos < len) buffer[pos++] = static_cast<uint8_t>(readch);
    } else if (skipch == 0 && wait_len) {
        if (pos < len) buffer[pos++] = static_cast<uint8_t>(readch);
        skipch = readch + 1;
        wait_len = false;
    } else if (skipch > 0) {
        if (pos < len) buffer[pos++] = static_cast<uint8_t>(readch);
        if (--skipch == 0 && !wait_len) return pos;
    }

    return -1;
}

bool TCLClimate::is_valid_xor(uint8_t *buffer, int len) {
    if (len < 1) return false;
    uint8_t xor_byte = 0;
    for (int i = 0; i < len - 1; i++) {
        xor_byte ^= buffer[i];
    }
    return xor_byte == buffer[len - 1];
}

void TCLClimate::print_hex_str(uint8_t *buffer, int len) {
    if (len <= 0) return;
    char str[MAX_LINE_LENGTH * 3] = {0};
    char *pstr = str;
    for (int i = 0; i < len && (pstr - str) < sizeof(str) - 3; i++) {
        pstr += sprintf(pstr, "%02X ", buffer[i]);
    }
    ESP_LOGD("TCL", "Received: %s", str);
}

void TCLClimate::loop() {
    static uint8_t buffer[MAX_LINE_LENGTH];

    while (available()) {
        int len = read_data_line(read(), buffer, MAX_LINE_LENGTH);
        if (len == sizeof(m_get_cmd_resp) && buffer[3] == 0x04) {
            memcpy(m_get_cmd_resp.raw, buffer, len);

            if (is_valid_xor(buffer, len)) {
                print_hex_str(buffer, len);

                float curr_temp = ((static_cast<uint16_t>(buffer[17] << 8) | buffer[18]) / 374.0f - 32.0f) / 1.8f;
                this->is_changed = false;

                if (m_get_cmd_resp.data.power == 0x00) {
                    this->set_mode(climate::CLIMATE_MODE_OFF);
                } else {
                    static const std::map<uint8_t, climate::ClimateMode> MODE_MAP = {
                        {0x01, climate::CLIMATE_MODE_COOL},
                        {0x03, climate::CLIMATE_MODE_DRY},
                        {0x02, climate::CLIMATE_MODE_FAN_ONLY},
                        {0x04, climate::CLIMATE_MODE_HEAT},
                        {0x05, climate::CLIMATE_MODE_AUTO}
                    };
                    auto it = MODE_MAP.find(m_get_cmd_resp.data.mode);
                    if (it != MODE_MAP.end()) {
                        this->set_mode(it->second);
                    }
                }

                if (m_get_cmd_resp.data.mute) {
                  StringRef current_cfan(StringRef(this->get_custom_fan_mode()));
                  if (current_cfan.empty() || current_cfan != "Mute") {
                    this->set_custom_fan_mode(StringRef("Mute"));
                  }
                } else {
                  climate::ClimateFanMode target_fan = climate::CLIMATE_FAN_AUTO;
                  switch (m_get_cmd_resp.data.fan) {
                      case 0x01: target_fan = climate::CLIMATE_FAN_LOW;    break;
                      case 0x02: target_fan = climate::CLIMATE_FAN_MEDIUM; break;
                      case 0x03: target_fan = climate::CLIMATE_FAN_HIGH;   break;
                      default:   target_fan = climate::CLIMATE_FAN_AUTO;   break;
                  }
                  if (!this->fan_mode.has_value() || this->fan_mode.value() != target_fan) {
                    this->fan_mode = target_fan;
                    this->is_changed = true;
                  }
                }

                if (m_get_cmd_resp.data.hswing && m_get_cmd_resp.data.vswing) {
                    this->set_swing_mode(climate::CLIMATE_SWING_BOTH);
                } else if (!m_get_cmd_resp.data.hswing && !m_get_cmd_resp.data.vswing) {
                    this->set_swing_mode(climate::CLIMATE_SWING_OFF);
                } else if (m_get_cmd_resp.data.vswing) {
                    this->set_swing_mode(climate::CLIMATE_SWING_VERTICAL);
                } else if (m_get_cmd_resp.data.hswing) {
                    this->set_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
                }

                // When the AC is not actively reporting a move/fix position it returns
                // 0xFF in these bytes, which the bitfield reads as mv=0x03/fix=0x07.
                // Treat that sentinel as "no specific position reported".
                uint8_t vswing_mv = m_get_cmd_resp.data.vswing_mv;
                uint8_t vswing_fix = m_get_cmd_resp.data.vswing_fix;
                if (vswing_mv == 0x03 && vswing_fix == 0x07) {
                  set_vswing_pos("Last position");
                } else if (vswing_mv == 0x01) set_vswing_pos("Move full");
                else if (vswing_mv == 0x02) set_vswing_pos("Move upper");
                else if (vswing_mv == 0x03) set_vswing_pos("Move lower");
                else if (vswing_fix == 0x01) set_vswing_pos("Fix top");
                else if (vswing_fix == 0x02) set_vswing_pos("Fix upper");
                else if (vswing_fix == 0x03) set_vswing_pos("Fix mid");
                else if (vswing_fix == 0x04) set_vswing_pos("Fix lower");
                else if (vswing_fix == 0x05) set_vswing_pos("Fix bottom");
                else set_vswing_pos("Last position");

                if (m_get_cmd_resp.data.hswing_mv == 0x01) set_hswing_pos("Move full");
                else if (m_get_cmd_resp.data.hswing_mv == 0x02) set_hswing_pos("Move left");
                else if (m_get_cmd_resp.data.hswing_mv == 0x03) set_hswing_pos("Move mid");
                else if (m_get_cmd_resp.data.hswing_mv == 0x04) set_hswing_pos("Move right");
                else if (m_get_cmd_resp.data.hswing_fix == 0x01) set_hswing_pos("Fix left");
                else if (m_get_cmd_resp.data.hswing_fix == 0x02) set_hswing_pos("Fix mid left");
                else if (m_get_cmd_resp.data.hswing_fix == 0x03) set_hswing_pos("Fix mid");
                else if (m_get_cmd_resp.data.hswing_fix == 0x04) set_hswing_pos("Fix mid right");
                else if (m_get_cmd_resp.data.hswing_fix == 0x05) set_hswing_pos("Fix right");
                else set_hswing_pos("Last position");

                climate::ClimatePreset prev_preset = this->preset.value_or(climate::CLIMATE_PRESET_NONE);
                this->eco_ = m_get_cmd_resp.data.eco;
                if (this->eco_) {
                    this->preset = climate::CLIMATE_PRESET_ECO;
                    sleep_mode_ = 0x00;
                } else if (m_get_cmd_resp.data.turbo) {
                    this->preset = climate::CLIMATE_PRESET_BOOST;
                    sleep_mode_ = 0x00;
                } else if (sleep_mode_ != 0x00) {
                    this->preset = climate::CLIMATE_PRESET_SLEEP;
                } else {
                    this->preset = climate::CLIMATE_PRESET_NONE;
                }
                if (prev_preset != this->preset.value_or(climate::CLIMATE_PRESET_NONE)) {
                    this->is_changed = true;
                    ESP_LOGI("TCL", "Preset changed to: %s",
                             this->preset.value_or(climate::CLIMATE_PRESET_NONE) == climate::CLIMATE_PRESET_ECO ? "ECO" :
                             this->preset.value_or(climate::CLIMATE_PRESET_NONE) == climate::CLIMATE_PRESET_SLEEP ? "SLEEP" :
                             this->preset.value_or(climate::CLIMATE_PRESET_NONE) == climate::CLIMATE_PRESET_BOOST ? "BOOST" : "NONE");
                }

                this->set_target_temperature(static_cast<float>(m_get_cmd_resp.data.temp + 16));
                this->set_current_temperature(curr_temp);

                if (this->is_changed) {
                    this->publish_state();
                }
            }
        }
    }
}

}  // namespace tcl_climate
}  // namespace esphome
#endif  // USE_ARDUINO
