#include <Arduino.h>
#include <Servo.h>

// Servo configuration
Servo myServo;
const int servoPin = 9;

void setup() {
  // Initialize serial communication (default pins: RX=0, TX=1)
  Serial.begin(9600);
  
  // Attach servo to pin 9
  myServo.attach(servoPin);
  
  // Set initial position to 0 degrees
  myServo.write(90);
  
  Serial.println("Servo control ready!");
  Serial.println("Send '1' to open servo to 90 degrees");
  Serial.println("Send '0' to close servo to 0 degrees");
}

void loop() {
  // Check if data is available on serial port
  if (Serial.available() > 0) {
    char command = Serial.read();
    
    if (command == '1') {
      // Open servo to 90 degrees
      myServo.write(180);
      Serial.println("Servo opened to 90 degrees");
    }
    else if (command == '0') {
      // Close servo to 0 degrees
      myServo.write(90);
      Serial.println("Servo closed to 0 degrees");
    }
    else {
      Serial.println("Invalid command. Send '1' or '0'");
    }
    
    // Clear any remaining characters in the serial buffer
    while (Serial.available() > 0) {
      Serial.read();
    }
  }
}