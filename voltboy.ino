/* Pin definitions ~~~~~~~~~~~~~~~~~~~~~*/
#define P_LED     A0 // should be A0
#define P_MCU_SIG 0
#define P_POW     1  // should be 1
#define P_SW_IN   A1
#define P_VOLT_IN A3 // should be A3
#define P_USB_IN  A2

/* Constants ~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define MIN_VOLTAGE 3.9456 // Minimum allowed battery voltage. 5.8V through 4.7K - 10K voltage divider.

/* Enumerators ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
enum LED_Request {LED_Off, LED_Fade_In, LED_Fade_Out, LED_Pulse_Rapid, LED_Pulse_Slow};
enum Board_State {BOARD_Active, BOARD_Inactive, BOARD_Inactive_USB};

/* Object to represent pushbutton status for debouncing. */
struct Button 
{
  public:
    unsigned int debounce_wait_time_ms; // Time to wait for switch bounce to finish
    unsigned int cooldown_wait_time_ms; // Time to wait after button press before next press
    
  private:
    volatile bool is_triggered; // Has bouncing process begun?
    volatile unsigned long target_time;   // Time at which debouncing is complete
    unsigned long cooldown_time; // Limit time between button presses

  public:
    void ButtonSetup()
    {
      is_triggered = false;
      target_time = 0;
      cooldown_time = 0;
    }
    
    /* Trigger debouncer if not already triggered. Record target time for debounce wait.*/
    void trigger() 
    {
      if ((is_triggered == false) && (millis() > cooldown_time))
      {
        target_time = millis() + debounce_wait_time_ms; // Set target debounce time
        is_triggered = true; // Trigger the debouncer to start
      }
    }
  
    /* Check for a valid pushbutton press in accordance with debouncer. */
    bool pushed()
    {
      // Check that the debouncer is triggered and its wait time has elapsed
      if ((is_triggered == true) && (millis() > target_time) && (digitalRead(P_SW_IN) == false))
      {
        cooldown_time = millis() + cooldown_wait_time_ms; // Set cooldown time
        is_triggered = false; // Untrigger the button object
        return true; // Button has been pushed
      }
      else
        return false;
    }
};

/* Object to represent LED indicator status. */
struct Light
{
  public: 
    unsigned long slow_prd_ms;  // Slow pulse period
    unsigned long rapid_prd_ms; // Rapid pulse period
    unsigned int rapid_n;      // Number of rapid pulses

  private:
    bool brightness;            // Current brightness
    
    enum LED_Request request;  // Request for an LED behaviour
    unsigned long incr_time;   // Target time for next request step, in ms
    unsigned long req_time;    // Target time for next period in ms
    bool request_completed;    // Have we finished the request?
    
    unsigned long rapid_incr_ms; // Rapid pulse step time
    unsigned long slow_incr_ms;  // Slow pulse step time

  public:
    /* Set up private member variables. */
    void LightSetup()
    {
      brightness = LOW;
      digitalWrite(P_LED, brightness);
      
      request = LED_Off;
      incr_time = 0;
      req_time = 0;
      request_completed = true;
  
      // Calculate pulse increments in milliseconds
      rapid_incr_ms = round((float)rapid_prd_ms/2);
      slow_incr_ms  = round((float)slow_prd_ms/2);
    }

    /* Request a new LED indicator behaviour. */
    void makeRequest(enum LED_Request new_request)
    {
      request_completed = false; // Mark request as not completed
  
      switch (new_request) 
      {
        case LED_Off:       break;
        case LED_Fade_In:   break;
        case LED_Fade_Out:  break;
        case LED_Pulse_Rapid: 
          req_time  = millis() + rapid_prd_ms*rapid_n + rapid_incr_ms/2;
          incr_time = millis() + rapid_incr_ms;   
          break;
        case LED_Pulse_Slow:  
          if (request != LED_Pulse_Slow) // Start pulse if not already pulsing
            incr_time = millis() + slow_incr_ms; 
          break;     
      }  

      request = new_request; // Set the new request
    }

    /* Blocks program until current LED request is completed. */
    void waitForRequest()
    {
      if (request != LED_Pulse_Slow) { // Slow pulse never completes, avoid infinite loop
        while (!request_completed)
          updateLED();
      }
    }

    /* Calls appropriate LED behaviour method based on current request */
    void updateLED()
    {
      if (request_completed == false) {
        switch (request) {
          case LED_Off:         ledOff();     break;
          case LED_Fade_In:     fadeIn();     break;
          case LED_Fade_Out:    fadeOut();    break;
          case LED_Pulse_Rapid: pulseRapid(); break;
          case LED_Pulse_Slow:  pulseSlow();  break;  
        }
      }
    }

  private:
    void ledOff()
    {
      brightness = LOW; // Turn LED off
      digitalWrite(P_LED, brightness);
      request_completed = true; // Mark request instantly complete
    }

    void fadeIn()
    {
      if (brightness == HIGH) // Complete the request upon fade-in
        request_completed = true;      
      else {
          brightness = HIGH;               // Set brightness
          digitalWrite(P_LED, brightness); // Update LED
          request_completed = true; 
      }
    }

    void fadeOut()
    {
      if (brightness == LOW) // Complete the request upon fade-in
        request_completed = true;      
      else {
          brightness = LOW;                // Set brightness
          digitalWrite(P_LED, brightness); // Update LED
          request_completed = true; 
      }
    }

    void pulseRapid()
    {
      unsigned long cur_time = millis();     
      if (cur_time > req_time) // Check that request time has elapsed  
        request_completed = true;
        
      else if (cur_time > incr_time)
      {
        brightness = !brightness;             // Invert brightness for pulse          
        digitalWrite(P_LED, brightness);      // Apply brightness
        incr_time = cur_time + rapid_incr_ms; // Increment timer for next brightness step
      }      
    }
  
    void pulseSlow()
    {
      unsigned long cur_time = millis();   
      if (cur_time > incr_time)
      {
        brightness = !brightness;             // Invert brightness for pulse          
        digitalWrite(P_LED, brightness);      // Apply brightness
        incr_time = cur_time + slow_incr_ms;  // Increment timer for next brightness step
      }      
    }
};

/* Major global objects ~~~~~~~~~~~~~~~~~~~~~~~~~~*/
struct Button button; // Pushbutton debouncer object
struct Light light;   // LED management object

/* State machine to handle board state changes, power connection updates and LED indication. */
struct StateMachine
{
  private:
    enum Board_State board_state;
    struct Light* light_ptr;

  public:
    void StateMachineSetup(bool usb_connected, struct Light* new_light_ptr)
    {
      if (usb_connected == true)
        board_state = BOARD_Inactive_USB;
      else
        board_state = BOARD_Active;

      light_ptr = new_light_ptr; // Set pointer to global light object address

      updatePower(); // Update power connections based on state change.
    }

    /* Handles board state transitions. */
    void updateState(bool button_pressed, bool usb_connected)
    {
      enum Board_State new_board_state = board_state; 
      
      switch (board_state)
      {
        case BOARD_Active: 
          if (button_pressed == true)
          {
            if (usb_connected == true)
              new_board_state = BOARD_Inactive_USB; 
            else
              new_board_state = BOARD_Inactive; 
          }
          break;
          
        case BOARD_Inactive_USB:
          if (button_pressed == true)
            new_board_state = BOARD_Active;
          else if (usb_connected == false)
            new_board_state = BOARD_Inactive;
          break;

        case BOARD_Inactive: // Should never reach this case on VoltBoy
          if (button_pressed == true)
            new_board_state = BOARD_Active;
          else if (usb_connected == true)
            new_board_state = BOARD_Inactive_USB;
          break;
      }

      if (new_board_state != board_state) // If board state has been changed
      {
        board_state = new_board_state; // Update board state
        updatePower();                 // Update power connections
      }
    }

    /* Updates LED indicator based on USB activity. */
    void updateLight(bool usb_connected)
    {
      if (usb_connected == true)
        light_ptr->makeRequest(LED_Pulse_Slow); // Request slow pulse if charging
      else {
        switch (board_state) 
        {
          case BOARD_Active:
            light_ptr->makeRequest(LED_Fade_In); // Turn LED on
            break;
          case BOARD_Inactive:
            light_ptr->makeRequest(LED_Fade_Out); // Turn LED on
            break;
          case BOARD_Inactive_USB: // Should never be here
            light_ptr->makeRequest(LED_Pulse_Slow); // Request slow pulse if charging
            break;            
        }
      }
    }
    
    void batteryCritical()
    {
      if (board_state == BOARD_Active) // Only take action if we are currently active 
      {
        light_ptr->makeRequest(LED_Pulse_Rapid); // LED warning flash
        light_ptr->waitForRequest();
        
        board_state = BOARD_Inactive; // Deactivate board
        updatePower();                // Update power connections based on state change.

        /* If USB is plugged in currently, the MCU will remain powered and
         *  switch over to BOARD_Inactive_USB inside the main loop.
         */
      }
    }

    private:
      void updatePower()
      {
        switch(board_state) 
        {
          case BOARD_Active:
            light_ptr->makeRequest(LED_Fade_In); // Turn indicator LED on
            digitalWrite(P_MCU_SIG, HIGH);  // Latch MCU power
            digitalWrite(P_POW,     HIGH);  // Connect power to robot
            break;
              
          case BOARD_Inactive:
            light_ptr->makeRequest(LED_Fade_Out); // Turn indicator LED off
            digitalWrite(P_POW,     LOW);    // Disconnect power to robot
            digitalWrite(P_MCU_SIG, LOW);    // Unlatch MCU power
            break;
      
          case BOARD_Inactive_USB:
            light_ptr->makeRequest(LED_Fade_Out); // Turn indicator LED off
            digitalWrite(P_POW,     LOW);      // Disconnect power to robot
            digitalWrite(P_MCU_SIG, HIGH);     // Keep MCU power until USB unplugged
            break;
        }
      }
};

struct StateMachine state_machine; // State machine to handle board modes.

void setup() 
{
  // Immediately latch the MCU power circuit on
  pinMode(P_MCU_SIG, OUTPUT);
  digitalWrite(P_MCU_SIG, HIGH);

  // Configure other pins normally  
  pinMode(P_USB_IN,   INPUT);
  pinMode(P_LED,      OUTPUT); digitalWrite(P_LED, LOW);  
  pinMode(P_POW,      OUTPUT); digitalWrite(P_POW, LOW);  
  pinMode(P_VOLT_IN,  INPUT);  
  pinMode(P_SW_IN,    INPUT);
  
  setupLED();        // Set up LED management object
  setupPushbutton(); // Set up pushbutton interrupts and debouncing 
   
  state_machine.StateMachineSetup(digitalRead(P_USB_IN), &light); // Initialise state machine

  monitorVoltage();   // Check battery voltage before sending power to robot    
}

/* Monitors the voltage input pin. Shuts down MCU if battery voltage is below safety threshold,
 *  and otherwise does nothing.
 */
void monitorVoltage()
{
  float voltage = ((float)analogRead(P_VOLT_IN)/1023)*5.0;

  if (voltage < MIN_VOLTAGE) // If battery voltage dangerously low
    state_machine.batteryCritical(); // Signal battery danger to state machine to shut down
}

/* Sets up the Button struct object and configures interrupts on PB2 (SW_IN pin). */
void setupPushbutton()
{
  // Initialise Button struct object variables
  button.debounce_wait_time_ms = 100;
  button.cooldown_wait_time_ms = 50;
  button.ButtonSetup();

  MCUCR |= 0b00000011; // Rising edge
  GIMSK |= 0b01000000; // Enable external interrupts
  SREG  |= 0b10000000; // Global interrupt enable
}

/* Sets up the Light struct object member variables. */
void setupLED()
{
  light.slow_prd_ms     = 3000;
  light.rapid_prd_ms    = 400;
  light.rapid_n         = 5;
  light.LightSetup();
}

void loop() 
{       
  bool usb_connected = digitalRead(P_USB_IN); // Read USB line
  
  // Change board state and power connections based on pushbutton and USB.
  state_machine.updateState(button.pushed(), usb_connected);

  // Change lights based on USB activity
  state_machine.updateLight(usb_connected);    

  monitorVoltage();  // Check voltage periodically to make sure battery is safe
  light.updateLED(); // Update the LED based on the current behaviour request
}

/* Interrupt service routine for pushbutton press on SW_IN, pin PB2 (INT0). */
ISR(INT0_vect)
{
  button.trigger(); // Trigger the debouncer
}
