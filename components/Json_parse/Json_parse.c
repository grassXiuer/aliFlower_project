#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include "esp_system.h"
#include "Json_parse.h"
#include "Nvs.h"
#include "esp_log.h"

#include "esp_wifi.h"
#include "Smartconfig.h"
#include "E2prom.h"
#include "Beep.h"
#include "sht31.h"
#include "PMS7003.h"
#include "Human.h"

int beep=0;


esp_err_t parse_Uart0(char *json_data)
{
    cJSON *json_data_parse = NULL;
    cJSON *json_data_parse_ProductKey = NULL;
    cJSON *json_data_parse_DeviceName = NULL;
    cJSON *json_data_parse_DeviceSecret = NULL;

    if(json_data[0]!='{')
    {
        printf("uart0 Json Formatting error1\n");
        return 0;
    }

    json_data_parse = cJSON_Parse(json_data);
    if (json_data_parse == NULL) //如果数据包不为JSON则退出
    {
        printf("uart0 Json Formatting error\n");
        cJSON_Delete(json_data_parse);

        return 0;
    }
    else
    {
        /*
            {"Command":"SetupProduct","ProductKey":"a148brGI1Gw","DeviceName":"A0001AIR1","DeviceSecret":"deOTkO9yw6PlAceTLzXlTIPJs4uzbSIc"}
        */
        
        
        json_data_parse_ProductKey = cJSON_GetObjectItem(json_data_parse, "ProductKey"); 
        printf("ProductKey= %s\n", json_data_parse_ProductKey->valuestring);

        json_data_parse_DeviceName = cJSON_GetObjectItem(json_data_parse, "DeviceName"); 
        printf("DeviceName= %s\n", json_data_parse_DeviceName->valuestring);

        json_data_parse_DeviceSecret = cJSON_GetObjectItem(json_data_parse, "DeviceSecret"); 
        printf("DeviceSecret= %s\n", json_data_parse_DeviceSecret->valuestring);

        char zero_data[256];
        bzero(zero_data,sizeof(zero_data));
        E2prom_Write(0x00, (uint8_t *)zero_data, sizeof(zero_data)); 
    
        sprintf(ProductKey,"%s%c",json_data_parse_ProductKey->valuestring,'\0');
        E2prom_Write(PRODUCTKEY_ADDR, (uint8_t *)ProductKey, strlen(ProductKey));
        
        sprintf(DeviceName,"%s%c",json_data_parse_DeviceName->valuestring,'\0');
        E2prom_Write(DEVICENAME_ADDR, (uint8_t *)DeviceName, strlen(DeviceName)); 

        sprintf(DeviceSecret,"%s%c",json_data_parse_DeviceSecret->valuestring,'\0');
        E2prom_Write(DEVICESECRET_ADDR, (uint8_t *)DeviceSecret, strlen(DeviceSecret));


        printf("{\"status\":\"success\",\"err_code\": 0}");
        cJSON_Delete(json_data_parse);
        fflush(stdout);//使stdout清空，就会立刻输出所有在缓冲区的内容。
        esp_restart();//芯片复位 函数位于esp_system.h
        return 1;

    }
}



/*
{
	"method": "thing.service.property.set",
	"id": "216373306",
	"params": {
		"Switch_JDQ": 0
	},
	"version": "1.0.0"
}
*/


esp_err_t parse_objects_mqtt(char *json_data)
{
    cJSON *json_data_parse = NULL;
    cJSON *json_data_parse_method = NULL;
    cJSON *json_data_parse_params = NULL;

    cJSON *iot_params_parse = NULL;
    cJSON *iot_params_parse_Switch_JDQ = NULL;

    json_data_parse = cJSON_Parse(json_data);

    if(json_data[0]!='{')
    {
        printf("mqtt Json Formatting error\n");

        return 0;       
    }

    if (json_data_parse == NULL) //如果数据包不为JSON则退出
    {

        printf("Json Formatting error1\n");
        cJSON_Delete(json_data_parse);
        return 0;
    }
    else
    {
        json_data_parse_method = cJSON_GetObjectItem(json_data_parse, "method"); 
        printf("method= %s\n", json_data_parse_method->valuestring);

        json_data_parse_params = cJSON_GetObjectItem(json_data_parse, "params"); 
        char *iot_params;
        iot_params=cJSON_PrintUnformatted(json_data_parse_params);
        printf("cJSON_Print= %s\n",iot_params);

        iot_params_parse = cJSON_Parse(iot_params);
        iot_params_parse_Switch_JDQ = cJSON_GetObjectItem(iot_params_parse, "Switch_JDQ"); 
        printf("Switch_JDQ= %d\n", iot_params_parse_Switch_JDQ->valueint);
        beep=iot_params_parse_Switch_JDQ->valueint;
        if(beep==1)
        {
            Beep_On();
        }
        else if(beep==0)
        {
            Beep_Off();
        }
        
        free(iot_params);
        cJSON_Delete(iot_params_parse);
    }
    
    cJSON_Delete(json_data_parse);
    

    return 1;
}



/*
{
	"method": "thing.event.property.post",
	"params": {
		"Temperature": 1,
		"LightLux": 5,
		"PM25": 3,
		"Humidity": 2,
		"PM10": 4,
		"HumanDetectionSwitch": 1,
		"Switch_JDQ": 0
	}
}
*/

void create_mqtt_json(creat_json *pCreat_json)
{

    cJSON *root = cJSON_CreateObject();
    cJSON *next = cJSON_CreateObject();

    cJSON_AddItemToObject(root, "method", cJSON_CreateString("thing.event.property.post"));
    cJSON_AddItemToObject(root, "params", next);

    if (sht31_readTempHum()) 
    {		
        double Temperature = sht31_readTemperature();
        double Humidity = sht31_readHumidity();

        cJSON_AddItemToObject(next, "Temperature", cJSON_CreateNumber(Temperature));
        cJSON_AddItemToObject(next, "Humidity", cJSON_CreateNumber(Humidity)); 
        ESP_LOGI("SHT30", "Temperature=%.1f, Humidity=%.1f", Temperature, Humidity);
	} 
    else 
    {
		ESP_LOGE("SHT30", "SHT31_ReadTempHum : failed");
	}
    cJSON_AddItemToObject(next, "LightLux", cJSON_CreateNumber(0));
    cJSON_AddItemToObject(next, "PM25", cJSON_CreateNumber(PM2_5));
    cJSON_AddItemToObject(next, "PM10", cJSON_CreateNumber(PM10));
    ESP_LOGI("PMS7003", "PM2_5=%d,PM10=%d", PM2_5,PM10);
    cJSON_AddItemToObject(next, "HumanDetectionSwitch", cJSON_CreateNumber(Human_status));
    cJSON_AddItemToObject(next, "Switch_JDQ", cJSON_CreateNumber(beep));
    wifi_ap_record_t wifidata;
    if (esp_wifi_sta_get_ap_info(&wifidata) == 0)
    {
        cJSON_AddItemToObject(next, "RSSI", cJSON_CreateNumber(wifidata.rssi));
    }

    char *cjson_printunformat;
    cjson_printunformat=cJSON_PrintUnformatted(root);
    pCreat_json->creat_json_c=strlen(cjson_printunformat);
    bzero(pCreat_json->creat_json_b,sizeof(pCreat_json->creat_json_b));
    memcpy(pCreat_json->creat_json_b,cjson_printunformat,pCreat_json->creat_json_c);
    printf("len=%d,mqtt_json=%s\n",pCreat_json->creat_json_c,pCreat_json->creat_json_b);
    free(cjson_printunformat);
    cJSON_Delete(root);
    
}



