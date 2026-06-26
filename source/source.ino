#include <Modbus.h>
#include <ModbusSerial.h>

const byte MODE_BUTTON_PIN = 2;
const byte MANUAL_LEFT_BUTTON_PIN = 5;
const byte MANUAL_RIGHT_BUTTON_PIN = 6;
const byte COLOR_SENSOR_PIN = 12;

const byte MOTOR_LED_PIN = 9;
const byte LEFT_SERVO_LED_PIN = 10;
const byte RIGHT_SERVO_LED_PIN = 11;

const word CYCLE_OPERATION_MODE_COIL = 100;
const word ACTIVATE_LEFT_SERVO_COIL = 102;
const word ACTIVATE_RIGHT_SERVO_COIL = 103;
const word RESET_COUNTERS_COIL = 104;

const word CONVEYOR_ON_ISTS = 0;
const word AUTOMATIC_MODE_ISTS = 1;
const word LEFT_SERVO_ACTIVE_ISTS = 2;
const word RIGHT_SERVO_ACTIVE_ISTS = 3;

const word LEFT_ITEM_COUNT_IREG = 0;
const word RIGHT_ITEM_COUNT_IREG = 1;
const word COLOR_SENSOR_READING_IREG = 2;
const word OPERATION_MODE_IREG = 3;
const word COLOR_SENSOR_SIGNAL_IREG = 4;

const byte SLAVE_ID = 1;
const long BAUD_RATE = 9600;
const unsigned long BUTTON_DEBOUNCE_MS = 50;
const unsigned long SERVO_PULSE_MS = 1000;
const unsigned long COLOR_SENSOR_STABLE_MS = 50;
// Ajuste para LOW se a saída digital do módulo LDR estiver invertida.
const byte COLOR_SENSOR_LEFT_SIGNAL = HIGH;

enum Destination {
  DESTINATION_NONE = 0,
  DESTINATION_LEFT = 1,
  DESTINATION_RIGHT = 2
};

enum OperationMode { MODE_OFF = 0, MODE_MANUAL = 1, MODE_AUTO = 2 };

struct ButtonState {
  bool lastReading;
  bool stableState;
  unsigned long lastChangeTime;
};

bool wasButtonPressed(byte pin, ButtonState &button);
bool isConveyorOn();
bool isManualMode();
bool isAutomaticMode();
void cycleOperationMode();
void updateColorSensorReading();
bool readStableColorSensorSignal(byte &stableSignal);
void activateServo(Destination destination);
void countServoDestination(Destination destination);

ModbusSerial modbus;

ButtonState modeButton = {HIGH, HIGH, 0};
ButtonState manualLeftButton = {HIGH, HIGH, 0};
ButtonState manualRightButton = {HIGH, HIGH, 0};

OperationMode operationMode = MODE_OFF;
Destination colorSensorReading = DESTINATION_NONE;
Destination activeServo = DESTINATION_NONE;
unsigned long servoActivatedAt = 0;
word leftItemCount = 0;
word rightItemCount = 0;
word colorSensorDigitalValue = 0;
byte lastColorSensorSignal = LOW;
byte stableColorSensorSignal = LOW;
unsigned long colorSensorLastChangeTime = 0;
bool colorSensorInitialized = false;

void setup() {
  configurePins();
  configureModbus();
  registerModbusPoints();
  updateOutputs();
  publishScadaState();
}

void loop() {
  modbus.task();

  updateServoPulse();
  handleScadaCommands();
  handlePhysicalInputs();
  updateColorSensorReading();
  updateOutputs();
  publishScadaState();
}

void configurePins() {
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MANUAL_LEFT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MANUAL_RIGHT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(COLOR_SENSOR_PIN, INPUT);

  pinMode(MOTOR_LED_PIN, OUTPUT);
  pinMode(LEFT_SERVO_LED_PIN, OUTPUT);
  pinMode(RIGHT_SERVO_LED_PIN, OUTPUT);
}

void configureModbus() {
  modbus.config(&Serial, BAUD_RATE, SERIAL_8N1);
  modbus.setSlaveId(SLAVE_ID);
}

void registerModbusPoints() {
  modbus.addCoil(CYCLE_OPERATION_MODE_COIL, false);
  modbus.addCoil(ACTIVATE_LEFT_SERVO_COIL, false);
  modbus.addCoil(ACTIVATE_RIGHT_SERVO_COIL, false);
  modbus.addCoil(RESET_COUNTERS_COIL, false);

  modbus.addIsts(CONVEYOR_ON_ISTS, isConveyorOn());
  modbus.addIsts(AUTOMATIC_MODE_ISTS, isAutomaticMode());
  modbus.addIsts(LEFT_SERVO_ACTIVE_ISTS, false);
  modbus.addIsts(RIGHT_SERVO_ACTIVE_ISTS, false);

  modbus.addIreg(LEFT_ITEM_COUNT_IREG, leftItemCount);
  modbus.addIreg(RIGHT_ITEM_COUNT_IREG, rightItemCount);
  modbus.addIreg(COLOR_SENSOR_READING_IREG, colorSensorReading);
  modbus.addIreg(OPERATION_MODE_IREG, operationMode);
  modbus.addIreg(COLOR_SENSOR_SIGNAL_IREG, colorSensorDigitalValue);
}

void handleScadaCommands() {
  if (consumeScadaCommand(CYCLE_OPERATION_MODE_COIL)) {
    cycleOperationMode();
  }

  if (consumeScadaCommand(ACTIVATE_LEFT_SERVO_COIL) && isManualMode()) {
    activateServo(DESTINATION_LEFT);
  }

  if (consumeScadaCommand(ACTIVATE_RIGHT_SERVO_COIL) && isManualMode()) {
    activateServo(DESTINATION_RIGHT);
  }

  if (consumeScadaCommand(RESET_COUNTERS_COIL)) {
    leftItemCount = 0;
    rightItemCount = 0;
  }
}

void handlePhysicalInputs() {
  if (wasButtonPressed(MODE_BUTTON_PIN, modeButton)) {
    cycleOperationMode();
  }

  if (wasButtonPressed(MANUAL_LEFT_BUTTON_PIN, manualLeftButton) &&
      isManualMode()) {
    activateServo(DESTINATION_LEFT);
  }

  if (wasButtonPressed(MANUAL_RIGHT_BUTTON_PIN, manualRightButton) &&
      isManualMode()) {
    activateServo(DESTINATION_RIGHT);
  }
}

bool wasButtonPressed(byte pin, ButtonState &button) {
  bool reading = digitalRead(pin);

  if (reading != button.lastReading) {
    button.lastChangeTime = millis();
    button.lastReading = reading;
  }

  if (millis() - button.lastChangeTime < BUTTON_DEBOUNCE_MS ||
      reading == button.stableState) {
    return false;
  }

  button.stableState = reading;
  return button.stableState == LOW;
}

bool consumeScadaCommand(word coilOffset) {
  if (!modbus.Coil(coilOffset)) {
    return false;
  }

  modbus.Coil(coilOffset, false);
  return true;
}

void cycleOperationMode() {
  if (operationMode == MODE_OFF) {
    operationMode = MODE_MANUAL;
  } else if (operationMode == MODE_MANUAL) {
    operationMode = MODE_AUTO;
  } else {
    operationMode = MODE_OFF;
    stopServo();
  }
}

void updateColorSensorReading() {
  byte sensorSignal;

  if (!readStableColorSensorSignal(sensorSignal)) {
    return;
  }

  Destination newReading = sensorSignal == COLOR_SENSOR_LEFT_SIGNAL
                               ? DESTINATION_LEFT
                               : DESTINATION_RIGHT;

  if (newReading == colorSensorReading) {
    return;
  }

  colorSensorReading = newReading;

  if (isAutomaticMode()) {
    activateServo(colorSensorReading);
  }
}

bool readStableColorSensorSignal(byte &stableSignal) {
  byte currentSignal = digitalRead(COLOR_SENSOR_PIN);
  colorSensorDigitalValue = currentSignal == HIGH ? 1 : 0;

  if (!colorSensorInitialized) {
    lastColorSensorSignal = currentSignal;
    stableColorSensorSignal = currentSignal;
    colorSensorLastChangeTime = millis();
    colorSensorInitialized = true;
    stableSignal = stableColorSensorSignal;
    return true;
  }

  if (currentSignal != lastColorSensorSignal) {
    colorSensorLastChangeTime = millis();
    lastColorSensorSignal = currentSignal;
    return false;
  }

  if (millis() - colorSensorLastChangeTime < COLOR_SENSOR_STABLE_MS ||
      currentSignal == stableColorSensorSignal) {
    return false;
  }

  stableColorSensorSignal = currentSignal;
  stableSignal = stableColorSensorSignal;
  return true;
}

void activateServo(Destination destination) {
  if (!isConveyorOn() || activeServo != DESTINATION_NONE) {
    return;
  }

  activeServo = destination;
  servoActivatedAt = millis();
  countServoDestination(destination);
}

void updateServoPulse() {
  if (activeServo == DESTINATION_NONE) {
    return;
  }

  if (millis() - servoActivatedAt >= SERVO_PULSE_MS) {
    stopServo();
  }
}

void stopServo() { activeServo = DESTINATION_NONE; }

void updateOutputs() {
  digitalWrite(MOTOR_LED_PIN, isConveyorOn() ? HIGH : LOW);
  digitalWrite(LEFT_SERVO_LED_PIN,
               activeServo == DESTINATION_LEFT ? HIGH : LOW);
  digitalWrite(RIGHT_SERVO_LED_PIN,
               activeServo == DESTINATION_RIGHT ? HIGH : LOW);
}

void publishScadaState() {
  updateScadaStatus(CONVEYOR_ON_ISTS, isConveyorOn());
  updateScadaStatus(AUTOMATIC_MODE_ISTS, isAutomaticMode());
  updateScadaStatus(LEFT_SERVO_ACTIVE_ISTS, activeServo == DESTINATION_LEFT);
  updateScadaStatus(RIGHT_SERVO_ACTIVE_ISTS, activeServo == DESTINATION_RIGHT);

  updateScadaValue(LEFT_ITEM_COUNT_IREG, leftItemCount);
  updateScadaValue(RIGHT_ITEM_COUNT_IREG, rightItemCount);
  updateScadaValue(COLOR_SENSOR_READING_IREG, colorSensorReading);
  updateScadaValue(OPERATION_MODE_IREG, operationMode);
  updateScadaValue(COLOR_SENSOR_SIGNAL_IREG, colorSensorDigitalValue);
}

void updateScadaStatus(word offset, bool value) { modbus.Ists(offset, value); }

void updateScadaValue(word offset, word value) { modbus.Ireg(offset, value); }

void countServoDestination(Destination destination) {
  if (destination == DESTINATION_LEFT) {
    leftItemCount++;
  } else if (destination == DESTINATION_RIGHT) {
    rightItemCount++;
  }
}

bool isConveyorOn() { return operationMode != MODE_OFF; }

bool isManualMode() { return operationMode == MODE_MANUAL; }

bool isAutomaticMode() { return operationMode == MODE_AUTO; }
