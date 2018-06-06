/*----------------------------------------------------------------------------
 * Copyright (c) <2016-2018>, <Huawei Technologies Co., Ltd>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *---------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * Notice of Export Control Law
 * ===============================================
 * Huawei LiteOS may be subject to applicable export control laws and regulations, which might
 * include those applicable to Huawei LiteOS of U.S. and the country in which you are located.
 * Import, export and usage of Huawei LiteOS in any manner by you shall be in compliance with such
 * applicable export control laws and regulations.
 *---------------------------------------------------------------------------*/

#include "los_base.h"
#include "los_task.ph"
#include "los_typedef.h"
#include "los_sys.h"
#include "mqtt_client.h"
#include "atiny_adapter.h"
#include "MQTTClient.h"

void mqtt_message_arrived(MessageData* md);

#define MQTT_COMMAND_TIMEOUT_MS (60*1000)
#define MQTT_KEEPALIVE_INTERVAL (100)
#define MQTT_SENDBUF_SIZE (100)
#define MQTT_READBUF_SIZE (100)

typedef struct
{
    atiny_device_info_t *device_info;
    MQTTClient client;
    atiny_param_t *atiny_params;
} handle_data_t;

static handle_data_t g_atiny_handle;

int  atiny_init(atiny_param_t* atiny_params, void** phandle)
{
    if (NULL == atiny_params || NULL == phandle)
    {
        //ATINY_LOG(LOG_FATAL, "Invalid args");
        return ATINY_ARG_INVALID;
    }

    memset((void*)&g_atiny_handle, 0, sizeof(handle_data_t));
    g_atiny_handle.atiny_params = atiny_params;

    *phandle = &g_atiny_handle;

    return ATINY_OK;
}

void atiny_deinit(void* phandle)
{
    handle_data_t* handle;
    MQTTClient *client;
    Network* network;

    if(NULL == phandle)
    {
        //ATINY_LOG(LOG_FATAL, "Parameter null");
        return;
    }
    handle = (handle_data_t*)phandle;
    client = &(handle->client);
    network = client->ipstack;
    
    MQTTDisconnect(client);
    NetworkDisconnect(network);

    return;
}

int mqtt_add_interest_topic(char *topic, cloud_qos_level_e qos, atiny_rsp_cb cb)
{
    int i, rc = -1;
    atiny_interest_uri_t* interest_uris = g_atiny_handle.device_info->interest_uris;

    if(!topic || !cb || !(qos>=CLOUD_QOS_MOST_ONCE && qos<CLOUD_QOS_LEVEL_MAX))
    {
        //ATINY_LOG(LOG_FATAL, "invalid params");
        return -1;
    }

    for(i=0; i<ATINY_INTEREST_URI_MAX_NUM; i++)
    {
        if(0 == strcmp(interest_uris[i].uri, topic))
        {
            interest_uris[i].qos = qos;
            interest_uris[i].cb = cb;
            return 0;
        }
    }

    for(i=0; i<ATINY_INTEREST_URI_MAX_NUM; i++)
    {
        if(interest_uris[i].uri == NULL)
        {
            interest_uris[i].uri = topic;
            interest_uris[i].qos = qos;
            interest_uris[i].cb = cb;
            rc = 0;
            break;
        }
    }

    if(i == ATINY_INTEREST_URI_MAX_NUM)
        printf("[%s][%d] num of interest uris is up to %d\n", __FUNCTION__, __LINE__, ATINY_INTEREST_URI_MAX_NUM);

    return rc;
}

int mqtt_del_interest_topic(const char *topic)
{
    int i, rc = -1;
    atiny_interest_uri_t* interest_uris = g_atiny_handle.device_info->interest_uris;

    if(!topic)
    {
        //ATINY_LOG(LOG_FATAL, "invalid params");
        return -1;
    }

    for(i=0; i<ATINY_INTEREST_URI_MAX_NUM; i++)
    {
        if(0 == strcmp(interest_uris[i].uri, topic))
        {
            memset(&(interest_uris[i]), 0x0, sizeof(interest_uris[i]));
        }
    }

    return rc;
}


int mqtt_topic_subscribe(MQTTClient *client, char *topic, cloud_qos_level_e qos, atiny_rsp_cb cb)
{
     int rc;

    if(!client || !topic || !cb || !(qos>=CLOUD_QOS_MOST_ONCE && qos<CLOUD_QOS_LEVEL_MAX))
    {
        //ATINY_LOG(LOG_FATAL, "invalid params");
        return -1;
    }

    if(0 != mqtt_add_interest_topic(topic, qos, cb))
        return -1;

    rc = MQTTSubscribe(client, topic, qos, mqtt_message_arrived);
    if(0 != rc)
        printf("[%s][%d] MQTTSubscribe %s[%d]\n", __FUNCTION__, __LINE__, topic, rc);

    return rc;
}

int mqtt_topic_unsubscribe(MQTTClient *client, const char *topic)
{
    int rc;

    if(!client || !topic)
    {
        //ATINY_LOG(LOG_FATAL, "invalid params");
        return -1;
    }

    if(0 != mqtt_del_interest_topic(topic))
        return -1;

    rc = MQTTUnsubscribe(client, topic);
    if(0 != rc)
        printf("[%s][%d] MQTTUnsubscribe %s[%d]\n", __FUNCTION__, __LINE__, topic, rc);

    return rc;
}

int mqtt_message_publish(MQTTClient *client, cloud_msg_t* send_data)
{
    int rc = -1;
    MQTTMessage message;

    if(!client || !send_data)
    {
        //ATINY_LOG(LOG_FATAL, "invalid params");
        return -1;
    }

    memset(&message, 0x0, sizeof(message));
    message.qos = send_data->qos;
    message.retained = 0;
    message.payload = send_data->payload;
    message.payloadlen = send_data->payload_len;
    if((rc = MQTTPublish(client, send_data->uri, &message)) != 0)
        printf("[%s][%d] Return code from MQTT publish is %d\n", __FUNCTION__, __LINE__, rc);

    return rc;
}

// ���ﻹ��Ҫ������ͨ�������(luminais mark)
void mqtt_message_arrived(MessageData* md)
{
    MQTTMessage* message = md->message;
    MQTTString* topic = md->topicName;
    atiny_interest_uri_t* interest_uris = g_atiny_handle.device_info->interest_uris;
    cloud_msg_t msg;
    int i;

    printf("[%s][%d] %.*s : %.*s\n", __FUNCTION__, __LINE__, topic->lenstring.len, topic->lenstring.data, message->payloadlen, (char *)message->payload);

    for(i=0; i<ATINY_INTEREST_URI_MAX_NUM; i++)
    {
        if(0 == strncmp(topic->lenstring.data, interest_uris[i].uri, topic->lenstring.len))
        {
            memset(&msg, 0x0, sizeof(msg));
            msg.uri = topic->lenstring.data;
            msg.uri_len = topic->lenstring.len;
            msg.method = CLOUD_METHOD_POST;
            msg.qos = message->qos;
            msg.payload_len = message->payloadlen;
            msg.payload = message->payload;
            interest_uris[i].cb(&msg);
        }
    }

    return;
}

void mqtt_subscribe_interest_topics(MQTTClient *client, atiny_interest_uri_t interest_uris[ATINY_INTEREST_URI_MAX_NUM])
{
    int i, rc;

    if(NULL == client || NULL == interest_uris)
    {
        //ATINY_LOG(LOG_FATAL, "Parameter null");
        return;
    }

    for(i=0; i<ATINY_INTEREST_URI_MAX_NUM; i++)
    {
        if(NULL == interest_uris[i].uri || '\0' == interest_uris[i].uri[0] || NULL == interest_uris[i].cb
            || !(interest_uris[i].qos>=CLOUD_QOS_MOST_ONCE && interest_uris[i].qos<CLOUD_QOS_LEVEL_MAX))
            continue;
        rc = MQTTSubscribe(client, interest_uris[i].uri, interest_uris[i].qos, mqtt_message_arrived);
        printf("[%s][%d] MQTTSubscribe %s[%d]\n", __FUNCTION__, __LINE__, interest_uris[i].uri, rc);
    }

    return;
}

int atiny_bind(atiny_device_info_t* device_info, void* phandle)
{
    Network n;
    handle_data_t* handle;
    MQTTClient *client;
    atiny_param_t *atiny_params;
    int rc;
    unsigned char sendbuf[MQTT_SENDBUF_SIZE];
    unsigned char readbuf[MQTT_READBUF_SIZE];
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

    if ((NULL == device_info) || (NULL == phandle))
    {
        //ATINY_LOG(LOG_FATAL, "Parameter null");
        return ATINY_ARG_INVALID;
    }

    if(NULL == device_info->client_id)
    {
        //ATINY_LOG(LOG_FATAL, "Parameter null");
        return ATINY_ARG_INVALID;
    }

    if(device_info->will_flag == 1 && NULL == device_info->will_options)
    {
        //ATINY_LOG(LOG_FATAL, "Parameter null");
        return ATINY_ARG_INVALID;
    }

    handle = (handle_data_t*)phandle;
    client = &(handle->client);
    atiny_params = handle->atiny_params;

    handle->device_info = device_info;

    NetworkInit(&n);

    switch(atiny_params->security_typ)
    {
        case CLOUD_SECURITY_TYPE_NONE:
            n.proto = MQTT_PROTO_NONE;
            break;
        case CLOUD_SECURITY_TYPE_PSK:
            n.proto = MQTT_PROTO_TLS_PSK;
            n.psk.psk_id = atiny_params->psk.psk_id;
            n.psk.psk_id_len = atiny_params->psk.psk_id_len;
            n.psk.psk = atiny_params->psk.psk;
            n.psk.psk_len = atiny_params->psk.psk_len;
            break;
        case CLOUD_SECURITY_TYPE_CA:
            printf("[%s][%d] CLOUD_SECURITY_TYPE_CA unsupported now\n", __FUNCTION__, __LINE__);
            return ATINY_ARG_INVALID;
        default:
            printf("[%s][%d] invalid security_typ : %d\n", __FUNCTION__, __LINE__, atiny_params->security_typ);
            break;
    }

    rc = NetworkConnect(&n, atiny_params->server_ip, atoi(atiny_params->server_port));
    printf("[%s][%d] NetworkConnect : %d\n", __FUNCTION__, __LINE__, rc);

    MQTTClientInit(client, &n, MQTT_COMMAND_TIMEOUT_MS, sendbuf, MQTT_SENDBUF_SIZE, readbuf, MQTT_READBUF_SIZE);

    data.willFlag = device_info->will_flag;
    data.MQTTVersion = 3;
    data.clientID.cstring = device_info->client_id;
    data.username.cstring = device_info->user_name;
    data.password.cstring = device_info->password;
    if(device_info->will_flag)
    {
        data.will.topicName.cstring = device_info->will_options->topic_name;
        data.will.message.cstring = device_info->will_options->topic_msg;
        data.will.qos= device_info->will_options->qos;
        data.will.retained = 0;
    }
    data.keepAliveInterval = MQTT_KEEPALIVE_INTERVAL;
    data.cleansession = 1;
    printf("[%s][%d] Send mqtt CONNECT to %s:%s\n", __FUNCTION__, __LINE__, atiny_params->server_ip, atiny_params->server_port);
    rc = MQTTConnect(client, &data);
    printf("[%s][%d] CONNACK : %d\n", __FUNCTION__, __LINE__, rc);

    mqtt_subscribe_interest_topics(client, device_info->interest_uris);

    while (rc >= 0)
    {
        rc = MQTTYield(client, 1000);    
    }
    
    printf("[%s][%d] Stopping\n", __FUNCTION__, __LINE__);
    atiny_deinit(phandle);

    return ATINY_OK;
}

int atiny_data_send(void* phandle, cloud_msg_t* send_data, atiny_rsp_cb cb)
{
    handle_data_t* handle;
    MQTTClient *client;
    int rc= -1;

    if (NULL == phandle || NULL == send_data || NULL == send_data->uri 
        || !(send_data->qos>=CLOUD_QOS_MOST_ONCE && send_data->qos<CLOUD_QOS_LEVEL_MAX)
        || !(send_data->method>=CLOUD_METHOD_GET&& send_data->method<CLOUD_METHOD_MAX))
    {
        //ATINY_LOG(LOG_ERR, "invalid args");
        return ATINY_ARG_INVALID;
    }

    handle = (handle_data_t*)phandle;
    client = &(handle->client);

    switch(send_data->method)
    {
        case CLOUD_METHOD_GET:
            if(NULL == cb)
                return ATINY_ARG_INVALID;
            rc = mqtt_topic_subscribe(client, send_data->uri, send_data->qos, cb);
            break;
        case CLOUD_METHOD_POST:
            if(send_data->payload_len<= 0 || send_data->payload_len > MAX_REPORT_DATA_LEN || NULL == send_data->payload)
                return ATINY_ARG_INVALID;
            rc = mqtt_message_publish(client, send_data);
            break;
        case CLOUD_METHOD_DEL:
            rc = mqtt_topic_unsubscribe(client, send_data->uri);
            break;
        default:
            printf("[%s][%d] unsupported method : %d\n", __FUNCTION__, __LINE__, send_data->method);
    }

    return rc;
}
