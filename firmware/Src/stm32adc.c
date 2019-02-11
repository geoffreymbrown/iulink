/**************************************************************************
*      Copyright 2018  Geoffrey Brown                                     *
*                                                                         *
*                                                                         *
*                                                                         *
* Licensed under the Apache License, Version 2.0 (the "License");         *
* you may not use this file except in compliance with the License.        *
* You may obtain a copy of the License at                                 *
*                                                                         *
*     http://www.apache.org/licenses/LICENSE-2.0                          *
*                                                                         *
* Unless required by applicable law or agreed to in writing, software     *
* distributed under the License is distributed on an "AS IS" BASIS,       *
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
* See the License for the specific language governing permissions and     *
* limitations under the License.                                          *
**************************************************************************/

#include "hal.h"
#define ADC_CCR_CKMODE_AHB_DIV1         (1 << 16)

static void adc_lld_analog_on(void) {
 ADC1->CR |= ADC_CR_ADEN;
  while ((ADC1->ISR & ADC_ISR_ADRDY) == 0)
    chThdYield();
}

static void adc_lld_analog_off(void){
  ADC1->CR |= ADC_CR_ADDIS;
  while ((ADC1->CR & ADC_CR_ADDIS) != 0)
    chThdYield();
}

static void adc_lld_calibrate(void){
  //  osalDbgAssert(ADC1->CR == ADC_CR_ADVREGEN, "invalid register state");

    /* (1) Ensure that ADEN = 0 */
  /* (2) Clear ADEN by setting ADDIS*/
  /* (3) Clear DMAEN */
  /* (4) Launch the calibration by setting ADCAL */
  /* (5) Wait until ADCAL=0 */
  
  if ((ADC1->CR & ADC_CR_ADEN) != 0) /* (1) */
    {
      ADC1->CR |= ADC_CR_ADDIS;      /* (2) */
    }
  while ((ADC1->CR & ADC_CR_ADEN) != 0)
    {
      chThdYield();
      /* For robust implementation, add here time-out management */
    }
  ADC1->CFGR1 &= ~ADC_CFGR1_DMAEN; /* (3) */
  ADC1->CR |= ADC_CR_ADCAL; /* (4) */
  while ((ADC1->CR & ADC_CR_ADCAL) != 0) /* (5) */
    {
      chThdYield();
      /* For robust implementation, add here time-out management */
    }
}

static void adc_lld_stop_adc(void) {

  if (ADC1->CR & ADC_CR_ADSTART) {
    ADC1->CR |= ADC_CR_ADSTP;
    while (ADC1->CR & ADC_CR_ADSTP)
      chThdYield();
  }
}


void adc1Start(void) {
  rccEnableADC1(true);
  rccResetADC1();  
  ADC1_COMMON->CCR = ADC_CCR_CKMODE_AHB_DIV1;  
  ADC1->IER = 0;
  adc_lld_calibrate();
  adc_lld_analog_on();
}

void adc1Stop(void) {
  adc_lld_stop_adc();
  adc_lld_analog_off();
  rccDisableADC1();
}

int adc1StartConversion(uint16_t channel, uint16_t delay) {
  if ((channel > 18) || (delay > 7))
    return -1;
  
  // stop any outstanding conversion !
  adc_lld_stop_adc();
  // configure SMPR
  ADC1->SMPR = delay;
  // configure sequence
  ADC1->CHSELR = 1<<channel; 
  // start conversion
  ADC1->CR |= ADC_CR_ADSTART;
  return 0;
}

bool adc1Eoc(void) {
  return ADC1->ISR & ADC_ISR_EOC;
}

uint32_t adc1DR(void) {
  return ADC1->DR;
}

void adc1StopConversion(void) {
  adc_lld_stop_adc();
}

void adc1EnableVREF(void){
  ADC1_COMMON->CCR |= ADC_CCR_VREFEN;
}

void adc1DisableVREF(void){
  ADC1_COMMON->CCR &= ~ADC_CCR_VREFEN;
}

void adc1EnableTS(void) {
  ADC1_COMMON->CCR |= ADC_CCR_TSEN;
}

void adc1DisableTS(void) {
  ADC1_COMMON->CCR &= ~ADC_CCR_TSEN;
}
