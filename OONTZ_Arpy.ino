// OONTZ ARPY
// Arpeggiator for the Adafruit OONTZ & HELLA OONTZ
// Plays a MIDI note sequence relative to button pressed
//
// For HELLA OONTZ change HELLA value to 1 (line 20)
// For external MIDI clock sync change EXT_CLOCK value to 1 (line 21)
// Scale can be chosen by specifying a mode on line 129
//
// Optional hardware:
//      Pattern control - connect rotary encoder to pins 4 & 5
//      Tempo control   - connect rotary encoder to pins 8 & 9
//

void setup();   // Added to avoid Arduino IDE #ifdef bug

#include <Wire.h>
#include <Adafruit_Trellis.h>
#include <Adafruit_OONTZ.h>

#define LED       13 // Pin for heartbeat LED (shows code is working)
#define CHANNEL   1  // MIDI channel number
#define HELLA     1  // 0 for standard OONTZ, 1 for HELLA OONTZ
#define EXT_CLOCK 1  // 0 for internal clock, 1 for external

#if HELLA
Adafruit_Trellis T[8];
OONTZ            oontz(&T[0], &T[1], &T[2], &T[3],
                       &T[4], &T[5], &T[6], &T[7]);
const uint8_t    addr[] = { 0x70, 0x71, 0x72, 0x73,
                            0x74, 0x75, 0x76, 0x77 };
#else
Adafruit_Trellis T[4];
OONTZ            oontz(&T[0], &T[1], &T[2], &T[3]);
const uint8_t    addr[] = { 0x70, 0x71,
                            0x72, 0x73 };
#endif

#define WIDTH      ((sizeof(T) / sizeof(T[0])) * 2)
#define HEIGHT     (N_BUTTONS / WIDTH)
#define N_BUTTONS  ((sizeof(T) / sizeof(T[0])) * 16)
#define ARP_NOTES  6
#define NULL_INDEX 255

uint8_t       heart        = 0;  // Heartbeat LED counter
unsigned long prevReadTime = 0L; // Keypad polling timer
//bool          extClock = true;
uint8_t       quantDiv = 8;      // Quantization division, 2 = half note
uint8_t       clockPulse = 0;

#define QUANT_PULSE (96/quantDiv)// Number of pulses per quantization division

uint8_t       noteDivs[] = { 1,2,4,8,16,32};

// Note patterns for arpeggiator
int8_t
arpA[ARP_NOTES][2] = {
    {  0,   0  },
    {  -1,  -1 },
    {  1,   -1 },
    {  1,   1  },
    {  -1,  1  },
    {  -1,  1  },
},
arpB[ARP_NOTES][2] = {
    {  0,   0  },
    {  1,   -1 },
    {  2,   -2 },
    {  2,   -2 },
    {  1,   -1 },
    {  2,   -1 },
},
arpC[ARP_NOTES][2] = {
    {  0,   0  },
    {  -1,  0  },
    {  0,   0  },
    {  0,   -1 },
    {  0,   0  },
    {  1,   0  },
},
arpD[ARP_NOTES][2] = {
    {  0,   0 },
    {  1,   0 },
    {  0,   0 },
    {  1,   0 },
    {  1,   2 },
    {  1,   2 }
},
arpE[ARP_NOTES][2] = {
    {  0,   0 },
    {  1,   0 },
    {  2,   0 },
    {  3,   0 },
    {  3,   1 },
    {  2,   1 }
},
arpF[ARP_NOTES][2] = {
    {  0,   0 },
    {  1,   0 },
    {  -1,  0 },
    {  2,   0 },
    {  -2,  0 },
    {  0,   0 }
},
arpG[ARP_NOTES][2] = {
    {  0,   0 },
    {  1,   -1 },
    {  2,  0 },
    {  3,   -1 },
    {  2,  -1 },
    {  1,   0 }
};

int8_t (*arpCollection[])[ARP_NOTES][2] = { &arpA, &arpB, &arpC, &arpD,
                                            &arpE, &arpF, &arpG };

uint8_t arpCount = sizeof(arpCollection) / sizeof(arpCollection[0]);
int8_t (*arp)[ARP_NOTES][2] = &(*arpCollection)[0];
uint8_t pitchMap[N_BUTTONS];

// Musical mode/scale intervals
uint8_t chromatic[12]   = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11 },
        ionian[12]      = { 0, 0, 2, 4, 4, 5, 5, 7, 7, 9,11,11 },
        dorian[12]      = { 0, 0, 2, 3, 3, 5, 5, 7, 7, 9,10,10 },
        phrygian[12]    = { 0, 1, 2, 3, 3, 5, 5, 7, 8, 8,10,10 },
        lydian[12]      = { 0, 0, 2, 2, 4, 4, 6, 7, 7, 9,10,11 },
        mixolydian[12]  = { 0, 0, 2, 2, 4, 5, 5, 7, 7, 9,10,10 },
        aeolian[12]     = { 0, 0, 2, 3, 3, 5, 5, 7, 8, 8,10,10 },
        locrian[12]     = { 0, 1, 1, 3, 3, 5, 6, 6, 8, 8,10,10 };

uint8_t (*scale)[12]    = &chromatic;    // Chosen scale

uint8_t firstNote = 36,  // First note, upper left of grid
        colIntvl  = 2,   // Semitones between each column
        rowIntvl  = 5;   // Semitones between each row

boolean pressedButtonIndex[N_BUTTONS] = {false};        // Pressed state for each button
uint8_t arpSeqIndex[N_BUTTONS]        = {NULL_INDEX},   // Current place in button arpeggio sequence
        arpButtonIndex[N_BUTTONS]     = {NULL_INDEX};   // Button index being played for each actual pressed button

// Encoders for tempo and arpeggio selection
enc tempoEncoder(8, 9);
enc arpEncoder(4, 5);
unsigned int  bpm          = 320;          // Tempo
unsigned long beatInterval = 60000L / bpm, // ms/beat
              prevArpTime  = 0L;
//uint8_t       arpVelocityMax  = 102,
//              arpVelocityMin  = 65;


void setup(){
    
    pinMode(LED, OUTPUT);
    
    // 8x8
    if (WIDTH <= 8) {
        oontz.begin(addr[0], addr[1], addr[2], addr[3]);
    }
    
    // HELLA 16x8
    else{
        oontz.begin(addr[0], addr[1], addr[2], addr[3],
                    addr[4], addr[5], addr[6], addr[7]);
    }
    
#ifdef __AVR__
    // Default Arduino I2C speed is 100 KHz, but the HT16K33 supports
    // 400 KHz.  We can force this for faster read & refresh, but may
    // break compatibility with other I2C devices...so be prepared to
    // comment this out, or save & restore value as needed.
    TWBR = 12;
#endif
    oontz.clear();
    oontz.writeDisplay();
    
#if EXT_CLOCK
    // Set up tempo encoder for External clock mode
    uint8_t divCount = sizeof(noteDivs) / sizeof(noteDivs[0]) -1;
    tempoEncoder.setBounds(0, divCount * 4, true); // Set tempo limits
    tempoEncoder.setValue(3 * 4);                  // *4's for encoder detents
#else
    // Set up tempo encoder for Internal clock mode
    tempoEncoder.setBounds(60 * 4, 480 * 4 + 3); // Set tempo limits
    tempoEncoder.setValue(bpm * 4);              // *4's for encoder detents
#endif

    
    //Set up arpeggio pattern encoder
    arpEncoder.setBounds(0, arpCount * 4 - 1, true);
    arpEncoder.setValue(6 * 4);
    
    //Set up the note for the grid
    writePitchMap();

#if EXT_CLOCK
    usbMIDI.setHandleRealTimeSystem(handleRealTimeSystem);
#endif
    
}


void loop(){
    
    checkMidi();
    
    unsigned long t = millis();
    unsigned long tDiff = t - prevReadTime;
    
    enc::poll(); // Read encoder(s)
    
    if(tDiff >= 20L) { // 20ms = min Trellis poll time
        
        if(oontz.readSwitches()) {  // Button state change?
            
            for(uint8_t i=0; i<N_BUTTONS; i++) { // For each button...
                
                //Button was pressed
                if(oontz.justPressed(i)) {
                    
                    //add note to pressed buttons array
                    pressedButtonIndex[i] = true;
                }
                
                //Button was released
                else if(oontz.justReleased(i)) {
                    
                    //remove note from pressed buttons array
                    pressedButtonIndex[i] = false;
                    stopArp(i);
                }
            }
        }
        
        // Iterate array, play arp sequence for pressed buttons
        
#if EXT_CLOCK
        // EXTERNAL CLOCK
        //Set note length from encoder value
        quantDiv     = noteDivs[tempoEncoder.getValue() / 4];
        
#else
        // INTERNAL CLOCK
        if ((t - prevArpTime) >= beatInterval) {
            respondToPresses();
            prevArpTime = t;
        }
        
        //Set Tempo from encoder value
        bpm          = tempoEncoder.getValue() / 4; // Div for encoder detents
        beatInterval = 60000L / bpm;
#endif
        
        //Set current arp notes
        int16_t arpIndex = arpEncoder.getValue() / 4;
        arp = &(*arpCollection)[arpIndex];
        
        //update LEDs
        oontz.writeDisplay();
        
        prevReadTime = t;
        digitalWrite(LED, ++heart & 32); // Blink = alive
        
    }
    
}


void checkMidi(){

#if EXT_CLOCK
    // EXTERNAL CLOCK
    usbMIDI.read(CHANNEL);
    
#else
    // INTERNAL CLOCK
    while(usbMIDI.read(CHANNEL)); // Discard incoming MIDI messages

#endif
    
}


void handleRealTimeSystem(uint8_t data){
    
    if (data == 0xF8) {      // Clock pulse
        
        clockPulse++;
        if (clockPulse > 96) {   //MIDI clock sends 24 pulses per quarter note
            clockPulse = 1;
        }
        else if ((clockPulse % QUANT_PULSE) == 0){
            respondToPresses();
        }
    }
    
    else if (data == 0xFA){  // START message
        clockPulse = 0;
    }
    
//    else if (data == 0xFC){  // STOP message
//
//    }
    
//    else if (data == 0xFB){  // CONTINUE message
//
//
//    }
    
}


void writePitchMap(){
    
    //Write first row of notes, establish column intervals
    for (int i = 0; i < WIDTH; i++){
        pitchMap[i] = i * colIntvl + firstNote;
    }
    
    //Write remaining rows, first row + row intervals
    for (int i = WIDTH; i < N_BUTTONS; i++) {
        pitchMap[i] = i / WIDTH * rowIntvl + pitchMap[i%WIDTH];
    }
    
    //Apply diatonic filter - TESTING
    for (int i = 0; i < N_BUTTONS; i++) {
        uint8_t deg = pitchMap[i] % 12;
        uint8_t oct = pitchMap[i] / 12;
        pitchMap[i] = oct * 12 + (*scale)[deg];
    }
    
    //Test notes
//    int x,y = 0;
//    for (int i = 0; i < (WIDTH); i++) {
//        
//        if (x >= WIDTH){ x = 0; y++; }
//        if (y >= HEIGHT) { break; }
//        
//        uint8_t index = oontz.xy2i(x,y);
//        
//        playNoteForButton(index);
//        delay(200);
//        oontz.writeDisplay();
//        delay(20);
//        stopNoteForButton(index);
//        oontz.writeDisplay();
//        
//        x++;
//    }
    
}


void respondToPresses(){
    
    for (uint8_t i=0; i < N_BUTTONS; i++) {
        if (pressedButtonIndex[i]) {
            playArp(i);
        }
    }
    
}


void setAllLEDs(bool lit){
    
    for (uint8_t i=0; i < N_BUTTONS; i++) {
        if (lit) {
            oontz.setLED(i);
        }
        else{
            oontz.clrLED(i);
        }
    }
    
}


void playArp(uint8_t buttonIndex){
    
    uint8_t seqIndex, seqButtonIndex, seqNote,
            x, y;
    seqIndex = arpSeqIndex[buttonIndex] + 1;
    
    // Loop sequence
    if (seqIndex >= ARP_NOTES) {
        seqIndex = 0;
    }
    
    // Find current button coordinates
    oontz.i2xy(buttonIndex, &x, &y);
    
    // Add note offsets
    x = (int8_t)x + (*arp)[seqIndex][0];
    y = (int8_t)y + (*arp)[seqIndex][1];
    
    // Wrap notes to grid
    if (x >= WIDTH) {x %= WIDTH;}
    if (y >= HEIGHT) {y %= HEIGHT;}
    
    // Find new note and index
    seqNote = findNoteFromXY(x, y);
    seqButtonIndex = oontz.xy2i(x, y);
    
    // Stop prev note in sequence
    stopNoteForButton(arpButtonIndex[buttonIndex]);
    
    // Store new note
    arpSeqIndex[buttonIndex] = seqIndex;
    arpButtonIndex[buttonIndex] = seqButtonIndex;
    
    // Play new note
    playNoteForButton(seqButtonIndex);
    
}


void stopAll(){
    
    for (uint8_t i=0; i < N_BUTTONS; i++) {
        if (pressedButtonIndex[i]) {
            stopArp(i);
        }
    }
    
}


void stopArp(uint8_t button){
    
    //stop playing the note
    stopNoteForButton(arpButtonIndex[button]);
    
    //store an invalid button index in its place
    arpSeqIndex[button] = NULL_INDEX;  //check for invalid
    
}


uint8_t findNoteFromIndex(uint8_t buttonIndex){
    
    uint8_t x, y;
    oontz.i2xy(buttonIndex, &x, &y);
    
    return findNoteFromXY(x,y);
    
}


uint8_t findNoteFromXY(uint8_t x, uint8_t y){
    
    return pitchMap[y * WIDTH + x];
    
}


void playNoteForButton(uint8_t buttonIndex){
    
  // Set a random velocity
//  uint8_t vel = random(arpVelocityMin, arpVelocityMax);
  
    usbMIDI.sendNoteOn(findNoteFromIndex(buttonIndex), 100, CHANNEL);   //default velocity of 100
    oontz.setLED(buttonIndex);
    
}


void stopNoteForButton(uint8_t buttonIndex){
    
    usbMIDI.sendNoteOff(findNoteFromIndex(buttonIndex), 0, CHANNEL);
    oontz.clrLED(buttonIndex);
    
}

