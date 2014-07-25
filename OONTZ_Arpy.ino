// Super-basic OONTZ MIDI example.  Maps OONTZ buttons to MIDI note
// on/off events; this is NOT a sequencer or anything fancy.
// Requires an Arduino Leonardo w/TeeOnArdu config (or a PJRC Teensy),
// software on host computer for synth or to route to other devices.

#include <Wire.h>
#include <Adafruit_Trellis.h>
#include <Adafruit_OONTZ.h>

#define LED     13 // Pin for heartbeat LED (shows code is working)
#define CHANNEL 1  // MIDI channel number

//#define HELLA   1  // If this be HELLA OONTZ

// A standard OONTZ has four Trellises in a 2x2 arrangement
// (8x8 buttons total).  addr[] is the I2C address of the upper left,
// upper right, lower left and lower right matrices, respectively,
// assuming an upright orientation, i.e. labels on board are in the
// normal reading direction.
// A HELLA OONTZ has eight Trellis boards...

//Standard OONTZ
//Adafruit_Trellis T[4];
//OONTZ            oontz(&T[0], &T[1], &T[2], &T[3]);
//const uint8_t    addr[] = { 0x70, 0x71,
//                            0x72, 0x73 };

// HELLA OONTZ
Adafruit_Trellis T[8];
OONTZ            oontz(&T[0], &T[1], &T[2], &T[3],
                       &T[4], &T[5], &T[6], &T[7]);
const uint8_t    addr[] = { 0x70, 0x71, 0x72, 0x73,
                            0x74, 0x75, 0x76, 0x77 };

// For this example, MIDI note numbers are simply centered based on
// the number of Trellis buttons; each row doesn't necessarily
// correspond to an octave or anything.
#define WIDTH     ((sizeof(T) / sizeof(T[0])) * 2)
#define N_BUTTONS ((sizeof(T) / sizeof(T[0])) * 16)
#define LOWNOTE   ((128 - N_BUTTONS) / 2)

uint8_t       heart        = 0;  // Heartbeat LED counter
unsigned long prevReadTime = 0L; // Keypad polling timer

//Arpy stuff
#define HEIGHT  (N_BUTTONS / WIDTH)
#define ARP_NOTES 6
#define INVALID_BUTTON_INDEX 255
bool extClock = true;
uint8_t quantDiv = 8;  // Quantization division, 2 = half note
#define QUANT_PULSE (96/quantDiv)  // Number of pulses per quantization division

uint8_t noteDivs[6] = {
    1,2,4,8,16,32
};

uint8_t clockPulse = 0;
bool    pulseHi    = false;

//#define NOTE_LENGTH 240L //note's length in milliseconds

int8_t arpA[ARP_NOTES][2] = {
    {  0,   0  },
    {  -1,  -1 },
    {  1,   -1 },
    {  1,   1  },
    {  -1,  1  },
    {  -1,  1  },
};
int8_t arpB[ARP_NOTES][2] = {
    {  0,   0  },
    {  1,   -1 },
    {  2,   -2 },
    {  2,   -2 },
    {  1,   -1 },
    {  2,   -1 },
};
int8_t arpC[ARP_NOTES][2] = {
    {  0,   0  },
    {  -1,  0  },
    {  0,   0  },
    {  0,   -1 },
    {  0,   0  },
    {  1,   0  },
};
int8_t arpD[ARP_NOTES][2] = {
    {  0,   0 },
    {  1,   0 },
    {  0,   0 },
    {  1,   0 },
    {  1,   2 },
    {  1,   2 }
};
int8_t arpE[ARP_NOTES][2] = {
    {  0,   0 },
    {  1,   0 },
    {  2,   0 },
    {  3,   0 },
    {  3,   1 },
    {  2,   1 }
};
int8_t arpF[ARP_NOTES][2] = {
    {  0,   0 },
    {  1,   0 },
    {  -1,  0 },
    {  2,   0 },
    {  -2,  0 },
    {  0,   0 }
};
int8_t arpG[ARP_NOTES][2] = {
    {  0,   0 },
    {  1,   -1 },
    {  2,  0 },
    {  3,   -1 },
    {  2,  -1 },
    {  1,   0 }
};

int8_t (*arpCollection[])[ARP_NOTES][2] = {
    &arpA,
    &arpB,
    &arpC,
    &arpD,
    &arpE,
    &arpF,
    &arpG
};

uint8_t arpCount = sizeof(arpCollection) / sizeof(arpCollection[0]);

int8_t (*arp)[ARP_NOTES][2] = &(*arpCollection)[0];

uint8_t pitchMapA[N_BUTTONS] = {    //Test note map for 8x8
    36, 38, 39, 41, 43, 46, 48, 50,
    41, 43, 44, 46, 48, 51, 53, 55,
    46, 48, 50, 51, 53, 55, 58, 60,
    51, 53, 55, 56, 58, 60, 63, 65,
    55, 58, 60, 62, 63, 65, 67, 70,
    60, 63, 65, 67, 68, 70, 72, 75,
    65, 67, 70, 72, 74, 75, 77, 79,
    70, 72, 75, 77, 79, 80, 82, 84
};

uint8_t pitchMap[N_BUTTONS];

// Musical modes / scales
uint8_t ionian[12]      = { 0, 0, 2, 4, 4, 5, 5, 7, 7, 9,11,11},
        dorian[12]      = { 0, 0, 2, 3, 3, 5, 5, 7, 7, 9,10,10},
        phrygian[12]    = { 0, 1, 2, 3, 3, 5, 5, 7, 8, 8,10,10},
        lydian[12]      = { 0, 0, 2, 2, 4, 4, 6, 7, 7, 9,10,11},
        mixolydian[12]  = { 0, 0, 2, 2, 4, 5, 5, 7, 7, 9,10,10},
        aeolian[12]     = { 0, 0, 2, 3, 3, 5, 5, 7, 8, 8,10,10},
        locrian[12]     = { 0, 1, 1, 3, 3, 5, 6, 6, 8, 8,10,10};

uint8_t firstNote = 36,  // First note, upper left of grid
        colIntvl  = 2,   // Semitones between each column
        rowIntvl  = 5;   // Semitones between each row

boolean pressedButtonIndex[N_BUTTONS] = {false};   //button being virtually pressed for each sustained button press
uint8_t arpSeqIndex[N_BUTTONS] = {INVALID_BUTTON_INDEX},
        arpButtonIndex[N_BUTTONS] = {INVALID_BUTTON_INDEX};   //button being virtually pressed for each sustained button press

// Encoders for tempo and arpeggio selection
enc tempoEncoder(8, 9);
enc arpEncoder(4, 5);
unsigned int  bpm          = 320;          // Tempo
unsigned long beatInterval = 60000L / bpm, // ms/beat
              prevArpTime  = 0L;
uint8_t       arpVelocityMax  = 102,
              arpVelocityMin  = 65;

void setup() {
    
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
    
    if (extClock == false) {
        // Set up tempo encoder for Internal clock mode
        tempoEncoder.setBounds(60 * 4, 480 * 4 + 3); // Set tempo limits
        tempoEncoder.setValue(bpm * 4);              // *4's for encoder detents
    }
    else{
        // Set up tempo encoder for External clock mode
        tempoEncoder.setBounds(0, sizeof(noteDivs) / sizeof(noteDivs[0]) * 4 - 1, true); // Set tempo limits
        tempoEncoder.setValue(3 * 4);        // *4's for encoder detents
    }
    
    //Set up arpeggio pattern encoder
    arpEncoder.setBounds(0, arpCount * 4 - 1, true);
    arpEncoder.setValue(6 * 4);
    
    //Set up the note for the grid
    writePitchMap();
    
}


void loop() {
    
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
        
        //iterate array, play arp sequence for pressed buttons
        // INTERNAL CLOCK
        if (extClock == false) {
            if ((t - prevArpTime) >= beatInterval) {
                
                for (uint8_t i=0; i < N_BUTTONS; i++) {
                    if (pressedButtonIndex[i]) {
                        playArp(i);
                    }
                }
                prevArpTime = t;
            }
            
            //Set Tempo from encoder value
            bpm          = tempoEncoder.getValue() / 4; // Div for encoder detents
            beatInterval = 60000L / bpm;
        }
        
        // EXTERNAL CLOCK
        else{
            //Set note length from encoder value
            quantDiv     = noteDivs[tempoEncoder.getValue() / 4];
        }
        
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
    
    // INTERNAL CLOCK
    if (extClock == false) {
        while(usbMIDI.read()); // Discard incoming MIDI messages
    }
    
    // EXTERNAL CLOCK
    else{
        if (usbMIDI.read() && usbMIDI.getType() == 8){   // Respond to clock signal
            pulseHi = true;
            clockPulse++;
            if (clockPulse > 96) {   //MIDI clock sends 64 pulses per note
                clockPulse = 1;
            }
            else if ((clockPulse % QUANT_PULSE) == 0){
                for (uint8_t i=0; i < N_BUTTONS; i++) {
                    if (pressedButtonIndex[i]) {
                        playArp(i);
                    }
                }
            }
        }
    }
    
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
        pitchMap[i] = oct * 12 + aeolian[deg];
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


void playArp(uint8_t buttonIndex) {
    
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


void stopArp(uint8_t button){
    
    //stop playing the note
    stopNoteForButton(arpButtonIndex[button]);
    
    //store an invalid button index in its place
    arpSeqIndex[button] = INVALID_BUTTON_INDEX;  //check for invalid
    
}


uint8_t findNoteFromIndex(uint8_t buttonIndex){
    
    uint8_t x, y;
    oontz.i2xy(buttonIndex, &x, &y);
    
    return findNoteFromXY(x,y);
    
}


uint8_t findNoteFromXY(uint8_t x, uint8_t y){
    
    return pitchMap[y * WIDTH + x];
    
//    return LOWNOTE + y * WIDTH + x;
    
}


void playNoteForButton(uint8_t buttonIndex){
    
  // Set a random velocity
  uint8_t vel = random(arpVelocityMin, arpVelocityMax);
  
    usbMIDI.sendNoteOn(findNoteFromIndex(buttonIndex), vel, CHANNEL);
    oontz.setLED(buttonIndex);
    
}


void stopNoteForButton(uint8_t buttonIndex){
    
    usbMIDI.sendNoteOff(findNoteFromIndex(buttonIndex), 0, CHANNEL);
    oontz.clrLED(buttonIndex);
    
}


void flashDebug(uint8_t count){
    
    for (uint8_t f = 0; f < count; f++) {
        digitalWrite(LED, HIGH);
        delay(200);
        digitalWrite(LED, LOW);
        delay(200);
    }
    
}


/* Here's the set of MIDI functions for making your own projects:
 
 usbMIDI.sendNoteOn(note, velocity, channel)
 usbMIDI.sendNoteOff(note, velocity, channel)
 usbMIDI.sendPolyPressure(note, pressure, channel)
 usbMIDI.sendControlChange(control, value, channel)
 usbMIDI.sendProgramChange(program, channel)
 usbMIDI.sendAfterTouch(pressure, channel)
 usbMIDI.sendPitchBend(value, channel)
 usbMIDI.sendSysEx(length, array)
 usbMIDI.send_now()
 
 Some info on MIDI note numbers can be found here:
 http://www.phys.unsw.edu.au/jw/notes.html
 
 Rather than MIDI, one could theoretically try using Serial to
 create a sketch compatible with serialosc or monomeserial, but
 those tools have proven notoriously unreliable thus far. */
