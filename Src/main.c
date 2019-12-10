/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/******************************************************************************/
/* 																			  */
/* 	 						   	  INCLUDES				     				  */
/* 														 					  */
/******************************************************************************/
#include "time.h"
#include "main.h"
#include "adc.h"
#include "spi.h"
#include "gpio.h"
#include "tim.h"
#include <stdio.h>
#include "lcd_driver.h"

/******************************************************************************/
/* 																			  */
/* 	 	    		  TYPE DEFINITIONS, DEFINES & MACROS					  */
/* 																			  */
/******************************************************************************/
typedef enum {
	STANDARD,
	PRONOUNCED
} mode;

/******************************************************************************/
/* 																			  */
/* 	 						  PRIVATE VARIABLES								  */
/* 																			  */
/******************************************************************************/

// METRONOME SETTINGS
mode op_mode;

int bpm = 50;
int MIN_BPM = 40;
int MAX_BPM = 200;
int vol = 0;
int MAX_VOL = 100;
int beat = 1;
int beats_per_bar = 4;
int MIN_BPB = 2;
int MAX_BPB = 12;

// BUTTON FLAGS
uint8_t bpb_up_flag = 0;
uint8_t bpb_down_flag = 0;
uint8_t toggle_mode_flag = 0;

// BUFFERS FOR TEXT OUTPUT
char* vol_tbuff[100];
char* bpm_tbuff[100];
char* mode_tbuff[100];
char* bar_tbuff[2];
char* beat_tbuff[2];

// ADC
const uint32_t ADC_MAX = 4095; // max analog value
uint32_t pot1_raw = 0;
uint32_t pot2_raw = 0;


/******************************************************************************/
/* 																			  */
/* 	 						 FUNCTION PROTOTYPES							  */
/* 																			  */
/******************************************************************************/
void SystemClock_Config(void);
int fputc(int ch, FILE *f) {  ITM_SendChar(ch);  return(ch); }

void beep_and_blink(int duration, int pitch, float volume);
void update_volume();
void update_bpm();
void update_display();
void beat_machine();
void standard_beatroutine();
void pronounced_beatroutine();
void button_push();

/******************************************************************************/
/* 																			  */
/* 	 					   FUNCTION DEFINITIONS								  */
/* 																			  */
/******************************************************************************/
// EXTERNAL INTERRUPTS
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	__HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_Pin);

	// down
	if (GPIO_Pin == GPIO_PIN_0) {
		bpb_down_flag = 1;
	}

	// up
	if (GPIO_Pin == GPIO_PIN_4) {
		bpb_up_flag = 1;
	}

	// center
	if (GPIO_Pin == GPIO_PIN_5) {
		toggle_mode_flag = 1;
	}
}

// TIMER INTERRUPTS
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim6){
	beat_machine();
}

// METRONOME FUNCTIONS
void beep_and_blink(int duration, int pitch, float volume){
	// calculate prescaler for given pitch
	uint32_t clockspeed = 16000000;
	uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim2);
	uint32_t prescaler = clockspeed / (pitch * period);

	// calculate duty cycle
	uint32_t dc = 0;
	dc = ((float)volume/(float)MAX_VOL * 0.1) * period;

	// set prescaler new prescaler
	TIM2->PSC = (uint32_t) prescaler;

	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, dc);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
	HAL_Delay(duration);
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 0);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
}

void update_volume(){
	// read analog signal
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 50);
	pot1_raw = HAL_ADC_GetValue(&hadc1);
	HAL_ADC_Stop(&hadc1);

	// update value
	vol = ((float)pot1_raw/(float)ADC_MAX) * (float)MAX_VOL;
}

void update_bpm(){
	// read analog signal
	HAL_ADC_Start(&hadc2);
	HAL_ADC_PollForConversion(&hadc2, 50);
	pot2_raw = HAL_ADC_GetValue(&hadc2);
	HAL_ADC_Stop(&hadc2);

	// update value
	bpm = MIN_BPM + (float)pot2_raw/(float)ADC_MAX * (float)(MAX_BPM - MIN_BPM);

	// calculate new prescaler
	uint32_t ps;
	ps = 16000000 / (float)((float)bpm/60 * TIM6->ARR);

	// adjust timer prescaler to change bpm
	TIM6->PSC = (uint32_t) ps;
}

void update_display(){
	lcd_clear();

	// add some lines for nice formatting
	lcd_setLine(0,0,0,31,1);
	lcd_setLine(48,0,48,31,1);
	lcd_setLine(127,0,127,31,1);
	lcd_setLine(0,15,127,15,1);
	lcd_setLine(110,0,110,31,1);

	// display volume
	sprintf((char*)vol_tbuff,"vol %d", vol);
	lcd_setString(4,4,(const char*)vol_tbuff,LCD_FONT_8,false);

	// display bpm
	sprintf((char*)bpm_tbuff,"%d bpm", bpm);
	lcd_setString(4,20,(const char*)bpm_tbuff, LCD_FONT_8,false);

	// display mode
	if (op_mode == STANDARD){
		sprintf((char*)mode_tbuff,"STANDARD");
	}else if(op_mode == PRONOUNCED){
		sprintf((char*)mode_tbuff,"PRONOUNCED");
	}
	lcd_setString(50,4,(const char*)mode_tbuff, LCD_FONT_8,false);

	// display number of beats per repetition
	sprintf((char*)bar_tbuff,"%d", beats_per_bar);
	lcd_setString(114,4,(const char*)bar_tbuff, LCD_FONT_8,false);

	// display beats
	int min_dist = 52;
	int max_dist = 105;
	int pos = 0;
	for (int b = 0; b < beats_per_bar; ++b){
		pos = min_dist + (float)b / (float)beats_per_bar * (max_dist - min_dist);
		lcd_setString(pos, 17, "o", LCD_FONT_8,false);
	}

	// display beat indicator
	pos = min_dist + (float)(beat-1) / (float)beats_per_bar * (max_dist - min_dist);
	lcd_setString(pos, 24, "*", LCD_FONT_8,false);

	// display beat number
	sprintf((char*)beat_tbuff,"%d", beat);
	lcd_setString(114,20,(const char*)beat_tbuff, LCD_FONT_8,false);

	lcd_show();

}

void beat_machine(){
	switch (op_mode){
		case STANDARD:
			standard_beatroutine();
			break;

		case PRONOUNCED:
			pronounced_beatroutine();
			break;
	}
}

void standard_beatroutine(){
	int beep_time = 50;
	if (beat < beats_per_bar){
		beep_and_blink(beep_time, 1000, vol);
		beat += 1;
	}else{
		beep_and_blink(beep_time, 1000, vol);
		beat = 1;
	}
}

void pronounced_beatroutine(){
	int beep_time = 50;
	if (beat < beats_per_bar){
		beat += 1;
		beep_and_blink(beep_time, 1000, vol);
	}else{
		beat = 1;
		beep_and_blink(beep_time, 2000, vol); // higher pitched beep for first beat
	}
}

void button_push(){
	// checks if buttons have been pushed (debouncing)

	// down
	if (bpb_down_flag == 1) {
		if (beats_per_bar > MIN_BPB){
			beats_per_bar -= 1;
		}else{
			beats_per_bar = MAX_BPB;
		}
		bpb_down_flag = 0;
	}

	// up
	if (bpb_up_flag == 1) {
		if (beats_per_bar < MAX_BPB){
			beats_per_bar += 1;
		}else{
			beats_per_bar = MIN_BPB;
		}
		bpb_up_flag = 0;
	}

	// center
	if (toggle_mode_flag == 1) {
		if (op_mode == PRONOUNCED){
			op_mode = STANDARD;
		}else{
			op_mode = PRONOUNCED;
		}
		toggle_mode_flag = 0;
	}
}


/******************************************************************************/
/* 																			  */
/* 	 						       	MAIN 									  */
/* 																			  */
/******************************************************************************/
int main(void)
{
	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the system clock */
	SystemClock_Config();

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_ADC1_Init();
	MX_ADC2_Init();
	MX_SPI1_Init();
	MX_TIM2_Init();
	MX_TIM6_Init();

	// initialize LEDs
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);

	// initialize LCD
	sendData(0xA5);
	lcd_init();

	// start PWM timer and timer interrupt
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
	HAL_TIM_Base_Start_IT(&htim6);

	// set default operation mode
	op_mode = STANDARD;

	/* Infinite loop */
	while (1)
	{
		button_push();
		update_volume();
		update_bpm();
		update_display();
		HAL_Delay(5);
	}
}

/******************************************************************************/
/* 																			  */
/* 	 					AUTOMATICALLY GENERATED FUNCTIONS					  */
/* 																			  */
/******************************************************************************/
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	*/
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
	/** Initializes the CPU, AHB and APB busses clocks
	*/
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
	Error_Handler();
	}
	/** Initializes the CPU, AHB and APB busses clocks
	*/
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
							  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
	{
	Error_Handler();
	}
}


/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	lcd_setString(4,16,"	ERROR OCCURED	",LCD_FONT_8,false);
	lcd_show();
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
