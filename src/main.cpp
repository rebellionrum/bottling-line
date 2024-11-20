// A script to control a bottling machine
// there are a number of relays on different pins
// 9 - clamp
// 8 - cap
// 7 - push (registration)
// 6 - fill
// 2 - conveyor belt

// Plus there is a ultrasound sensor to detect the bottles, it has pin 10 (echo) and pin 11 (trig)

// the order of operations is
// 1. run the conveyor belt for 1 minute or until the ultrasound senses a bottle > 4cm for 2 seconds
// 2. push the bottles to the capper
// 3. load a cap
// 4. clamp the bottle
// 5. fill the bottle
// 6. release the clamp

// while the bottle is being filled, the clamp will be closed
// also while the bottle is being filled, we should run the conveyour until it detects a loaded bottle.

const int clampPin = 9;
const int capPin = 8;
const int pushPin = 7;
const int fillPin = 6;
const int beltPin = 2;
const int echoPin = 10;
const int trigPin = 11;

const int maxDistance = 4;
const long beltTimeout = 60L * 1000L;

const int capTime = 1000;
const int pushTime = 2500;
const int fillTime = 16000;
const int releaseTime = 1000;

#include <Arduino.h>
#include <AsyncTimer.h>

AsyncTimer timer;

void setup()
{
  Serial.begin(9600);
  // put your setup code here, to run once:
  pinMode(clampPin, OUTPUT);
  pinMode(capPin, OUTPUT);
  pinMode(pushPin, OUTPUT);
  pinMode(fillPin, OUTPUT);
  pinMode(beltPin, OUTPUT);

  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);

  Serial.println("Setup complete");
}

int step = 1;
bool stateComplete = true;
int ultrasonicDistance = 0;
short beltTimerId = 0;
bool triggeredBelt = false;
bool isLoadingBottle = false;
void loadBottle(bool isStep = false)
{
  if (!isLoadingBottle)
  {
    isLoadingBottle = true;
    if (isStep)
    {
      stateComplete = false;
    }

    digitalWrite(beltPin, HIGH);

    beltTimerId = timer.setTimeout([]
                                   {
    if(triggeredBelt) {
      digitalWrite(beltPin, LOW);
    } }, beltTimeout);
  }
}

void registerBottle()
{

  stateComplete = false;

  digitalWrite(pushPin, HIGH);
  timer.setTimeout([]
                   {
    if(step == 2) {
      digitalWrite(pushPin, LOW);
    }

    if(pushTime > capTime) {
      step = 3;
      stateComplete = true;
    } }, pushTime);

  digitalWrite(capPin, HIGH);

  timer.setTimeout([]()
                   {
    digitalWrite(capPin, LOW);
    if(capTime > pushTime){
      step = 3;
      stateComplete = true;
    } }, capTime);
}

void fillBottle()
{

  stateComplete = false;

  // Do clamp
  digitalWrite(clampPin, HIGH);

  loadBottle(false);

  // Do fill
  timer.setTimeout([]
                   { digitalWrite(fillPin, HIGH); }, releaseTime);

  // Release fill
  timer.setTimeout([]
                   { digitalWrite(fillPin, LOW); }, fillTime + releaseTime);

  // Release clamp & reset process
  timer.setTimeout([]
                   {
      digitalWrite(clampPin, LOW);
      step = 1;
      stateComplete = true; }, fillTime + (releaseTime * 2));
}

int ultrasonicRead()
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  int distance = duration * 0.034 / 2;
  ultrasonicDistance = distance;

  return distance;
}

void handleBelt()
{
  if (triggeredBelt == false && ultrasonicRead() <= maxDistance)
  {
    triggeredBelt = true;
    isLoadingBottle = false;
    timer.setTimeout([]()
                     {
                    triggeredBelt = false;
      digitalWrite(beltPin, LOW);
      if (beltTimerId > 0)
      {
        timer.cancel(beltTimerId);
      }
      if (step == 1)
      {
        step = 2;
        stateComplete = true;
      } }, 1000);
  }
}

void loop()
{
  Serial.print("Distance: ");
  Serial.println(ultrasonicRead());
  Serial.print("Triggered: ");
  Serial.println(triggeredBelt);
  Serial.print("Step: ");
  Serial.println(step);
  Serial.print("State complete: ");
  Serial.println(stateComplete);

  if (stateComplete)
  {
    switch (step)
    {
    case 1:
      loadBottle(true);
      break;
    case 2:
      registerBottle();
      break;
    case 3:
      fillBottle();
      break;
    }
  }
  timer.handle();
  handleBelt();
  delay(1000);
}