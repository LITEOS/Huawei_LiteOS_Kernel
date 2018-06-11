#include "atadapter.h"
#include "at_api_interface.h"
#include "stm32f4xx_hal.h"
#include "los_task.h"
#include "los_sem.h"
#if defined(USE_AT_FRAMEWORK)

/* FUNCTION */
void at_init();
int32_t at_read(int32_t id, int8_t * buf, uint32_t len, int32_t timeout);
int32_t at_write(int8_t * cmd, int8_t * suffix, int8_t * buf, int32_t len);
int32_t at_get_unuse_linkid();
void at_listener_list_add(at_listener * p);
void at_listner_list_del(at_listener * p);
int32_t at_cmd(int8_t * cmd, int32_t len, const char * suffix, char * rep_buf);
int at_oob_register(char* cmd,int cmdlen, oob_callback callback);

/* VARIABLE */
UART_HandleTypeDef at_usart;
#ifdef USE_USARTRX_DMA

DMA_HandleTypeDef  at_hdma_usart3_rx;
#endif

uint32_t list_mux;

uint32_t wi = 0;
uint32_t wi_bak= 0;
uint32_t ri = 0;

//init function for at struct
at_task at = {
    .init = at_init,
    .cmd = at_cmd,
    .write = at_write,
    .oob_register = at_oob_register,
    .get_id = at_get_unuse_linkid,
};
at_oob_t at_oob;

#ifdef USE_USARTRX_DMA
void at_usart3rx_dma_irqhandler(void)
{
    HAL_DMA_IRQHandler(&at_hdma_usart3_rx);
}

void at_usart3rx_dma_config(DMA_HandleTypeDef *hdma)
{
    printf("Now Init DMA1\n");
    __HAL_RCC_DMA1_CLK_ENABLE();
    hdma->Instance = DMA1_Stream1;
    hdma->Init.Channel = DMA_CHANNEL_4;
    hdma->Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma->Init.PeriphInc = DMA_PINC_DISABLE;
    hdma->Init.MemInc = DMA_MINC_ENABLE;
    hdma->Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma->Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;

    hdma->Init.Mode = DMA_NORMAL;
    hdma->Init.Priority = DMA_PRIORITY_HIGH;//DMA_PRIORITY_LOW;
    hdma->Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    hdma->Init.FIFOThreshold = DMA_FIFO_THRESHOLD_FULL;
    hdma->Init.MemBurst = DMA_MBURST_SINGLE;
    hdma->Init.PeriphBurst = DMA_PBURST_SINGLE;
    HAL_DMA_Init(hdma);

    __HAL_LINKDMA(&at_usart,hdmarx,*hdma);
        
    LOS_HwiCreate(DMA1_Stream1_IRQn, 0, 0,at_usart3rx_dma_irqhandler, NULL);
}
#endif


void at_irq_handler(void)
{

#ifndef USE_USARTRX_DMA
    if(__HAL_UART_GET_FLAG(&at_usart, UART_FLAG_RXNE) != RESET)
    {
        at.recv_buf[wi++] = (uint8_t)(at_usart.Instance->DR & 0x00FF);
        if (wi >= at_user_conf.user_buf_len)wi = 0;
    }
    else 
#endif
    if (__HAL_UART_GET_FLAG(&at_usart,UART_FLAG_IDLE) != RESET)
    {
        __HAL_UART_CLEAR_IDLEFLAG(&at_usart);
#ifdef USE_USARTRX_DMA
        HAL_UART_DMAStop(&at_usart);  
        uint32_t empty = __HAL_DMA_GET_COUNTER(&at_hdma_usart3_rx);
        if(empty > at_user_conf.user_buf_len)
        {
        //error
        }
        else
        {
            wi = (at_user_conf.user_buf_len - empty);
        }
#endif   
        wi_bak = wi;
        LOS_SemPost(at.recv_sem);
    }
}

void at_usart_config(UART_HandleTypeDef * usart)
{
    usart->Instance = at_user_conf.usart;
    usart->Init.BaudRate = at_user_conf.buardrate;

    usart->Init.WordLength = UART_WORDLENGTH_8B;
    usart->Init.StopBits = UART_STOPBITS_1;
    usart->Init.Parity = UART_PARITY_NONE;
    usart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    usart->Init.Mode = UART_MODE_RX | UART_MODE_TX;
    if(HAL_UART_Init(usart) != HAL_OK)
    {
      _Error_Handler(__FILE__, __LINE__);
    }
    __HAL_UART_CLEAR_FLAG(usart,UART_FLAG_TC);
    LOS_HwiCreate(at_user_conf.irqn, 0, 0, at_irq_handler, 0);
    __HAL_UART_ENABLE_IT(usart, UART_IT_IDLE);

#ifdef USE_USARTRX_DMA
    at_usart3rx_dma_config(&at_hdma_usart3_rx);
#else
    __HAL_UART_ENABLE_IT(usart, UART_IT_RXNE);
#endif
}

at_listener * get_at_listener_head()
{
    at_listener * p = NULL;
    LOS_MuxPend(list_mux, LOS_WAIT_FOREVER);
    p = at.head;
    LOS_MuxPost(list_mux);
    return p;
}
//add p to tail;
void at_listener_list_add(at_listener * p)
{
    at_listener * head = at.head;

    LOS_MuxPend(list_mux, LOS_WAIT_FOREVER);

    if (NULL == head)
    {
        at.head = p;
        p->next = NULL;
        LOS_MuxPost(list_mux);
        return;
    }

    LOS_MuxPost(list_mux);

}

void at_listner_list_del(at_listener * p)
{
    at_listener * head = at.head;
    at_listener * node = head;

    LOS_MuxPend(list_mux, LOS_WAIT_FOREVER);
    if (p == head)
    {
        at.head = head->next;
        LOS_MuxPost(list_mux);
        return;
    }

    LOS_MuxPost(list_mux);

}

int32_t at_get_unuse_linkid()
{
    int i = 0;

    if (AT_MUXMODE_MULTI == at.mux_mode)
    {
        for (i = 0; i < at_user_conf.linkid_num; i++)
        {
            if (AT_LINK_UNUSE == at.linkid[i].usable)
            {
                break;
            }
        }
    }

    if (i >= 0 && i < at_user_conf.linkid_num)
        at.linkid[i].usable = AT_LINK_INUSE;
    return i;
}


void store_resp_buf(int8_t * resp_buf, int8_t * buf, uint32_t len)
{
    if (NULL == buf || 0 == len || len > at_user_conf.user_buf_len)
        return;

    strcat((char*)resp_buf, (char *)buf);
}

int32_t at_cmd(int8_t * cmd, int32_t len, const char * suffix, char * rep_buf)
{
    at_listener listener;
    char * line_end = at_user_conf.line_end;
    int ret = AT_FAILED;

    listener.perfix = (int8_t *)at_user_conf.cmd_begin;
    listener.suffix = (int8_t *)suffix;
    listener.resp = (int8_t*)rep_buf;

    ret = LOS_SemCreate(0, (UINT32*)&listener.resp_sem);
    if (ret != LOS_OK)
    {
        AT_LOG("init listener.resp_sem failed(ret = %x)!", ret);
        return AT_FAILED;
    }

    at_listener_list_add(&listener);

    LOS_MuxPend(at.cmd_mux, LOS_WAIT_FOREVER);
    HAL_UART_Transmit(&at_usart, (uint8_t*)cmd, len, 0xffff);
    HAL_UART_Transmit(&at_usart, (uint8_t*)line_end, strlen(at_user_conf.line_end), 0xffff);

    ret = LOS_SemPend(listener.resp_sem, at.timeout);
    LOS_MuxPost(at.cmd_mux);

    LOS_SemDelete(listener.resp_sem);

    at_listner_list_del(&listener); 

    if (ret != LOS_OK)
    {
        AT_LOG("LOS_SemPend for listener.resp_sem failed(ret = %x)!", ret);
        return AT_FAILED;
    }
    return len;
}

int32_t at_write(int8_t * cmd, int8_t * suffix, int8_t * buf, int32_t len)
{
    at_listener listener;
    char * line_end = at_user_conf.line_end;
    int ret = AT_FAILED;

    listener.perfix = (int8_t *)at_user_conf.cmd_begin;
    listener.suffix = (int8_t *)suffix;
    listener.resp = NULL;

    ret = LOS_SemCreate(0, (UINT32*)&listener.resp_sem);
    if (ret != LOS_OK)
    {
        AT_LOG("init listener.resp_sem failed(ret = %x)!", ret);
        return AT_FAILED;
    }
    at_listener_list_add(&listener);

    LOS_MuxPend(at.cmd_mux, LOS_WAIT_FOREVER);

    HAL_UART_Transmit(&at_usart, (uint8_t*)cmd, strlen((char*)cmd), 0xffff);
    HAL_UART_Transmit(&at_usart, (uint8_t*)line_end, strlen(at_user_conf.line_end), 0xffff);
    osDelay(osMs2Tick(200));

    HAL_UART_Transmit(&at_usart, (uint8_t*)buf, len, 0xffff);
    ret = LOS_SemPend(listener.resp_sem, at.timeout);

    LOS_MuxPost(at.cmd_mux);

    LOS_SemDelete(listener.resp_sem);

    at_listner_list_del(&listener); 
    if (ret != LOS_OK)
    {
        AT_LOG("LOS_SemPend for listener.resp_sem failed(ret = %x)!", ret);
        return AT_FAILED;
    }
    return len;
}


int read_resp(uint8_t * buf)
{
    uint32_t len = 0;
    uint32_t wi = wi_bak;
    uint32_t tmp_len = 0;
    if (NULL == buf){
        return -1;
    }

    if (wi == ri){
        return 0;
    }

    if (wi > ri){
        len = wi - ri;
        memcpy(buf, &at.recv_buf[ri], len);
    } 
    else 
    {
#ifndef USE_USARTRX_DMA
        tmp_len = at_user_conf.user_buf_len - ri;
        memcpy(buf, &at.recv_buf[ri], tmp_len);
        memcpy(buf + tmp_len, at.recv_buf, wi);
        len = wi + tmp_len;
#endif
    }
  
#ifndef USE_USARTRX_DMA
    ri = wi;
#else
    ri = 0;
    HAL_UART_Receive_DMA(&at_usart,&at.recv_buf[0],at_user_conf.user_buf_len-1);
#endif
    return len;
}

int cloud_cmd_matching(int8_t * buf, int32_t len)
{
    int32_t ret = 0;
    int i;
//    char* buf = (char*)at.userdata;
    for(i=0;i<at_oob.oob_num;i++){
        //ret = strstr(at_oob.oob[i].featurestr,buf);
        if(memcmp(at_oob.oob[i].featurestr,buf,at_oob.oob[i].len) == 0)
        {
            AT_LOG("cloud match:%s buf:%s",at_oob.oob[i].featurestr,buf);
            if(at_oob.oob[i].cb != NULL)
                ret = at_oob.oob[i].cb(at_oob.oob[i].arg,buf,len);
            return ret;
        }
    }
    return 0;
}

void at_recv_task(uint32_t p)
{
    uint32_t recv_len = 0;
    static uint32_t suffix_catching = 0;
    uint8_t * tmp = at.userdata; //[MAX_USART_BUF_LEN] = {0};
    int ret = 0;
    at_listener * listener = NULL;

    while(1){
        LOS_SemPend(at.recv_sem, LOS_WAIT_FOREVER);

        memset(tmp, 0, at_user_conf.user_buf_len);
        recv_len = read_resp(tmp);

        if (0 >= recv_len)
            continue;

        int32_t data_len = 0;
        char * p1, * p2;
        AT_LOG_DEBUG("get recv_len = %lu buf = %s", recv_len, tmp);

        p1 = (char *)tmp;
        p2 = (char *)(tmp + recv_len);
        for (;;){

            data_len = p2 - p1;
            if (data_len <= 0)
                break;

            char * suffix;
            ret = cloud_cmd_matching((int8_t*)p1, data_len);
            if(ret > 0)
            {
                p1 += ret;
                continue;
            }

            listener = get_at_listener_head();
            if (NULL == listener)
                break;

            if(listener->suffix == NULL)//用户不需要得到回应，分两种情况，一种rbuf直接丢弃，一种rbuf交给下一上报任务
            {
                //listener->resp这里存的时候要判断是否非空
                //store_resp_buf((int8_t *)listener->resp, (int8_t*)p1, p2 - p1);
                LOS_SemPost(listener->resp_sem);
                listener = NULL;
                break;
            }

            suffix = strstr(p1, (const char*)listener->suffix);
            AT_LOG_DEBUG("GOT suffix (%s) at %p", listener->suffix, suffix);

            if (NULL == suffix)
            {
                if(NULL != listener->resp)
                    store_resp_buf((int8_t*)listener->resp, (int8_t*)p1, p2 - p1);
            }
            else 
            {
                if(NULL != listener->resp)
                    store_resp_buf((int8_t *)listener->resp, (int8_t*)p1, suffix + strlen((char*)listener->suffix) - p1);
                LOS_SemPost(listener->resp_sem);
            } 
            break;
        }
    }
}

uint32_t create_at_recv_task()
{
    uint32_t uwRet = LOS_OK;
    TSK_INIT_PARAM_S task_init_param;

    task_init_param.usTaskPrio = 0;
    task_init_param.pcName = "at_recv_task";
    task_init_param.pfnTaskEntry = (TSK_ENTRY_FUNC)at_recv_task;
    task_init_param.uwStackSize = 0x1000;

    uwRet = LOS_TaskCreate((UINT32*)&at.tsk_hdl, &task_init_param);
    if(LOS_OK != uwRet)
    {
        return uwRet;
    }
    return uwRet;

}

void at_init_oob(void)
{
    at_oob.oob_num = 0;
    memset(at_oob.oob,0,5*sizeof(struct oob_s));
}

int32_t at_struct_init(at_task * at)
{
    int ret = -1;
    if (NULL == at)
    {
        AT_LOG("invaild param!");
        return ret;
    }

    ret = LOS_SemCreate(0, (UINT32*)&at->recv_sem);
    if (ret != LOS_OK)
    {
        AT_LOG("init at_recv_sem failed!");
        goto at_recv_sema_failed;
    }


    ret = LOS_MuxCreate((UINT32*)&at->cmd_mux);
    if (ret != LOS_OK)
    {
        AT_LOG("init cmd_mux failed!");
        goto at_cmd_mux_failed;
    }

    at->recv_buf = atiny_malloc(at_user_conf.user_buf_len);
    if (NULL == at->recv_buf)
    {
        AT_LOG("malloc recv_buf failed!");
        goto malloc_recv_buf;
    }

    at->cmdresp = atiny_malloc(at_user_conf.user_buf_len);
    if (NULL == at->cmdresp)
    {
        AT_LOG("malloc cmdresp failed!");
        goto malloc_resp_buf;
    }

    at->userdata = atiny_malloc(at_user_conf.user_buf_len);
    if (NULL == at->userdata)
    {
        AT_LOG("malloc userdata failed!");
        goto malloc_userdata_buf;
    }

    at->linkid = (at_link*)atiny_malloc(at_user_conf.linkid_num * sizeof(at_link));
    if (NULL == at->linkid)
    {
       AT_LOG("malloc for at linkid array failed!");
       goto malloc_linkid_failed;
    }

    ret = LOS_MuxCreate((UINT32*)&list_mux);
    if (ret != LOS_OK)
    {
        AT_LOG("init list_mux failed!");
        goto at_list_mux_failed;
    }

    at->head = NULL;
    at->mux_mode = at_user_conf.mux_mode;
    at->timeout = at_user_conf.timeout;
    return AT_OK;

 //       LOS_MuxDelete(list_mux);
    at_list_mux_failed:
        atiny_free(at->linkid);
    malloc_linkid_failed:
        atiny_free(at->userdata);
    malloc_userdata_buf:
        atiny_free(at->cmdresp);
    malloc_resp_buf:
        atiny_free(at->recv_buf);
    malloc_recv_buf:
        LOS_MuxDelete(list_mux);
    at_cmd_mux_failed:
        LOS_SemDelete(at->recv_sem);
    at_recv_sema_failed:
        return AT_FAILED;
}

void at_init()
{
    AT_LOG("Config %s......\n", at_user_conf.name);

    osDelay(2000);
    if (AT_OK != at_struct_init(&at))
    {
        AT_LOG("prepare at struct failed!");
        return;
    }

    at_usart_config(&at_usart);
    create_at_recv_task();

    AT_LOG("Config complete!!\n");

}

int at_oob_register(char* cmd,int cmdlen, oob_callback callback)
{
    oob_t *oob;
    if(at_oob.oob_num == OOB_MAX_NUM || cmdlen >= 10)
        return -1;
    oob = &(at_oob.oob[at_oob.oob_num++]);
    memcpy(oob->featurestr,cmd,cmdlen);
    oob->len = strlen(cmd);
    oob->cb = callback;
    return 0;
}
#endif