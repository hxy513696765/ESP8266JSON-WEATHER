#ifndef __OLED_H__
#define __OLED_H__
#include "c_types.h"
#include "i2c_master.h"
#include "gpio.h"

#define RS_H gpio_output_conf(GPIO_Pin_5,0,GPIO_Pin_5,0)
#define RS_L gpio_output_conf(0,GPIO_Pin_5,0,GPIO_Pin_5)

#define CS_H gpio_output_conf(GPIO_Pin_13,0,GPIO_Pin_13,0)
#define CS_L gpio_output_conf(0,GPIO_Pin_13,0,GPIO_Pin_13)

void Write_IIC_Command(uint8 IIC_Command);
void Write_IIC_Data(uint8 IIC_Data);
void Initial_LCD(void);
void Clear_lcd(void);
void Picture_show(uint8_t x,uint8_t y,uint8_t with,uint8_t high,const uint8_t DisplayData[]);
void LCD_print(uint8_t x,uint8_t y,char *p_string);
void Big_print(uint8_t x,uint8_t y,char *p_string);
void gpio_init(void);
#endif
