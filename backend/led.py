"""
EcoBloom - LED crossfade controller (QNX / Raspberry Pi)

Green LED = healthy, Yellow LED = dying.
As mood_score drops from 100 -> 0, green fades out and yellow fades in.

Wiring:
- Green LED anode -> resistor -> GPIO 18
- Yellow LED anode -> resistor -> GPIO 13
- Both cathodes -> GND

Run this on the QNX Pi itself (not your laptop) - it needs the rpi_gpio module.
"""

import time
import requests

# QNX's GPIO module - close to standard RPi.GPIO syntax
import RPi.GPIO as GPIO

GREEN_PIN = 18
YELLOW_PIN = 13

MOOD_SERVER_URL = "http://localhost:5000/mood"  # change to your Flask server's IP if on a different device
POLL_INTERVAL = 1.0  # seconds between checks


def setup():
    GPIO.setmode(GPIO.BCM)  # use Broadcom pin numbering (matches "GPIO 18" naming)
    GPIO.setup(GREEN_PIN, GPIO.OUT)
    GPIO.setup(YELLOW_PIN, GPIO.OUT)

    # 100 Hz is fast enough that your eye sees smooth brightness, not flicker
    green_pwm = GPIO.PWM(GREEN_PIN, 100)
    yellow_pwm = GPIO.PWM(YELLOW_PIN, 100)

    green_pwm.start(100)   # start fully green (healthy default)
    yellow_pwm.start(0)    # start with yellow off

    return green_pwm, yellow_pwm


def mood_to_duty_cycles(mood_score):
    """
    Crossfade logic: mood_score 100 = full green, 0 duty on yellow.
    mood_score 0 = full yellow, 0 duty on green.
    Linear crossfade in between.
    """
    mood_score = max(0, min(100, mood_score))  # clamp to safe range
    green_duty = mood_score          # 100 -> 0 as mood drops
    yellow_duty = 100 - mood_score   # 0 -> 100 as mood drops
    return green_duty, yellow_duty


def run():
    green_pwm, yellow_pwm = setup()
    
    current_green = 100.0
    current_yellow = 0.0
    FADE_DURATION = 1.0  # seconds to fade between mood changes
    FADE_STEPS = 20      # how many small steps during the fade

    try:
        while True:
            try:
                response = requests.get(MOOD_SERVER_URL, timeout=5)
                mood = response.json()["mood"]
            except requests.exceptions.RequestException as e:
                print(f"Could not reach mood server: {e}")
                time.sleep(POLL_INTERVAL)
                continue

            target_green, target_yellow = mood_to_duty_cycles(mood)
            
            # Smoothly fade from current to target over FADE_DURATION seconds
            for step in range(FADE_STEPS):
                progress = (step + 1) / FADE_STEPS  # 0 to 1
                current_green = current_green + (target_green - current_green) * progress
                current_yellow = current_yellow + (target_yellow - current_yellow) * progress
                
                green_pwm.ChangeDutyCycle(current_green)
                yellow_pwm.ChangeDutyCycle(current_yellow)
                
                time.sleep(FADE_DURATION / FADE_STEPS)
            
            # Ensure we end exactly at target (no rounding errors)
            current_green = target_green
            current_yellow = target_yellow
            green_pwm.ChangeDutyCycle(current_green)
            yellow_pwm.ChangeDutyCycle(current_yellow)
            
            print(f"mood={mood:.1f}  green={current_green:.1f}%  yellow={current_yellow:.1f}%")

    except KeyboardInterrupt:
        print("\nStopping, cleaning up GPIO...")
    finally:
        green_pwm.stop()
        yellow_pwm.stop()
        GPIO.cleanup()  # always release the pins cleanly - avoids "pin busy" errors next run


if __name__ == "__main__":
    run()