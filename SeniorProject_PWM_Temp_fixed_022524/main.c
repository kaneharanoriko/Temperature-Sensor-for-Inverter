#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/debug.h"
#include "driverlib/pwm.h"
#include "driverlib/pin_map.h"
#include "inc/hw_gpio.h"
#include "driverlib/rom.h"
#include "inc/tm4c123gh6pm.h"
#include "driverlib/i2c.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#define PWM_FREQUENCY 30000
uint32_t initPWM();
void initI2C();
void initUART();
uint32_t ReadI2CTemperature();
int32_t ConvertToCelcius(uint32_t temperature);
int32_t ConvertToFahrenheit(uint32_t temperature);

int main(void)
{
    uint32_t duty[12] = {63, 85, 98, 98, 85, 63, 37, 15, 2, 2, 15, 37};
    uint32_t i, j;//set up for which one in array
    volatile uint32_t ui32period;
    int32_t celcius, fahrenheit, temperature;
    //Set bus clock to 40 MHz
    SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);
    ui32period = initPWM();
    initI2C();
    initUART();
    i = 0; //first index
    while(1)
    {
        PWMPulseWidthSet(PWM1_BASE, PWM_OUT_0,  ui32period * duty[i] / 100);// Setup for % duty cycle
        if(i++ > 10)  {
            i = 0; //once max set back to min
        }
        temperature = ReadI2CTemperature();
        celcius = ConvertToFahrenheit(temperature);
        fahrenheit = ConvertToFahrenheit(temperature);
        UARTprintf("Temperature = %d %x %d(celcius) %d(fahrenheit) \n", temperature, temperature,celcius,fahrenheit);
        //Delay Loop 1ms
        for(j = 0; j<4000; j++){}
    }
}

int32_t ConvertToCelcius(uint32_t temperature)
{
    float tempFloat,tempC;
    int32_t celcius;
    tempFloat = temperature;
    tempC = tempFloat * (0.0625);
    celcius = tempC;
    return celcius;
}
int32_t ConvertToFahrenheit(uint32_t temperature)
{
    float tempFloat,tempC,tempF;
    int32_t fahrenheit;
    tempFloat = temperature;
    tempC = tempFloat * (0.0625);
    tempF = tempC*(9.0/5.0) + 32.0 + 0.5;
    fahrenheit = tempF;
    return fahrenheit;
}

uint32_t ReadI2CTemperature()
{
    int32_t temperature;
    uint32_t frame_value, assemble;

    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_RECEIVE_START); //Outputs first byte(address) and receives top 8 bits of temperature
    while(I2CMasterBusy(I2C0_BASE))  //Wait for I2CO master to finish receiving
    {}//8 bits back
    frame_value = I2CMasterDataGet(I2C0_BASE); //Get the top 8 bits, MSB's, of the temperature from I2C hardware
    assemble = frame_value << 24; //Dr.Putty has done for us.
    I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_RECEIVE_FINISH);  //Receives the bottom 4 bits of the temperature
    while(I2CMasterBusy(I2C0_BASE))  //Wait for I2CO master to finish receiving
    {}
    frame_value = I2CMasterDataGet(I2C0_BASE); //Get the bottom 4 bits, MSB's, of the temperature from the I2C hardware
    frame_value =(frame_value<<16)&0x00F00000;
    assemble = assemble | frame_value;
    temperature = assemble;
    temperature = temperature >> 20;
    return temperature;
}

uint32_t initPWM()
{
    uint32_t ui32period; //volatile
    uint32_t ui32PWMClock; //volatile
    uint8_t ui8Adjust; //volatile
    ui8Adjust = 10;

    //Set PWM clock to 10 MHz
    SysCtlPWMClockSet(SYSCTL_PWMDIV_4); //40 MHz/4  = 10 MHz by setting divider 4
    //Enable, Turn on clock to PWM1, GPIOD, GPIOF //Pin PDO
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    //Configure pin PDO for PWM output
    GPIOPinTypePWM(GPIO_PORTD_BASE, GPIO_PIN_0);
    GPIOPinConfigure(GPIO_PD0_M1PWM0);
    //Configure PORTF switches
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;  //unlock PF0
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) |= 0x01;
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = 0;
    GPIODirModeSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_DIR_MODE_IN);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_4|GPIO_PIN_0, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);  //Sets pins 1,2,3 as outputs
    //Set up PWM Clock
    ui32PWMClock = SysCtlClockGet() / 4;  //PWM clock is  10 MHz
    ui32period = (ui32PWMClock / PWM_FREQUENCY) - 1;  //PWM period
    PWMGenConfigure(PWM1_BASE, PWM_GEN_0, PWM_GEN_MODE_DOWN);  //Count down mode
    PWMGenPeriodSet(PWM1_BASE, PWM_GEN_0, ui32period);  //Set the PWM period

    PWMPulseWidthSet(PWM1_BASE, PWM_OUT_0, ui8Adjust * ui32period / 1000);
    PWMOutputState(PWM1_BASE, PWM_OUT_0_BIT, true);  //Enable PWM output
    PWMGenEnable(PWM1_BASE, PWM_GEN_0);  //Turn PWM1 on
    return ui32period;
}
void initI2C()
{
    //Turn on clock to GPIOB and I2C0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_I2C0)){}
    //Configure GPIOB pins to be used by I2C0
    GPIOPinConfigure(GPIO_PB2_I2C0SCL);
    GPIOPinConfigure(GPIO_PB3_I2C0SDA);
    GPIOPinTypeI2CSCL(GPIO_PORTB_BASE, GPIO_PIN_2);
    GPIOPinTypeI2C(GPIO_PORTB_BASE, GPIO_PIN_3);
    //Set the Launcpad as the I2C master with the slow clock 100 Kbps
    I2CMasterInitExpClk(I2C0_BASE, SysCtlClockGet(), false);
    //Set the address to the TMP102. First byte sent to the TMP102 for each read.
    //*Set this value correctly***************************************/
    I2CMasterSlaveAddrSet(I2C0_BASE, 0x48, true);  //This value is the top 7 MSB's
}
void initUART()
{
    /***  UART Setup   *****************************************************************/
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlDelay(3);
    //Configure GPIOA pins for UART0
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    //Enable UART0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlDelay(3);
    //Configure UART0 Baud Rate 115200
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
                    (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    //Configure UART to use stdio libraries printf
    UARTStdioConfig(0, 115200, SysCtlClockGet());
}


