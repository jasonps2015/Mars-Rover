#include <Arduino.h>
#include "ServoEasing.h"

#include "switch_checker.h"

typedef struct {
  uint8_t forward_value;
  uint8_t endstop_min;
  uint8_t endstop_max;
  Servo servo;
} calibrated_servo;

enum MotorDirection {
  IDLE,
  FORWARD,
  BACKWARD
};

#define SERIAL_PORT_SPEED 9600
#define RC_NUM_CHANNELS  6

#define SERVO_MOVE_SPEED 60
#define MIN_SERVO_ANGLE 20
#define MAX_SERVO_ANGLE 160

#define SERVO_STEER_SPEED 180

#define RC_CH1  0
#define RC_CH2  1
#define RC_CH3  2
#define RC_CH4  3
#define RC_CH5  4
#define RC_CH6  5

#define RC_CH1_INPUT  2
#define RC_CH2_INPUT  3
#define RC_CH3_INPUT  4
#define RC_CH4_INPUT  5
#define RC_CH5_INPUT  6
#define RC_CH6_INPUT  7

#define RC_STEER_CHANNEL_INPUT  RC_CH1_INPUT
#define RC_MOTOR_CHANNEL_INPUT  RC_CH2_INPUT

#define RC_SERVO1_CHANNEL_INPUT  RC_CH1_INPUT
#define RC_SERVO2_CHANNEL_INPUT  RC_CH2_INPUT
#define RC_SERVO3_CHANNEL_INPUT  RC_CH4_INPUT

#define RC_STEER_CHANNEL RC_CH1
#define RC_MOTOR_CHANNEL RC_CH2

#define RC_SERVO1_CHANNEL RC_CH1
#define RC_SERVO2_CHANNEL RC_CH2
#define RC_SERVO3_CHANNEL RC_CH4



uint16_t rc_values[RC_NUM_CHANNELS];

MotorDirection motorState = IDLE;
RoverMode currentRoverMode = DRIVE_TURN_NORMAL;

ServoEasing Servo1;
ServoEasing Servo2;
ServoEasing Servo3;
ServoEasing motors;
ServoEasing frontLeft;
ServoEasing frontRight;
ServoEasing backLeft;
ServoEasing backRight;

int lastMs = 0;

uint16_t filterSignal(uint16_t signal) {
  if (signal == 0) return 0;
  if (signal < 1000) return 1000;
  if (signal > 2000) return 2000;
  if (signal > 1450 && signal < 1550) return 1500;
  return signal;
}

void modeChanged(RoverMode mode) {
  Serial.print("MODE CHANGED: ");
  Serial.println(mode);
  currentRoverMode = mode;
}

void setup() {
  Serial.begin(SERIAL_PORT_SPEED);


  pinMode(RC_CH1_INPUT, INPUT);
  pinMode(RC_CH2_INPUT, INPUT);
  pinMode(RC_CH3_INPUT, INPUT);
  pinMode(RC_CH4_INPUT, INPUT);
  pinMode(RC_CH5_INPUT, INPUT);
  pinMode(RC_CH6_INPUT, INPUT);
  InitSwitchChecker(1000, RC_CH5_INPUT, &modeChanged);
  
  motors.attach(11);
  motors.writeMicroseconds(1500);

  /*
  A0 = backLeft
  A1 = frontLeft
  A2 = backRight 
  A3 = frontRight
  */
  frontLeft.attach(A1);
  frontRight.attach(A3);
  backLeft.attach(A0);
  backRight.attach(A2);

  frontLeft.write(90);
  frontRight.write(90);
  backLeft.write(90);
  backRight.write(90);

  Servo1.attach(8);
  Servo2.attach(9);
  Servo3.attach(10);
  setEasingTypeForAllServos(EASE_LINEAR);
  Servo1.setSpeed(30);
  Servo2.setSpeed(30);
  Servo3.setSpeed(30);
  Servo1.setEaseTo(90);
  Servo2.setEaseTo(90);
  Servo3.startEaseTo(90);
  while (Servo1.isMoving() || Servo2.isMoving() || Servo3.isMoving());
  Serial.println("Ready");
  lastMs = millis();
}

void handleRobotArmServo21(ServoEasing* servo, uint16_t channel) {
  uint16_t speed;
  uint16_t signal = filterSignal(rc_values[channel]);
  if (signal < 1500 && signal > 0) {
    //Serial.print("CH:"); Serial.print(channel); Serial.print(" "); Serial.print(rc_values[channel]); Serial.print("\t");
    speed = map(signal, 1000, 1500, 0, MAX_SERVO_ANGLE);
    speed = max(MAX_SERVO_ANGLE - speed, 1);
    //Serial.print("Signal: ");
    //Serial.print(signal);
    //Serial.print("Speed: ");
    //Serial.println(speed);
    servo->startEaseTo(MIN_SERVO_ANGLE, speed);
  } else if (signal > 1500) {
    //Serial.print("CH:"); Serial.print(channel); Serial.print(" "); Serial.print(rc_values[channel]); Serial.print("\t");
    speed = map(signal, 1500, 2000, 0, MAX_SERVO_ANGLE);
    speed = max(speed, 1);
    //Serial.print("Signal: ");
    //Serial.print(signal);
    //Serial.print("Speed: ");
    //Serial.println(speed);
    servo->startEaseTo(MAX_SERVO_ANGLE, speed);
  } else {
    servo->mServoMoves = false;
    if (!isOneServoMoving()) {
      stopAllServos();
    }
  }
}

void handleMoveMotos(void) {
  if (currentRoverMode != DRIVE_TURN_NORMAL) {
    return; // TODO When I get two ESC:s they will run in opposite directions when in TURN_SPIN_MODE
  }
  uint16_t signal = filterSignal(rc_values[RC_MOTOR_CHANNEL]);
  if (signal < 1500 && signal > 0) { // Backward
    switch (motorState) {
      case IDLE:
      Serial.println("Was IDLE");
        motors.writeMicroseconds(1000);
        delay(200);
        motors.writeMicroseconds(1500);
        delay(200);
        motorState = BACKWARD;
        break;
      case FORWARD:
        break;
      case BACKWARD:
        break;
    }
    //Serial.print("CH:"); Serial.print(RC_MOTOR_CHANNEL); Serial.print(" "); Serial.print(rc_values[RC_MOTOR_CHANNEL]); Serial.print("\n");
  } else if (signal > 1500) {
    motorState = FORWARD;
    //Serial.print("CH:"); Serial.print(RC_MOTOR_CHANNEL); Serial.print(" "); Serial.print(rc_values[RC_MOTOR_CHANNEL]); Serial.print("\n");
  } else {
    motorState = IDLE;
  }

  motors.writeMicroseconds(signal); // Motor controller handles filtering test?
}

void steer(uint16_t signal) {
  if (signal < 1500 && signal > 0) { // Left turn
    if (currentRoverMode == DRIVE_TURN_NORMAL) {
      uint16_t diff = 1500 - signal;
      frontLeft.writeMicrosecondsOrUnits(signal);
      frontRight.writeMicrosecondsOrUnits(signal);
      backLeft.writeMicrosecondsOrUnits(1500 + diff);
      backRight.writeMicrosecondsOrUnits(1500 + diff);
    } else if (currentRoverMode == DRIVE_TURN_SPIN) {
      // TODO
    }
  } else if (signal > 1500) { // Right turn
    if (currentRoverMode == DRIVE_TURN_NORMAL) {
      uint16_t diff = signal - 1500;
      frontLeft.writeMicrosecondsOrUnits(signal);
      frontRight.writeMicrosecondsOrUnits(signal);
      backLeft.writeMicrosecondsOrUnits(1500 - diff);
      backRight.writeMicrosecondsOrUnits(1500 - diff);
    } else if(currentRoverMode == DRIVE_TURN_SPIN) {
      // TODO
    }

  } else {
    frontLeft.writeMicrosecondsOrUnits(1500);
    frontRight.writeMicrosecondsOrUnits(1500);
    backLeft.writeMicrosecondsOrUnits(1500);
    backRight.writeMicrosecondsOrUnits(1500);
  }
}

void handleSteer(void) {
  uint16_t signal = filterSignal(rc_values[RC_STEER_CHANNEL]);
  steer(signal);
}

void loop() {
  SwitchCheckerUpdate();
  int time = millis();
  //Serial.print("Loop: ");
  //Serial.println(time - lastMs);
  lastMs = time;
  switch (currentRoverMode) {
    case DRIVE_TURN_NORMAL:
    case DRIVE_TURN_SPIN:
      rc_values[RC_STEER_CHANNEL] = pulseIn(RC_STEER_CHANNEL_INPUT, HIGH, 100000);
      rc_values[RC_MOTOR_CHANNEL] = pulseIn(RC_MOTOR_CHANNEL_INPUT, HIGH, 100000);
      handleMoveMotos();
      handleSteer();
      break;
    case ROBOT_ARM:
      rc_values[RC_SERVO1_CHANNEL] = pulseIn(RC_SERVO1_CHANNEL_INPUT, HIGH, 100000);
      rc_values[RC_SERVO2_CHANNEL] = pulseIn(RC_SERVO2_CHANNEL_INPUT, HIGH, 100000);
      rc_values[RC_SERVO3_CHANNEL] = pulseIn(RC_SERVO3_CHANNEL_INPUT, HIGH, 100000);
      handleRobotArmServo21(&Servo1, RC_CH1);
      //handleRobotArmServo21(&Servo2, RC_CH3);
      //handleRobotArmServo21(&Servo3, RC_CH4);
      break;
  }
}