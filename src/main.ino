/*************************************************
  8k Macro Keypad
  Written by thnikk.
*************************************************/

#include <Bounce2.h>
#include <Keyboard.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>


bool serialDebug = 0;

// Version number (chnge to update EEPROM values)
bool version = 0;

// First-time mapping values. Update version to write
// new values to EEPROM. Values available at
// ASCII table here: http://www.asciitable.com/
byte byteMapping[6][8] = {
  { 97, 115, 100, 102, 122, 120, 99, 118 }, // asdzxc (osu)
  { 113, 119, 101, 114, 97, 115, 100, 102 }, // qweasd (FPS)
  { 0, 218, 0, 216, 217, 215, 122, 122 }, // arrow keys
  { 49, 50, 51, 52, 53, 54, 122, 122 }, // 123456
  { 0, 0, 0, 0, 0, 0, 0, 0 }, // Blank
  { 0, 0, 0, 0, 0, 0, 0, 0 }  // Blank
};

// Arrays for modifier interpreter
byte specialLength = 30; // Number of "special keys"
String specialKeys[] = {
  "shift", "ctrl", "super",
  "alt", "f1", "f2", "f3",
  "f4", "f5", "f6", "f7",
  "f8", "f9", "f10", "f11",
  "f12", "insert",
  "delete", "backspace",
  "enter", "home", "end",
  "pgup", "pgdn", "up",
  "down", "left", "right",
  "tab", "escape"
};
byte specialByte[] = {
  129, 128, 131, 130,
  194, 195, 196, 197,
  198, 199, 200, 201,
  202, 203, 204, 205,
  209, 212, 178, 176,
  210, 213, 211, 214,
  218, 217, 216, 215,
  179, 177
};

byte inputBuffer; // Stores specialByte after conversion

byte b = 50; // LED brightness

// MacroPad Specific

// Array for buttons (for use in for loop.)
const byte button[] = { 2, 3, 7, 9, 10, 11, 12, 13, 6, 8 };

// How many keys (0 indexed)
#define numkeys 8 // Use define for compiler level
const byte pages = 6;
byte page = 0; // Default page

byte rgbMap[] = { 0, 1, 2, 3, 7, 6, 5, 4 };
bool powCheck = 0;
bool faceCheck = 0;

// Makes button press/release action happen only once
bool pressed[numkeys];

// Array for storing bounce values
//bool bounce[numkeys+1]; // +1 because length is NOT zero indexed

// Mapping multidimensional array
char mapping[pages][numkeys][3];

// Neopixel library initializtion
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(numkeys, 5, NEO_GRB + NEO_KHZ800);
//Adafruit_DotStar pixels = Adafruit_DotStar( 1, 7, 8, DOTSTAR_BRG);
byte rgb[3][1]; // Declare RGB array
byte customRGB[numkeys] = {0,0,0,0,0,0,0,0};

unsigned long previousMillis = 0; // Serial monitor timer
unsigned long sbMillis = 0;       // Side button timer
unsigned long effectMillis = 0;   // LED effect timer (so y'all don't get seizures)
byte hold = 0; // Hold counter
byte set = 0;  //
byte rb = 0; // rainbow effect counter (rollover at 255 to reset)
bool blink = 0;
bool power = 1;

// Bounce declaration
Bounce * bounce = new Bounce[numkeys+2];

void setup() {
  Serial.begin(9600);
  pixels.begin();
  // Load + Initialize EEPROM
  loadEEPROM();

  // Set input pullup resistors
  for (int x = 0; x <= numkeys; x++) {
   pinMode(button[x], INPUT_PULLUP);
  }

  // Bounce initializtion
  for (byte x=0; x<=numkeys+1; x++) {	pinMode(button[x], INPUT_PULLUP);	bounce[x].attach(button[x]); bounce[x].interval(8); }

}

void loadEEPROM() {
  // Initialize EEPROM
  if (EEPROM.read(0) != version) {
    EEPROM.write(90, power);
    EEPROM.write(1, page);
    // Single values
    EEPROM.write(0, version);
    for (int z = 0; z < pages; z++) {     // Pages
      for (int x = 0; x < numkeys; x++) { // Keys
        for (int  y= 0; y < 3; y++) {     // 3 key slots per key
          // Write the default values (no combos to avoid accidents)
          if (y == 0) EEPROM.write(((40+(x*3)+y) + (z*3*numkeys)), byteMapping[z][x]);
          // Write 0 to remailing two slots per key for null values
          if (y > 0) EEPROM.write((40+(x*3)+y) + (z*3*numkeys), 0);
        }
      }
    }
  }
  // Load values from EEPROM
  page = EEPROM.read(1);
  power = EEPROM.read(90);
  // Load button mapping
  for (int z = 0; z < pages; z++) {     // Pages
    for (int x = 0; x < numkeys; x++) { // Keys
      for (int  y= 0; y < 3; y++) {     // 3 key slots per key
        mapping[z][x][y] = char(EEPROM.read((40+(x*3)+y) + (z*3*numkeys)));
      }
    }
  }
  for (byte x=0;x<numkeys;x++) customRGB[x] = EEPROM.read(100+x);
}

void loop() {
  // Run to get latest bounce values
  bounceSetup(); // Moved here for program-wide access to latest debounced button values
  if (power == 1) {
  if (((millis() - previousMillis) > 1000) && (!serialDebug)) { // Check once a second to reduce overhead
    if (Serial && set == 0) { // Run once when serial monitor is opened to avoid flooding the serial monitor
      Serial.println("Please press 0 to enter the serial remapper.");
      set = 1;
    }
    // If 0 is received at any point, enter the remapper.
    if (Serial.available() > 0) if (Serial.read() == '0') remapSerial();
    // If the serial monitor is closed, reset so it can prompt the user to press 0 again.
    if (!Serial) set = 0;
    previousMillis = millis();

    /*
    for (int x = 0; x < numkeys; x++) {
      for (int y = 0; y < 3; y++) {
        byte mapCheck = int(mapping[0][x][y]);
        if (mapCheck != 0){ // If not null...
          // Print if regular character (prints as a char)
          if (mapCheck > 33 && mapCheck < 126) Serial.print(mapping[0][x][y]);
          // Otherwise, check it through the byte array and print the text version of the key.
          else for (int z = 0; z < specialLength; z++) if (specialByte[z] == mapCheck){
            Serial.print(specialKeys[z]);
            Serial.print(" ");
          }
        }
      }
    }*/
  }

  if (bounce[numkeys].read()) custom();
  sideButton(); // Run sidebutton code
  if (bounce[numkeys+1].read()) if (bounce[numkeys].read()) keyboard();
  }
  else {
    // Turn off LEDs when "power" is off
    for (byte x=0;x<numkeys;x++) pixels.setPixelColor(x, 0, 0, 0);
    pixels.show();
  }

  // If the top side button is held (this should go in its own function)
  if (!bounce[numkeys+1].read()){
    powCheck = 1;
    // Check if any keys are pressed
    for (byte x=0;x<numkeys;x++) {
      if (!bounce[rgbMap[x]].read() && power == 1){
        // If so, increment RGB value
        if ((millis() - effectMillis) > 10) {
          customRGB[x] = customRGB[x]+1;
          effectMillis = millis();
        }
        faceCheck = 1;
      }
    }
  }
  if (bounce[numkeys+1].read()) {
    if (powCheck == 1 && faceCheck == 0) {
      // Print power off to terminal
      Serial.print("Power state change: ");
      Serial.println(power);
      if (power == 0) power = 1;
      else power = 0;
      powCheck = 0;
      // Write new value to EEPROM
      EEPROM.write(90, power);
    }
    // If one of the face buttons is pressed, write the new color values to EEPROM
    if (faceCheck == 1 && power == 1) {
      for (byte x=0;x<numkeys;x++) EEPROM.write(100+x, customRGB[x]);
      Serial.println("RGB Values saved.");
      powCheck = 0;
      faceCheck = 0;
    }
  }

//  Serial.println(bounce[numkeys+1].read());
}

// LED code
// Three modes along with the "wheel" function, which lets you set the color between r, g, and b with a single byte (0-255)
void status(byte inPage) { // Displays single color
  if ((millis() - effectMillis) > 10) {
    wheel(inPage*40, 0);
    for (byte x=0;x<numkeys;x++) pixels.setPixelColor(x, pixels.Color(rgb[0][0]*b/255, rgb[1][0]*b/255, rgb[2][0]*b/255));
    pixels.show();
    effectMillis = millis();
  }
}

void wheel(byte shortColor, byte key) { // Set RGB color with byte
  // convert shortColor to r, g, or b
  if (shortColor >= 0 && shortColor < 85) {
    rgb[0][key] = (shortColor * -3) +255;
    rgb[1][key] = shortColor * 3;
    rgb[2][key] = 0;
  }
  if (shortColor >= 85 && shortColor < 170) {
    rgb[0][key] = 0;
    rgb[1][key] = ((shortColor - 85) * -3) +255;
    rgb[2][key] = (shortColor - 85) * 3;
  }
  if (shortColor >= 170 && shortColor < 255) {
    rgb[0][key] = (shortColor - 170) * 3;
    rgb[1][key] = 0;
    rgb[2][key] = ((shortColor - 170) * -3) +255;
  }
}

void custom() {
  for (byte x=0;x<numkeys;x++) {
    wheel(customRGB[x], x);
    pixels.setPixelColor(x, pixels.Color(rgb[0][x]*b/255, rgb[1][x]*b/255, rgb[2][x]*b/255));
    pixels.show();
  }
}

// Remapper code
// Allows keys to be remapped through the serial monitor
void remapSerial() {
  Serial.println("Welcome to the serial remapper!");
  while(true){
    // Buffer variables (puting these at the root of the relevant scope to reduce memory overhead)
    byte input = 0;
    byte pageMap = 0;

    // Main menu
    Serial.println();
    Serial.println("Please enter the page you'd like to change the mapping for:");
    Serial.println("0 = Exit, 1-6 = Page 1-6");
    while(!Serial.available()){} // Display rainbow on LED when waiting for page input
    // Set page or quit
    if (Serial.available()) input = byte(Serial.read()); // Save as variable to reduce overhead
    if (input == 48) break; // Quit if user inputs 0
    else if (input > 48 && input <= 48 + pages) pageMap = input - 49; // -49 for zero indexed mapping array

    // Print current EEPROM values
    Serial.print("Current values are: ");
    for (int x = 0; x < numkeys; x++) {
      for (int y = 0; y < 3; y++) {
        byte mapCheck = int(mapping[pageMap][x][y]);
        if (mapCheck != 0){ // If not null...
          // Print if regular character (prints as a char)
          if (mapCheck > 33 && mapCheck < 126) Serial.print(mapping[pageMap][x][y]);
          // Otherwise, check it through the byte array and print the text version of the key.
          else for (int z = 0; z < specialLength; z++) if (specialByte[z] == mapCheck){
            Serial.print(specialKeys[z]);
            Serial.print(" ");
          }
        }
      }
      // Print delineation
      if (x < (numkeys - 1)) Serial.print(", ");
    }
    Serial.println();
    // End of print

    // Take serial inputs
    Serial.println("Please input special keys first and then a printable character.");
    Serial.println();
    Serial.println("For special keys, please enter a colon and then the corresponding");
    Serial.println("number (example: ctrl = ':1')");
    // Print all special keys
    byte lineLength = 0;

    // Print table of special values
    for (int y = 0; y < 67; y++) Serial.print("-");
    Serial.println();
    for (int x = 0; x < specialLength; x++) {
      // Make every line wrap at 30 characters
      byte spLength = specialKeys[x].length(); // save as variable within for loop for repeated use
      lineLength += spLength + 6;
      Serial.print(specialKeys[x]);
      spLength = 9 - spLength;
      for (spLength; spLength > 0; spLength--) { // Print a space
        Serial.print(" ");
        lineLength++;
      }
      if (x > 9) lineLength++;
      Serial.print(" = ");
      if (x <= 9) {
        Serial.print(" ");
        lineLength+=2;
      }
      Serial.print(x);
      if (x != specialLength) Serial.print(" | ");
      if (lineLength > 55) {
        lineLength = 0;
        Serial.println();
      }
    }
    // Bottom line
    if ((specialLength % 4) != 0) Serial.println(); // Add a new line if table doesn't go to end
    for (int y = 0; y < 67; y++) Serial.print("-"); // Bottom line of table
    Serial.println();
    Serial.println("If you want two or fewer modifiers for a key and");
    Serial.println("no printable characters, finish by entering 'xx'");
    // End of table

    for (int x = 0; x < numkeys; x++) { // Main for loop for each key

      byte y = 0; // External loop counter for while loop
      byte z = 0; // quickfix for bug causing wrong input slots to be saved
      while (true) {
        while(!Serial.available()){}
        String serialInput = Serial.readString();
        byte loopV = inputInterpreter(serialInput);

        // If key isn't converted
        if (loopV == 0){ // Save to array and EEPROM and quit; do and break
          // If user finishes key
          if (serialInput[0] == 'x' && serialInput[1] == 'x') { // Break if they use the safe word
            for (y; y < 3; y++) { // Overwrite with null values (0 char = null)
              EEPROM.write((40+(x*3)+y) + (pageMap*3*numkeys), 0);
              mapping[pageMap][x][y] = 0;
            }
            if (x < numkeys-1) Serial.print("(finished key,) ");
            if (x == numkeys-1) Serial.print("(finished key)");
            break;
          }
          // If user otherwise finishes inputs
          Serial.print(serialInput); // Print once
          if (x < numkeys - 1) Serial.print(", ");
          for (y; y < 3; y++) { // Normal write/finish
            EEPROM.write((40+(x*3)+y) + (pageMap*3*numkeys), int(serialInput[y-z]));
            mapping[pageMap][x][y] = serialInput[y-z];
          }
          break;
        }

        // If key is converted
        if (loopV == 1){ // save input buffer into slot and take another serial input; y++ and loop
          EEPROM.write((40+(x*3)+y) + (pageMap*3*numkeys), inputBuffer);
          mapping[pageMap][x][y] = inputBuffer;
          y++;
          z++;
        }

        // If user input is invalid, print keys again.
        if (loopV == 2){

          for (int a = 0; a < x; a++) {
            for (int d = 0; d < 3; d++) {
              byte mapCheck = int(mapping[pageMap][a][d]);
              if (mapCheck != 0){ // If not null...
                // Print if regular character (prints as a char)
                if (mapCheck > 33 && mapCheck < 126) Serial.print(mapping[pageMap][a][d]);
                // Otherwise, check it through the byte array and print the text version of the key.
                else for (int c = 0; c < specialLength; c++) if (specialByte[c] == mapCheck){
                  Serial.print(specialKeys[c]);
                  // Serial.print(" ");
                }
              }
            }
            // Print delineation
            Serial.print(", ");
          }
          if (y > 0) { // Run through rest of current key if any inputs were already entered
            for (int d = 0; d < y; d++) {
              byte mapCheck = int(mapping[pageMap][x][d]);
              if (mapCheck != 0){ // If not null...
                // Print if regular character (prints as a char)
                if (mapCheck > 33 && mapCheck < 126) Serial.print(mapping[pageMap][x][d]);
                // Otherwise, check it through the byte array and print the text version of the key.
                else for (int c = 0; c < specialLength; c++) if (specialByte[c] == mapCheck){
                  Serial.print(specialKeys[c]);
                  Serial.print(" ");
                  //pixels.setPixelColor(0, pixels.Color(rgb[0][0], rgb[1][0], rgb[2][0]));
                  //pixels.show();
                }
              }
            }
          }

        }
      } // Mapping loop
    } // Key for loop
    Serial.println();
    Serial.println("Mapping saved!");
  } // Main while loop
  Serial.println("Exiting.");
} // Remapper loop
byte inputInterpreter(String input) { // Checks inputs for a preceding colon and converts said input to byte
  if (input[0] == ':') { // Check if user input special characters
    input.remove(0, 1); // Remove colon
    int inputInt = input.toInt(); // Convert to integer
    if (inputInt >= 0 && inputInt < specialLength) { // Checks to make sure length matches
      inputBuffer = specialByte[inputInt];
      Serial.print(specialKeys[inputInt]); // Print within function for easier access
      Serial.print(" "); // Space for padding
      return 1;
    }
    Serial.println();
    Serial.println("Invalid code added, please try again.");
    return 2;
  }
  else if (input[0] != ':' && input.length() > 3){
    Serial.println();
    Serial.println("Invalid, please try again.");
    return 2;
  }
  else return 0;
}

// Input code
// Checks bounce values, sidebutton, and keyboard
void bounceSetup() { // Upates input values once per loop after debouncing
  // Check each state
  for(byte x=0; x<=numkeys+1; x++) bounce[x].update();
}
void sideButton(){
  if(!bounce[numkeys].read()) { // Change hold value for release
    if (((millis() - sbMillis) < 500)) hold = 1;
    if (((millis() - sbMillis) > 500)) hold = 2;
  }
  if (hold == 2) {
    if (!blink) {
      //blinkLED();
      blink = 1;
    }
    for (byte x = 0; x < numkeys; x++) {
      if (!bounce[x].read()) {
        page = x; // Changes page depending on which key is pressed (1-6)
        status(page);
//        delay(200);
      }
    }
  }
  if (hold != 2) {
//    for (byte x=0;x<numkeys;x++) pixels.setPixelColor(x, pixels.Color(0, 0, 0));
//    pixels.show();
  }
  if(bounce[numkeys].read()){
    if (hold == 1) { // Press escape if pressed and released
      Keyboard.press(177);
      delay(8);
      Keyboard.release(177);
    }
    if (hold == 2) {
      EEPROM.write(1, page);
     } // save page value
    hold = 0;
    blink = 0;
    sbMillis = millis();
  }

}
void keyboard(){
  if (hold < 2){ // Only run when the side button isn't pressed
    for (int a = 0; a < numkeys; a++) { // checks each key
      if (!pressed[a]) { // This made sense to me at one point
        if (!bounce[a].read()) { // For each
          Serial.print("Button ");
          Serial.print(a+1);
          Serial.print(" (");
          for (byte x=0;x<3;x++) Serial.print(mapping[page][a][x]);
          Serial.println(") pressed.");
          for (int b = 0; b < 3; b++)  {
          if (mapping[page][a][b] != 0) {
            Keyboard.press(mapping[page][a][b]);
          }
          }
          pressed[a] = 1; // nonsense
        }
      }
      if (pressed[a]) {
        if (bounce[a].read()) {
          for (int b = 0; b < 3; b++) {
          if (mapping[page][a][b] != 0) {
              Keyboard.release(mapping[page][a][b]);
            }
          }
          pressed[a] = 0;
        }
      }
    }
  }
}
