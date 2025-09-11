#include "math.h"

class MyNTC
{
public:
  MyNTC(int pin) : thermistor_output(pin) {}
  float readTemperature()
  {
    int thermistor_adc_val;
    double output_voltage, thermistor_resistance, therm_res_ln, temperature; 
    thermistor_adc_val = analogRead(thermistor_output);
    output_voltage = ( (thermistor_adc_val * ADC_reference_voltage) / 4095 );
    thermistor_resistance = ( ( ADC_reference_voltage * ( NTC_reference_resistance_kohm / output_voltage ) ) - NTC_reference_resistance_kohm ); /* Resistance in kilo ohms */
    thermistor_resistance = thermistor_resistance * 1000 ; /* Resistance in ohms   */
    therm_res_ln = log(thermistor_resistance);
    
    /*
      Steinhart-Hart Thermistor Equation: 
      Temperature in Kelvin = 1 / (A + B[ln(R)] + C[ln(R)]^3)
      where A = 0.001129148, B = 0.000234125 and C = 8.76741*10^-8
    */
    temperature = ( 1 / ( 0.001129148 + ( 0.000234125 * therm_res_ln ) + ( 0.0000000876741 * therm_res_ln * therm_res_ln * therm_res_ln ) ) ); /* Temperature in Kelvin */
    temperature = temperature - roomTemperatureKelvin;

    return temperature;
  }
private:
  int thermistor_output;
  const float ADC_reference_voltage = 3.3;
  const float NTC_reference_resistance_kohm = 12;
  const float roomTemperatureKelvin = 273.15;
};