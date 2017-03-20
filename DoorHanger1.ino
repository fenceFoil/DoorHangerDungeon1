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
void setPassagesOpen(bool open) {
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
}

bool arePassagesOpen() {
	return rooms[3].numNeighbors > 0;
}

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
	
	setupMap();
}

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

void show3BitsOnZombies(int i) {
	setZombieLight(0, 255 * ((i & 0x01) > 0));
	setZombieLight(1, 255 * ((i & 0x02) > 0));
	setZombieLight(2, 255 * ((i & 0x04) > 0));
}


const byte SECRET_PASSAGES_SHIFT_PINS[] = {7+8, 5+8};
void setSecretPassagesLight(byte light, byte state) {
	setShiftLight(SECRET_PASSAGES_SHIFT_PINS[light], state);
}

const byte SECRET_SWITCH_SHIFT_PIN = 0;
void setSecretSwitchLight(byte state) {
	setShiftLight(SECRET_SWITCH_SHIFT_PIN, state);
}

int currRoom = 0;

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

byte playerCounter = 7;
byte passagesCounter = 3;
byte currRingLight = 0;
void loop() {
	// for (int i=0;i<18;i++){
		// setPlayerLight(i%9, i/9);
		// delay(1000);
	// }
	
	if (--playerCounter <= 0) {
		strip[0] = CRGB(0, 255, 255);
		FastLED.show();
		
		setPlayerLight(currRoom, 0);
		currRoom = getNextWanderRoom();
		rooms[currRoom].visited = true;
		setPlayerLight(currRoom, 1);
		
		playerCounter = 10;
	}
	
	if (--passagesCounter <= 0) {
		setPassagesOpen(random(6) > 2);
		passagesCounter = 10;
	}
	
	// update magic ring!
	setMagicRingLight(currRingLight, false);
	currRingLight = (currRingLight+1) % 4;
	if (currRoom == 3) {
		setMagicRingLight(currRingLight, true);
	}
	
	delay(100);
}
