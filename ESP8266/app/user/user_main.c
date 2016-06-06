/*
 * ESPRSSIF MIT License
 *
 * Copyright (c) 2015 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <stdio.h>
#include <string.h>
#include "esp_common.h"
#include "uart.h"
#include "i2c_master.h"
#include "oled.h"
#include "icon.h"
#include "gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "cJSON.h"

uint8_t dis_flag = 1;

typedef struct 
{
	int32_t day;
	int32_t night;
	int32_t min;
	int32_t max;
	int32_t morn;
	int32_t eve;

}xTemperature_t;
#define STA_SIZE			(16)
typedef struct
{
	int32_t id;
	int8_t wmain[STA_SIZE];
	int8_t wdes[STA_SIZE];
	int8_t wicon[STA_SIZE];
}xWeather_t;

typedef struct
{
	int32_t dt;					/* 当前时间 */
	int32_t pressure;
	int32_t humidity;
	int32_t speed;
	int32_t deg;
	int32_t clouds;
	xTemperature_t temperature;
	xWeather_t weather;
}xWeatherList_t;

#define BUFFER_SIZE						(1024)



//获取网页Json数据
int32_t vGetRawData(int8_t *pbuf,const int32_t maxsiz)
{
	int32_t temp=0;
	int32_t recbytes=0;
	int32_t buf_index=0;
	int8_t *c=NULL;
	int8_t buffer[128];
	struct sockaddr_in remote_ip;
	const int32_t server_port=80;
	const static int8_t *serverurl="api.openweathermap.org";
	const int8_t request[256]="GET /data/2.5/forecast/daily?q=Shenzhen&mode=json&units=metric&cnt=3&appid=d9176758eea113e3813f472f387439e2 HTTP/1.1\nHOST: api.openweathermap.org\nCache-Control: no-cache\n\n\n";
//    const int8_t request[256]="GET /data/2.5/forecast/daily?q=isachsen&mode=json&units=metric&cnt=3&appid=d9176758eea113e3813f472f387439e2 HTTP/1.1\nHOST: api.openweathermap.org\nCache-Control: no-cache\n\n\n";
	const struct hostent *pURL=gethostbyname(serverurl);
	const int socketfd=socket(AF_INET, SOCK_STREAM, 0);
	if (socketfd<0)
	{
		printf("C > socket fail!\n");
		close(socketfd);
		return -1;
	}
	bzero(&remote_ip, sizeof(struct sockaddr_in));
	remote_ip.sin_family = AF_INET;
	remote_ip.sin_addr.s_addr = *((unsigned long*)pURL->h_addr_list[0]);//inet_addr(server_ip);
	remote_ip.sin_port = htons(server_port);
	if (0 != connect(socketfd, (struct sockaddr *)(&remote_ip), sizeof(struct sockaddr)))
	{
		close(socketfd);
		printf("C > connect fail!\n");
		vTaskDelay(4000 / portTICK_RATE_MS);
		return -1;
	}
	if (write(socketfd, request, strlen(request) + 1) < 0)
	{
		close(socketfd);
		printf("C > send fail\n");
		return -1;
	}
	memset(buffer,0,sizeof(buffer));
	//memset(pbuf,0,maxsiz);
	vTaskDelay(500 / portTICK_RATE_MS);
	//printf("-------------------Read-------------------\n");
	while ((recbytes = read(socketfd , buffer, sizeof(buffer)-1)) > 0)					/* 接收起始头 */
	{
		//buffer[recbytes] = 0;
		c=strstr(buffer,"{");
		if (NULL==c)
		{
			memset(buffer,0,sizeof(buffer));
			vTaskDelay(100 / portTICK_RATE_MS);
			continue;
		}
		buf_index=recbytes-(c-buffer);
		memcpy(pbuf,c,recbytes-(c-buffer));
		//c-buffer;
		//printf("Start:%u,Offset:%u,%u\n",buffer,c,c-buffer);
		//printf("-------------------Index-------------------\n");
		//printf("Offset:%ld\n",(long)(c-buffer));
		//printf("%s\n", pbuf);
		break;
		//memset(buffer,0,sizeof(buffer));
		//vTaskDelay(100 / portTICK_RATE_MS);
	}
	if (recbytes<1)
	{
		close(socketfd);
		return -1;
	}
	memset(buffer,0,sizeof(buffer));
	while ((recbytes = read(socketfd , buffer, sizeof(buffer)-1)) > 0)					/* 接收剩下部分数据 */
	{
		
		if (recbytes+buf_index>maxsiz)
			temp=maxsiz-buf_index;
		else
			temp=recbytes;
		//temp
		memcpy(pbuf+buf_index,buffer,temp);
		buf_index=buf_index+temp;
		if (temp<recbytes)
		{
			//printf("Full\n");
			break;																		/* 缓冲区满 */
		}
		//buffer[recbytes] = 0;
		//printf("%s\n", buffer);
		//c=strstr(buffer,"{");
		//if (NULL==c)
		//{
			//memset(buffer,0,sizeof(buffer));
			//vTaskDelay(100 / portTICK_RATE_MS);
		//	continue;
		//}
		//memset(buffer,0,sizeof(buffer));
		vTaskDelay(100 / portTICK_RATE_MS);
	}
	temp=maxsiz;
	while ('}'!=pbuf[temp])
	{
		pbuf[temp]='\0';
		--temp;
	}
	/*printf("-------------------Read-------------------\n");
	printf("%s\n", pbuf);
	printf("-------------------ReadEnd----------------\n");*/
	close(socketfd);
	return 0;
}


//解析Json
int32_t vGetWeatherList(const int8_t *jsStr,xWeatherList_t *weatherList,const uint8_t dt)
{
	int32_t cpySiz=0;
	cJSON *jsTmp=NULL;			//临时变量
	cJSON *jsonRoot=cJSON_Parse(jsStr);     //将字符串解析成json结构体
	cJSON *jsWeatherRoot=NULL;
	cJSON *jsWeatherList=NULL;
	cJSON *jsTemp=NULL;
	cJSON *jsWeather=NULL;
	/***************************************************************/
	if (cJSON_GetArraySize(jsonRoot)<5)
		return -1;
	/***************************************************************/
	jsWeatherRoot=cJSON_GetObjectItem(jsonRoot, "list");		/* 获取所有天气信息 */
	if (dt+1>cJSON_GetArraySize(jsWeatherRoot))
		return -1;
	jsWeatherList=cJSON_GetArrayItem(jsWeatherRoot,dt);			/* 获取第n天天气信息 */
	jsTemp=cJSON_GetArrayItem(jsWeatherList,1);					/* 温度数据 */
	jsTmp=cJSON_GetArrayItem(jsWeatherList,4);					/* 天气数据 */
	//jsWeather=cJSON_GetArrayItem(jsWeatherList,4);					/* 天气数据 */
	jsWeather=cJSON_GetArrayItem(jsTmp,0);
	/***************************************************************/									/* 时间 */
	weatherList->dt=cJSON_GetArrayItem(jsWeatherList,0)->valueint;
	weatherList->pressure=cJSON_GetArrayItem(jsWeatherList,2)->valueint;	/* 气压 */
	weatherList->humidity=cJSON_GetArrayItem(jsWeatherList,3)->valueint;	/* 湿度 */
	weatherList->speed=cJSON_GetArrayItem(jsWeatherList,5)->valueint;
	weatherList->deg=cJSON_GetArrayItem(jsWeatherList,6)->valueint;
	weatherList->clouds=cJSON_GetArrayItem(jsWeatherList,7)->valueint;
	//weatherList->rain=cJSON_GetArrayItem(jsWeatherList,3)->valueint;
	/**************************天气数据*********************************/
	weatherList->weather.id=cJSON_GetObjectItem(jsWeather,"id")->valueint;
	
	cpySiz=strlen(cJSON_GetObjectItem(jsWeather,"main")->valuestring);
	if (cpySiz>STA_SIZE)
		cpySiz=STA_SIZE;
	strcpy(weatherList->weather.wmain,cJSON_GetObjectItem(jsWeather,"main")->valuestring);
	
	cpySiz=strlen(cJSON_GetObjectItem(jsWeather,"description")->valuestring);
	if (cpySiz>STA_SIZE)
		cpySiz=STA_SIZE;
	strcpy(weatherList->weather.wdes,cJSON_GetObjectItem(jsWeather,"description")->valuestring);
	
	cpySiz=strlen(cJSON_GetObjectItem(jsWeather,"icon")->valuestring);
	if (cpySiz>STA_SIZE)
		cpySiz=STA_SIZE;
	strcpy(weatherList->weather.wicon,cJSON_GetObjectItem(jsWeather,"icon")->valuestring);
	/**************************温度数据*********************************/
	weatherList->temperature.day=cJSON_GetObjectItem(jsTemp,"day")->valueint;
	weatherList->temperature.night=cJSON_GetObjectItem(jsTemp,"night")->valueint;
	weatherList->temperature.min=cJSON_GetObjectItem(jsTemp,"min")->valueint;
	weatherList->temperature.max=cJSON_GetObjectItem(jsTemp,"max")->valueint;
	weatherList->temperature.morn=cJSON_GetObjectItem(jsTemp,"morn")->valueint;
	weatherList->temperature.eve=cJSON_GetObjectItem(jsTemp,"eve")->valueint;
	cJSON_Delete(jsWeatherRoot);
	cJSON_Delete(jsWeatherList);
	cJSON_Delete(jsWeather);
	cJSON_Delete(jsTemp);
	cJSON_Delete(jsonRoot);
	return 0;
}

void chang_valu(int32_t temp_valu,char *dis_str)
{
    if(temp_valu > 0){            
        if(temp_valu < 10)
            sprintf(dis_str,"  %d^",temp_valu);
        else
            sprintf(dis_str," %d^",temp_valu);
    }
    else{       
//        temp_valu = -1 * temp_valu;
        
        if(temp_valu > -10)
            sprintf(dis_str," %d^",temp_valu);
        else
            sprintf(dis_str,"%d^",temp_valu);
    }    
}

int8_t get_icondata(char *ico_str)
{
    int8_t ico_dat = 0,ico_offset = 0;
    
    ico_dat = (*ico_str - '0') * 10 + (*(ico_str + 1) - '0');
    switch(ico_dat)
    {
        case 1:     //天晴和月亮
            if(*(ico_str + 2) == 'd')
                ico_offset = 3;    //太阳
            else
                ico_offset = 7;    //月亮             
        break;
        
        case 2:    //白天阴和晚上阴
            if(*(ico_str + 2) == 'd')
                ico_offset = 1;    //白天阴
            else
                ico_offset = 4;    //晚上阴         
        break;
        
        case 3:    //云
                ico_offset = 8;                        
        break;
        
        case 4:    //云
                ico_offset = 8;    
        break;
        
        case 9:    //雨
                ico_offset = 6;    
        break;
        
        case 10:    //雨
                ico_offset = 6;    
        break;
        
        case 11:    //打雷
                ico_offset = 2;    
        break;
        
        case 13:    //雪
                ico_offset = 5;    
        break;
        
        default :ico_offset = 0;
        break;            
    }
    
    return ico_offset;
}

#define DT_CNT				(3)				//n天数据
void vTaskNetwork(void *pvParameters)
{
	int32_t i,tempvalu = 0;
	xWeatherList_t weatherList[DT_CNT];
	int8_t *pbuf=NULL;
	pbuf=(int8_t *)zalloc(BUFFER_SIZE+1);
    char *dis_str = "   ^";
    char *ico_str;
    char *main_weath = "        ";
    int  ico_dat;
    int  ico_offset = 0;
    uint8_t LCD_clear = 1;
	if (NULL==pbuf)
	{
		printf("ERROR\n");
        
        Clear_lcd();
        LCD_print(0,0,"NET ERROR");
        
		while (1);
	}
	printf("%s Running...\n",__func__);
	//失败处理
    while (1)
	{
		memset(pbuf,'\0',BUFFER_SIZE+1);
		//printf("%s\n",__func__);
		if (0!=vGetRawData(pbuf,BUFFER_SIZE))
		{
			printf("GetRawData Error\n");
			continue;
		}
		for (i=0;i<DT_CNT;++i)
		{
			if (0!=vGetWeatherList(pbuf,weatherList+i,i))
			{
				continue;
			}
		}
		
#if 1
		printf("\n-------------------Vals-------------------\n");
		printf("Dt:%d\n",weatherList[0].dt);
		printf("Speed:%d\n",weatherList[0].speed);
		printf("Deg:%d\n",weatherList[0].deg);
		printf("Clouds:%d\n",weatherList[0].clouds);
		printf("Pressure:%d\n",weatherList[0].pressure);
		printf("Humidity:%d\n",weatherList[0].humidity);
		
		printf("TempDay:%d\n",weatherList[0].temperature.day);
		printf("TempNight:%d\n",weatherList[0].temperature.night);
		printf("TempMin:%d\n",weatherList[0].temperature.min);
		printf("TempMax:%d\n",weatherList[0].temperature.max);
		printf("TempMorn:%d\n",weatherList[0].temperature.morn);
		printf("TempEve:%d\n",weatherList[0].temperature.eve);
		
		printf("WeatherId:%d\n",weatherList[0].weather.id);
		printf("WeatherMain:%s\n",weatherList[0].weather.wmain);
		printf("WeatherDes:%s\n",weatherList[0].weather.wdes);
		printf("WeatherIcon:%s\n",weatherList[0].weather.wicon);
        
                       
#endif

#if 0
        ico_dat = (*ico_str - '0') * 10 + (*(ico_str + 1) - '0');
  
        if(tempvalu > 0){            
            if(tempvalu < 10)
                sprintf(dis_str,"  %d^",tempvalu);
            else
                sprintf(dis_str," %d^",tempvalu);
        }
        else{       
            tempvalu = -1 * tempvalu;
            
            if(tempvalu < 10)
                sprintf(dis_str," %d^",tempvalu);
            else
                sprintf(dis_str,"%d^",tempvalu);
        }
#endif        

        LCD_clear = LCD_clear % 4;
        
        if(LCD_clear == 3)
        {    
            dis_flag = 0;
            ico_str = weatherList[1].weather.wicon;
            
            if(*(ico_str + 2) == 'd')   //判断是白天还是晚上
                tempvalu = weatherList[1].temperature.day;
            else
                tempvalu = weatherList[1].temperature.night;
            
            chang_valu(tempvalu,dis_str);
            Clear_lcd();
            
//            LCD_print(0,0,"ab");    //显示“明”
//            LCD_print(0,16,"ef");   //显示“天”        
            
            ico_offset = get_icondata(ico_str);        
            Picture_show(1,0,48,48,lcd_show + ico_offset* 288);   
            
            sprintf(dis_str,"%sC",dis_str);
            LCD_print(1,48,dis_str);   //显示温度值  
            
            ico_str = weatherList[2].weather.wicon;
            
            if(*(ico_str + 2) == 'd')   //判断是白天还是晚上
                tempvalu = weatherList[2].temperature.day;
            else
                tempvalu = weatherList[2].temperature.night;
            
            chang_valu(tempvalu,dis_str);
            
//            LCD_print(8,0,"cd");    //显示“明”
//            LCD_print(8,16,"ef");   //显示“天”        
            
            ico_offset = get_icondata(ico_str);        
            Picture_show(9,0,48,48,lcd_show + ico_offset* 288);   
            
            sprintf(dis_str,"%sC",dis_str);
            
            LCD_print(9,48,dis_str);   //显示温度值
            
            LCD_clear = 0;
        }
        
        if(LCD_clear == 1)
        {
            dis_flag = 1;
            ico_str = weatherList[0].weather.wicon;
            
            if(*(ico_str + 2) == 'd')   //判断是白天还是晚上
                tempvalu = weatherList[0].temperature.day;
            else
                tempvalu = weatherList[0].temperature.night;
        
            chang_valu(tempvalu,dis_str);
            Clear_lcd();
            
            Big_print(7,0,dis_str);
            Big_print(14,0,"C");
            
            ico_offset = get_icondata(ico_str);        
            Picture_show(1,8,48,48,lcd_show + ico_offset* 288);
            
            sprintf(main_weath,"%s",weatherList[0].weather.wmain);
            LCD_print(9,32,main_weath);   
        }
        LCD_clear++;
        
       
		vTaskDelay(10000 / portTICK_RATE_MS);
    }
	free(pbuf);
}

void vTaskDisplay(void *pvParameters)
{
	//i2c_master_init();
    uint8_t dis_count = 0;
//    uint32_t count = 0;
    char *dis_cont= "        ";
    uint8_t loop = 0;
    while (1) 
	{
		//printf("%s\n",__func__);
//        Picture_show(1,8,48,48,lcd_show + 288*dis_count);
        if(dis_flag > 0)
        {
            dis_count = dis_count % 8;
            for(loop = 0;loop < 8;loop++)
            {
                *(dis_cont + loop) = ' ';
            }
            
            *(dis_cont + dis_count) = '>';            
            *(dis_cont + 7 - dis_count) = '<';


                LCD_print(8,48,dis_cont);
            
            dis_count++;            
        }

        vTaskDelay(1500 / portTICK_RATE_MS);
//        count++;
//        
//        dis_count = dis_count % 8;
    }
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
	UART_SetBaudrate(UART0, BIT_RATE_115200);               //设置串口通讯波特率
    printf("SDK version:%s\n", system_get_sdk_version());
    printf("Chip ID:0x%x\n", system_get_chip_id());
//void wifi_handle_event_cb(System_Event_t *evt);
//	wifi_set_event_handler_cb(wifi_handle_event_cb);

    i2c_master_gpio_init();         //初始化LCD的GPIO口
    printf("IIC Init OK \n");
    
    gpio_init();                    //初始化其它LED控制的GPIO口
    
    Initial_LCD();                  //初始化LCD的相关寄存器
    printf("Initial_LCD OK \n"); 
  
    Clear_lcd();                    //清屏

    LCD_print(0,26,"INITIALIZING...");
    
    /* need to set opmode before you set config */
    wifi_set_opmode(STATION_MODE);//(STATIONAP_MODE);   //设置模块为Station模式
    {
        struct station_config *config = (struct station_config *)zalloc(sizeof(struct station_config));
        sprintf(config->ssid, "FitechCamera");
        sprintf(config->password, "fitech+2015");
//        sprintf(config->ssid, "LuQiu");
//        sprintf(config->password, "LuQiuJianPing");

        /* need to sure that you are in station mode first,
         * otherwise it will be failed. */
        wifi_station_set_config(config);
        free(config);
    }
    
    Clear_lcd();    

    Big_print(7,0,"     ");
    Picture_show(5,0,48,48,lcd_show);
    

    printf("Picture_show OK\n ");
    
    xTaskCreate(vTaskNetwork, "tskNetwork", 2048, NULL, 2, NULL);
    xTaskCreate(vTaskDisplay, "tskDisplay", 256, NULL, 2, NULL);
}

