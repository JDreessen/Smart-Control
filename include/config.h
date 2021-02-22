#define MQTT_SERVER "192.168.0.200";            // MQTT Broker address

const char mqtt_topicPrefix[] = "zuhause/obergeschoss/test/rollos/";
//char mqtt_statusTopic[] = "zuhause/obergeschoss/test/rollos/status";
const char mqtt_subTopic[] = "zuhause/obergeschoss/test/rollos/+/command";

#define mqtt_prefixLen sizeof(mqtt_topicPrefix)/sizeof(mqtt_topicPrefix[0])-1
char mqtt_statusTopicBuf[mqtt_prefixLen+1+2+7] = "zuhause/obergeschoss/test/rollos/";