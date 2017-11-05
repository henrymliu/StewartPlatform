#include <SPI.h>
#include "HWDefs.h"

//Initialize PWM value variables
int PWM[NUM_MOTORS];
int MaxMotorPWM;

//Initialize direction value variables
MotorDirection Direction[NUM_MOTORS];

//Initialize position value variables
int Position[NUM_MOTORS];
// int ZeroPosition[NUM_MOTORS] = { 0, 200, 192, 191, 194, 200 };
int ZeroPosition[NUM_MOTORS] = { 199, 186, 194, 194, 192, 199 };
//int MaxPosition[NUM_MOTORS] = { 1024, 822, 821, 825, 826, 829 };
int MaxPosition[NUM_MOTORS] = { 833, 827, 826, 829, 831, 833 };

//Initialize desired position value variables												
int DesiredPosition[NUM_MOTORS];

//Initialize correct position check variables
boolean CorrectPosition[NUM_MOTORS];

//Initialize serial input variable
int InputData[2];
int InputArray[NUM_MOTORS];

//Initialize time and time between serial port writing variables
unsigned long CurrentTime;
unsigned long PreviousTime;
unsigned long SendInterval;

//Initialize index variables
int IndexSerialInput;
int MotorCounter;

/******* Set up L9958 chips *********
' L9958 Config Register
' Bit
'0 - RES
'1 - DR - reset
'2 - CL_1 - curr limit
'3 - CL_2 - curr_limit
'4 - RES
'5 - RES
'6 - RES
'7 - RES
'8 - VSR - voltage slew rate (1 enables slew limit, 0 disables)
'9 - ISR - current slew rate (1 enables slew limit, 0 disables)
'10 - ISR_DIS - current slew disable
'11 - OL_ON - open load enable
'12 - RES
'13 - RES
'14 - 0 - always zero
'15 - 0 - always zero
*/
unsigned int configWord = 0b0000010000001100;

void setup() {
	// L9958 Slave Select Pins
	pinMode(SlaveSelectPinMotor1, OUTPUT); digitalWrite(SlaveSelectPinMotor1, LOW); // HIGH = not selected
	pinMode(SlaveSelectPinMotor2, OUTPUT); digitalWrite(SlaveSelectPinMotor2, LOW);
	pinMode(SlaveSelectPinMotor3, OUTPUT); digitalWrite(SlaveSelectPinMotor3, LOW);
	pinMode(SlaveSelectPinMotor4, OUTPUT); digitalWrite(SlaveSelectPinMotor4, LOW);
	pinMode(SlaveSelectPinMotor5, OUTPUT); digitalWrite(SlaveSelectPinMotor5, LOW);
	pinMode(SlaveSelectPinMotor6, OUTPUT); digitalWrite(SlaveSelectPinMotor6, LOW);

	// L9958 Direction Pins
	pinMode(DirectionPinMotor1, OUTPUT);
	pinMode(DirectionPinMotor2, OUTPUT);
	pinMode(DirectionPinMotor3, OUTPUT);
	pinMode(DirectionPinMotor4, OUTPUT);
	pinMode(DirectionPinMotor5, OUTPUT);
	pinMode(DirectionPinMotor6, OUTPUT);

	// L9958 PWM Pins
	pinMode(PWMPinMotor1, OUTPUT); digitalWrite(PWMPinMotor1, LOW);
	pinMode(PWMPinMotor2, OUTPUT); digitalWrite(PWMPinMotor2, LOW);
	pinMode(PWMPinMotor3, OUTPUT); digitalWrite(PWMPinMotor3, LOW);
	pinMode(PWMPinMotor4, OUTPUT); digitalWrite(PWMPinMotor4, LOW);
	pinMode(PWMPinMotor5, OUTPUT); digitalWrite(PWMPinMotor5, LOW);
	pinMode(PWMPinMotor6, OUTPUT); digitalWrite(PWMPinMotor6, LOW);

	// L9958 Enable for all motors
	pinMode(ENABLE_MOTORS1, OUTPUT); digitalWrite(ENABLE_MOTORS1, HIGH); // HIGH = disabled
	pinMode(ENABLE_MOTORS2, OUTPUT); digitalWrite(ENABLE_MOTORS2, HIGH); // HIGH = disabled

	// SPI stuff
	// See: https://www.arduino.cc/en/Reference/SPI
	SPI.begin();
	SPI.setBitOrder(LSBFIRST);
	SPI.setDataMode(SPI_MODE1); // clock polarity = low, phase = high

	// Motor 1
	digitalWrite(SlaveSelectPinMotor1, LOW);
	SPI.transfer(lowByte(configWord));
	SPI.transfer(highByte(configWord));
	digitalWrite(SlaveSelectPinMotor1, HIGH);

	// Motor 2
	digitalWrite(SlaveSelectPinMotor2, LOW);
	SPI.transfer(lowByte(configWord));
	SPI.transfer(highByte(configWord));
	digitalWrite(SlaveSelectPinMotor2, HIGH);

	// Motor 3
	digitalWrite(SlaveSelectPinMotor3, LOW);
	SPI.transfer(lowByte(configWord));
	SPI.transfer(highByte(configWord));
	digitalWrite(SlaveSelectPinMotor3, HIGH);

	// Motor 4
	digitalWrite(SlaveSelectPinMotor4, LOW);
	SPI.transfer(lowByte(configWord));
	SPI.transfer(highByte(configWord));
	digitalWrite(SlaveSelectPinMotor4, HIGH);

	// Motor 5
	digitalWrite(SlaveSelectPinMotor5, LOW);
	SPI.transfer(lowByte(configWord));
	SPI.transfer(highByte(configWord));
	digitalWrite(SlaveSelectPinMotor5, HIGH);

	// Motor 6
	digitalWrite(SlaveSelectPinMotor6, LOW);
	SPI.transfer(lowByte(configWord));
	SPI.transfer(highByte(configWord));
	digitalWrite(SlaveSelectPinMotor6, HIGH);

	digitalWrite(ENABLE_MOTORS1, LOW);// LOW = enabled
	digitalWrite(ENABLE_MOTORS2, LOW);// LOW = enabled
									  //Set initial actuator settings to pull at 0 speed for safety
	ResetToZero();
	delay(4000);
	for (MotorCounter = 0; MotorCounter < NUM_MOTORS; MotorCounter++) {
		DesiredPosition[MotorCounter] = 0;
		PWM[MotorCounter] = 0;
	}

	//Set initial previous send times to 0
	PreviousTime = 0;

	//Set Read/Send interval
	SendInterval = 1000;

	//calculate actuator lengths

	Serial.begin(19200);
	Serial.println("Begin");
	
	// ExtendAll();
}

void loop() {
	NormalOp();
	// Calibrate();
	/*
	int motor = 3;
	MoveOne(motor, EXTEND);
	delay(2000);
	MoveOne(motor, RETRACT);
	delay(2000);*/
}

// Triggered when data comes into RX
// example data: -0,20,40,60,80,100+ 
// packet begins with '-', ends with '+'
// comma delimited
//TODO: figure out why additional serial.printlns are required for more reliable (ish) input parsing
void MySerialEvent() {
	//Test Manual Control Mode
	
	for (MotorCounter = 0; MotorCounter < NUM_MOTORS; MotorCounter++) {
		CorrectPosition[MotorCounter] = false; // originally 0
	}

	// max size: 4 bytes per position (6 digits), 7 more bytes for delimiters (commas + ends)
	// so 31 bytes
	//if (Serial.available() >= 31) {
	String inString = "";
	char inChar;
	bool start_found = false;
	bool end_found = false;

	IndexSerialInput = 0;
	// TODO: consider strtok instead, parseInt()
	while (Serial.available() > 0) {
		inChar = Serial.read();

		// don't parse until we find our starting char
		if (!start_found  && inChar == start_char)
		{
			// Serial.println("StartChar found!");
			start_found = true;
		}
		// TODO: if start has been found, and we find a start char again
		else if (!start_found)
		{
			continue;
		}

		// add to temp string if it is a valid digit
		if (isDigit(inChar)) {
			inString += inChar;
		}

		// Parse  temp string to int if delimiter or sentinel found
		if (inChar == delimiter_char || inChar == sentinel_char) {
			// Serial.println("Int parsed!");
			InputArray[IndexSerialInput] = inString.toInt();
			IndexSerialInput++;
			inString = "";
		}

		// if sentinel, break out
		if (start_found  && inChar == sentinel_char)
		{
			Serial.println("Sentinel found!");
			end_found = true;
			break;
		}

		// invalid char
		if (inChar != delimiter_char && inChar != start_char && inChar != sentinel_char && !isDigit(inChar)) {
			// Serial.print("Invalid data due to character with ascii code: ");
			Serial.println(inChar);
			return;
		}
	}

	// validate only if start and end chars found
	if (start_found && end_found)
	{
		// validate and print input array (why not do it in above loop and save a few cycles)
		for (int i = 0; i < sizeof(InputArray) / sizeof(InputArray[0]); i++) {
			if (InputArray[i] > 1023 || InputArray[i] < 0) {
				Serial.println("Invalid data due to Position set outside of range [0,1023]");
				Serial.print("Value of invalid data was: ");
				Serial.println(InputArray[i]);
				Serial.flush(); // "waits until transmission of outgoing data complete"
				return;
			}
		}

		// LEAP controller Mode
		// if (Serial.available() >= MotorNumber)
		// {
		// Serial.readBytes(InputArray, MotorNumber);
		//
		// for (int i = 0; i<=5; i++)
		// {
		// InputArray[i] = 4*InputArray[i];
		// }

		for (int i = 0; i < NUM_MOTORS; i++)
		{
			DesiredPosition[i] = InputArray[i];
		}
		Serial.println("DesiredPositions updated!");
		//DesiredPosition[0] = InputArray[0];
		//DesiredPosition[1] = InputArray[1];
		//DesiredPosition[2] = InputArray[2];
		//DesiredPosition[3] = InputArray[3];
		//DesiredPosition[4] = InputArray[4];
		//DesiredPosition[5] = InputArray[5];
	}
	// Serial.flush();
}

void NormalOp()
{
	// Read potentiometer positions
	Position[0] = analogRead(PotentiometerPinMotor1);
	Position[1] = analogRead(PotentiometerPinMotor2);
	Position[2] = analogRead(PotentiometerPinMotor3);
	Position[3] = analogRead(PotentiometerPinMotor4);
	Position[4] = analogRead(PotentiometerPinMotor5);
	Position[5] = analogRead(PotentiometerPinMotor6);

	for (MotorCounter = 0; MotorCounter < NUM_MOTORS; MotorCounter++) {
		// Set actuator position to [0, 1023]
		//Position[MotorCounter] = (int) (1024 * (Position[MotorCounter] - ZeroPosition[MotorCounter] + 4) / (float)(MaxPosition[MotorCounter] + 4 - ZeroPosition[MotorCounter]));
		Position[MotorCounter] = map(Position[MotorCounter], ZeroPosition[MotorCounter], MaxPosition[MotorCounter], 0, 1024);
	}

	// Trigger our serial function if enough chars are available
	if (Serial.available() >= 31 / 2) {
		MySerialEvent();
	}
	CurrentTime = millis();

	if (CurrentTime - PreviousTime > SendInterval) {
		// Time to send a command

		// Print desired positions
		Serial.println("DesiredPositions");
		for (MotorCounter = 0; MotorCounter < NUM_MOTORS; MotorCounter++) {
			Serial.print(DesiredPosition[MotorCounter]); Serial.print(" ");
		}
		Serial.println("");

		// Print current positions
		Serial.println("Current Positions");
		for (MotorCounter = 0; MotorCounter < NUM_MOTORS; MotorCounter++) {
			Serial.print(Position[MotorCounter]); Serial.print(" ");
		}

		// Print PWM
		Serial.println("\nPWM Values");
		for (MotorCounter = 0; MotorCounter < NUM_MOTORS; MotorCounter++) {
			Serial.print(PWM[MotorCounter]); Serial.print(" ");
		}
		Serial.println("");
		PreviousTime = CurrentTime;
	}

	for (MotorCounter = 0; MotorCounter < NUM_MOTORS; MotorCounter++) {
		// Check motor positions
		if (abs(Position[MotorCounter] - DesiredPosition[MotorCounter]) <= TOL) {
			CorrectPosition[MotorCounter] = true;
			PWM[MotorCounter] = 0;
		}
		else
		{
			CorrectPosition[MotorCounter] = false;
		}

		// If an actuator not in a correct position
		if (!CorrectPosition[MotorCounter]) {

			// If extended further than desired
			if (Position[MotorCounter] > DesiredPosition[MotorCounter]) {
				Direction[MotorCounter] = RETRACT;
				PWM[MotorCounter] = abs(Position[MotorCounter] - DesiredPosition[MotorCounter]);
				PWM[MotorCounter] = 100; // Override with 100??
			}

			// Retracted further than desired
			else if (Position[MotorCounter] < DesiredPosition[MotorCounter]) {
				Direction[MotorCounter] = EXTEND;
				PWM[MotorCounter] = abs(Position[MotorCounter] - DesiredPosition[MotorCounter]);
				PWM[MotorCounter] = 100;
			}
		}

	}
	/*
	MaxMotorPWM = 0;
	for (MotorCounter = 0; MotorCounter <= MotorNumber - 1; MotorCounter++) {
	MaxMotorPWM = max(MaxMotorPWM, PWM[MotorCounter]);
	}
	for (MotorCounter = 0; MotorCounter <= MotorNumber - 1; MotorCounter++) {
	PWM[MotorCounter] = 100 * PWM[MotorCounter] / MaxMotorPWM;
	}
	*/

	digitalWrite(DirectionPinMotor1, Direction[0]); analogWrite(PWMPinMotor1, PWM[0]);
	digitalWrite(DirectionPinMotor2, Direction[1]); analogWrite(PWMPinMotor2, PWM[1]);
	digitalWrite(DirectionPinMotor3, Direction[2]); analogWrite(PWMPinMotor3, PWM[2]);
	digitalWrite(DirectionPinMotor4, Direction[3]); analogWrite(PWMPinMotor4, PWM[3]);
	digitalWrite(DirectionPinMotor5, Direction[4]); analogWrite(PWMPinMotor5, PWM[4]);
	digitalWrite(DirectionPinMotor6, Direction[5]); analogWrite(PWMPinMotor6, PWM[5]);
}

void ResetToZero()
{
	digitalWrite(DirectionPinMotor1, RETRACT); analogWrite(PWMPinMotor1, 1023);
	digitalWrite(DirectionPinMotor2, RETRACT); analogWrite(PWMPinMotor2, 1023);
	digitalWrite(DirectionPinMotor3, RETRACT); analogWrite(PWMPinMotor3, 1023);
	digitalWrite(DirectionPinMotor4, RETRACT); analogWrite(PWMPinMotor4, 1023);
	digitalWrite(DirectionPinMotor5, RETRACT); analogWrite(PWMPinMotor5, 1023);
	digitalWrite(DirectionPinMotor6, RETRACT); analogWrite(PWMPinMotor6, 1023);
}

// Extends all motors.
void ExtendAll()
{
	digitalWrite(DirectionPinMotor1, EXTEND); analogWrite(PWMPinMotor1, 1023);
	digitalWrite(DirectionPinMotor2, EXTEND); analogWrite(PWMPinMotor2, 1023);
	digitalWrite(DirectionPinMotor3, EXTEND); analogWrite(PWMPinMotor3, 1023);
	digitalWrite(DirectionPinMotor4, EXTEND); analogWrite(PWMPinMotor4, 1023);
	digitalWrite(DirectionPinMotor5, EXTEND); analogWrite(PWMPinMotor5, 1023);
	digitalWrite(DirectionPinMotor6, EXTEND); analogWrite(PWMPinMotor6, 1023);
}

// Moves one motor with given direction.
// Prints out raw analogread position per call.
void MoveOne(int motor, MotorDirection dir)
{
	int dirpin = 1, pwmpin = 1, potpin = 1;
	switch (motor)
	{
	case 1:
		dirpin = DirectionPinMotor1;
		pwmpin = PWMPinMotor1;
		potpin = PotentiometerPinMotor1;
		break;
	case 2:
		dirpin = DirectionPinMotor2;
		pwmpin = PWMPinMotor2;
		potpin = PotentiometerPinMotor2;
		break;
	case 3:
		dirpin = DirectionPinMotor3;
		pwmpin = PWMPinMotor3;
		potpin = PotentiometerPinMotor3;
		break;
	case 4:
		dirpin = DirectionPinMotor4;
		pwmpin = PWMPinMotor4;
		potpin = PotentiometerPinMotor4;
		break;
	case 5:
		dirpin = DirectionPinMotor5;
		pwmpin = PWMPinMotor5;
		potpin = PotentiometerPinMotor5;
		break;
	case 6:
		dirpin = DirectionPinMotor6;
		pwmpin = PWMPinMotor6;
		potpin = PotentiometerPinMotor6;
		break;
	}
	digitalWrite(dirpin, dir); analogWrite(pwmpin, 1023);
	Serial.println(analogRead(potpin));
}

void Calibrate()
{
	// Read potentiometer positions
	Position[0] = analogRead(PotentiometerPinMotor1);
	Position[1] = analogRead(PotentiometerPinMotor2);
	Position[2] = analogRead(PotentiometerPinMotor3);
	Position[3] = analogRead(PotentiometerPinMotor4);
	Position[4] = analogRead(PotentiometerPinMotor5);
	Position[5] = analogRead(PotentiometerPinMotor6);

	for (MotorCounter = 0; MotorCounter < NUM_MOTORS; MotorCounter++) {
		Serial.print(Position[MotorCounter]); Serial.print(" ");
	}
	Serial.println("");
}