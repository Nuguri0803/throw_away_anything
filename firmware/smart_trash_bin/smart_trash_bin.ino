#include <FastLED.h>

// Board target: Arduino Mega 2560
// Install the FastLED library before uploading.

constexpr uint32_t SERIAL_BAUD = 115200;

constexpr uint8_t HUMAN_TRIG_PIN = 22;
constexpr uint8_t HUMAN_ECHO_PIN = 23;
constexpr uint8_t FILL_TRIG_PIN = 24;
constexpr uint8_t FILL_ECHO_PIN = 25;

constexpr uint8_t LED_PIN = 6;
constexpr uint16_t LED_COUNT = 30;
constexpr uint8_t LED_BRIGHTNESS = 48;

constexpr uint8_t FAN_PIN = 8;
constexpr uint8_t HEAT_PIN = 9;

constexpr uint8_t DEFAULT_MOTOR_PWM = 160;
constexpr uint8_t DEFAULT_HEAT_PWM = 80;

constexpr uint32_t MAX_MOTOR_RUNTIME_MS = 7000;
constexpr uint32_t MAX_HEAT_RUNTIME_MS = 2500;
constexpr uint32_t SENSOR_INTERVAL_MS = 350;
constexpr uint32_t AUTO_FULL_HOLD_MS = 3000;

constexpr uint16_t BIN_DEPTH_CM = 45;
constexpr uint16_t EMPTY_DISTANCE_CM = 40;
constexpr uint16_t MEDIUM_DISTANCE_CM = 25;
constexpr uint16_t ALMOST_FULL_DISTANCE_CM = 14;
constexpr uint16_t FULL_DISTANCE_CM = 9;
constexpr uint16_t HUMAN_DETECT_CM = 45;

enum SystemState {
  IDLE,
  BIN_EMPTY,
  BIN_MEDIUM,
  BIN_ALMOST_FULL,
  BIN_FULL,
  MOTOR_RUNNING,
  HEATING,
  COOLING,
  ERROR_STATE
};

enum FillLevel {
  FILL_EMPTY,
  FILL_MEDIUM,
  FILL_ALMOST_FULL,
  FILL_FULL,
  FILL_SENSOR_ERROR
};

struct MotorPins {
  uint8_t rpwm;
  uint8_t lpwm;
  uint8_t rEn;
  uint8_t lEn;
};

class MotorDriver {
 public:
  MotorDriver(const char *name, MotorPins pins)
      : name_(name), pins_(pins) {}

  void begin() {
    pinMode(pins_.rpwm, OUTPUT);
    pinMode(pins_.lpwm, OUTPUT);
    pinMode(pins_.rEn, OUTPUT);
    pinMode(pins_.lEn, OUTPUT);
    digitalWrite(pins_.rEn, HIGH);
    digitalWrite(pins_.lEn, HIGH);
    stop();
  }

  void forward(uint8_t pwm) {
    analogWrite(pins_.lpwm, 0);
    analogWrite(pins_.rpwm, pwm);
    running_ = true;
    startedAt_ = millis();
    direction_ = 1;
  }

  void backward(uint8_t pwm) {
    analogWrite(pins_.rpwm, 0);
    analogWrite(pins_.lpwm, pwm);
    running_ = true;
    startedAt_ = millis();
    direction_ = -1;
  }

  void stop() {
    analogWrite(pins_.rpwm, 0);
    analogWrite(pins_.lpwm, 0);
    running_ = false;
    direction_ = 0;
  }

  bool update() {
    if (running_ && millis() - startedAt_ > MAX_MOTOR_RUNTIME_MS) {
      stop();
      return false;
    }
    return true;
  }

  bool isRunning() const {
    return running_;
  }

  const char *name() const {
    return name_;
  }

 private:
  const char *name_;
  MotorPins pins_;
  bool running_ = false;
  int8_t direction_ = 0;
  uint32_t startedAt_ = 0;
};

class HeatSealer {
 public:
  explicit HeatSealer(uint8_t pin) : pin_(pin) {}

  void begin() {
    pinMode(pin_, OUTPUT);
    stop();
  }

  void start(uint8_t pwm, uint32_t durationMs) {
    targetDurationMs_ = min(durationMs, MAX_HEAT_RUNTIME_MS);
    startedAt_ = millis();
    active_ = true;
    analogWrite(pin_, pwm);
  }

  void stop() {
    analogWrite(pin_, 0);
    active_ = false;
  }

  bool update() {
    if (!active_) {
      return true;
    }
    if (millis() - startedAt_ >= targetDurationMs_) {
      stop();
    }
    if (millis() - startedAt_ > MAX_HEAT_RUNTIME_MS) {
      stop();
      return false;
    }
    return true;
  }

  bool isActive() const {
    return active_;
  }

 private:
  uint8_t pin_;
  bool active_ = false;
  uint32_t startedAt_ = 0;
  uint32_t targetDurationMs_ = 0;
};

class FanController {
 public:
  explicit FanController(uint8_t pin) : pin_(pin) {}

  void begin() {
    pinMode(pin_, OUTPUT);
    off();
  }

  void on() {
    digitalWrite(pin_, HIGH);
    active_ = true;
  }

  void off() {
    digitalWrite(pin_, LOW);
    active_ = false;
  }

  bool isActive() const {
    return active_;
  }

 private:
  uint8_t pin_;
  bool active_ = false;
};

class UltrasonicSensor {
 public:
  UltrasonicSensor(uint8_t trig, uint8_t echo) : trig_(trig), echo_(echo) {}

  void begin() {
    pinMode(trig_, OUTPUT);
    pinMode(echo_, INPUT);
    digitalWrite(trig_, LOW);
  }

  long readCm() {
    digitalWrite(trig_, LOW);
    delayMicroseconds(2);
    digitalWrite(trig_, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig_, LOW);

    const uint32_t duration = pulseIn(echo_, HIGH, 30000);
    if (duration == 0) {
      return -1;
    }
    return duration / 58;
  }

 private:
  uint8_t trig_;
  uint8_t echo_;
};

class RgbStatusBar {
 public:
  void begin() {
    FastLED.addLeds<WS2813, LED_PIN, GRB>(leds_, LED_COUNT);
    FastLED.setBrightness(LED_BRIGHTNESS);
    showSolid(CRGB::Black);
  }

  void update(SystemState state, FillLevel fill) {
    const uint32_t now = millis();
    switch (state) {
      case ERROR_STATE:
        blink(CRGB::Purple, now, 200);
        break;
      case HEATING:
        showSolid(CRGB::Orange);
        break;
      case COOLING:
        showSolid(CRGB::Aqua);
        break;
      case MOTOR_RUNNING:
        flow(CRGB::Blue, now);
        break;
      default:
        showFill(fill, now);
        break;
    }
    FastLED.show();
  }

 private:
  void showSolid(const CRGB &color) {
    fill_solid(leds_, LED_COUNT, color);
  }

  void blink(const CRGB &color, uint32_t now, uint16_t intervalMs) {
    showSolid((now / intervalMs) % 2 == 0 ? color : CRGB::Black);
  }

  void flow(const CRGB &color, uint32_t now) {
    fadeToBlackBy(leds_, LED_COUNT, 70);
    const uint8_t head = (now / 70) % LED_COUNT;
    leds_[head] = color;
    leds_[(head + LED_COUNT - 1) % LED_COUNT] = color;
  }

  void showFill(FillLevel fill, uint32_t now) {
    switch (fill) {
      case FILL_EMPTY:
        showSolid(CRGB::Green);
        break;
      case FILL_MEDIUM:
        showSolid(CRGB::Yellow);
        break;
      case FILL_ALMOST_FULL:
        showSolid(CRGB::OrangeRed);
        break;
      case FILL_FULL:
        blink(CRGB::Red, now, 300);
        break;
      case FILL_SENSOR_ERROR:
        blink(CRGB::Purple, now, 250);
        break;
    }
  }

  CRGB leds_[LED_COUNT];
};

MotorDriver motors[] = {
    MotorDriver("case-opener", {2, 3, 30, 31}),
    MotorDriver("heat-frame-mover", {4, 5, 32, 33}),
    MotorDriver("garbage-pusher", {10, 11, 34, 35}),
};

HeatSealer heatSealer(HEAT_PIN);
FanController fan(FAN_PIN);
UltrasonicSensor humanSensor(HUMAN_TRIG_PIN, HUMAN_ECHO_PIN);
UltrasonicSensor fillSensor(FILL_TRIG_PIN, FILL_ECHO_PIN);
RgbStatusBar statusBar;

SystemState systemState = IDLE;
FillLevel fillLevel = FILL_EMPTY;
FillLevel mockFillLevel = FILL_SENSOR_ERROR;

bool emergencyStopped = false;
bool autoSequenceActive = false;
uint8_t autoStep = 0;
uint32_t autoStepStartedAt = 0;
uint32_t lastSensorReadAt = 0;
uint32_t fullStateStartedAt = 0;

uint8_t runningMotorCount() {
  uint8_t count = 0;
  for (MotorDriver &motor : motors) {
    if (motor.isRunning()) {
      count++;
    }
  }
  return count;
}

bool anyMotorRunning() {
  return runningMotorCount() > 0;
}

void stopAllMotors() {
  for (MotorDriver &motor : motors) {
    motor.stop();
  }
}

void stopAllOutputs() {
  stopAllMotors();
  heatSealer.stop();
  fan.off();
  autoSequenceActive = false;
}

void enterError(const __FlashStringHelper *message) {
  Serial.print(F("[ERROR] "));
  Serial.println(message);
  stopAllOutputs();
  systemState = ERROR_STATE;
}

bool canStartMotor() {
  if (emergencyStopped || systemState == ERROR_STATE) {
    Serial.println(F("Motor rejected: system is stopped or in error."));
    return false;
  }
  if (heatSealer.isActive()) {
    Serial.println(F("Motor rejected: heat sealer is active."));
    return false;
  }
  if (runningMotorCount() >= 2) {
    Serial.println(F("Motor rejected: at most 2 motors can run together."));
    return false;
  }
  return true;
}

bool startMotor(uint8_t index, bool forward, uint8_t pwm) {
  if (index >= sizeof(motors) / sizeof(motors[0])) {
    return false;
  }
  if (!canStartMotor()) {
    return false;
  }
  if (forward) {
    motors[index].forward(pwm);
  } else {
    motors[index].backward(pwm);
  }
  systemState = MOTOR_RUNNING;
  Serial.print(F("Motor started: "));
  Serial.println(motors[index].name());
  return true;
}

bool startHeat(uint8_t pwm, uint32_t durationMs) {
  if (emergencyStopped || systemState == ERROR_STATE) {
    Serial.println(F("Heat rejected: system is stopped or in error."));
    return false;
  }
  if (anyMotorRunning()) {
    Serial.println(F("Heat rejected: motor is running."));
    return false;
  }
  heatSealer.start(pwm, durationMs);
  systemState = HEATING;
  Serial.println(F("Heat sealer started."));
  return true;
}

FillLevel classifyFillDistance(long distanceCm) {
  if (distanceCm < 0 || distanceCm > BIN_DEPTH_CM + 20) {
    return FILL_SENSOR_ERROR;
  }
  if (distanceCm <= FULL_DISTANCE_CM) {
    return FILL_FULL;
  }
  if (distanceCm <= ALMOST_FULL_DISTANCE_CM) {
    return FILL_ALMOST_FULL;
  }
  if (distanceCm <= MEDIUM_DISTANCE_CM) {
    return FILL_MEDIUM;
  }
  return FILL_EMPTY;
}

SystemState stateFromFill(FillLevel fill) {
  switch (fill) {
    case FILL_EMPTY:
      return BIN_EMPTY;
    case FILL_MEDIUM:
      return BIN_MEDIUM;
    case FILL_ALMOST_FULL:
      return BIN_ALMOST_FULL;
    case FILL_FULL:
      return BIN_FULL;
    case FILL_SENSOR_ERROR:
      return ERROR_STATE;
  }
  return ERROR_STATE;
}

void readSensors() {
  if (millis() - lastSensorReadAt < SENSOR_INTERVAL_MS) {
    return;
  }
  lastSensorReadAt = millis();

  fillLevel = mockFillLevel == FILL_SENSOR_ERROR
                  ? classifyFillDistance(fillSensor.readCm())
                  : mockFillLevel;

  const long humanDistance = humanSensor.readCm();
  const bool humanDetected = humanDistance > 0 && humanDistance <= HUMAN_DETECT_CM;

  if (systemState != ERROR_STATE && !anyMotorRunning() && !heatSealer.isActive() && !autoSequenceActive) {
    systemState = stateFromFill(fillLevel);
  }

  if (humanDetected) {
    Serial.println(F("Human detected near bin."));
  }

  if (fillLevel == FILL_FULL) {
    if (fullStateStartedAt == 0) {
      fullStateStartedAt = millis();
    }
  } else {
    fullStateStartedAt = 0;
  }
}

void startAutoSequence() {
  if (emergencyStopped || systemState == ERROR_STATE) {
    Serial.println(F("Auto rejected: system is stopped or in error."));
    return;
  }
  if (autoSequenceActive) {
    return;
  }
  autoSequenceActive = true;
  autoStep = 0;
  autoStepStartedAt = millis();
  Serial.println(F("Auto sequence started."));
}

void updateAutoSequence() {
  if (!autoSequenceActive) {
    return;
  }

  const uint32_t elapsed = millis() - autoStepStartedAt;

  switch (autoStep) {
    case 0:
      startMotor(0, true, DEFAULT_MOTOR_PWM);
      startMotor(1, true, DEFAULT_MOTOR_PWM);
      autoStep = 1;
      autoStepStartedAt = millis();
      break;

    case 1:
      if (elapsed >= 1800) {
        stopAllMotors();
        autoStep = 2;
        autoStepStartedAt = millis();
      }
      break;

    case 2:
      startHeat(DEFAULT_HEAT_PWM, 1800);
      autoStep = 3;
      autoStepStartedAt = millis();
      break;

    case 3:
      if (!heatSealer.isActive()) {
        fan.on();
        systemState = COOLING;
        autoStep = 4;
        autoStepStartedAt = millis();
      }
      break;

    case 4:
      if (elapsed >= 2500) {
        fan.off();
        startMotor(2, true, DEFAULT_MOTOR_PWM);
        autoStep = 5;
        autoStepStartedAt = millis();
      }
      break;

    case 5:
      if (elapsed >= 2200) {
        stopAllMotors();
        startMotor(0, false, DEFAULT_MOTOR_PWM);
        startMotor(1, false, DEFAULT_MOTOR_PWM);
        autoStep = 6;
        autoStepStartedAt = millis();
      }
      break;

    case 6:
      if (elapsed >= 1800) {
        stopAllOutputs();
        fullStateStartedAt = 0;
        systemState = IDLE;
        Serial.println(F("Auto sequence completed."));
      }
      break;
  }
}

void updateSafety() {
  for (MotorDriver &motor : motors) {
    if (!motor.update()) {
      enterError(F("motor runtime exceeded"));
      return;
    }
  }
  if (!heatSealer.update()) {
    enterError(F("heat runtime exceeded"));
    return;
  }
  if (heatSealer.isActive() && anyMotorRunning()) {
    enterError(F("motor and heater overlap"));
  }
}

void maybeStartAutoOnFull() {
  if (autoSequenceActive || emergencyStopped || systemState == ERROR_STATE) {
    return;
  }
  if (fillLevel == FILL_FULL && fullStateStartedAt > 0 && millis() - fullStateStartedAt >= AUTO_FULL_HOLD_MS) {
    startAutoSequence();
  }
}

void handleSerialCommand(char command) {
  switch (command) {
    case 'f':
      startMotor(0, true, DEFAULT_MOTOR_PWM);
      break;
    case 'b':
      startMotor(0, false, DEFAULT_MOTOR_PWM);
      break;
    case 'g':
      startMotor(1, true, DEFAULT_MOTOR_PWM);
      break;
    case 'p':
      startMotor(2, true, DEFAULT_MOTOR_PWM);
      break;
    case 'h':
      startHeat(DEFAULT_HEAT_PWM, 1200);
      break;
    case 'a':
      startAutoSequence();
      break;
    case 's':
      emergencyStopped = false;
      stopAllOutputs();
      systemState = IDLE;
      Serial.println(F("Stopped all outputs."));
      break;
    case 'e':
      emergencyStopped = true;
      stopAllOutputs();
      systemState = ERROR_STATE;
      Serial.println(F("Emergency stop."));
      break;
    case 'c':
      emergencyStopped = false;
      stopAllOutputs();
      systemState = IDLE;
      Serial.println(F("Error cleared."));
      break;
    case '0':
      mockFillLevel = FILL_EMPTY;
      Serial.println(F("Mock fill: empty"));
      break;
    case '1':
      mockFillLevel = FILL_MEDIUM;
      Serial.println(F("Mock fill: medium"));
      break;
    case '2':
      mockFillLevel = FILL_ALMOST_FULL;
      Serial.println(F("Mock fill: almost full"));
      break;
    case '3':
      mockFillLevel = FILL_FULL;
      Serial.println(F("Mock fill: full"));
      break;
    case 'r':
      mockFillLevel = FILL_SENSOR_ERROR;
      Serial.println(F("Mock disabled: using real sensor"));
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  for (MotorDriver &motor : motors) {
    motor.begin();
  }
  heatSealer.begin();
  fan.begin();
  humanSensor.begin();
  fillSensor.begin();
  statusBar.begin();

  Serial.println(F("Smart Auto-Sealing Trash Bin firmware ready."));
  Serial.println(F("Commands: f b g p h a s e c 0 1 2 3 r"));
}

void loop() {
  while (Serial.available() > 0) {
    handleSerialCommand(static_cast<char>(Serial.read()));
  }

  readSensors();
  updateSafety();
  maybeStartAutoOnFull();
  updateAutoSequence();
  statusBar.update(systemState, fillLevel);
}
