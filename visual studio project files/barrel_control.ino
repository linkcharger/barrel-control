#include <DHT.h>
#include <math.h>

//#define TEST

const int dhtPin = A1;
DHT dht(dhtPin, DHT11);

//fans
const int sinkPin = 9;
const int intakePin = 10;
const int caseFan = 11;

int sinkP = 20;
int intakeP = 20;

const int sinkAim = 35;
const int sinkHys = 5;

const int thermistor = A0;
const int beta = 3950;
float T;
int h, t;

//button
const int button = 3;
volatile int presses = 0;
volatile bool pressed = false;
volatile unsigned long pushTime = 0;

//timing
unsigned long lastFanTime = 0;

//lighting
bool flower = false;
bool automatic = true;
bool status = 0;
const int lights = A4;
unsigned long lastLightTime, lightsOffPeriod;

//RGB LED
char color;
const int rgb[4] = {8,7,6,5};


void setup() {
	//pins
	pinMode(sinkPin, OUTPUT);
	pinMode(intakePin, OUTPUT);
	pinMode(caseFan, OUTPUT);
	analogWrite(caseFan, 175);

	for (int i = 0; i <= 3; i++) {
		pinMode(rgb[i], OUTPUT);
		digitalWrite(rgb[i], 0);
	}
	pinMode(button, INPUT_PULLUP);
	pinMode(2, OUTPUT);
	digitalWrite(2, LOW);

	//fast pwm
	//waveform generation mode 
	TCCR1A |= bit(WGM10);
	TCCR1A &= ~bit(WGM11);
	TCCR1B |= bit(WGM12);
	TCCR1B &= ~bit(WGM13);

	//clock select: 1 (no prescaler)
	TCCR1B |= bit(CS10);
	TCCR1B &= ~(bit(CS11) | bit(CS12));
	

	dht.begin();
	attachInterrupt(digitalPinToInterrupt(button), buttonISR, LOW);
}

void loop() {
	outputs();
	readSensors();
	delay(1000);

	//==== adjust fan speeds ======= every 2min 
	if (millis() - lastFanTime >= 120000ul) {
		lastFanTime = millis();
		adjustPower();
	}

	//set value
	analogWrite(sinkPin, sinkP * 255 / 100);
	analogWrite(intakePin, intakeP * 255 / 100);

	//==== light control ========== 
	if (!flower) lightsOffPeriod = 21600000ul;		//6h
	else lightsOffPeriod = 43200000ul;				//12h

#ifdef TEST
	digitalWrite(lights, 1);
#else
	//rest of whole days
	if ((automatic && millis() % 86400000ul > lightsOffPeriod) || (!automatic && status)) digitalWrite(lights, 1);
	else digitalWrite(lights, 0);
#endif
}

long readVcc() {
	long result;
	// Read 1.1V reference against AVcc
	ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
	delay(2); // Wait for Vref to settle
	ADCSRA |= _BV(ADSC); // Convert
	while (bit_is_set(ADCSRA, ADSC));
	result = ADCL;
	result |= ADCH << 8;
	result = 1125300L / result; // Back-calculate AVcc in mV
	return result;
}

float readTemp() {
	//NTC calculation
	float vcc = readVcc() / 1000.0;
	float v = vcc * (float) analogRead(thermistor) / 1023;
	float r = -4700 * (v / (v - vcc));
	float revT = (float) 1 / 298 + (float) 1 / beta * (float) log(r / 4700);
	return (1 / revT - 277);
}

void blink(int blinks, int ontime) {
	int offtime = ontime * 3;
	int r, g, b;

	switch (color) {
		case 'g':
			r = 0;
			g = 200;
			b = 0;
			break;
		case 'y':
			r = 255;
			g = 100;
			b = 0;
			break;
		case 'r':
			r = 200;
			g = 0;
			b = 0;
			break;
		case 'b':
			r = 50;
			g = 100;
			b = 200;
			break;
		case 'c':
			r = 200;
			g = 150;
			b = 75;
			break;
	}

	analogWrite(rgb[0], r);
	analogWrite(rgb[2], g);
	analogWrite(rgb[3], b);

	for (int i = 0; i < blinks; i++) {

		digitalWrite(rgb[1], 0);
		delay(ontime);

		digitalWrite(rgb[1], 1);
		delay(offtime);
	}

}

void readSensors() {
	h = dht.readHumidity() + 11;
	t = dht.readTemperature() + 1;
	T = readTemp();
}

void displayTempHum() {
	//temperature
	//sink
	if (T <= sinkAim) color = 'b';
	else if (T > sinkAim && T <= sinkAim + sinkHys) color = 'y';
	else if (T > sinkAim + sinkHys) color = 'r';

	blink((int) T / 10, 100);
	delay(1000);
	blink((int) T % 10, 100);
	delay(2000);

	//air
	color = 'c';

	blink((int) t / 10, 100);
	delay(1000);
	blink((int) t % 10, 100);
	delay(2000);

	//humidity
	blink((int) h / 10, 100);
}

void displayFans() {
	color = 'c';
	blink((int) sinkP / 10, 100);
	delay(1000);
	blink((int) sinkP % 10, 100);
	delay(2000);
	blink((int) intakeP / 10, 100);
	delay(1000);
	blink((int) intakeP % 10, 100);
}

void displayMode() {
	if (!flower) color = 'g'; else if (flower) color = 'r';
	blink(1, 1000);
}

void adjustPower() {
	//HEAT SINK
	//if lamps off: fan off
	if (T <= 27) sinkP = 0;
	//low enough temp
	else if (T <= sinkAim - sinkHys && sinkP >= 1) sinkP--;
	//higher temp
	else if (T > sinkAim + sinkHys && sinkP <= 85) sinkP += 10;


	//INTAKE
	//too hot or humid: crank it!
	if ((t >= 30 || h > 80) && intakeP < 85) intakeP += 5;
	//cool enough: calm it! 
	else if (t < 30 && intakeP > 10) intakeP--;
}

void changeMode() {
	digitalWrite(rgb[0], 1);
	analogWrite(rgb[2], 150);
	analogWrite(rgb[3], 75);
	digitalWrite(rgb[1], 0);

	//wait 5s
	for (int i = 0; i < 500; i++) delay(10);
	digitalWrite(rgb[1], 1);
	delay(800);

	if (pressed && presses == 5 + 2) {
		flower = !flower;
		//confirmation
		if (!flower) color = 'g'; else if (flower) color = 'r';
		blink(2, 50);

		pressed = false;
		presses = 0;
	}
}

void switchLightControl() {
	//current control mode
	if (automatic) color = 'g'; else color = 'c';
	blink(1, 1000);
	
	automatic = !automatic;

	//updated mode
	if (automatic) color = 'g'; else color = 'c';
	blink(1, 1000);
}



void outputs() {
	if (pressed) {

		//wait 3s for other presses
		for (int i = 0; i < 300; i++) delay(10);

		// 5sec are over,  display stuff:
		switch (presses) {
			case 1:	displayTempHum(); break;
			case 2: displayFans(); break;
			case 3: status = !status; break;
			case 4: switchLightControl(); break;
			case 5: displayMode(); break;
			case 6: changeMode(); break;
			default:
				color = 'r';
				blink(3, 500);

		}

		// reset communication with buttonISR
		presses = 0;
		pressed = false;
	}
}

void buttonISR() {
	if (millis() - pushTime >= 200) {
		presses++;

		//set flag
		pressed = true;
		pushTime = millis();
	}
}