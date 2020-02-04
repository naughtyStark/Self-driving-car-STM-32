// I2Cdev library collection - MPU9150 I2C device class
// Based on InvenSense MPU-9150 register map document rev. 2.0, 5/19/2011 (RM-MPU-6000A-00)
// 8/24/2011 by Jeff Rowberg <jeff@rowberg.net>
// Updates should (hopefully) always be available at https://github.com/jrowberg/i2cdevlib
//
// Changelog:
//     ... - ongoing debug release

// NOTE: THIS IS ONLY A PARIAL RELEASE. THIS DEVICE CLASS IS CURRENTLY UNDERGOING ACTIVE
// DEVELOPMENT AND IS STILL MISSING SOME IMPORTANT FEATURES. PLEASE KEEP THIS IN MIND IF
// YOU DECIDE TO USE THIS PARTICULAR CODE FOR ANYTHING.

/* ============================================
I2Cdev device library code is placed under the MIT license
Copyright (c) 2012 Jeff Rowberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
===============================================
*/

#include "MPU9150.h"
#include "SIDMATH.h" //the library is needed for certain functions

#define LPF_GAIN (float)1/2 //gains 100Hz LPF
#define C1 (float)0.0

//filter stuff

/** Default constructor, uses default I2C address.
 * @see MPU9150_DEFAULT_ADDRESS
 */
MPU9150::MPU9150() {
    devAddr = MPU9150_DEFAULT_ADDRESS;
    V = 0;
    bias = 0;
    V_Error = 0;
    mh_Error = 0;
    pitch_Error = 0;
    roll_Error = 0;
    heading_drift = 0;
    for(int i =0;i<4;i++)
    {
      lastG[i] = 0;
      gyro_Bias[i] = 0;
      for(int j=0;j<2;j++)
      {
        xA[i][j] = yA[i][j] = 0; //initializing things from 0
      }
    }
    return;
}

/** Specific address constructor.
 * @param address I2C address
 * @see MPU9150_DEFAULT_ADDRESS
 * @see MPU9150_ADDRESS_AD0_LOW
 * @see MPU9150_ADDRESS_AD0_HIGH
 */
// MPU9150::MPU9150(uint8_t address) {
//     devAddr = address;
// }

void MPU9150::setAddress(uint8_t address)//default constructor for initializing I2C address.
{
  devAddr = address;
  return;
}

/** Power on and prepare for general usage.
 * This will activate the device and take it out of sleep mode (which must be done
 * after start-up). This function also sets both the accelerometer and the gyroscope
 * to their most sensitive settings, namely +/- 2g and +/- 250 degrees/sec, and sets
 * the clock source to use the X Gyro for reference, which is slightly better than
 * the default internal clock source.
 */
bool MPU9150::initialize() //initialize the gyro with the apt scaling factors.
{
    setClockSource(MPU9150_CLOCK_PLL_XGYRO);
    setFullScaleGyroRange(MPU9150_GYRO_FS_2000);
    long timeout = micros();
    error_code = 0;
    while(getFullScaleGyroRange() != MPU9150_GYRO_FS_2000)
    {
      setFullScaleGyroRange(MPU9150_GYRO_FS_2000); //keep trying 
      if(micros() - timeout>1000)
      {
        error_code += 1;
        break;
      }
    }
    setFullScaleAccelRange(MPU9150_ACCEL_FS_8);
    while(getFullScaleAccelRange() != MPU9150_ACCEL_FS_8 )
    {
      setFullScaleAccelRange(MPU9150_ACCEL_FS_8);
      if(micros() - timeout>1000)
      {
        error_code += 1;
        break;
      }
    }
    setSleepEnabled(false); // thanks to Jack Elston for pointing this one out!
    pinMode(MPU_LED,OUTPUT);
    if(error_code != 0)
    {
      return 0; //return 0 if error code is non zero because a badly setup gyro is worse than no gyro
    }
    return testConnection();
}

/** Verify the I2C connection.
 * Make sure the device is connected and responds as expected.
 * @return True if connection is valid, false otherwise
 */
bool MPU9150::testConnection() {
    return getDeviceID();//== 1;
}

void MPU9150::setClockSource(uint8_t source) {
    I2Cdev::writeBits(devAddr, MPU9150_RA_PWR_MGMT_1, MPU9150_PWR1_CLKSEL_BIT, MPU9150_PWR1_CLKSEL_LENGTH, source);
    return;
}

void MPU9150::setSleepEnabled(bool enabled) {
    I2Cdev::writeBit(devAddr, MPU9150_RA_PWR_MGMT_1, MPU9150_PWR1_SLEEP_BIT, enabled);
    return;
}

void MPU9150::setFullScaleGyroRange(uint8_t range) {
    I2Cdev::writeBits(devAddr, MPU9150_RA_GYRO_CONFIG, MPU9150_GCONFIG_FS_SEL_BIT, MPU9150_GCONFIG_FS_SEL_LENGTH, range);
    return;
}

uint8_t MPU9150::getFullScaleGyroRange() {
    I2Cdev::readBits(devAddr, MPU9150_RA_GYRO_CONFIG, MPU9150_GCONFIG_FS_SEL_BIT, MPU9150_GCONFIG_FS_SEL_LENGTH, buffer);
    return buffer[0];
}

void MPU9150::setFullScaleAccelRange(uint8_t range) {
    I2Cdev::writeBits(devAddr, MPU9150_RA_ACCEL_CONFIG, MPU9150_ACONFIG_AFS_SEL_BIT, MPU9150_ACONFIG_AFS_SEL_LENGTH, range);
    return;
}

uint8_t MPU9150::getFullScaleAccelRange() {
    I2Cdev::readBits(devAddr, MPU9150_RA_ACCEL_CONFIG, MPU9150_ACONFIG_AFS_SEL_BIT, MPU9150_ACONFIG_AFS_SEL_LENGTH, buffer);
    return buffer[0];
}

uint8_t MPU9150::getDeviceID() {
    I2Cdev::readBits(devAddr, MPU9150_RA_WHO_AM_I, MPU9150_WHO_AM_I_BIT, MPU9150_WHO_AM_I_LENGTH, buffer);
    return buffer[0];
}

void MPU9150::readMag()
{
  byte buf[6];

  I2Cdev::readBytes(0x0C, 0x03, 6, buf); // get 6 bytes of data
  m[1] = (((int16_t)buf[1]) << 8) | buf[0]; // the mag has the X axis where the accelero has it's Y and vice-versa
  m[0] = (((int16_t)buf[3]) << 8) | buf[2]; // so I just do this switch over so that the math appears easier to me. 
  m[2] = (((int16_t)buf[5]) << 8) | buf[4];

  I2Cdev::writeByte(devAddr,0x37,0x02);
  I2Cdev::writeByte(0x0C, 0x0A, 0x01); //enable the magnetometer

  return;
}

void MPU9150::readIMU()
{
  Wire.beginTransmission(devAddr);  //begin transmission with the gyro
  Wire.write(0x3B); //start reading from high byte register for accel
  Wire.endTransmission();
  Wire.requestFrom(devAddr,14); //request 14 bytes from mpu
  //300us for all data to be received. 
  //each value in the mpu is stored in a "broken" form in 2 consecutive registers.(for example, acceleration along X axis has a high byte at 0x3B and low byte at 0x3C 
  //to get the actual value, all you have to do is shift the highbyte by 8 bits and bitwise add it to the low byte and you have your original value/. 
  a[0]=Wire.read()<<8|Wire.read();  
  a[1]=Wire.read()<<8|Wire.read(); 
  a[2]=Wire.read()<<8|Wire.read(); 
  t=Wire.read()<<8|Wire.read();  //this one is actually temperature but i dont need temp so why waste memory.
  g[0]=Wire.read()<<8|Wire.read();  
  g[1]=Wire.read()<<8|Wire.read();
  g[2]=Wire.read()<<8|Wire.read();

  return;
}

void MPU9150::getMotion6(int16_t* ax, int16_t* ay, int16_t* az, int16_t* gx, int16_t* gy, int16_t* gz) {
    I2Cdev::readBytes(devAddr, MPU9150_RA_ACCEL_XOUT_H, 14, buffer);
    *ax = (((int16_t)buffer[0]) << 8) | buffer[1];
    *ay = (((int16_t)buffer[2]) << 8) | buffer[3];
    *az = (((int16_t)buffer[4]) << 8) | buffer[5];
    *gx = (((int16_t)buffer[8]) << 8) | buffer[9];
    *gy = (((int16_t)buffer[10]) << 8) | buffer[11];
    *gz = (((int16_t)buffer[12]) << 8) | buffer[13];
}

void MPU9150::getMotion9(int16_t* ax, int16_t* ay, int16_t* az, int16_t* gx, int16_t* gy, int16_t* gz, int16_t* mx, int16_t* my, int16_t* mz) {

    //get accel and gyro
    getMotion6(ax, ay, az, gx, gy, gz);
    I2Cdev::writeByte(devAddr, MPU9150_RA_INT_PIN_CFG, 0x02); //set i2c bypass enable pin to true to access magnetometer
    delay(10);
    I2Cdev::writeByte(MPU9150_RA_MAG_ADDRESS, 0x0A, 0x01); //enable the magnetometer
    delay(10);
    //read mag
    I2Cdev::readBytes(MPU9150_RA_MAG_ADDRESS, MPU9150_RA_MAG_XOUT_L, 6, buffer);
    *mx = (((uint16_t)buffer[1]) << 8) | buffer[0];
    *my = (((uint16_t)buffer[3]) << 8) | buffer[2];
    *mz = (((uint16_t)buffer[5]) << 8) | buffer[4];

}

void MPU9150::blink(int8_t n)
{
  for(int8_t i=0;i<n;i++)
  {
    digitalWrite(MPU_LED,1);
    delay(500);
    digitalWrite(MPU_LED,0);
    delay(500);
  }

  return;
}

void MPU9150::gyro_caliberation()
{
  //just leave the car stationary. the LED will blink once when the process begins, twice when it ends.
  blink(1);
  float dummy[3] = {0,0,0};
  float dummyT = 0;
  int i,j;
  for(i = 0;i<1000;i++)
  {
    readIMU();
    for(j = 0;j<3;j++)
    {
      dummy[j] += g[j];
    }
    dummyT += t;
    delayMicroseconds(500); //give it some rest
    // Serial.println(g[0]);//debug
  }
  for(j=0;j<3;j++)
  {
    offsetG[j] = dummy[j]/1000;
  }
  offsetT = dummyT/1000;
  delay(1000);
  blink(2);
  return;
}

void MPU9150::accel_caliberation()
{
  //place the car on a roughly horizontal surface, wait for the led to blink twice, then thrice, then rotate the car M_PI_DEG degrees within
  //5 seconds, the process repeats. 
  float dummy[2][3] = {{0,0,0},
                       {0,0,0}};
  int i,j,k;
  for(k = 0;k<2;k++)
  { 
    blink(2);
    for(i = 0;i<1000;i++)
    {
      readIMU();
      for(j = 0;j<3;j++)
      {
        dummy[k][j] += float(a[j])*0.001;
      }
      delay(1);
      // Serial.println(a[0]);
    }
    blink(3);
    delay(5000);//should be enough time to change the orientation of the car.
  }
  for(i = 0;i<3;i++)
  {
    offsetA[i] = (dummy[0][i]+dummy[1][i])/2;//got the offsets!
  }

  return;
}

void MPU9150::mag_caliberation()
{
  //the mpu LED will blink once.
  //point the nose of the car in the NS direction, rotate the car around the lateral axis of the car for ~8 seconds,
  //point the nose of the car in the EW direction, rotate the car around the longitudenal axis of the car until you see
  //the MPU LED blink twice
  int16_t i,j,oldmax[3],oldmin[3];
  getMotion9(&a[0],&a[1],&a[2],&g[0],&g[1],&g[2],&m[1],&m[0],&m[2]);
  for(j=0;j<3;j++)
  {
    oldmin[j]=oldmax[j] = m[j];
  }
  blink(1);//indicate that the process has started
  delay(1000);
  for(i=0;i<800;i++) //16 seconds
  {
    getMotion9(&a[0],&a[1],&a[2],&g[0],&g[1],&g[2],&m[1],&m[0],&m[2]);
    for(j=0;j<3;j++)
    {
      if (oldmax[j] < m[j])
      {
        oldmax[j] = m[j];
      }
      if(oldmin[j] > m[j])
      {
        oldmin[j] = m[j];
      }//basically finding the min max values of each field and then taking an average.
    }
    // Serial.println(m[0]);
    digitalWrite(MPU_LED,1); //visual indication
    delay(20);
    digitalWrite(MPU_LED,0);
  }
  blink(2);
  float magnitude[3],total=0;
  for(j=0;j<3;j++)
  {
    offsetM[j] = (oldmin[j]+oldmax[j])/2;
    magnitude[j] = fabs(oldmax[j] - oldmin[j]);
    total += magnitude[j];
  }
  axis_gain[0] = int16_t(3000*(magnitude[0]/total));
  axis_gain[1] = int16_t(3000*(magnitude[1]/total));
  axis_gain[2] = int16_t(3000*(magnitude[2]/total));
  return;
}

void MPU9150::getOffset(int16_t offA[3],int16_t offG[3],int16_t offM[3],int16_t &offT, int16_t gain[3]) //remember that arrays are passed by address by default.
{
  for(int i=0;i<3;i++)
  {
    offA[i] = offsetA[i];
    offG[i] = offsetG[i];
    offM[i] = offsetM[i];
    gain[i] = axis_gain[i];
  }
  offT = offsetT;
  return;
}

void MPU9150::setOffset(int16_t offA[3],int16_t offG[3],int16_t offM[3], int16_t &offT, int16_t gain[3])
{
  for(int i=0;i<3;i++)
  {
    offsetA[i] = offA[i];
    offsetG[i] = offG[i];
    offsetM[i] = offM[i];
    axis_gain[i] = gain[i];
  }
  offsetT = offT;
  return;
}

void MPU9150::readAll(bool mag_Read)
{
  readIMU();
  for(int i=0;i<3;i++)
  {
    A[i] = float(a[i] - offsetA[i])*ACCEL_SCALING_FACTOR;
    // A[i] = LPF(i,A[i]);

    G[i] = float(g[i] - offsetG[i])*GYRO_SCALING_FACTOR + temp_Compensation(t);
    // G[i] = filter_gyro(lastG[i],G[i]);
    delG[i] = G[i]-lastG[i];
    lastG[i] = G[i];
  }
  if(mag_Read)
  {
    readMag();
    for(int i=0;i<3;i++)
    {
      M[i] = (float)(m[i] - offsetM[i]); //hard iron shit
      M[i] *= invert_axis_gain[i];//(float(axis_gain[i])*1e-3); //soft iron shit.
    }
  }
  return;
}

float MPU9150::tilt_Compensate(float cosPitch,float cosRoll, float sinPitch, float sinRoll) //function to compensate the magnetometer readings for the pitch and the roll.
{
  float heading;
  //the following formula compensates for the pitch and roll of the object when using magnetometer reading. 
  float Xh = -M[0]*cosRoll + M[2]*sinRoll;
  float Yh = M[1]*cosPitch - M[0]*sinRoll*sinPitch + M[2]*cosRoll*sinPitch;
  float mag = fast_sqrt(M[0]*M[0] + M[1]*M[1] + M[2]*M[2])*0.6805f;

  Xh = Xh*0.2f + magbuf[0]*0.8f; //smoothing out the X readings
  magbuf[0] = Xh;

  Yh = Yh*0.2f + magbuf[1]*0.8f; //smoothing out the Y readings
  magbuf[1] = Yh;
  
  heading = RAD2DEG*atan2f(Yh,Xh);
  float X_B_E = Xh*HORIZ_EARTH_MAG_INV;
  Sanity_Check(1.0f,X_B_E);
  del = RAD2DEG*acosf(X_B_E);
  mag_gain = spike(mag,49.0f); //49 -> 0.49 guass = earth's magnetic field strength in delhi, India. 
  fabs(mag-49)>20? mag_failure = true: mag_failure=false ;//check for high levels of interference.

  if(mh > M_PI_DEG && mh < M_2PI_DEG)
  {
    del = M_2PI_DEG - del;
  }

  if(mh <130.0f || mh > 230.0f)
  {
    heading = del*mag_gain + (1-mag_gain)*heading;
  }
  heading += DECLINATION;
  if(heading<0.0f) //atan2 goes from -pi to pi 
  {
    return M_2PI_DEG + heading; //2pi - theta
  }

  return heading;
}//800us worst case on arduino uno 16mhz 8 bit. 800/13.85 on stm32f103c8t6

void MPU9150::compute_All()
{ 
  //define all variables
  float d_Yaw_Radians;
  float Anet;
  float trust,trust_1;
  float innovation[3];
  float V_mes;
  bool mag_Read = false;
  float cosRoll,_sinRoll,cosPitch,_sinPitch;
  float gain;

  testConnection() ? failure = false : failure = true; //checking for sensor failure before reading. this has a 63us penalty :'( 
  
  if(failure)
  {
    initialize(); //413us penalty.
    return; //break it off right here. take a break. have a kit kat.
  }

  if(millis() - stamp>MAG_UPDATE_TIME_MS) //more than 10ms have passed since last mag read. maintain 100hz update rate for magnetometer
  {
    stamp = millis();
    mag_Read = true;
  }

  readAll(mag_Read);//read the mag if the condition is true.
  //PREDICTION STEP (ROLL AND PITCH FIRST)
  d_Yaw_Radians = (G[2]*dt*DEG2RAD); //change in yaw around the car's Z axis (this is not exactly the change in heading)
  roll  += (G[1] - gyro_Bias[1])*dt - pitch*d_Yaw_Radians; // the roll is calculated first because everything else is actually dependent on the roll. 
  cosRoll = cosf(roll*DEG2RAD); //precomputing them as they are used repetitively.
  _sinRoll = -sinf(roll*DEG2RAD);
  //chaning the roll doesn't change the heading. Changing the pitch can change the heading.
  //YES there will be some error in pitch that will cause an error in the roll, that is exactly why I have a low pass filter applied to both pitch and roll for values between 2 and 5 degrees
  pitch += dt*(G[0]*cosRoll - G[2]*_sinRoll - gyro_Bias[0]) + roll*d_Yaw_Radians; //compensates for the effect of yaw and roll on pitch
  cosPitch = cosf(pitch*DEG2RAD);
  _sinPitch = -sinf(pitch*DEG2RAD);

  //if the car is going around a banked turn, then the change in heading is not the same as yawRate*dt. P.S: cos is an even function.
  mh += dt*(G[2]*cosRoll +G[0]*_sinRoll - gyro_Bias[2]); //compensates for pitch and roll of gyro(roll pitch compensation to the yaw).
  //INCREMENT ERROR
  pitch_Error += GYRO_VARIANCE; //increment the errors each cycle
  roll_Error += GYRO_VARIANCE;
  mh_Error += GYRO_VARIANCE;

  //CORRECTION STEP AND REDUCING THE ERROR AFTER CORRECTION IN ROLL AND PITCH
  Anet = (A[0]*A[0] + A[1]*A[1] + (A[2]+GRAVITY)*(A[2]+GRAVITY)); //square of net acceleration. TODO : FIX THIS BITCH
  trust = exp_spike(G_SQUARED,Anet);// spiky boi filter. Basically the farther away the net acceleration is from g,

  if( fabs(A[1])<GRAVITY-1.0f ) //sanity check on accelerations so that we don't get Naans
  {
    innovation[0] = pitch; //dummy;
    float pitch_trust = trust*pitch_Error;
    trust_1 = (1 - pitch_trust);
    pitch = trust_1*pitch + pitch_trust*RAD2DEG*my_asin(A[1]*G_INVERSE); //G_INVERSE = 1/9.8
    pitch_Error *= trust_1; //reduce the error everytime you make a correction
    innovation[0] -= pitch; //actual innovation
    gyro_Bias[0] += pitch_trust*innovation[0]*dt; //get bias
  }
  if( fabs(A[0])<GRAVITY-1.0f )
  {
    innovation[1] = roll;
    float roll_trust = trust*roll_Error;
    trust_1 = (1 - roll_trust);
    roll  = trust_1*roll  - roll_trust*RAD2DEG*my_asin(A[0]*G_INVERSE); //using the accelerometer to correct the roll and pitch.
    roll_Error *= trust_1;
    innovation[1] -= roll;
    gyro_Bias[1] += roll_trust*innovation[1]*dt;
  }
  yawRate = G[2] - gyro_Bias[2]; // yaw_Rate.

  //conditional low pass filtering between 1 and 4 degrees. 100Hz LPF  
  if(fabs(roll)>1.0f&&fabs(roll)<4.0f)
  {
    roll = LPF(0,roll);
  }
  if(fabs(pitch)>1.0f&&fabs(pitch)<4.0f)
  {
    pitch = LPF(1,pitch); //applying a 100 Hz LPF to these signals. 
  }//TODO : do we need this low pass filter?
  Sanity_Check(M_PIB2_DEG,roll); //sanity checks.
  Sanity_Check(M_PIB2_DEG,pitch);

  if(mh >= M_2PI_DEG) // the mh must be within [0.0,360.0]
  {
    mh -= M_2PI_DEG;
  }
  if(mh < 0.0f)
  {
    mh += M_2PI_DEG;
  }
  // CORRECTION OF HEADING AND REDUCING ERROR AFTER CORRECTION.
  if( mag_Read )//check if mag has been read or not.
  { 
    float mag_head = tilt_Compensate(cosPitch,cosRoll,-_sinPitch,-_sinRoll); //get tilt compensated heading
    if(fabs(mag_head - mh)>M_PI_DEG) // this happens when the heading is in the range of 5-0-355 degrees
    {
      mh = M_2PI_DEG-mh;// doing this because mag is the measured value. can't change that you know.
    }
    innovation[2] = mh;//dummy
    mag_gain /= max(fabs(yawRate),1.0f);
    mag_gain *= mh_Error;//scale the gain in proportion to the mh_Error
    Sanity_Check(0.05f,mag_gain);//just in case
    mh = (1.0f-mag_gain)*mh + mag_gain*(mag_head);
    innovation[2] -= mh; //actual innovation
    mh_Error *= (1.0f-mag_gain); //reduce the error.
    gyro_Bias[2] += mag_gain*innovation[2]*MAG_UPDATE_RATE;//adjust bias
    heading_drift = innovation[2]; //integrate heading drift.
  }
  //Estimating speed.
  Ha = (A[1] + GRAVITY*_sinPitch)*cosPitch - bias;// world frame NOTE : due to the LPF, this can report incorrect values when you shake the car. 
  // Sanity_Check(10.0f,Ha);
  La = A[0] - GRAVITY*_sinRoll; //lateral acceleration in global reference. 
  La = LPF(2,La);
  Sanity_Check(10.0f,La);
  if(fabs(yawRate)>10.0f) //this check is to ensure a decent signal to noise ratio.
  {
    float yawRadians = yawRate*DEG2RAD;//rate of rotation in radians. OPTIMIZE
    float yawRadians2 = yawRadians*yawRadians; //square it 
    // radius = -La/yawRadians2;//estimating radius of curvature using R = Ax/omega^2
    // radius = 0.05f*radius + 0.95f*encoder_velocity[2]; //this is to make sure we don't have too much noise. encoder_velocity[2] is estimated roc from steering signal 
    radius = encoder_velocity[2]; //this already considers the lateral acceleration so the above 2 lines are not important
    Ha -= (La*DIST_BW_ACCEL_AXLE/radius); //This is the correction for not having the accelerometer at the rear axle
    Sanity_Check(10.0f,Ha);
    //TODO : insert method to trust A/w less when dw/dt is large (Steering angle changing because that's why we get errors in speed bruh)
    // La -= d_Yaw_Radians*(d_Yaw_Radians + 2*yawRadians)*radius; //correction for centrifugal force.
    V_mes = -(La + d_Yaw_Radians*(d_Yaw_Radians + 2*yawRadians)*radius)/yawRadians; //measure velocity as V = A/w, since A = wr.w and wr is the speed.
    V += Ha*dt; //propogate the state of the car through time.
    V_Error += dt*fabs(ACCEL_VARIANCE*cosPitch + A[1]*_sinPitch*pitch_Error + 2.0f*GRAVITY*my_cos(2.0f*pitch*DEG2RAD)*pitch_Error);//expression for error. God I wish this was fixed too!
    float mes_error = max(fabs(Ha),1.0f)*CIRCULAR_VELOCITY_ERROR/(min(fabs(yawRadians2),1.0f));
    gain = V_Error/(mes_error + V_Error); //CIRCULAR VELOCITY ERROR is fixed and is defined in the header.
    V = (1.0f-gain)*V + gain*V_mes;//correction step
    V_Error *= (1.0f-gain);//adjusting the error in the velocity.
  }
  else//if the car ain't turnin 
  {
    // Ha -= (La*DIST_BW_ACCEL_AXLE*1e-3);
    V += Ha*dt;//propogate the state through time.
    V_Error += dt*fabs(ACCEL_VARIANCE*cosPitch + A[1]*_sinPitch*pitch_Error + 2.0f*GRAVITY*my_cos(2.0f*pitch*DEG2RAD)*pitch_Error);//expression for error
  }
  return;
}//570us worst case. 

void MPU9150::get_Rotations(float omega[3])
{
  omega[0] = DEG2RAD*G[0];
  omega[1] = DEG2RAD*G[1]; //pass the rotations in radians.
  omega[2] = V; //pass the velocity too
}

void MPU9150::Setup()//initialize the state of the marg.
{
  for(int i=0;i<3;i++){ invert_axis_gain[i] = 1000/float(axis_gain[i]); }
  readAll(1); //read accel,gyro,mag 
  delay(10);
  readAll(1);
  pitch = RAD2DEG*my_asin(A[1]*G_INVERSE); //G_INVERSE = 1/9.8
  roll  = -RAD2DEG*my_asin(A[0]*G_INVERSE);   
  mh = tilt_Compensate(cosf(pitch*DEG2RAD),cosf(roll*DEG2RAD),sinf(pitch*DEG2RAD),sinf(roll*DEG2RAD)); //find initial heading. Roll, pitch have to be in radians
  yawRate = G[2]; //initial YawRate of the car.
  stamp = millis(); //get first time stamp.
  return;
}

float MPU9150::filter_gyro(float mean, float x)
{
  float i =  x - mean;//innovation
  i *= (i*i)/(GYRO_FILTER_FACTOR + (i*i) ); //notch filter around the mean value.
  return mean + i;
}

float MPU9150::LPF(int i,float x)
{
  xA[i][0] = xA[i][1]; 
  xA[i][1] = x*LPF_GAIN;
  yA[i][0] = yA[i][1]; 
  yA[i][1] =   (xA[i][0] + xA[i][1]) + ( C1* yA[i][0]);
  return yA[i][1];
}

float MPU9150::temp_Compensation(int16_t temp)
{
  return GYRO_SCALING_FACTOR*TEMP_COMP*(temp - offsetT);
}

void MPU9150::Velocity_Update(float &velocity,float VelError, float Accbias)
{
  V = LPF(3,velocity);//is this needed?
  Sanity_Check(50,V);
  velocity = V; 
  V_Error = VelError;
  bias = Accbias; 
}

// void MARG_FUSE(MPU9150 marg[2])
// {
//   if(marg[0].failure + marg[0].failure == 0 )//neither of them failed
//   {
//     Fuse( marg[0].roll, marg[0].roll_Error, marg[1].roll, marg[1].roll_Error);
//     Fuse( marg[0].pitch, marg[0].pitch_Error, marg[1].pitch, marg[1].pitch_Error);
//     Fuse( marg[0].mh, marg[0].mh_Error, marg[1].mh, marg[1].mh_Error);
//     Fuse( marg[0].V, marg[0].V_Error, marg[1].V, marg[1].V_Error); 
//   }
//   if(marg[0].failure)
//   {
//     marg[0].roll = marg[1].roll;
//     marg[0].roll_Error = marg[1].roll_Error;

//     marg[0].pitch = marg[1].pitch;
//     marg[0].pitch_Error = marg[1].pitch_Error;

//     marg[0].mh = marg[1].mh;
//     marg[0].mh_Error = marg[1].mh_Error;

//     marg[0].V = marg[1].V;
//     marg[0].V_Error = marg[1].V_Error;
//   }
//   if(marg[1].failure)
//   {
//     marg[1].roll = marg[0].roll;
//     marg[1].roll_Error = marg[0].roll_Error;

//     marg[1].pitch = marg[0].pitch;
//     marg[1].pitch_Error = marg[0].pitch_(1-Error);

//     marg[1].mh = marg[0].mh;
//     marg[1].mh_Error = marg[0].mh_Error;

//     marg[1].V = marg[0].V;
//     marg[1].V_Error = marg[0].V_Error;
//   }
//   else if (marg[0].failure && marg[1].failure)
//   {
//     //cry me a river
//   }
// }
//TODO : an NED acceleration and velocity thingy for drone.
//TODO : velocity estimator for car.
