
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\

  Description
    Foobar

  Author
    Tobias Holzmann
  Date
    April 2020

\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "battery.h"

//* * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * *//

Battery::Battery()
:
  mode_(Battery::EMPTY),
  id_(-1),
  channel_(-1),
  overSampling_(-1),
  nCycles_(-1),
  tOld_(0),
  t_(0),
  tOffset_(0),
  R_(0),
  U_(0),
  I_(0),
  P_(0),
  C_(0),
  e_(0),
  tDischarge_(0),
  nDischarges_(0),
  tCharge_(0),
  UCharge_(0),
  ICharge_(0)
{}


Battery::Battery(const int id, const unsigned long tOffset, const float R)
:
  mode_(Battery::EMPTY),
  id_(id),
  channel_(-1),
  overSampling_(20),
  nCycles_(2),
  tOld_(0),
  t_(0),
  tOffset_(tOffset),
  R_(R),
  U_(0),
  I_(0),
  P_(0),
  C_(0),
  e_(0),
  tDischarge_(0),
  nDischarges_(0),
  tCharge_(0),
  UCharge_(0),
  ICharge_(0)
{
  reset();
}


Battery::~Battery()
{
  Serial << " ****** Battery object destroyed ******" << endl << endl;
  delay (2000);
}


//* * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * * *//

bool Battery::checkIfReplacedOrEmpty() 
{
  // Get actual voltage
  const float U = readU();

  // Check if slot is empty (measured voltage < 0.5 V)
  if (U < 0.5)
  {
    setMode(Battery::EMPTY);
    reset();
    return true;
  }
  else
  {
    // Check if the state was empty, if so, set to first and return true
    if (mode() == Battery::EMPTY)
    {
      setMode(Battery::FIRST);
      
      // It was replaced
      return true;
    }

    // It was not replaced and previously not empty
    return false;
  }

  // TODO: might be optimized
  // Check if current U at A0 is around the last measurement
  // point +- 50 mV, or if we are < 2.4 V its a new one
  /*if (U < 2.4)
  {
    return true;
  }
  
  if 
  ( 
    (U > (U_ - 0.05) && U < (U_ + 0.051))
  )
  {
    return false;
  }
  else
  {
    return true;
  }*/
}


void Battery::setOffset(const unsigned long tOffset)
{
  tOffset_ = tOffset;
}


void Battery::setU(const float U)
{
  U_ = U;
}


void Battery::setU()
{
  U_ = readU();
}


void Battery::setI(const float I)
{
  I_ = I;
}


void Battery::incrementDischarges()
{
  ++nDischarges_;
}


unsigned int Battery::nDischarges() const
{
  return nDischarges_;
}


void Battery::setMode(const enum mode m)
{
  mode_ = m;

  if (mode_ == Battery::CHARGE)
  {
    digitalWrite(D1, HIGH);
  }
  else if (mode_ == Battery::DISCHARGE)
  {
    digitalWrite(D1, LOW);
  }
  else if (mode_ == Battery::EMPTY)
  {
    digitalWrite(D1, HIGH);
  }
}


enum Battery::mode Battery::mode() const
{
  return mode_;
}


void Battery::reset()
{
  tOld_ = 0;
  t_ = 0;
  tOffset_ = 0;
  U_ = 0;
  I_ = 0;
  P_ = 0;
  C_ = 0;
  e_ = 0;
  tDischarge_ = 0;
  tCharge_ = 0;
  UCharge_ = 0;
  ICharge_ = 0;

  // Set all points to 0
  for (size_t i = 0; i < sizeof(UBat_)/sizeof(float); ++i)
  {
    UBat_[i] = 0;
  }
}


void Battery::update()
{
  // Update the actual voltage - we do it for both modes
  // + charging
  // + discharging
  setU();

  // The rest is only used for discharging
  if (mode() == Battery::DISCHARGE)
  {
    // Calculate the current (mA)
    I_ = U_/R_ * 1000.;
    
    // Calculate the current dissipation (mW)
    P_ = U_ * I_;
    
    // Update the time
    tOld_ = t_;
    t_ = millis() - tOffset_;
    const float dt = t_ - tOld_;
    tDischarge_ += dt / 1000.;
    
    // Calcualte the capacity (mAh)
    C_ += I_ * dt / 1000. / 3600.;
    
    // Calculate the energy (mWh)
    e_ += P_ * dt / 1000. / 3600.;
    
    Serial 
      << "   ++ t = " << _FLOAT(tDischarge_, 2) << " (s)  "
      << "   ++ U = " << _FLOAT(U_, 5) << " (V)  " 
      << "   ++ I = " << _FLOAT(I_, 5) << " (mA)  " 
      << "   ++ P = " << _FLOAT(P_, 2) << " (mW)  "
      << "   ++ C = " << _FLOAT(C_, 2) << " (mAh)  "
      << "   ++ e = " << _FLOAT(e_, 2) << " (mWh)" << endl; 
  }
}


bool Battery::charging()
{
  // Increment time
  tCharge_ += (millis() - tOffset_)/ 1000.;
  
  // If current voltage is lower than 4.05V we are not fully charged
  // The charging modules charge up to 4.05V
  if (U_ < 4.1)
  {
    return true;
  }

  // This needs to be done by checking the voltage AND the current through
  // the battery

  // Re-arange UBat
  int i = -1;
  for (auto& value : UBat_)
  {
    if (i >= 0)
    {
      UBat_[i] = value;
    }

    ++i;
    
    // Last datapoint is the actual measured voltage
    if (i == sizeof(UBat_)/sizeof(float) -1)
    {
      UBat_[i] = U_;
    }
  }

  // Check if all values are identical by an error of 1e-5 to the actual value
  float tmp = 0;

  for (const auto value : UBat_)
  {
    tmp += value;
  }

  tmp /= sizeof(UBat_)/sizeof(float);

  // Charging finished
  if (abs(U_ - tmp) < 1e-5)
  {
    return false;
  }
  // Still charging
  else
  {
    return true;
  }
}


bool Battery::discharging()
{
  // If the current voltage is lower than 2.5V we stop discharging
  if (U_ < 2.60)
  {
    return false;
  }
  // Still discharging
  else
  {
    return true;
  }
}


void Battery::checkIfFullyTested()
{
  if (nDischarges_ == nTotalDischarges_)
  {
    setMode(Battery::TESTED);
  }
  else
  {
    setMode(Battery::DISCHARGE);
  }
}


float Battery::readU() const
{
    // Make 20 measeurements and create the mean value
    int Udigital = 0;

    // Sampling more values with delay of 25 ms
    for (int k = 0; k < overSampling_; ++k)
    {
      int tmp = analogRead(A0 - 17);
      Udigital += constrain(tmp, 0, 1023);
      delay(10);
    }

    Udigital /= overSampling_;
    
    // Convert digital voltage to analog voltage
    // The values given below need to be calibrated 
    return DtoA(Udigital, 0, 794, 0, 3.2835);
}


//* * * * * * * * * * * * PRIVATE Member Functions  * * * * * * * * * * * * *//

float Battery::DtoA
(
  const unsigned int digital,
  const int lowD,
  const int highD,
  const float lowA,
  const float highA
) const 
{
  // Steps in digital way
  const unsigned int digStep = highD - lowD;

  // Analog width
  const float widthA = highA - lowA;

  // Analogh width per digital step
  const float DtoA = widthA / digStep;
  
  return (DtoA * digital);
}
