// Super-basic OONTZ MIDI example.  Maps OONTZ buttons to MIDI note
// on/off events; this is NOT a sequencer or anything fancy.
// Requires an Arduino Leonardo w/TeeOnArdu config (or a PJRC Teensy),
// software on host computer for synth or to route to other devices.

#include <Wire.h>
#include <Adafruit_Trellis.h>
#include <Adafruit_OONTZ.h>

#define LED     13 // Pin for heartbeat LED (shows code is working)
#define CHANNEL 1  // MIDI channel number

#ifndef HELLA
// A standard OONTZ has four Trellises in a 2x2 arrangement
// (8x8 buttons total).  addr[] is the I2C address of the upper left,
// upper right, lower left and lower right matrices, respectively,
// assuming an upright orientation, i.e. labels on board are in the
// normal reading direction.
Adafruit_Trellis T[4];
OONTZ            oontz(&T[0], &T[1], &T[2], &T[3]);
const uint8_t    addr[] = { 0x70, 0x71,
                            0x72, 0x73 };
#else
// A HELLA OONTZ has eight Trellis boards...
Adafruit_Trellis T[8];
OONTZ            oontz(&T[0], &T[1], &T[2], &T[3],
                       &T[4], &T[5], &T[6], &T[7]);
const uint8_t    addr[] = { 0x70, 0x71, 0x72, 0x73,
                            0x74, 0x75, 0x76, 0x77 };
#endif // HELLA

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

int8_t (*arpCollection[])[ARP_NOTES][2] = {
    &arpA,
    &arpB,
    &arpC,
    &arpD
};

uint8_t arpCount = sizeof(arpCollection) / sizeof(arpCollection[0]);

int8_t (*arp)[ARP_NOTES][2] = &(*arpCollection)[0];

const uint8_t pitchMapA[N_BUTTONS] = {
    36, 38, 39, 41, 43, 46, 48, 50,
    41, 43, 44, 46, 48, 51, 53, 55,
    46, 48, 50, 51, 53, 55, 58, 60,
    51, 53, 55, 56, 58, 60, 63, 65,
    55, 58, 60, 62, 63, 65, 67, 70,
    60, 63, 65, 67, 68, 70, 72, 75,
    65, 67, 70, 72, 74, 75, 77, 79,
    70, 72, 75, 77, 79, 80, 82, 84
};

boolean pressedButtonIndex[N_BUTTONS] = {false};   //button being virtually pressed for each sustained button press
uint8_t arpSeqIndex[N_BUTTONS] = {INVALID_BUTTON_INDEX};
uint8_t arpButtonIndex[N_BUTTONS] = {INVALID_BUTTON_INDEX};   //button being virtually pressed for each sustained button press

// Encoders for tempo and arpeggio selection
enc tempoEncoder(8, 9);
enc arpEncoder(4, 5);
unsigned int  bpm          = 240;          // Tempo
unsigned long beatInterval = 60000L / bpm, // ms/beat
              prevArpTime = 0L;

void setup() {
    
    pinMode(LED, OUTPUT);
#ifndef HELLA
    oontz.begin(addr[0], addr[1], addr[2], addr[3]);
#else
    oontz.begin(addr[0], addr[1], addr[2], addr[3],
                addr[4], addr[5], addr[6], addr[7]);
#endif // HELLA
#ifdef __AVR__
    // Default Arduino I2C speed is 100 KHz, but the HT16K33 supports
    // 400 KHz.  We can force this for faster read & refresh, but may
    // break compatibility with other I2C devices...so be prepared to
    // comment this out, or save & restore value as needed.
    TWBR = 12;
#endif
    oontz.clear();
    oontz.writeDisplay();
    
    tempoEncoder.setBounds(60 * 4, 480 * 4 + 3); // Set tempo limits
    tempoEncoder.setValue(bpm * 4);              // *4's for encoder detents
    
    arpEncoder.setBounds(0, arpCount * 4 - 1, true);
    arpEncoder.setValue(0);
}


void loop() {
    
    unsigned long t = millis();
    unsigned long tDiff = t - prevReadTime;
//    unsigned long aDiff = t - prevArpTime;
    
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
        if ((t - prevArpTime) >= beatInterval) {
            for (uint8_t i=0; i < N_BUTTONS; i++) {
                if (pressedButtonIndex[i]) {
                    playArp(tDiff, i);
                }
            }
            prevArpTime = t;
            
            //Set Tempo
            bpm          = tempoEncoder.getValue() / 4; // Div for encoder detents
            beatInterval = 60000L / bpm;
            
            //Set current arp notes
            int16_t arpIndex = arpEncoder.getValue() / 4;
            arp = &(*arpCollection)[arpIndex];
        }
        
        //update LEDs
        oontz.writeDisplay();
        
        prevReadTime = t;
        digitalWrite(LED, ++heart & 32); // Blink = alive
        
    }
    
    while(usbMIDI.read()); // Discard incoming MIDI messages
    
}


void playArp(unsigned long incTime, uint8_t buttonIndex) {
    
    //determine what note in the sequence we should be playing
    uint8_t seqIndex, seqButtonIndex, seqNote;
    uint8_t x, y;
    seqIndex = arpSeqIndex[buttonIndex] + 1;
    
    //Loop sequence
    if (seqIndex >= ARP_NOTES) {
        seqIndex = 0;
    }
    
    //find current button coordinates
    oontz.i2xy(buttonIndex, &x, &y);
    
    //Add note offsets
    x = (int8_t)x + (*arp)[seqIndex][0];
    y = (int8_t)y + (*arp)[seqIndex][1];
    
    //Wrap notes to grid
    if (x >= WIDTH) {x %= WIDTH;}
    if (y >= HEIGHT) {y %= HEIGHT;}
    
    //Find new note and index
    seqNote = findNoteFromXY(x, y);
    seqButtonIndex = oontz.xy2i(x, y);
    
        //stop prev note in sequence
        stopNoteForButton(arpButtonIndex[buttonIndex]);
        
        //store new note
        arpSeqIndex[buttonIndex] = seqIndex;
        arpButtonIndex[buttonIndex] = seqButtonIndex;
    
        //play new note
        playNoteForButton(seqButtonIndex);
//    }
}


void stopArp(uint8_t button){
    
    //stop playing the note
    stopNoteForButton(arpButtonIndex[button]);
    
    //store an invalid button index
    arpSeqIndex[button] = INVALID_BUTTON_INDEX;  //check for invalid
    
}


uint8_t findNoteFromIndex(uint8_t buttonIndex){
    
    uint8_t x, y;
    oontz.i2xy(buttonIndex, &x, &y);
    
    return findNoteFromXY(x,y);
    
}


uint8_t findNoteFromXY(uint8_t x, uint8_t y){
    
    return pitchMapA[y * WIDTH + x];
    
//    return LOWNOTE + y * WIDTH + x;
    
}


void playNoteForButton(uint8_t buttonIndex){
    
    usbMIDI.sendNoteOn(findNoteFromIndex(buttonIndex), 127, CHANNEL);
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

//void playNote(uint8_t buttonIndex){
//    
//    
//    
//}


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
