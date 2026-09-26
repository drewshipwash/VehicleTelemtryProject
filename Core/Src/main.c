/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
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

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
int16_t accel_x = 0;
int16_t accel_y = 0;
int16_t accel_z = 0;

int16_t gyro_x = 0;
int16_t gyro_y = 0;
int16_t gyro_z = 0;


int32_t ax_offset_raw = 0;
int32_t ay_offset_raw = 0;
int32_t az_offset_raw = 0;

int32_t gx_offset = 0;
int32_t gy_offset = 0;
int32_t gz_offset = 0;

int accel_count = 0;
int brake_count = 0;

int left_turn_count = 0;
int right_turn_count = 0;

uint32_t distance_samples[3] = {0, 0, 0};
uint8_t distance_index = 0;
uint8_t distance_samples_filled = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint32_t median3(uint32_t a, uint32_t b, uint32_t c)
{
    if (a > b)
    {
        uint32_t temp = a;
        a = b;
        b = temp;
    }

    if (b > c)
    {
        uint32_t temp = b;
        b = c;
        c = temp;
    }

    if (a > b)
    {
        uint32_t temp = a;
        a = b;
        b = temp;
    }

    return b;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  char message[] = "Hello from STM32!\r\n";

  HAL_UART_Transmit(&huart2,
                    (uint8_t *)message,
                    strlen(message),
                    HAL_MAX_DELAY);


  uint8_t who_am_i = 0;
  HAL_StatusTypeDef status;
  char buffer[64];

  status = HAL_I2C_Mem_Read(&hi2c1, (0x68 << 1), 0x75, I2C_MEMADD_SIZE_8BIT, &who_am_i, 1, 100);

  if(status == HAL_OK){
	  snprintf(buffer, sizeof(buffer), "MPU6050 WHO_AM_I = 0x%02X\r\n", who_am_i);
  }
  else{
	  snprintf(buffer, sizeof(buffer), "MPU6050 I2C ERROR\r\n");

  }

  HAL_UART_Transmit(&huart2, (uint8_t *)buffer, strlen(buffer), HAL_MAX_DELAY);


  uint8_t wake = 0x00;
  //wake mpu6050
  HAL_I2C_Mem_Write(&hi2c1, (0x68 << 1), 0x6B, I2C_MEMADD_SIZE_8BIT, &wake, 1, 100);

  //starts timer
  HAL_TIM_Base_Start(&htim2);

  // Give sensor time to stabilize
  HAL_Delay(500);

  // ========================================
  // ACCELEROMETER CALIBRATION - 500 samples
  // ========================================
  int64_t ax_sum = 0;
  int64_t ay_sum = 0;
  int64_t az_sum = 0;

  uint8_t accel_cal[6];

  for(int i = 0; i < 500; i++){
	  //read accelerometer
	  HAL_I2C_Mem_Read(&hi2c1, (0x68 << 1), 0x3B, I2C_MEMADD_SIZE_8BIT, accel_cal, 6, 100);

	  int16_t ax_raw = (int16_t)((accel_cal[0] << 8) | accel_cal[1]);
	  int16_t ay_raw = (int16_t)((accel_cal[2] << 8) | accel_cal[3]);
	  int16_t az_raw = (int16_t)((accel_cal[4] << 8) | accel_cal[5]);

	  // accumulate raw accelerometer readings
	  ax_sum += ax_raw;
	  ay_sum += ay_raw;
	  az_sum += az_raw;

	  HAL_Delay(2);

  }

  ax_offset_raw = ax_sum / 500;
  ay_offset_raw = ay_sum / 500;
  az_offset_raw = az_sum / 500;


  // ========================================
  // GYROSCOPE CALIBRATION - keep 100 loop
  // ========================================

  int32_t gx_sum = 0;
  int32_t gy_sum = 0;
  int32_t gz_sum = 0;

  uint8_t gyro_cal[6];

  for(int i = 0; i < 100; i++){

	  //read gyroscope
	  HAL_I2C_Mem_Read(&hi2c1, (0x68 << 1), 0x43, I2C_MEMADD_SIZE_8BIT, gyro_cal, 6, 100);

	  int16_t gx_raw = (int16_t)((gyro_cal[0] << 8) | gyro_cal[1]);
	  int16_t gy_raw = (int16_t)((gyro_cal[2] << 8) | gyro_cal[3]);
	  int16_t gz_raw = (int16_t)((gyro_cal[4] << 8) | gyro_cal[5]);

	  gx_sum += gx_raw / 131;
	  gy_sum += gy_raw / 131;
	  gz_sum += gz_raw / 131;

	  HAL_Delay(10);

  }

  gx_offset = gx_sum / 100;
  gy_offset = gy_sum / 100;
  gz_offset = gz_sum / 100;

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  uint32_t echo_time = 0;
	  uint32_t distance_cm = 0;
	  uint8_t echo_valid = 1;
	  uint32_t filtered_distance_cm = 0;
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */


	//Make sure TRIG begins LOW
	HAL_GPIO_WritePin(HC_TRIG_GPIO_Port, HC_TRIG_Pin, GPIO_PIN_RESET);

	//wait about 2 microseconds
	__HAL_TIM_SET_COUNTER(&htim2, 0);
	while(__HAL_TIM_GET_COUNTER(&htim2) < 2){
		//waiting
	}
	//Sed=nd the HC_SR04 trigger pulse
	HAL_GPIO_WritePin(HC_TRIG_GPIO_Port, HC_TRIG_Pin, GPIO_PIN_SET);

	//keep it HIGH for 10 microseconds
	__HAL_TIM_SET_COUNTER(&htim2, 0);
	while(__HAL_TIM_GET_COUNTER(&htim2) < 10){
		//waiting
	}
	//End trigger pulse
	HAL_GPIO_WritePin(HC_TRIG_GPIO_Port, HC_TRIG_Pin, GPIO_PIN_RESET);



	// Reset timer while waiting for ECHO to begin
	__HAL_TIM_SET_COUNTER(&htim2, 0);

	// Wait for ECHO to go HIGH
	while (HAL_GPIO_ReadPin(HC_ECHO_GPIO_Port, HC_ECHO_Pin) == GPIO_PIN_RESET)
	{
	    // Give up if no echo arrives within 30 ms
	    if (__HAL_TIM_GET_COUNTER(&htim2) > 30000)
	    {
	        echo_valid = 0;
	        break;
	    }
	}

	// Only measure pulse if ECHO actually arrived
	if (echo_valid)
	{
	    // ECHO is HIGH now, so start timing FROM ZERO
	    __HAL_TIM_SET_COUNTER(&htim2, 0);

	    // Wait for ECHO to go LOW
	    while (HAL_GPIO_ReadPin(HC_ECHO_GPIO_Port, HC_ECHO_Pin) == GPIO_PIN_SET)
	    {
	        if (__HAL_TIM_GET_COUNTER(&htim2) > 30000)
	        {
	            echo_valid = 0;
	            break;
	        }
	    }

	    // Only calculate distance if pulse completed successfully
	    if (echo_valid)
	    {
	        echo_time = __HAL_TIM_GET_COUNTER(&htim2);
	        distance_cm = echo_time / 58;

	        // Save this valid reading into our 3-reading history
	        distance_samples[distance_index] = distance_cm;

	        // Move to the next position: 0 -> 1 -> 2 -> 0...
	        distance_index++;

	        if (distance_index >= 3)
	        {
	            distance_index = 0;
	        }

	        // Keep track of whether we have collected all 3 yet
	        if (distance_samples_filled < 3)
	        {
	            distance_samples_filled++;
	        }


	        // Once we have 3 readings, use their median
	        if (distance_samples_filled == 3)
	        {
	            filtered_distance_cm = median3(distance_samples[0],
	                                           distance_samples[1],
	                                           distance_samples[2]);
	        }
	        else
	        {
	            // During the first two readings, just use the current value
	            filtered_distance_cm = distance_cm;
	        }
	    }
	}

	uint8_t accel_data[6];
	uint8_t gyro_data[6];

	HAL_I2C_Mem_Read(&hi2c1, (0x68 << 1), 0x3B, I2C_MEMADD_SIZE_8BIT, accel_data, 6, 100);
	HAL_I2C_Mem_Read(&hi2c1, (0x68 << 1), 0x43, I2C_MEMADD_SIZE_8BIT, gyro_data, 6, 100);


	accel_x = (int16_t)((accel_data[0] << 8) | accel_data[1]);
	accel_y = (int16_t)((accel_data[2] << 8) | accel_data[3]);
	accel_z = (int16_t)((accel_data[4] << 8) | accel_data[5]);

	gyro_x = (int16_t)((gyro_data[0] << 8) | gyro_data[1]);
	gyro_y = (int16_t)((gyro_data[2] << 8) | gyro_data[3]);
	gyro_z = (int16_t)((gyro_data[4] << 8) | gyro_data[5]);

	int32_t ax_mg = ((int32_t)(accel_x - ax_offset_raw) * 1000) / 16384;
	int32_t ay_mg = ((int32_t)(accel_y - ay_offset_raw) * 1000) / 16384;
	int32_t az_mg = ((int32_t)(accel_z - az_offset_raw) * 1000) / 16384;



	int32_t gx_dps = gyro_x / 131;
	int32_t gy_dps = gyro_y / 131;
	int32_t gz_dps = gyro_z / 131;

	//subtract offset
	gx_dps -= gx_offset;
	gy_dps -= gy_offset;
	gz_dps -= gz_offset;

	//get driving state
	char driving_state[30];

	if(ay_mg > 300){
		accel_count++;
		brake_count = 0;
	}
	else if(ay_mg < -300){
		brake_count++;
		accel_count = 0;
	}
	else{
		accel_count = 0;
		brake_count = 0;
	}
	// Only declare an event after 3 consecutive readings
	if(accel_count >= 3){
		strcpy(driving_state, "ACCELERATING");
	}
	else if(brake_count >= 3){
		strcpy(driving_state, "BRAKING");
	}
	else{
		strcpy(driving_state, "NORMAL");
	}

	char turn_state[20];

	// Negative GZ = counterclockwise = left
	if (gz_dps < -30)
	{
	    left_turn_count++;
	    right_turn_count = 0;
	}

	// Positive GZ = clockwise = right
	else if (gz_dps > 30)
	{
	    right_turn_count++;
	    left_turn_count = 0;
	}

	// Not rotating enough to count as a turn
	else
	{
	    left_turn_count = 0;
	    right_turn_count = 0;
	}


	// Require 3 consecutive readings
	if (left_turn_count >= 3)
	{
	    strcpy(turn_state, "LEFT");
	}
	else if (right_turn_count >= 3)
	{
	    strcpy(turn_state, "RIGHT");
	}
	else
	{
	    strcpy(turn_state, "STRAIGHT");
	}


	char obstacle_state[20];

	if (!echo_valid)
	{
	    strcpy(obstacle_state, "UNKNOWN");
	}
	else if (filtered_distance_cm <= 20)
	{
	    strcpy(obstacle_state, "OBSTACLE");
	}
	else if (filtered_distance_cm <= 50)
	{
	    strcpy(obstacle_state, "CAUTION");
	}
	else
	{
	    strcpy(obstacle_state, "SAFE");
	}


	// Onboard LED acts as an obstacle warning
	if (echo_valid && filtered_distance_cm <= 20)
	{
	    BSP_LED_On(LED_GREEN);
	}
	else
	{
	    BSP_LED_Off(LED_GREEN);
	}

	//one clean line
	char telemetry_buffer[220];

	if (echo_valid)
	{
	    snprintf(telemetry_buffer,
	             sizeof(telemetry_buffer),
	             "ACCEL[X:%ld Y:%ld Z:%ld mg]  DRIVE:%s  "
	             "GYRO[X:%ld Y:%ld Z:%ld dps]  TURN:%s  "
	             "DIST:%lu cm  OBJECT:%s\r\n",
	             ax_mg,
	             ay_mg,
	             az_mg,
	             driving_state,
	             gx_dps,
	             gy_dps,
	             gz_dps,
	             turn_state,
	             filtered_distance_cm,
	             obstacle_state);
	}
	else
	{
	    snprintf(telemetry_buffer,
	             sizeof(telemetry_buffer),
	             "ACCEL[X:%ld Y:%ld Z:%ld mg]  DRIVE:%s  "
	             "GYRO[X:%ld Y:%ld Z:%ld dps]  TURN:%s  "
	             "DIST:NO ECHO  OBJECT:UNKNOWN\r\n",
	             ax_mg,
	             ay_mg,
	             az_mg,
	             driving_state,
	             gx_dps,
	             gy_dps,
	             gz_dps,
	             turn_state);
	}

	HAL_UART_Transmit(&huart2,
	                  (uint8_t *)telemetry_buffer,
	                  strlen(telemetry_buffer),
	                  HAL_MAX_DELAY);


	HAL_Delay(100);
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10D19CE4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 79;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(HC_TRIG_GPIO_Port, HC_TRIG_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : HC_ECHO_Pin */
  GPIO_InitStruct.Pin = HC_ECHO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(HC_ECHO_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : HC_TRIG_Pin */
  GPIO_InitStruct.Pin = HC_TRIG_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(HC_TRIG_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
