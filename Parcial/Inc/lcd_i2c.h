/*
 * lcd_i2c.h
 *
 *  Created on: Jul 23, 2026
 *      Author: santiago
 */

#ifndef INC_LCD_I2C_H_
#define INC_LCD_I2C_H_

#include "stm32f4xx_hal.h"

// Dirección I2C base del PCF8574
#define LCD_ADDR (0x27 << 1)

void LCD_Init(I2C_HandleTypeDef *hi2c);
void LCD_Send_Cmd(I2C_HandleTypeDef *hi2c, uint8_t cmd);
void LCD_Send_Char(I2C_HandleTypeDef *hi2c, char data);
void LCD_SetCursor(I2C_HandleTypeDef *hi2c, uint8_t row, uint8_t col);
void LCD_Send_String(I2C_HandleTypeDef *hi2c, char *str);

#endif /* INC_LCD_I2C_H_ */
