"""
Smart Kitchen Safety System - Program 4
Platform: Wokwi Raspberry Pi Pico (MicroPython)
Role: Kitchen Occupancy Monitor + Smart Cook Timer + Smoke Detection

Components & Wiring:
  - PIR Motion Sensor       -> GP16
  - Potentiometer (smoke)   -> GP26 (ADC0)
  - NeoPixel Ring (8 LEDs)  -> GP0
  - Buzzer                  -> GP15
  - Button (start/stop)     -> GP14 (pull-up)
  - LCD 16x2 I2C            -> SDA=GP4, SCL=GP5 (address 0x27)
"""

from machine import Pin, ADC, PWM, I2C
from neopixel import NeoPixel
import time
import json

# ====== I2C LCD Driver (built-in, no library needed) ======
class LCD_I2C:
    def __init__(self, i2c, addr=0x27, rows=2, cols=16):
        self.i2c = i2c
        self.addr = addr
        self.rows = rows
        self.cols = cols
        self.backlight = 0x08
        self._init_lcd()

    def _write_byte(self, data):
        self.i2c.writeto(self.addr, bytes([data | self.backlight]))

    def _pulse_enable(self, data):
        self._write_byte(data | 0x04)
        time.sleep_us(1)
        self._write_byte(data & ~0x04)
        time.sleep_us(50)

    def _send_nibble(self, data):
        self._write_byte(data & 0xF0)
        self._pulse_enable(data & 0xF0)

    def _send_byte(self, data, mode=0):
        high = (data & 0xF0) | mode
        low = ((data << 4) & 0xF0) | mode
        self._write_byte(high)
        self._pulse_enable(high)
        self._write_byte(low)
        self._pulse_enable(low)

    def _init_lcd(self):
        time.sleep_ms(50)
        self._send_nibble(0x30)
        time.sleep_ms(5)
        self._send_nibble(0x30)
        time.sleep_us(150)
        self._send_nibble(0x30)
        self._send_nibble(0x20)
        # 4-bit mode, 2 lines, 5x8 font
        self._send_byte(0x28)
        self._send_byte(0x0C)  # display on, cursor off
        self._send_byte(0x06)  # entry mode: increment
        self.clear()

    def clear(self):
        self._send_byte(0x01)
        time.sleep_ms(2)

    def move_to(self, col, row):
        addr = col + (0x40 if row == 1 else 0x00)
        self._send_byte(0x80 | addr)

    def putstr(self, string):
        for char in string:
            self._send_byte(ord(char), 0x01)

    def show(self, line1, line2=""):
        self.clear()
        self.move_to(0, 0)
        self.putstr(line1[:self.cols])
        if line2:
            self.move_to(0, 1)
            self.putstr(line2[:self.cols])


# ====== Pin Setup ======
pir = Pin(16, Pin.IN)
smoke_adc = ADC(Pin(26))
buzzer_pwm = PWM(Pin(15))
buzzer_pwm.duty_u16(0)
button = Pin(14, Pin.IN, Pin.PULL_UP)
np = NeoPixel(Pin(0), 8)

# LCD on I2C0: SDA=GP4, SCL=GP5
i2c = I2C(0, sda=Pin(4), scl=Pin(5), freq=400000)
lcd = LCD_I2C(i2c, addr=0x27)

# ====== Config ======
SMOKE_WARNING = 30000
SMOKE_DANGER = 50000
NO_MOTION_TIMEOUT = 120
COOK_TIMER_SECONDS = 300  # 5 minutes

# ====== State ======
motion_detected = False
last_motion_time = time.time()
stove_active = False
cook_timer_running = False
cook_timer_start = 0
alert_active = False
button_pressed = False

# ====== Colors ======
OFF = (0, 0, 0)
GREEN = (0, 20, 0)
YELLOW = (20, 20, 0)
RED = (20, 0, 0)
BLUE = (0, 0, 20)


def clear_neopixels():
    for i in range(8):
        np[i] = OFF
    np.write()


def set_all_neopixels(color):
    for i in range(8):
        np[i] = color
    np.write()


def show_smoke_level(level):
    num_leds = int((level / 65535) * 8)
    for i in range(8):
        if i < num_leds:
            if num_leds <= 3:
                np[i] = GREEN
            elif num_leds <= 5:
                np[i] = YELLOW
            else:
                np[i] = RED
        else:
            np[i] = OFF
    np.write()


def show_cook_timer_ring(elapsed, total):
    remaining_ratio = 1.0 - (elapsed / total)
    leds_on = int(remaining_ratio * 8)
    for i in range(8):
        if i < leds_on:
            if remaining_ratio > 0.5:
                np[i] = GREEN
            elif remaining_ratio > 0.2:
                np[i] = YELLOW
            else:
                np[i] = RED
        else:
            np[i] = OFF
    np.write()


def format_time(seconds):
    """Convert seconds to MM:SS format"""
    m = int(seconds) // 60
    s = int(seconds) % 60
    return "{:02d}:{:02d}".format(m, s)


def smoke_percent(raw):
    return min(100, int((raw / 65535) * 100))


def buzzer_beep(freq, duration_ms):
    buzzer_pwm.freq(freq)
    buzzer_pwm.duty_u16(5000)
    time.sleep_ms(duration_ms)
    buzzer_pwm.duty_u16(0)


def buzzer_alarm():
    buzzer_pwm.freq(3000)
    buzzer_pwm.duty_u16(5000)


def buzzer_off():
    buzzer_pwm.duty_u16(0)


def send_serial_data(smoke, motion, stove, timer_left):
    data = {
        "node": "occupancy",
        "smoke": smoke,
        "motion": motion,
        "stove_on": stove,
        "timer_remaining": timer_left,
        "alert": alert_active
    }
    print(json.dumps(data))


# ====== Startup ======
print("Kitchen Occupancy Monitor Online")
lcd.show("Kitchen Safety", "Starting...")
for i in range(8):
    np[i] = BLUE
    np.write()
    time.sleep_ms(100)
clear_neopixels()
time.sleep(1)
lcd.clear()

# ====== Main Loop ======
last_serial = time.time()
last_lcd_update = time.time()

while True:
    # --- Read sensors ---
    smoke_raw = smoke_adc.read_u16()
    motion_now = pir.value()

    # --- Track motion ---
    if motion_now:
        motion_detected = True
        last_motion_time = time.time()
    else:
        motion_detected = False

    seconds_no_motion = time.time() - last_motion_time

    # --- Detect stove activity from smoke ---
    stove_active = smoke_raw > SMOKE_WARNING

    # --- Button: toggle cook timer ---
    if button.value() == 0 and not button_pressed:
        button_pressed = True
        if not cook_timer_running:
            cook_timer_running = True
            cook_timer_start = time.time()
            buzzer_beep(1000, 100)
            print("Cook timer STARTED")
        else:
            cook_timer_running = False
            clear_neopixels()
            buzzer_beep(500, 100)
            print("Cook timer STOPPED")
    elif button.value() == 1:
        button_pressed = False

    # --- Cook timer logic ---
    timer_remaining = 0
    if cook_timer_running:
        elapsed = time.time() - cook_timer_start
        timer_remaining = max(0, COOK_TIMER_SECONDS - elapsed)

        show_cook_timer_ring(elapsed, COOK_TIMER_SECONDS)

        # Timer expired
        if elapsed >= COOK_TIMER_SECONDS:
            cook_timer_running = False
            set_all_neopixels(RED)
            lcd.show("** TIMER UP! **", "Food is ready!")
            buzzer_beep(2000, 500)
            time.sleep_ms(200)
            buzzer_beep(2000, 500)
            time.sleep_ms(200)
            buzzer_beep(2000, 500)
            print("Cook timer EXPIRED!")
    else:
        show_smoke_level(smoke_raw)

    # --- Unattended stove alert ---
    alert_active = False
    if stove_active and seconds_no_motion > NO_MOTION_TIMEOUT:
        alert_active = True
        if int(time.time()) % 2 == 0:
            set_all_neopixels(RED)
        else:
            clear_neopixels()
        buzzer_alarm()
        print("ALERT: Stove on but kitchen empty!")
    elif not alert_active and not cook_timer_running:
        buzzer_off()

    # --- Smoke danger (overrides all) ---
    if smoke_raw > SMOKE_DANGER:
        alert_active = True
        set_all_neopixels(RED)
        buzzer_alarm()
        print("ALERT: High smoke level!")

    # --- Update LCD every 500ms ---
    if time.time() - last_lcd_update >= 0.5:
        if alert_active and smoke_raw > SMOKE_DANGER:
            lcd.show("!! SMOKE !!", "Level: {}%".format(smoke_percent(smoke_raw)))
        elif alert_active:
            lcd.show("!! UNATTENDED !!", "No motion {}s".format(int(seconds_no_motion)))
        elif cook_timer_running:
            lcd.show("Timer: " + format_time(timer_remaining),
                     "Smoke: {}%".format(smoke_percent(smoke_raw)))
        else:
            line1 = "Smk:{}% {}".format(
                smoke_percent(smoke_raw),
                "MOT:Yes" if motion_detected else "MOT:No"
            )
            line2 = "Stove:{} Btn:Tmr".format(
                "ON " if stove_active else "OFF"
            )
            lcd.show(line1, line2)
        last_lcd_update = time.time()

    # --- Serial data every 2s ---
    if time.time() - last_serial >= 2:
        send_serial_data(smoke_raw, motion_detected, stove_active, timer_remaining)
        last_serial = time.time()

    time.sleep_ms(100)
