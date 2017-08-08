/**********************************************************************
 * KUBO Hindbrain Code (Teensie 3.5)
 * @author: Connor Novak
 * @email: connor.novak@students.olin.edu
 * @version: 1.0
 * @description: Basic control of velocity actuator and steering actuator through ackermann steering messages over /drive
 **********************************************************************/

#include "RoboClaw.h"
#include <Arduino.h>
#include "ros.h"
#include "ackermann_msgs/AckermannDrive.h"
#include "estop.h"
#include "soft_switch.h"

// Define pins, serial location
#define RUNNING_LED_PIN 3
#define RC_SERIAL Serial1
#define address 0x80
#define ROBOCLAW_UPDATE_RATE 500

// Limit ranges of motors & controls (tuned)
#define VEL_HIGH 360
#define VEL_LOW 20
#define VEL_CONTROL_RANGE 2
#define STEER_HIGH 1900
#define STEER_LOW 100
#define STEER_CONTROL_RANGE 90

// Fidelity of actuators (subdivision of motion)
#define VEL_FIDELITY 5
#define STEER_FIDELITY 3

// Def/init vars
unsigned int velMsg = VEL_HIGH;
signed int steerMsg = (STEER_HIGH + STEER_LOW)/2;
unsigned long prevMillis = millis();
int prevSteerMsg;
int prevVelMsg;
// Estop *e;

// Define roboclaw on 1st Teensie serial port
RoboClaw rc(&Serial1,10000);

/* FUNCTION: /drive callback function
 *  Called upon a receipt of data from /drive
 */
void ackermannCB(const ackermann_msgs::AckermannDrive &drive){
  steerMsg = steerConvert(drive.steering_angle);
  velMsg = velConvert(drive.speed);
} //ackermannCB()

// ROS variables
ros::NodeHandle nh;
ros::Subscriber<ackermann_msgs::AckermannDrive> sub("drive", &ackermannCB );

/* FUNCTION: setup function
 *  runs once on startup
 */
void setup() {

  //Open Serial and roboclaw serial ports
  rc.begin(38400);

  // Set pin modes
  pinMode(RUNNING_LED_PIN, OUTPUT);
  pinMode(13, OUTPUT);

  // Set up ROS stuff
  nh.initNode(); // Initialize ROS nodehandle
  nh.subscribe(sub);
  // e = new Estop(&nh,5,1);

  // Turn on running led
  //digitalWrite(13, HIGH);

  // Set actuators to default positions
  rc.SpeedAccelDeccelPositionM1(address,0,300,0,velMsg,0);
  prevVelMsg = velMsg;
  rc.SpeedAccelDeccelPositionM2(address,0,500,0,steerMsg,0);
  prevSteerMsg = steerMsg;

} //setup()

/* FUNCTION: loop function
 *  runs constantly
 */
void loop() {

  // Sends commands to RoboClaw every ROBOCLAW_UPDATE_RATE milliseconds
  if (millis() - prevMillis > ROBOCLAW_UPDATE_RATE) {
    updateRoboClaw(velMsg, steerMsg);
  }


  nh.spinOnce();
  delay(1);

  //TODO: process estop interruption

} //loop()

/* FUNCTION: RoboClaw command function
 *  Sends current velocity and steering vals to RoboClaw; called at ROBOCLAW_UPDATE_RATE
 */
void updateRoboClaw(int velMsg, int steerMsg) {

  //Calculate step sizes based on fidelity
  int steerStep = (STEER_HIGH - STEER_LOW)/STEER_FIDELITY;
  int velStep = (VEL_HIGH - VEL_LOW)/VEL_FIDELITY;

  stepActuator(&velMsg, &prevVelMsg, velStep);
  stepActuator(&steerMsg, &prevSteerMsg, steerStep);
  
  prevVelMsg = velMsg;
  prevSteerMsg = steerMsg;
  
  rc.SpeedAccelDeccelPositionM1(address,0,1000,0,velMsg,0);
  rc.SpeedAccelDeccelPositionM2(address,0,1000,0,steerMsg,0);
  prevMillis = millis();

  /*char i[32];
  snprintf(i, sizeof(i), "velstep = %d, steerstep = %d", velStep, steerStep);
  nh.loginfo(i);
  */
  
  
  char i[32];
  snprintf(i, sizeof(i), "Velocity = %d, Steering = %d", velMsg, steerMsg);
  nh.loginfo(i);
} //updateRoboClaw()

/* FUNCTION: Steering conversion function
 *  Converts ackermann steering angle to motor encoder value for RoboClaw
 */
int steerConvert(float ack_steer){
  
  // Convert from range of input signal to range of output signal, then shift signal
  ack_steer = ack_steer * ((STEER_HIGH-STEER_LOW)/STEER_CONTROL_RANGE) + (STEER_HIGH + STEER_LOW)/2;
  
  // Safety limits for signal (possibly not necessary, Roboclaw may do this?)
  if (ack_steer > STEER_HIGH){
    ack_steer = STEER_HIGH;
  }
  else if (ack_steer < STEER_LOW){
    ack_steer = STEER_LOW;
  }

  return ack_steer;
} //steerConvert

/* FUNCTION: Velocity conversion function
 *  Converts ackermann velocity to motor encoder value for RoboClaw
 */
int velConvert(float ack_vel){
  
  // Reverse-removal Filter
  if (ack_vel < 0) {
    ack_vel = 0;
  }

  // Convert from range of input signal to range of output signal
  ack_vel = VEL_HIGH - ack_vel * ((VEL_HIGH - VEL_LOW)/VEL_CONTROL_RANGE);
  
  return ack_vel;
} //velConvert()

/* FUNCTION: Command stepping function
 *  Meters commands sent to motors to ensure quick response and low latency
 */

void stepActuator(int *msg, int *prevMsg, int step) {

  // Checks if stepping is necessary
  if (abs(*prevMsg - *msg) > step) {

    nh.loginfo("Stepped!");
    // If signal increasing
    if (*msg > *prevMsg) {
      *msg = *prevMsg + step;
    }

    // If signal decreasing
    else if (*msg < *prevMsg) {
      *msg = *prevMsg - step;
    }

    // Exception case
    else {
      *msg = *prevMsg;
    }
  } 
} //stepActuator()
