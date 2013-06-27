#include <TimerOne.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"

/*Pin delcarations*/
#define IRInterrupt   0 //IR Detector Interrupt correspondes to pin 2
const int buttonPin = 8;//the button pin
const int ledPin    = 13;
const int motor_neg = 9;
const int motor_pos = 10;
/*End Pin Declarations*/

/*IR Declarations*/
#define MAXPULSE 1000 //longest we will for a single pulse 
#define FUZZINESS 20 // What percent we will allow in variation to match the same code
volatile uint16_t signal[100]; //array to store the recieved IR signal

//COMMAND arrays stored in EEPORM
uint16_t NUMBER_Data[100];
uint16_t UP_Data[100];
uint16_t DOWN_Data[100];
uint16_t STOP_Data[100];

int recieved_signal = 0;
boolean valid_signal;
volatile unsigned long current_pulses;
volatile int IR_input = 0;
volatile int IR_state = 0;
volatile boolean Read_IR = false;
static unsigned long last_interrupt_time = 0;
boolean same_code = false;
int data_number = 0; //which command is being decoded. 0 is NUMBER
/*End IR Declarations*/

int counter = 0;

void setup(void) {
  
  Serial.begin(9600);   
  
  pinMode(buttonPin, INPUT);
  pinMode(ledPin, OUTPUT);  
  pinMode(motor_neg, OUTPUT); 
  pinMode(motor_pos, OUTPUT); 
  
  // initialize the IR Reciever interrupt to occur on both rising and falling edges
  attachInterrupt(IRInterrupt,IR_interrupt_handler, CHANGE);
  
  //read the co currently stored in EEPROM
  EEPROM_readAnything(0,NUMBER_Data); 
  EEPROM_readAnything(200,UP_Data);
  EEPROM_readAnything(400,DOWN_Data);
  EEPROM_readAnything(600,STOP_Data);
  
  Serial.println("Ready to Roll!");  
}

void loop(){
  
  if (Read_IR == true){
    /*If we have just read an complete IR_code this section determines 
    what to do with it*/
    valid_signal = correctpulses();
    if (valid_signal == true){
      if (data_number != 0){//if button is pressed wait one seconds
        if (digitalRead(buttonPin)==1){ //if button is still pressed than save new code to proper EEPROM
          switch(data_number){
            case 1:{
              Serial.println("Number");
              write_signal_to_EEPROM(0);
              EEPROM_readAnything(0,NUMBER_Data);
              data_number ++;
              break;
            }
            case 2:{
              Serial.println("Up");
              write_signal_to_EEPROM(200);
              EEPROM_readAnything(200,UP_Data);
              data_number ++;
              break;   
            }    
            case 3:{
              Serial.println("Down");
              write_signal_to_EEPROM(400);
              EEPROM_readAnything(400,DOWN_Data);
              data_number ++;
              break;  
            }  
            case 4:{
              Serial.println("Stop");
              write_signal_to_EEPROM(600);
              EEPROM_readAnything(600,STOP_Data);
              data_number = 0;
              break; 
            }
          }//end case
        }//end if button still on
        else{//if button is not still on
          data_number = 0;
        }//end if but not still on
      }//end if data_number != 0
      
     else{ //compare to already stored codes
        if (compare_IR(NUMBER_Data)){
          recieved_signal = 1;
        }
        else if (compare_IR(UP_Data)){
          recieved_signal = 2;
        }
        else if (compare_IR(DOWN_Data)){
          recieved_signal = 3;
        }
        else if (compare_IR(STOP_Data)){
          recieved_signal = 4;
        }
        else{
          recieved_signal = 0; //signal did not match
        }
     }//end compare routine
    }//end if valid_signal
    Read_IR = false;
  }//end if READ_IR
  
  else if (IR_state != 0){
    /*Serves as a time out to determine when the last IR Code has been sent.
    After completeing the delay this resets the IR_state and tells the loop()
    to Read the new IR code*/
    delay(MAXPULSE);
    IR_state = 0;
    Read_IR = true;
  }//end else if
  
  else if (digitalRead(buttonPin)==1){
    /*Handles encoding in EEPROM sitiutations between IR interrupts*/
    if (digitalRead(buttonPin)==1 && data_number == 0){
      blink_led(3);
      if (digitalRead(buttonPin)==1 && data_number == 0){
        Serial.println("ready");
        data_number = 1;
      }//end if button 2
    }//end if button 1
    if (digitalRead(buttonPin)==1 && data_number != 0){
      while(IR_state == 0 && digitalRead(buttonPin)==1){
        delay(100);
      }//end while
    }//end if data_number != 0
  }//end if read button pin
  
  else{
    /*What to do when not dealing with IR codes*/
    if (recieved_signal != 0){ //what to do if a signal is recieved
      recieved_signal_handler(recieved_signal);
    }
    counter ++;
    delay(1000);
    //Serial.print("counter= ");Serial.println(counter);
  }//end else
  
}//end loop

void recieved_signal_handler(int the_recieved_signal){
  /*determines how recieved signals are handled, respectively*/
  switch (the_recieved_signal){
    case 1:{
      Serial.println("Number");
      break;
    }
    case 2:{
      Serial.println("Up");
      digitalWrite(motor_neg,HIGH);
      digitalWrite(motor_pos,LOW);
      break;
    }
    case 3:{
      Serial.println("Down");
      digitalWrite(motor_neg,LOW);
      digitalWrite(motor_pos,HIGH);
      break;
    }
    case 4:{
      Serial.println("Stop");
      digitalWrite(motor_neg,LOW);
      digitalWrite(motor_pos,LOW);
      break;
    }
  }//end case
  recieved_signal = 0;
}//end recieved signal handler

void IR_interrupt_handler(){
  /*Works like a state machine. IR_states indicate not recieving, low, and high voltage.
  Since the interrupt will occur on both rising and falling edges we always know which state
  should be next. A delay() timer in the loop() determines when the transmission is finsihed*/
  
  unsigned long interrupt_time = micros(); //get the current time since the arduino was turned on
  unsigned long time_delta = interrupt_time - last_interrupt_time; //determine time since last interrupt
  last_interrupt_time = interrupt_time; //set last interrupt time to current
  
  switch (IR_state){ //controls what to do in each case. Controls have to be short so we don't miss the next interrupt
    
    case 0:{//0 is the idle state. On the first interrupt recieved we leave the idle state
      current_pulses = 0; //it's the first pulse being recieved
      IR_state = 1;//on the next interrupt we should be in IR_state 1
      break;
    }//end state 0
    
    case 1:{ //1 is LOW voltage
      signal[current_pulses] = time_delta;
      current_pulses ++;
      IR_state = 2;
      break;
    }//end state 1
    
    case 2:{ //1 is HIGH voltage
      signal[current_pulses] = time_delta;
      current_pulses ++;
      IR_state = 1;
      break;
    }//end state2
  }//end switch case
}//end IR_interrupt_handler

int write_signal_to_EEPROM(int addr){
  /*decodes one COMMAND array of IR codes into an EEPROM address, addr, 
  and stores addr into address addr_marker for later use*/
  unsigned int bytes;
    bytes = EEPROM_writeAnything(addr,signal);//COMMAND array address stored at address addr
    addr = addr + bytes; //next address
    Serial.print("bytes: ");
    Serial.println(bytes); 
    return(addr);
}//end decode_into_EEPROM

boolean compare_IR(unsigned int compare_array[]){
  /*compares the current signal to a given compare_array signal*/
  boolean same_code = true;
  int delta;
  for (int i=0; i<200; i++){
    
    //Serial.print("sig: ");Serial.print(signal[i]);Serial.print(" data: ");Serial.println(compare_array[i]);
    if ((signal[i] !=0) && (compare_array[i] != 0)){
      delta = signal[i]-compare_array[i];
      delta = abs(delta);
      if (delta >= signal[i]*FUZZINESS/100){ //delta is greater than FUZZINESS percentage
        same_code = false;
        break;
      }//end if
    }//end if
    
    else{ //if signal[i]==0
      break; //break because its the end of the transmission
    }//end else
    
  }//end for
  
  return(same_code);
}//end compare_IR
    
boolean correctpulses(){
  /*Divides the recieved pulses by ten to throw out the least significant digit.
  Checks for valid signal (i.e. a long enough transmission) incase of an unwanted interrupt*/
  int pulse_counter = 0; //count number of recieved pulses
  for (int i=0; i<200; i++){
    signal[i] = signal[i]/10;
    if (signal[i] != 0){
      pulse_counter ++;
    }
    else{
      break;
    }//end else
  }//end for
  if (pulse_counter > 4){ //at least 5 pulses must be reciever for a valid input
    return(true);
  }//end if
  else{
    return(false);
  }//end else
}//end correct pulses

void print_signal(uint16_t print_signal[]) {
 /*print IR code array data for debug only. Must use correct array in this function manually*/
  Serial.println("uint16_t print_signal IRsignal[] = {");
  Serial.println("// LOW, HIGH Voltage (in 10's of microseconds)");
  for (int i = 0; i < 100; i=i+2) {
    Serial.print("\t"); // tab
    Serial.print(print_signal[i]);
    Serial.print(", ");
    Serial.print(print_signal[i+1]);
    Serial.println("");
  }
  Serial.print("\t"); // tab
  Serial.print("};");
  Serial.println("");
}

void blink_led(int blink_count){
  /*Blinks an LED a given number of times*/
  for (int i=0; i < blink_count; i++){
      digitalWrite(ledPin, HIGH);
      delay(500);
      digitalWrite(ledPin, LOW);
      delay(500);
  } //end for loop
}//end blink_led

