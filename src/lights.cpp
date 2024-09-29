#include "lights.h"

// TODO:
//  Add lights value state instead of reading it from the LEDC peripheral
//  and use the value to implement the increment and decrement functions

constexpr auto FREQ = 5000;
constexpr auto RESOLUTION = 10;
constexpr auto FADE_TIME = 750;
constexpr auto GAMMA = 2.2;

static Ticker auto_off_timer;
static bool fading = false;

static uint32_t percent_to_duty(uint8_t percent) {
    auto normalized = _min(percent, 100) / 100.0;
    auto corrected = pow(normalized, GAMMA);
    auto max = (1 << RESOLUTION) - 1;
    log_v("%%: %d, norm: %f, corrected: %f, max: %d", percent, normalized, corrected, max);
    return static_cast<uint32_t>(corrected * max);
}

static uint8_t duty_to_percent(uint32_t duty) {
    auto max = static_cast<float>((1 << RESOLUTION) - 1);
    auto corrected = static_cast<float>(duty) / max;
    auto normalized = pow(corrected, 1 / GAMMA);
    log_v("Duty: %d, corrected: %f, norm: %f", duty, corrected, normalized);
    return static_cast<uint8_t>(normalized * 100);
}

static void timer_isr() { lights::fadeDown(); }

static void start_timer() {
    if (auto current = ledcRead(pins::LIGHTS); current == 0) return;
    auto_off_timer.once(static_cast<float>((global::lightDuration * 60)), timer_isr);
}

static void fade_isr() {
    fading = false;
    start_timer();
}

/*!
 * @brief Setup the lights PWM channel
 */
void lights::setup() {ledcAttach(pins::LIGHTS, FREQ, RESOLUTION);}

/*!
 * @brief Get the current lights brightness
 * @return The current lights brightness as a percentage
 */
uint32_t lights::get() {
    auto current = ledcRead(pins::LIGHTS);
    return duty_to_percent(current);
}

/*!
 * @brief Set the lights brightness. The value to be set is gamma corrected
 * @param percent The brightness percentage to set the lights to
 * @note If the lights are currently fading, this function will do nothing
 */
void lights::set(uint8_t percent) {
    if (fading) {
        log_i("Lights are currently fading, ignoring set request");
        return;
    }
    auto duty = percent_to_duty(percent);
    log_d("Setting lights to %ul (%d%%)", duty, percent);
    if (duty == 0) {
        auto_off_timer.detach();
    } else {
        start_timer();
    }
    ledcWrite(pins::LIGHTS, duty);
}

/*!
 * @brief Fade the lights to a given brightness. The value to be set is gamma corrected
 * @param percent The brightness percentage to fade the lights to
 * @note If the lights are currently fading, this function will do nothing
 */
void lights::fade(uint8_t percent) {
    if (fading) {
        log_i("Lights are currently fading, ignoring fade request");
        return;
    }
    auto target = percent_to_duty(percent);
    auto current = ledcRead(pins::LIGHTS);
    log_d("Fading lights from %ul (%d%%) to %ul (%d%%)", current, duty_to_percent(current), target, percent);
    fading = true;
#ifndef WOKWI // WOKWI doesn't support ledcFade APIs
    ledcFadeWithInterrupt(pins::LIGHTS, current, target, FADE_TIME, fade_isr);
#else
    ledcWrite(pins::LIGHTS, target);
    fade_isr();
#endif
}

/*!
 * @brief Fade the lights to full brightness
 * @see fade()
 */
void lights::fadeUp() { fade(100); }

/*!
 * @brief Fade the lights to off
 * @see fade()
 */
void lights::fadeDown() { fade(0); }
