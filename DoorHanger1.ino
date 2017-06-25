#include "FastLED.h"

FASTLED_USING_NAMESPACE
#define DATA_PIN 4
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
CRGB strip[1];

// Shift register pins
#define SFT_LATCH A3
#define SFT_CLK A4
#define SFT_DATA A5

/**
Color Notes:
"True" white: HSV 96, 36, * (THOUGH 255 255 255 is reasonable if color correction is removed.)

Hue for Red, Orange, Yellow, Green, Ice, Blue, Purple, Pink:
0, 31, 56, 95, 138, 161, 183, 225

 */

// Tree of room connections (indexes in player light addresses)
struct Room {
	byte numNeighbors;
	byte neighbors[3];
	byte visited;
} rooms[9];
// 255 is a "filler" value
// Secret passages (2-3, 4-3) not noted
// Bidirectional links specified twice
void setupMap () {
	rooms[0] = {1, {1, 255, 255}, 0};
	rooms[1] = {3, {5, 7, 0}, 0}; // put 0 last. see wander logic
	rooms[2] = {1, {5, 3, 255}, 0};
	rooms[3] = {0, {2, 4, 255}, 0};
	rooms[4] = {2, {5, 8, 3}, 0};
	rooms[5] = {3, {1, 2, 4}, 0};
	rooms[6] = {1, {7, 255, 255}, 0};
	rooms[7] = {3, {1, 6, 8}, 0};
	rooms[8] = {2, {4, 7, 255}, 0};
	
	setPassagesOpen(false);
}

// Changes the map graph to open the secret passages!
// The secret passages links are at the end of the list of neighbors --
// just "extend" list of neighbors to cover or hide them.
void setSecretPassagesLight(byte light, byte state);
// Secret passage flicker on animation
byte passageAnimFrame = 0;
void setPassagesOpen(bool open) {
	if (arePassagesOpen() != open) {
		if (open) {
			rooms[2].numNeighbors = 2;
			rooms[3].numNeighbors = 2;
			rooms[4].numNeighbors = 3;
		} else {
			rooms[2].numNeighbors = 1;
			rooms[3].numNeighbors = 0;
			rooms[4].numNeighbors = 2;
		}
		setSecretPassagesLight(0, !open);
		setSecretPassagesLight(1, open);
		passageAnimFrame = 0;
	}
}

bool arePassagesOpen() {
	return rooms[3].numNeighbors > 0;
}

// Call again to reset state of map
void setup() {
	Serial.begin(115200);
	randomSeed(analogRead(2));
	
	// Strip lights
	FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(strip, 1).setCorrection(TypicalLEDStrip);
	// Set master brightness control
	FastLED.setBrightness(255);

	// Setup pin-connected lights
	pinMode(7, OUTPUT);
	pinMode(8, OUTPUT);
	pinMode(A1, OUTPUT);
	pinMode(A0, OUTPUT);
	pinMode(12, OUTPUT);

	// Setup PWM pin-connected lights
	pinMode(5, OUTPUT);
	pinMode(6, OUTPUT);
	pinMode(9, OUTPUT);
	pinMode(10, OUTPUT);
	pinMode(11, OUTPUT);

	// Setup shift register pins
	pinMode(SFT_LATCH, OUTPUT);
	pinMode(SFT_DATA, OUTPUT);
	pinMode(SFT_CLK, OUTPUT);
	
	// Reset any changes to room graph
	setupMap();
}

// Update a light attached to a shift register immediately
byte shiftBuffer[2] = {0, 0};
void setShiftLight(byte light, byte state) {
	// Change buffer entry
	if (state) {
		shiftBuffer[light / 8] |= 1 << (light%8);
	} else {
		shiftBuffer[light / 8] &= ~(1 << (light%8));
	}

	// Write out change to shift registers
	shiftOut(SFT_DATA, SFT_CLK, MSBFIRST, shiftBuffer[0]);
	shiftOut(SFT_DATA, SFT_CLK, MSBFIRST, shiftBuffer[1]);
	digitalWrite(SFT_LATCH, HIGH);
	digitalWrite(SFT_LATCH, LOW);
}

// Zombie lights are dimmable
const byte ZOMBIE_LIGHT_PINS[] = {11, 10, 9, 5, 6};
void setZombieLight(byte light, byte value) {
	analogWrite(ZOMBIE_LIGHT_PINS[light], value);
}

const byte PLAYER_LIGHT_SHIFT_PINS[] = {1, 2, 14, 8};
const byte PLAYER_LIGHT_PINS[] = {7, 8, A0, A1, 12};
void setPlayerLight(byte light, byte state) {
	if (light <= 3) {
		// Shift register
		setShiftLight(PLAYER_LIGHT_SHIFT_PINS[light], state);
	} else {
		// Direct pin
		digitalWrite(PLAYER_LIGHT_PINS[light-4], state);
	}
}

const byte MAGIC_RING_SHIFT_PINS[] = {9, 11, 12, 10};
void setMagicRingLight(byte light, byte state) {
	setShiftLight(MAGIC_RING_SHIFT_PINS[light], state);
}

const byte MAGIC_STUFF_SHIFT_PINS[] = {3, 4, 5};
void setMagicStuffLight(byte light, byte state) {
	setShiftLight(MAGIC_STUFF_SHIFT_PINS[light], state);
}

void setMagicStuffByFlag(byte flags) {
	setShiftLight(3, (flags & 0x01) != 0);
	setShiftLight(4, (flags & 0x02) != 0);
	setShiftLight(5, (flags & 0x04) != 0);
}

// Convenience function to output a 3 bit number to the row of 3 zombie lights.
void show3BitsOnZombies(int i) {
	setZombieLight(0, 255 * ((i & 0x01) > 0));
	setZombieLight(1, 255 * ((i & 0x02) > 0));
	setZombieLight(2, 255 * ((i & 0x04) > 0));
}

// on second shift register
const byte SECRET_PASSAGES_SHIFT_PINS[] = {7+8, 5+8};
void setSecretPassagesLight(byte light, byte state) {
	setShiftLight(SECRET_PASSAGES_SHIFT_PINS[light], state);
}

const byte SECRET_SWITCH_SHIFT_PIN = 0;
void setSecretSwitchLight(byte state) {
	setShiftLight(SECRET_SWITCH_SHIFT_PIN, state);
}

// Wanderer's location
int currRoom = 0;
int lastRoom = 0;

// Follows a few rules to choose which room the wanderer visits next
int getNextWanderRoom () {
	// get neighbors
	Room r = rooms[currRoom];
	int numNeighbors = r.numNeighbors;
	if (currRoom == 1) {
		numNeighbors--; // cut out exit from list
	}
	
	// choose one
	// strongly prefer secret passages!
	if (arePassagesOpen() && ((currRoom == 2) || (currRoom == 4))) {
		return 3;
	}
	// prefer unvisited rooms
	byte numUnvisited = 0;
	byte unvisitedRooms[3];
	for (int i=0;i<numNeighbors;i++) {
		if (!rooms[r.neighbors[i]].visited) {
			unvisitedRooms[numUnvisited] = r.neighbors[i];
			numUnvisited++;
		}
	}
	if (numUnvisited > 0) {
		return unvisitedRooms[random(numUnvisited)];
	} else if (numNeighbors > 0) {
		return r.neighbors[random(numNeighbors)];
	} else {
		// can't go anywhere
		return currRoom;
	}
}

// Timing counters, used for animation
byte playerCounter = 0;
#define NO_TRAIL_REMAINING 255
byte playerTrailRemaining = 0;
byte passagesCounter = 10;
byte ringCounter = 0;
byte currRingLight = 0;


// Updates at 100 hz
int fps = 85;

unsigned int time = 0;

#define WANDER_STATE 0
#define INTRO_STATE 1
#define CRYPT_FIGHT_STATE 2
#define ZOMBIE_FIGHT_STATE 3
#define MAGIC_STUFF_ANIMATION 4
#define START_FLOOR_ANIMATION 5

byte state = WANDER_STATE;

// Secret switch
#define SWITCH_BLINK
#define SWITCH_OFF
#define SWITCH_ON
byte secretSwitchMode = 0;

// Magic stuff
byte magicStuffFlags = 0;

// Magic stuff animation
unsigned int magicStuffAnimTime = 0;
byte newStuffIndex = 0;

// a number from 0 to 2
void magicStuffAnimation (byte newStuffNum) {
	state = MAGIC_STUFF_ANIMATION;
	magicStuffAnimTime = 0;
	newStuffIndex = newStuffNum;
}

void loop() {
	// Update individual animation state machines
	// Secret passage lights
	if (passageAnimFrame < 40) {
		passageAnimFrame++;
		// Flicker out once before popping on completely
		if ((passageAnimFrame > 10) && (passageAnimFrame <= 10+10)) {
			setSecretPassagesLight(0, arePassagesOpen());
			setSecretPassagesLight(1, !arePassagesOpen());
		} else {
			setSecretPassagesLight(0, !arePassagesOpen());
			setSecretPassagesLight(1, arePassagesOpen());
		}
	} else if (passageAnimFrame == 40) {
		passageAnimFrame = 150; // past 40
		setSecretPassagesLight(0, !arePassagesOpen());
		setSecretPassagesLight(1, arePassagesOpen());
	}
	
	// Update main state machine
	if (state == WANDER_STATE) {
		time++;
		
		if (currRoom == 6 && ((magicStuffFlags & 0x01) == 0)) {
			magicStuffAnimation (0);
		} else if (currRoom == 8 && ((magicStuffFlags & 0x02) == 0)) {
			magicStuffAnimation (1);
		} else if (currRoom == 2 && ((magicStuffFlags & 0x04) == 0)) {
			magicStuffAnimation (2);
		}

		// Update RGB lamp
		if (currRoom == 3) {
			strip[0] = CHSV ((time*3) % 256, 255, 127);
			FastLED.show();
		} else {
			// Pulse at 1 hz
			if (playerCounter < 105) {
				strip[0] = CHSV (170, 255, triwave8(playerCounter * 5 / 2)/2);
			} else {
				strip[0] = CRGB(0, 0, 0);
			}
			FastLED.show();
		}

		// Move player
		if (--playerCounter <= 0) {
			//setPlayerLight(currRoom, 0); // Let it stay on for a little
			lastRoom = currRoom;
			currRoom = getNextWanderRoom();
			rooms[currRoom].visited = true;
			setPlayerLight(currRoom, 1);
			
			playerCounter = 180;
			if (lastRoom != currRoom) {
				playerTrailRemaining = 6;
			}
		}
		
		// Toggle secret passages at random
		if (--passagesCounter <= 0) {
			setPassagesOpen(random(6) > 2);
			passagesCounter = 180;
		}
		
		// update magic ring!
		if (--ringCounter <= 0) {
			setMagicRingLight(currRingLight, false);
			currRingLight = (currRingLight+1) % 4;
			if (currRoom == 3) {
				setMagicRingLight(currRingLight, true);
			}
			
			ringCounter = 10;
		}
		
		delay(800/fps);
	} else if (state == MAGIC_STUFF_ANIMATION) {
		// Show an animation
		if (magicStuffAnimTime == 0) {
			strip[0] = CRGB (0, 0, 0);
			FastLED.show();
		}
		
		// Spin magic stuff for start
		#define MS_END_OF_START 27*3
		if (magicStuffAnimTime < MS_END_OF_START) {
			setMagicStuffByFlag((1 << (magicStuffAnimTime/3 % 3)));
		} else if (magicStuffAnimTime >= MS_END_OF_START && magicStuffAnimTime < MS_END_OF_START + 100) {
			setMagicStuffByFlag(magicStuffFlags | ((1 << newStuffIndex) * ((magicStuffAnimTime / 14) % 2)));
		} else {
			// Actually commit new flag
			magicStuffFlags |= (1 << newStuffIndex);
			
			// Display stuff
			setMagicStuffByFlag(magicStuffFlags);
			
			// End animation
			state = WANDER_STATE;
		}
		
		magicStuffAnimTime++;
		delay(1000/50);
	} else if (state == START_FLOOR_ANIMATION) {
		
	}
		
	// After a moment turn off the player's trail
	if (playerTrailRemaining != NO_TRAIL_REMAINING) {
		playerTrailRemaining--;
		if (playerTrailRemaining == 0) {
			playerTrailRemaining = NO_TRAIL_REMAINING;
			setPlayerLight(lastRoom, 0);
		}
	}
}
