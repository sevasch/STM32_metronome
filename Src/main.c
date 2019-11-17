/* USER CODE BEGIN Header */
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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "lcd_driver.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
char* buff[100];
char* buff2[100];
uint8_t tempData[2];

uint16_t LM75Address = 0x90; // I2C address of temperature sensor LM75BD
uint16_t PCA9685Address = 0x40; // I2C address of PCA9685

uint16_t tempRegPointer = 0x00; // Address offset of temperature Register

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
int fputc(int ch, FILE *f) {  ITM_SendChar(ch);  return(ch); }

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void pca9685_init(I2C_HandleTypeDef *hi2c, uint8_t address){
	#definePCA9685_MODE10x00uint8_t initStruct[2];
	uint8_t prescale = 3;       // hardcoded
	HAL_I2C_Master_Transmit(hi2c, address, PCA9685_MODE1, 1, 1);
	uint8_t oldmode = 0;        // hardcoded
	// HAL_I2C_Master_Receive(hi2c, address, &oldmode, 1, 1);
	uint8_t newmode = ((oldmode & 0x7F) | 0x10);
	initStruct[0] = PCA9685_MODE1;
	initStruct[1] = newmode;
	HAL_I2C_Master_Transmit(hi2c, address, initStruct, 2, 1);
	initStruct[1] = prescale;
	HAL_I2C_Master_Transmit(hi2c, address, initStruct, 2, 1);
	initStruct[1] = oldmode;
	HAL_I2C_Master_Transmit(hi2c, address, initStruct, 2, 1);
	osDelay(5);
	initStruct[1] = (oldmode | 0xA1);
	HAL_I2C_Master_Transmit(hi2c, address, initStruct, 2, 1);
}

void pca9685_pwm(I2C_HandleTypeDef *hi2c, uint8_t address, uint8_t num, uint16_t on, uint16_t off){
	uint8_t outputBuffer[5] = {0x06 + 4*num, on, (on >> 8), off, (off >> 8)};
	HAL_I2C_Master_Transmit(&hi2c1, address, outputBuffer, 5, 1);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
	/* USER CODE BEGIN 1 */
	static int32_t count=0;
	/* USER CODE END 1 */


	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_SPI1_Init();
	MX_I2C1_Init();
	/* USER CODE BEGIN 2 */
	sendData(0xA5);

	// Initialise LCD and show "Welcome" top/left justified
	lcd_init();
	lcd_clear();

	lcd_setLine(127,0,127,31,1);
	lcd_setString(4,4,"Init",LCD_FONT_8,false);
	//lcd_setString(4,16,"Line 2",LCD_FONT_8,false);
	lcd_setLine(0,0,0,31,1);
	lcd_setString(4,16,"",LCD_FONT_8,false);
	//lcd_setString(4,4,"LCD TEST",LCD_FONT_8,false);
	//  lcd_setString(0,0,"BIG ",LCD_FONT_24,false);	$bug no big font
	lcd_show();

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		volatile float temp = 0.0;
		HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port,LED_BLUE_Pin);
		//uint32_t StartTime = HAL_GetTick();
		HAL_Delay(200);
//		count++;

// 		// Access register with Master_Receive
		if(HAL_I2C_Master_Receive(&hi2c1, LM75Address|tempRegPointer, &tempData[0], 2,HAL_MAX_DELAY)!= HAL_OK)
		{
			 Error_Handler();
		}

// 		// Access register with Mem_Read
//		if(HAL_I2C_Mem_Read(&hi2c1, LM75Address, tempRegPointer, I2C_MEMADD_SIZE_8BIT,
//				&tempData[0], 2,HAL_MAX_DELAY)!= HAL_OK)
//		{
//			 Error_Handler();
//		}
		//tempData[0] XXXX XXXX and tempData[1] XXX0 0000
		temp = 0.125*(tempData[0]*8.0 + (tempData[1]>>5));
		sprintf((char*)buff2,"Temperature = %.3f", temp);


		lcd_setString(4,4,(const char*)buff2,LCD_FONT_8,false);
		lcd_show();

	/* USER CODE END WHILE */

	/* USER CODE BEGIN 3 */
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
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

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
