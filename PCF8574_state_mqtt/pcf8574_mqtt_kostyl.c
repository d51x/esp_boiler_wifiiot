/*
**************************************************************
Костыль для отправки статуса ног PCF8574 при измении по mqtt.
**************************************************************
*/

#define FW_VER "1.2"

#define READ_PCF8574_INTERVAL 100  // msec
#define PCF8574_ADDR 0x20
#define GPIO_PCF8574_START 220

#if mqtte || mqttjsone
	#define MQTT_SEND_INTERVAL 10 // sec
	#define MQTT_PAYLOAD_BUF 20
	MQTT_Client* mqtt_client;    //for non os sdk
	//char payload[MQTT_PAYLOAD_BUF];
	uint32_t mqtt_send_interval_sec = MQTT_SEND_INTERVAL;

	static volatile os_timer_t mqtt_send_timer;	
	void mqtt_send_all_cb();
#endif



static volatile os_timer_t read_pcf8574_timer;

void ICACHE_FLASH_ATTR read_pcf8574_cb();	

uint8_t pcf8574_data;
uint8_t used_pcf8574_gpio[8] = {1, 1, 1, 1, 0, 0, 0, 0};		// указать, какие ноги pcf8574 надо мониторить, 1 - мониторим, 0 - не мониторим
uint8_t pcf8574_val[8] = {1, 1, 1, 1, 1, 1, 1, 1}; // исходные значения, 1 - притянуты к +, 0 - притянуты к -

void get_config_values(uint8_t reload) {   
#if mqtte || mqttjsone	
	if ( reload && ( sensors_param.mqttts != mqtt_send_interval_sec )) {
		os_timer_disarm(&mqtt_send_timer);
		os_timer_setfn(&mqtt_send_timer, (os_timer_func_t *)mqtt_send_all_cb, NULL);
		os_timer_arm(&mqtt_send_timer, sensors_param.mqttts * 1000, 1);	
	}
	mqtt_send_interval_sec = sensors_param.mqttts;	
#endif	
}

void ICACHE_FLASH_ATTR startfunc(){
	
	get_config_values(0);
	
#if mqtte || mqttjsone
	os_timer_disarm(&read_pcf8574_timer);
	os_timer_setfn(&read_pcf8574_timer, (os_timer_func_t *)read_pcf8574_cb, NULL);
	os_timer_arm(&read_pcf8574_timer, READ_PCF8574_INTERVAL, 1);
	
	
	mqtt_client = (MQTT_Client*) &mqttClient;
	os_timer_disarm(&mqtt_send_timer);
	os_timer_setfn(&mqtt_send_timer, (os_timer_func_t *)mqtt_send_all_cb, NULL);
	os_timer_arm(&mqtt_send_timer, mqtt_send_interval_sec * 1000, 1);
#endif
	
}

void ICACHE_FLASH_ATTR timerfunc(uint32_t  timersrc) {
	if(timersrc%5==0){
		get_config_values(1);
	}
}

#if mqtte || mqttjsone
void ICACHE_FLASH_ATTR send_mqtt_pcf8574_gpio_state(uint8_t _gpio, uint8_t _data)
{
	if ( sensors_param.mqtten != 1 ) return;
	char payload[MQTT_PAYLOAD_BUF];
	char topic[10];

	os_memset(payload, 0, MQTT_PAYLOAD_BUF);	
	os_memset(topic, 0, 10);	
	os_sprintf(payload,"%d", _data );
	os_sprintf(topic,"output%d", _gpio);
	MQTT_Publish(mqtt_client, topic, payload, os_strlen(payload), 2, 0, 1);
	system_soft_wdt_feed();	
}
#endif
	

void ICACHE_FLASH_ATTR read_pcf8574_cb(){
//pcfgpiow8(uint8_t PCF8574_ADDR,uint8_t value) - запись
//pcfgpior8(uint8_t PCF8574_ADDR) - чтение

	pcf8574_data = pcfgpior8(PCF8574_ADDR);
	
	uint8_t i = 0;
	for (i = 0; i < 8; i++) 
	{
		if ( used_pcf8574_gpio[i] == 1 ) 
		{
			// проверяем состояние только использующихся ног PCF8574
			uint8_t data = (pcf8574_data >> (i)) & 1;
			if ( data != pcf8574_val[i] ) 
			{
				pcf8574_val[i] = data;
				// mqtt send changes
				send_mqtt_pcf8574_gpio_state( GPIO_PCF8574_START + i, data);
			}			
		}
	}	
}

#if mqtte || mqttjsone
void mqtt_send_all_cb() 
{
	if ( sensors_param.mqtten != 1 ) return;
	
	uint8_t i = 0;
	for (i = 0; i < 8; i++) {
		if ( used_pcf8574_gpio[i] == 1 ) {
			// публикуем состояния только использующихся ног PCF8574
			send_mqtt_pcf8574_gpio_state( GPIO_PCF8574_START + i, pcf8574_val[i]);
		}			
	}
}
#endif

void webfunc(char *pbuf) {
	os_sprintf(HTTPBUFF,"<b>Версия:</b> %s", FW_VER);
}