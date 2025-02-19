/*
 * SCU4 low level system control
 */


#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "io-func.h"




int main (void)
{
    while ( !readPGood3_3V() )
    {
        _delay_ms(DELAY_16MS_EARLY_OSC);
    }
    

    typedef enum {PWR_IDLE = 0,PWR_UP0,PWR_UP1,PWR_UP2,PWR_OK,PWR_DOWN1,PWR_DOWN0} pwr_state_t;

    pwr_state_t state = PWR_IDLE;

    uint16_t PORflag = 0; //Power on Reset Flag

    initIO();

    //Switch to 32MHz internal oscillator
    OSC.CTRL |= OSC_RC32MEN_bm;                 // Enable the internal 32MHz
    while(!(OSC.STATUS & OSC_RC32MRDY_bm));     // Wait for 32MHz oscillator to stabilize
    CCP = CCP_IOREG_gc;                         // Disable register security for clock update
    CLK.CTRL = CLK_SCLKSEL_RC32M_gc;            // Switch to 32MHz clock
    OSC.CTRL &= ~OSC_RC2MEN_bm;                 // Disable 2Mhz oscillator

    ADC_init();

    //Output Init

    enableCoreVoltage (low);
    enable1_8V (low);
    enable1_8VIO (low);
    enable5V (low);
    enableComXpowerOk (low);
    disableIO();
    performReset();

    while(1)
        {
            // Power Sequence State Machine
            switch(state)
                {
                case PWR_IDLE:
                    PORflag = 0;
                    indicatorLED(red);
                    if ( ( read_MP_ADC() >= MP_ON_ADC_THRES ) && readPGood3_3V())
                        {
                            state = PWR_UP0;
                        }
                    break;
                case PWR_UP0:
                    enableCoreVoltage (high);
                    indicatorLED(yellow);
                    if ( ( read_MP_ADC() <= MP_FAIL_ADC_THRES )|| !readPGood3_3V())
                        {
                            state = PWR_DOWN0;
                        }
                    else if (readPGoodCore())
                        {
                            state = PWR_UP1;
                        }
                    break;
                case PWR_UP1:
                    enable1_8V (high);
                    indicatorLED(yellow);
                    if ( ( read_MP_ADC() <= MP_FAIL_ADC_THRES )|| !readPGood3_3V() || !readPGoodCore())
                        {
                            state = PWR_DOWN1;
                        }
                    //else if (readPGood1_8V())
                    else if (read_V1_8_ADC() >= V1_8_GOOD_ADC_THRES)
                        {
                            state = PWR_UP2;
                        }
                    break;
                case PWR_UP2:
                    enable1_8VIO (high);
                    indicatorLED(yellow);
                    if ( ( read_MP_ADC() <= MP_FAIL_ADC_THRES ) || !readPGood3_3V() || !readPGoodCore() || !readPGood1_8V())
                        {
                            state = PWR_DOWN1;
                        }
                    else if (read_V1_8IO_ADC() >= V1_8IO_GOOD_ADC_THRES)
                        {
                            state = PWR_OK;
                        }
                    break;
                //Power Down
                case PWR_DOWN1:
                    enableComXpowerOk(low);
                    disableIO();
                    enable5V (low);
                    enable1_8VIO (low);
                    enable1_8V (low);
                    indicatorLED(magenta);
                    if ( (read_V1_8_ADC() <= V1_8_OFF_ADC_THRES) && (read_V1_8IO_ADC() <= V1_8IO_OFF_ADC_THRES) )
                        {
                            state = PWR_DOWN0;
                        }
                    break;
                case PWR_DOWN0:
                    enableCoreVoltage (low);
                    indicatorLED(magenta);
                    if (read_CORE_ADC() <= CORE_OFF_ADC_THRES)
                        {
                            state = PWR_IDLE;
                        }
                    break;
                // Power OK
                case PWR_OK:
                    if ( ( read_MP_ADC() <= MP_FAIL_ADC_THRES ) || !readPGood3_3V() || !readPGoodCore() || !readPGood1_8V() || ( read_V1_8IO_ADC() <= V1_8IO_FAIL_ADC_THRES ) )
                        {
                            performReset();
                            state = PWR_DOWN1;
                        }
                    enable5V (high);
                    enableComXpowerOk(high);
                    enableIO();
                    //Reset Behavior                
                    if (!readAllResets())
                        {
                            //Reset
                            performReset();
                            indicatorLED(blue);
                        }
                    else
                        {
                            if (!PORflag)
                                {
                                    PORflag = 1;
                                    releaseReset();
                                    indicatorLED(green);
                                    _delay_ms(POR_DELAY);
                                    performReset();
                                    indicatorLED(blue);
                                    _delay_ms(POR_DELAY);
                                }
                            else
                                {
                                    //System Running
                                    releaseReset();
                                    indicatorLED(green);
                                }
                        }
                    break;
                }

        }

    return 0;
}