#include <LiquidCrystal_I2C.h>
#include <Modbus.h>
#include <ModbusSerial.h>
#include <Servo.h>
#include <Wire.h>
#include <stdio.h>

const byte MODE_BUTTON_PIN = 2;
const byte MANUAL_LEFT_BUTTON_PIN = 5;
const byte MANUAL_RIGHT_BUTTON_PIN = 6;
const byte COLOR_SENSOR_PIN = 12;

const byte MOTOR_LED_PIN = 9;
const byte LEFT_SERVO_LED_PIN = 10;
const byte RIGHT_SERVO_PIN = 13;

const byte LCD_ADDRESS = 0x3F;
const byte LCD_COLUMNS = 16;
const byte LCD_ROWS = 2;
const byte LCD_PAGE_COUNT = 3;
const unsigned long LCD_PAGE_INTERVAL_MS = 2000;
const unsigned long LCD_REFRESH_INTERVAL_MS = 250;

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
const unsigned long LEFT_SERVO_PULSE_MS = 1000;
const unsigned long RIGHT_SERVO_HOLD_MS = 3000;
const unsigned long COLOR_SENSOR_STABLE_MS = 50;
const byte RIGHT_SERVO_INITIAL_ANGLE = 15;
const byte RIGHT_SERVO_ACTIVE_ANGLE = 165;
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
void configureServo();
void configureLcd();
void cycleOperationMode();
void updateColorSensorReading();
bool readStableColorSensorSignal(byte &stableSignal);
void activateServo(Destination destination);
unsigned long getServoActiveTime(Destination destination);
void moveRightServoToInitialPosition();
void moveRightServoToActivePosition();
void countServoDestination(Destination destination);
void updateLcdDisplay();

ModbusSerial modbus;
Servo rightServo;
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

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
byte currentLcdPage = 0;
unsigned long lastLcdPageChange = 0;
unsigned long lastLcdRefresh = 0;

void setup() {
  configurePins();
  configureServo();
  configureLcd();
  configureModbus();
  registerModbusPoints();
  updateOutputs();
  publishScadaState();
  updateLcdDisplay();
}

void loop() {
  modbus.task();

  updateServoPulse();
  handleScadaCommands();
  handlePhysicalInputs();
  updateColorSensorReading();
  updateOutputs();
  publishScadaState();
  updateLcdDisplay();
}

void configurePins() {
  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MANUAL_LEFT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MANUAL_RIGHT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(COLOR_SENSOR_PIN, INPUT);

  pinMode(MOTOR_LED_PIN, OUTPUT);
  pinMode(LEFT_SERVO_LED_PIN, OUTPUT);
}

void configureServo() {
  rightServo.attach(RIGHT_SERVO_PIN);
  moveRightServoToInitialPosition();
}

void configureModbus() {
  modbus.config(&Serial, BAUD_RATE, SERIAL_8N1);
  modbus.setSlaveId(SLAVE_ID);
}

void configureLcd() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
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
  if (destination == DESTINATION_RIGHT) {
    moveRightServoToActivePosition();
  }
  countServoDestination(destination);
}

void updateServoPulse() {
  if (activeServo == DESTINATION_NONE) {
    return;
  }

  if (millis() - servoActivatedAt >= getServoActiveTime(activeServo)) {
    stopServo();
  }
}

void stopServo() {
  if (activeServo == DESTINATION_RIGHT) {
    moveRightServoToInitialPosition();
  }

  activeServo = DESTINATION_NONE;
}

void updateOutputs() {
  digitalWrite(MOTOR_LED_PIN, isConveyorOn() ? HIGH : LOW);
  digitalWrite(LEFT_SERVO_LED_PIN,
               activeServo == DESTINATION_LEFT ? HIGH : LOW);
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

const char *getOperationModeLabel() {
  if (operationMode == MODE_MANUAL) {
    return "MAN";
  }

  if (operationMode == MODE_AUTO) {
    return "AUTO";
  }

  return "OFF";
}

const char *getConveyorLabel() { return isConveyorOn() ? "ON" : "OFF"; }

char getYesNoLabel(bool value) { return value ? 'S' : 'N'; }

char getBitLabel(bool value) { return value ? '1' : '0'; }

char getDestinationLabel(Destination destination) {
  if (destination == DESTINATION_LEFT) {
    return 'E';
  }

  if (destination == DESTINATION_RIGHT) {
    return 'D';
  }

  return 'N';
}

void setLcdLine(char *line, const char *text) {
  byte index = 0;

  while (index < LCD_COLUMNS && text[index] != '\0') {
    line[index] = text[index];
    index++;
  }

  while (index < LCD_COLUMNS) {
    line[index] = ' ';
    index++;
  }

  line[LCD_COLUMNS] = '\0';
}

void writeLcdLine(byte row, const char *line) {
  lcd.setCursor(0, row);
  lcd.print(line);
}

void buildOperationLcdPage(char *line1, char *line2) {
  char text[LCD_COLUMNS + 1];

  snprintf(text, sizeof(text), "Modo: %s", getOperationModeLabel());
  setLcdLine(line1, text);

  snprintf(text, sizeof(text), "Est:%s Aut:%c", getConveyorLabel(),
           getYesNoLabel(isAutomaticMode()));
  setLcdLine(line2, text);
}

void buildCountersLcdPage(char *line1, char *line2) {
  char text[LCD_COLUMNS + 1];

  snprintf(text, sizeof(text), "Esq:%u", leftItemCount);
  setLcdLine(line1, text);

  snprintf(text, sizeof(text), "Dir:%u", rightItemCount);
  setLcdLine(line2, text);
}

void buildSensorLcdPage(char *line1, char *line2) {
  char text[LCD_COLUMNS + 1];

  snprintf(text, sizeof(text), "Sensor:%c Sig:%u",
           getDestinationLabel(colorSensorReading), colorSensorDigitalValue);
  setLcdLine(line1, text);

  snprintf(text, sizeof(text), "Srv E:%c D:%c",
           getBitLabel(activeServo == DESTINATION_LEFT),
           getBitLabel(activeServo == DESTINATION_RIGHT));
  setLcdLine(line2, text);
}

void buildLcdPage(char *line1, char *line2) {
  if (currentLcdPage == 0) {
    buildOperationLcdPage(line1, line2);
  } else if (currentLcdPage == 1) {
    buildCountersLcdPage(line1, line2);
  } else {
    buildSensorLcdPage(line1, line2);
  }
}

void updateLcdDisplay() {
  unsigned long now = millis();
  bool pageChanged = false;

  if (now - lastLcdPageChange >= LCD_PAGE_INTERVAL_MS) {
    currentLcdPage = (currentLcdPage + 1) % LCD_PAGE_COUNT;
    lastLcdPageChange = now;
    pageChanged = true;
  }

  if (!pageChanged && now - lastLcdRefresh < LCD_REFRESH_INTERVAL_MS) {
    return;
  }

  lastLcdRefresh = now;

  char line1[LCD_COLUMNS + 1];
  char line2[LCD_COLUMNS + 1];

  buildLcdPage(line1, line2);
  writeLcdLine(0, line1);
  writeLcdLine(1, line2);
}

unsigned long getServoActiveTime(Destination destination) {
  return destination == DESTINATION_RIGHT ? RIGHT_SERVO_HOLD_MS
                                          : LEFT_SERVO_PULSE_MS;
}

void moveRightServoToInitialPosition() {
  rightServo.write(RIGHT_SERVO_INITIAL_ANGLE);
}

void moveRightServoToActivePosition() {
  rightServo.write(RIGHT_SERVO_ACTIVE_ANGLE);
}

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
