//Team Super Awesome Mega Cool Mini Project
//2/26/2023

//include Wire.h and address for easy i2c communication between the arduino and pi
#include <Wire.h>
#define SLAVE_ADDRESS 0x04
//the arduino by default will use A4 and A5 for sending and recieving data.

//*****
//pin out definitions for where everything should be connected to.
//*****

//encoder pins for left (1) and right (2) encoders
#define CLK_A 2
#define DT_B 5

#define CLK_A2 3
#define DT_B2 6

//motor pins
#define PWM 9
#define M1DIR 7 
#define M2DIR 8
#define nD2 4

//pin for a button that can reset the current motor position to be the starting zero position.
#define resetPin 13


//*****
//creates global variables that will be used in the future.
//*****

//global variables for left (1st) encoder
int count=0;
int A;
int B;
int Alast;
int Blast;
double velocityLeft=0;
unsigned long time1;
int deltaTimeLeft=0;
double thetaLeftNew=0;
double thetaLeftOld=0;

//global variables for right (2nd) encoder.
int count2=0;
int A_2;
int B_2;
int Alast2;
int Blast2;
double velocityRight=0;
unsigned long time2;
int deltaTimeRight=0;
double thetaRightNew=0;
double thetaRightOld=0;

//defines constant variables that will be used in the future.
double const radius=0.05;
double const b = 0.1;

//defines and sets starting positions for the robot.
double posX=0;
double posY=0;
double theta=0;

//setpoint that is read over i2c communcation that will tell the motor which position to move to. (will range from 1-4)
int setpoint = 0;


//*****
//transfer function values
//*****

float previousError = 0;
float integralError = 0;

//use previous micros starting at zero to serve as a starting point for the sample rate of the main program
int interval = 5000; //use as the sample rate (the amount of time that the program should wait between sampling values)

//use the encoder code to repeatedly calculate a value for the angular position of the wheels
double previousAngularPosition = 0;
double currentAngularPosition = 0;

//the targetAngularPosition will be changed via the Aruco camera to 0, pi/2, pi, or 3pi/2
double pi = 3.14159;
double targetAngularPosition = 1;

double angularVelocity = 0; //use the previous angularPosition and compare with the current angular position as well as the interval rate (time over which the position has changed)
//in order to determine the angularVelocity of the wheel

unsigned long previousmicros = 0;
int power = 0;

int previousCount = 0;
double angularVelocityMotor = 0;



void setup() {

  // Starts Serial to be able to print out and read encoder values
  Serial.begin(250000);

  // initialize i2c as slave
  Wire.begin(SLAVE_ADDRESS);

  //define callbacks for i2c communication
  Wire.onReceive(receiveData);
  Wire.onRequest(sendData);
  Serial.println("Ready!");


  //defines the different pins on the Arduino as inputs to read the values of the encoder.
  //pin for a button that can reset the current motor position to be the starting zero position.
  pinMode(resetPin, INPUT_PULLUP);
  
  //pinmode for both motor encoders
  pinMode(CLK_A, INPUT);
  pinMode(DT_B, INPUT);
  pinMode(CLK_A2, INPUT);
  pinMode(DT_B2, INPUT);

  //pinmode for the motor and motor shield
  pinMode(M1DIR, OUTPUT);
  pinMode(M2DIR, OUTPUT);
  pinMode(PWM, OUTPUT);
  pinMode(nD2, OUTPUT);

  //immediately set D2 to high to allow motor to be moved
  digitalWrite(nD2, HIGH);

  //defines Alast which will immediently be used in the loop for monitoring the count rotation state of the encoder.
  Alast = digitalRead(CLK_A);
  Alast2 = digitalRead(CLK_A2);

  //attachs the interrupt function
  //uses 0 or 1 for either the 2nd or 3rd pin interrupt, the function assossiacted with the interupt, and change in A.
  attachInterrupt(0,A_ISR , CHANGE);
  attachInterrupt(1,A_ISR2, CHANGE);

  //set the starting times to the current time
  time1=micros();
  time2=micros();

  //prints out the starting position values and time
  analogWrite(PWM, 0);
  output();   
  //delay(1000);
  //previousmicros = micros();
}



/// BEGIN MAIN


void loop() {
  //checks every loop to see if the button is reset to reset the count to 0. 
  if(digitalRead(resetPin) == 0){
    count = 0;
  }
  //update the current time
  unsigned long currentmicros = micros();

  //using the given sampling rate only sample when the rate has occurred



  if (currentmicros - previousmicros >= interval) {

   if(micros() >= 1000000){
    power = 125;
    powerMotor(1, power);
   }
   else{
     power = 0;
     powerMotor(1, power);
   }
    if(micros() <= (2000000)){

      angularVelocityMotor = ((toRadians(count) - toRadians(previousCount) / (currentmicros - previousmicros)));


      output();
    }
    //at the end of our sampling check to ensure that the duration we have spent taking our sample has not exceeded our actual sample rate
    //in the case that it has exceeded the sampling rate print an error
    
    if(currentmicros - previousmicros >= interval){
      Serial.print("ERROR: RUN TIME HAS EXCEEDED SAMPLING TIME");
      Serial.println();
    }
    
    //Serial.println("END SAMPLE PERIOD");
    previousmicros = currentmicros;
  }

}



//END MAIN





void A_ISR(){
  //This is the ISR function that triggers anytime that there is a change in A.

  //reads the current values of A and B
  A=digitalRead(CLK_A);
  B=digitalRead(DT_B);

  //checks whether the the encoder is moving clockwise or counter clockwise based on the current and past value of A and the current state of B.
  if(A != Alast){
    if(digitalRead(DT_B)!=A){
      count++;
    }else{
      count--;
    }
  }
  //updates the last value of A
  Alast=A;
  
}

void A_ISR2(){
  //This is the ISR function that triggers anytim xe that there is a change in A2.

  //reads the current values of A and B
  A_2=digitalRead(CLK_A2);
  B_2=digitalRead(DT_B2);
  
  //checks whether the the encoder is moving clockwise or counter clockwise based on the current and past value of A and the current state of B.
  if(A_2 != Alast2){
    if(digitalRead(DT_B2)!=A_2){
      count2++;
    }else{
      count2--;
    }
  }
  //updates the last value of A
  Alast2=A_2;

}

void output() {
  //grabs the new current theta position for each wheel in radians
  thetaLeftNew = toRadians(count);
  thetaRightNew = toRadians(count2);
  
  //Updates the left and right wheel velocities every time the output function is called, giving a good average velocity
  //Before doing this it checks to see if there is a time difference as not to produce an error or infinity value when dividing by 0
  if(micros()-time1 != 0)
  velocityLeft = radius * 1000000.0*(thetaLeftNew - thetaLeftOld)/(micros()-time1);

  if(micros()-time2 != 0)
  velocityRight = radius * 1000000.0*(thetaRightNew - thetaRightOld)/(micros()-time2);

  //updates the last time the output was called and the moves the newly read theta value into the old theta value
  time1 = micros();
  time2 = micros();
  thetaLeftOld = thetaLeftNew;
  thetaRightOld = thetaRightNew;
  
  //calculates the X, Y, and phi positions using the given equations 
  posX = posX + cos(theta)*(velocityLeft + velocityRight)/2.0;
  posY = posY + sin(theta)*(velocityLeft + velocityRight)/2.0;
  theta = theta + ( 1/b*(velocityLeft - velocityRight) );

  //prints out all the associated values to the terminal
  Serial.print( micros() );
  Serial.print("\t");
  //Serial.print(velocityRight,2);  
  Serial.print(velocityLeft, 2);
  /*Serial.print("\t");
  Serial.print(posX,3);
  Serial.print("\t");
  Serial.print(posY,3);
  Serial.print("\t"); 
  Serial.print(theta,3);
  Serial.print("\t");*/ 
  //Serial.println(count2);
  //Serial.print("\t"); 
  //Serial.print(toRadians(count));
  Serial.print("\t");
  Serial.print(power);

  Serial.println();
  
}

double toRadians(int count){
  return count*2*PI/1600.0;
}

//used to apply power and direction to a dc motor connected to the arduino
void powerMotor(int direction, int setPWM){

  //use analog write to set the PWM value of the motor
  analogWrite(PWM, setPWM);

  //now based off the direction input (1 for forward !1 for backward) apply the voltage to the motor
  if(direction == 1){
    //motor moves wheel forward
    digitalWrite(M1DIR, LOW);
  } 
  else if(direction == -1){ 
    //motor moves wheel backward
    digitalWrite(M1DIR, HIGH);
  }

}

//function to recieve data from pi
void receiveData(int byteCount) { 
  setpoint = int(Wire.read()); //reading setpoint from pi
  setpoint = int(setpoint - '0');
}

// callback for sending data
void sendData(){
  byte low = byte(count); //giving variable low first 8 bits to send
  byte high = byte(count >> 8); //giving variable high second 8 bits
  Wire.write(high); //sends high 8 bits
  Wire.write(low); //sends low 8 bits
}