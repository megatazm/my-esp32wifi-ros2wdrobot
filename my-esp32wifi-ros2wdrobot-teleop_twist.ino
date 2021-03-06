#include <Robojax_L298N_DC_motor.h>
#include <ros.h>
#include <std_msgs/String.h>
#include <WiFi.h>
#include <std_msgs/Int16.h>
#include <PID_v1.h>
#include <geometry_msgs/Vector3Stamped.h>
#include <geometry_msgs/Twist.h>
#include <ros/time.h>




                                                                            // WIFI CONFIGURATION
const char SSID[] = "ros-master-pi4D1C";
const char PASSWORD[] = "robotseverywhere";
IPAddress server(10,42,0,1); // e.g.: IPAddress server(192, 168, 1, 3);
const uint16_t serverPort = 11411;

WiFiClient client;

class WiFiHardware {
  public:
  WiFiHardware() {};

  void init() {
    client.connect(server, serverPort);
  }

  int read() {
    return client.read();
  }

  void write(uint8_t* data, int length) {
    for(int i=0; i<length; i++)
      client.write(data[i]);
  }

  unsigned long time() {
     return millis();
  }
};



                                                                            // ROBOT SPECIFIC PARAMETERS 

const double radius = 0.03;                   //Wheel radius, in m
const double wheelbase = 0.135;               //Wheelbase, in m
const double encoder_cpr = 20;                //Encoder ticks or counts per rotation
const double speed_to_pwm_ratio = 0.00235;     //Ratio to convert speed (in m/s) to PWM value. It was obtained by plotting the wheel speed in relation to the PWM motor command (the value is the slope of the linear function).
const double min_speed_cmd = 0.0882;           //(min_speed_cmd/speed_to_pwm_ratio) is the minimum command value needed for the motor to start moving. This value was obtained by plotting the wheel speed in relation to the PWM motor command (the value is the constant of the linear function).

double speed_req = 0;                         //Desired linear speed for the robot, in m/s
double angular_speed_req = 0;                 //Desired angular speed for the robot, in rad/s

double speed_req_left = 0;                    //Desired speed for left wheel in m/s
double speed_act_left = 0;                    //Actual speed for left wheel in m/s
double speed_cmd_left = 0;                    //Command speed for left wheel in m/s 

double speed_req_right = 0;                   //Desired speed for right wheel in m/s
double speed_act_right = 0;                   //Actual speed for right wheel in m/s
double speed_cmd_right = 0;                   //Command speed for right wheel in m/s 
                        
const double max_speed = 0.4;                 //Max speed in m/s

int PWM_leftMotor = 0;                        //PWM command for left motor
int PWM_rightMotor = 0;                       //PWM command for right motor 

//initializing all the variables
#define LOOPTIME                      100     //Looptime in millisecond
const byte noCommLoopMax = 10;                //number of main loops the robot will execute without communication before stopping
unsigned int noCommLoops = 0;                 //main loop without communication counter

double speed_cmd_left2 = 0;      
unsigned long lastMilli = 0;



                                                                            // ROS DECLARATION
ros::NodeHandle_<WiFiHardware> nh;

std_msgs::String str_msg;
std_msgs::Int16 int_msg;
geometry_msgs::Vector3Stamped speed_msg; //create a "speed_msg" ROS message


                                                                            //ROS PUBLISHER
ros::Publisher chatter("chatter", &str_msg);
char hello[13] = "hello world!";

//create a publisher to ROS topic "speed" using the "speed_msg" type
ros::Publisher speed_pub("speed", &speed_msg); 



                                                                           // MOTOR PARAMETERS


//pin definition
// Motor B - left look from back
#define CHB 0
#define enable2Pin 25  // brown 
#define motor2Pin3 33  // orange
#define motor2Pin4 32 // red

// Motor A - right look from back
#define CHA 1
#define motor1Pin1 27
#define motor1Pin2 26
#define enable1Pin 13


const int CCW = 2; // do not change
const int CW  = 1; // do not change

#define leftMotor 1 // do not change
#define rightMotor 2 // do not change

bool leftMotorCW; // flags motor rotation for encoder
bool leftMotorCCW;
bool rightMotorCW;
bool rightMotorCCW;

Robojax_L298N_DC_motor robotMotors(motor1Pin1, motor1Pin2, enable1Pin, CHA,  motor2Pin3, motor2Pin4, enable2Pin, CHB);



                                                                            // PID CONTROLLER PARAMETERS 

//PID constants

const double PID_left_param[] = { 1, 5, 0 }; //Respectively Kp, Ki and Kd for left motor PID
const double PID_right_param[] = { 1, 5, 0 }; //Respectively Kp, Ki and Kd for right motor PID

volatile float pos_left = 0;       //Left motor encoder position
volatile float pos_right = 0;      //Right motor encoder position

// speed_request=> speed as requested from keyboard
// speed_act=> speed after feedback from PID
// cmd input based on feedback speed_act. This the PWM value that relates to speed
PID PID_leftMotor(&speed_act_left, &speed_cmd_left, &speed_req_left, PID_left_param[0], PID_left_param[1], PID_left_param[2], DIRECT);          //Setting up the PID for left motor
PID PID_rightMotor(&speed_act_right, &speed_cmd_right, &speed_req_right, PID_right_param[0], PID_right_param[1], PID_right_param[2], DIRECT);   //Setting up the PID for right motor

//Input Only PIN
int PIN_ENCOD_A_MOTOR_LEFT = 35;  //Left motor encoder pin 34
int PIN_ENCOD_A_MOTOR_RIGHT = 34; //Right motor encoder pin 35




                                                                            // FUNCTION DEFINITION
void connectWiFi(){
  Serial.begin(115200);
  WiFi.begin(SSID,PASSWORD);
    Serial.print("WiFi connecting");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }

  Serial.println(" connected");
  //nh.initNode();
  //nh.advertise(chatter);
  delay(10);  
}

void setupMotor(){

  robotMotors.begin();
  
  Serial.begin(115200);

  // testing
  Serial.println("DC Motor Ready...");
}

void setupPID(){
  
   //setting PID parameters
  PID_leftMotor.SetSampleTime(95);
  PID_rightMotor.SetSampleTime(95);
  PID_leftMotor.SetOutputLimits(-max_speed, max_speed);
  PID_rightMotor.SetOutputLimits(-max_speed, max_speed);
  PID_leftMotor.SetMode(AUTOMATIC);
  PID_rightMotor.SetMode(AUTOMATIC);
    Serial.println("PID  Ready...");
  
}

void setEncoder(){

  // Define the rotary encoder for left motor
  pinMode(PIN_ENCOD_A_MOTOR_LEFT, INPUT); 
  //pinMode(PIN_ENCOD_B_MOTOR_LEFT, INPUT); 
  digitalWrite(PIN_ENCOD_A_MOTOR_LEFT, HIGH);                // turn on pullup resistor
  //digitalWrite(PIN_ENCOD_B_MOTOR_LEFT, HIGH);
  attachInterrupt(0, encoderLeftMotor, RISING);

  // Define the rotary encoder for right motor
  pinMode(PIN_ENCOD_A_MOTOR_RIGHT, INPUT); 
  //pinMode(PIN_ENCOD_B_MOTOR_RIGHT, INPUT); 
  digitalWrite(PIN_ENCOD_A_MOTOR_RIGHT, HIGH);                // turn on pullup resistor
  //digitalWrite(PIN_ENCOD_B_MOTOR_RIGHT, HIGH);
  attachInterrupt(1, encoderRightMotor, RISING);

    Serial.println("Encoder Ready...");
  
}

//function that will be called when receiving command from host
void handle_cmd (const geometry_msgs::Twist& cmd_vel) {
  noCommLoops = 0;                                                  //Reset the counter for number of main loops without communication
  
  speed_req = cmd_vel.linear.x;                                     //Extract the commanded linear speed from the message

  angular_speed_req = cmd_vel.angular.z;                            //Extract the commanded angular speed from the message
  
 // speed_req_left = speed_req - angular_speed_req*(wheelbase/2);     //Calculate the required speed for the left motor to comply with commanded linear and angular speeds
 // speed_req_right = speed_req + angular_speed_req*(wheelbase/2);    //Calculate the required speed for the right motor to comply with commanded linear and angular speeds


    // turning push j or l
    if(speed_req == 0 && angular_speed_req != 0){  
        speed_req_left = angular_speed_req * wheelbase / 2.0;
        speed_req_right = (-1) * speed_req_left;
    }
    // forward / backward push i or <
    else if(angular_speed_req == 0 && speed_req != 0){ 
        speed_req_left = speed_req_right = speed_req;
    }
    // turn slowly push u or o or m or >
    else if (angular_speed_req != 0 && speed_req != 0){ 
        //speed_req_left = speed_req - angular_speed_req * wheelbase / 2.0;
        //speed_req_right = speed_req + angular_speed_req * wheelbase / 2.0;
        
      if (speed_req > 0 && angular_speed_req > 0) { // turn left fwd slow push u
          speed_req_left = speed_req;
          speed_req_right = 0;
      }
      else if (speed_req > 0 && angular_speed_req < 0) { // turn right fwd slow push o
          speed_req_right = speed_req;
          speed_req_left = 0;
      }
      else if (speed_req < 0 && angular_speed_req < 0) { // turn left bwd slow push m
          speed_req_left =  speed_req;
          speed_req_right = 0;
      }
      else if (speed_req < 0 && angular_speed_req > 0) { // turn right bwd slow push >
          speed_req_right = speed_req;
          speed_req_left = 0; 
      }         

      
    }

 /* Serial.print("speed_req_left... ");
  Serial.print(speed_req_left);
  Serial.print("speed_req_right... ");
  Serial.println(speed_req_right);  */
}


                                                                                    // ROS SUBSCRIBER

ros::Subscriber<geometry_msgs::Twist> cmd_vel("cmd_vel", handle_cmd);   //create a subscriber to ROS topic for velocity commands (will execute "handle_cmd" function when receiving data)



                                                                                   // NODE HANDLER SETUP

void initNodeHandler(){

  nh.initNode();
  nh.advertise(chatter);

  nh.subscribe(cmd_vel);    //suscribe to ROS topic for velocity commands
  nh.advertise(speed_pub);  //prepare to publish speed in ROS topic

  Serial.println("Node_handler_ready");  
}
                      

                                                                                   // SETUP ALL

void setup() {

  connectWiFi();
  initNodeHandler();
  setupMotor();

  setupPID();
  setEncoder();
}
                                                                                   //LOOP MAIN
void loop() {



  if((millis()-lastMilli) >= LOOPTIME)   
  {                                                                           // enter timed loop
    lastMilli = millis();
   
    
    if (abs(pos_left) < 5){                                                   //Avoid taking in account small disturbances
      speed_act_left = 0;
    }
    else {
      speed_act_left=((pos_left/encoder_cpr)*2*PI)*(1000/LOOPTIME)*radius;           // calculate speed of left wheel
    }
    
    if (abs(pos_right) < 5){                                                  //Avoid taking in account small disturbances
      speed_act_right = 0;
    }
    else {
    speed_act_right=((pos_right/encoder_cpr)*2*PI)*(1000/LOOPTIME)*radius;          // calculate speed of right wheel
    }
    
    pos_left = 0;
    pos_right = 0;

    speed_cmd_left = constrain(speed_cmd_left, -max_speed, max_speed);
    PID_leftMotor.Compute();                                                 
    // compute PWM value for left motor. Check constant definition comments for more information.
    PWM_leftMotor = constrain(((speed_req_left+sgn(speed_req_left)*min_speed_cmd)/speed_to_pwm_ratio) + (speed_cmd_left/speed_to_pwm_ratio), -255, 255); //
    
    if (noCommLoops >= noCommLoopMax) {                   //Stopping if too much time without command
      robotMotors.brake(leftMotor);
      leftMotorCW=false;
      leftMotorCCW=false;      
    }
    else if (speed_req_left == 0){                        //Stopping
      robotMotors.brake(leftMotor);
      leftMotorCW=false;
      leftMotorCCW=false;
    }
    else if (PWM_leftMotor > 0){                          //Going forward
      robotMotors.rotate(leftMotor, abs(PWM_leftMotor), CW);
      leftMotorCW=true;
      leftMotorCCW=false;  
      
    }
    else {                                               //Going backward
      robotMotors.rotate(leftMotor, abs(PWM_leftMotor), CCW);
      leftMotorCW=false;
      leftMotorCCW=true; 
    }
    
    speed_cmd_right = constrain(speed_cmd_right, -max_speed, max_speed);    
    PID_rightMotor.Compute();                                                 
    // compute PWM value for right motor. Check constant definition comments for more information.
    PWM_rightMotor = constrain(((speed_req_right+sgn(speed_req_right)*min_speed_cmd)/speed_to_pwm_ratio) + (speed_cmd_right/speed_to_pwm_ratio), -255, 255); // 

    if (noCommLoops >= noCommLoopMax) {                   //Stopping if too much time without command
      robotMotors.brake(rightMotor);
      rightMotorCW=false;
      rightMotorCCW=false; 
      
    }
    else if (speed_req_right == 0){                       //Stopping
      robotMotors.brake(rightMotor);
      rightMotorCW=false;
      rightMotorCCW=false;
    }
    else if (PWM_rightMotor > 0){                         //Going forward
      robotMotors.rotate(rightMotor, abs(PWM_leftMotor), CW);
      rightMotorCW=true;
      rightMotorCCW=false;
    }
    else {                                                //Going backward
      robotMotors.rotate(rightMotor, abs(PWM_leftMotor), CCW);
      rightMotorCW=false;
      rightMotorCCW=true;
    }

    if((millis()-lastMilli) >= LOOPTIME){         //write an error if execution time of the loop in longer than the specified looptime
      Serial.println(" TOO LONG ");
    }

    noCommLoops++;
    if (noCommLoops == 65535){
      noCommLoops = noCommLoopMax;
    }
    
    publishSpeed(LOOPTIME);   //Publish odometry on ROS topic

/*
   Serial.print("PWM_leftMotor ");
   Serial.print(PWM_leftMotor);
   Serial.print("PWM_rightMotor ");
   Serial.println(PWM_rightMotor);    */
  
  }
  
  nh.spinOnce();
  delay(200);
}
 
                                                                    // FUNCTIONS 
                                                
//Publish function for odometry, uses a vector type message to send the data (message type is not meant for that but that's easier than creating a specific message type)
void publishSpeed(double time) {
  speed_msg.header.stamp = nh.now();      //timestamp for odometry data
  speed_msg.vector.x = speed_act_left;    //left wheel speed (in m/s)
  speed_msg.vector.y = speed_act_right;   //right wheel speed (in m/s)
  speed_msg.vector.z = time/1000;         //looptime, should be the same as specified in LOOPTIME (in s)
  speed_pub.publish(&speed_msg);
  nh.spinOnce();
  //nh.loginfo("Publishing odometry");
}
                                                
//Left motor encoder counter
void encoderLeftMotor() {
  //if (digitalRead(PIN_ENCOD_A_MOTOR_LEFT) == digitalRead(PIN_ENCOD_B_MOTOR_LEFT)) pos_left++;
  //else pos_left--;
    if (digitalRead(PIN_ENCOD_A_MOTOR_LEFT) && leftMotorCW)  pos_left++;
      
    else if (digitalRead(PIN_ENCOD_A_MOTOR_LEFT) && leftMotorCCW) pos_left--;
  
}

//Right motor encoder counter
void encoderRightMotor() {
  //if (digitalRead(PIN_ENCOD_A_MOTOR_RIGHT) == digitalRead(PIN_ENCOD_B_MOTOR_RIGHT)) pos_right--;
  //else pos_right++;
    if (digitalRead(PIN_ENCOD_A_MOTOR_RIGHT) && rightMotorCW)  pos_right++;
      
    else if (digitalRead(PIN_ENCOD_A_MOTOR_RIGHT) && rightMotorCCW) pos_right--;
}

template <typename T> int sgn(T val) {
return (T(0) < val) - (val < T(0));
}
