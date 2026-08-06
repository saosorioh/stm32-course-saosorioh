/*
 * mpu6050.h
 *
 *  Created on: Jul 23, 2026
 *      Author: santiago
 */

#ifndef INC_MPU6050_H_
#define INC_MPU6050_H_

#include "stm32f4xx_hal.h"

// AD0 a 3.3V cambia la dirección a 0x69 (0xD2 desplazado)
#define MPU6050_ADDR (0x69 << 1)

uint8_t MPU6050_Init(I2C_HandleTypeDef *hi2c);
void MPU6050_Read_Accel(I2C_HandleTypeDef *hi2c, float *ax, float *ay, float *az);

#endif /* INC_MPU6050_H_ */
