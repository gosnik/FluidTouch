#pragma once

typedef int esp_sleep_wakeup_cause_t;
typedef int esp_sleep_pd_domain_t;
typedef int esp_sleep_pd_option_t;

#define ESP_SLEEP_WAKEUP_ALL 0
#define ESP_PD_DOMAIN_RTC_PERIPH 0
#define ESP_PD_DOMAIN_XTAL 0
#define ESP_PD_OPTION_OFF 0

inline void esp_sleep_disable_wakeup_source(esp_sleep_wakeup_cause_t) {}
inline void esp_sleep_pd_config(esp_sleep_pd_domain_t, esp_sleep_pd_option_t) {}
inline void esp_deep_sleep_start() {}
