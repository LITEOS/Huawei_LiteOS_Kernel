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

#include "atiny_fota_manager.h"
#include "atiny_fota_state.h"
#include <string.h>
#include "firmware_update.h"

struct atiny_fota_manager_tag_s
{
    char *pkg_uri;
    atiny_fota_state_e state;
    atiny_update_result_e update_result;
    atiny_fota_idle_state_s idle_state;
    atiny_fota_downloading_state_s downloading_state;
    atiny_fota_downloaded_state_s downloaded_state;
    atiny_fota_updating_state_s updating_state;
    atiny_fota_state_s *current;
    atiny_fota_storage_device_s *device;
    lwm2m_context_t*  lwm2m_context;
    bool init_flag;
};

#define PULL_ONLY 0

char * atiny_fota_manager_get_pkg_uri(const atiny_fota_manager_s *thi)
{
    ASSERT_THIS(return NULL);
    return thi->pkg_uri;
}
int atiny_fota_manager_get_state(const atiny_fota_manager_s *thi)
{
    ASSERT_THIS(return ATINY_FOTA_IDLE);
    return thi->state;
}
int atiny_fota_manager_get_update_result(const atiny_fota_manager_s *thi)
{
    ASSERT_THIS(return ATINY_FIRMWARE_UPDATE_NULL);
    return thi->update_result;
}
void atiny_fota_manager_set_update_result(atiny_fota_manager_s *thi, atiny_update_result_e result)
{
    ASSERT_THIS(return);
    thi->update_result = result;
}

int atiny_fota_manager_get_deliver_method(const atiny_fota_manager_s *thi)
{
    return PULL_ONLY;
}
int atiny_fota_manager_start_download(atiny_fota_manager_s *thi, const char *uri, uint32_t len)
{

    ASSERT_THIS(return ATINY_ARG_INVALID);
    if(NULL == thi->current || uri == NULL)
    {
        ATINY_LOG(LOG_ERR, "null pointer");
        return ATINY_ERR;
    }

    if(thi->pkg_uri)
    {
        atiny_free(thi->pkg_uri);
        thi->pkg_uri = NULL;
    }
    thi->pkg_uri = atiny_malloc(len + 1);
    if(NULL == thi->pkg_uri)
    {
        ATINY_LOG(LOG_ERR, "lwm2m_strdup fail");
        return ATINY_ERR;
    }
    memcpy(thi->pkg_uri, uri, len);
    thi->pkg_uri[len] = '\0';
    ATINY_LOG(LOG_INFO, "start download %s", thi->pkg_uri);
    return thi->current->start_download(thi->current, thi->pkg_uri);
}
int atiny_fota_manager_execute_update(atiny_fota_manager_s *thi)
{
    ASSERT_THIS(return ATINY_ARG_INVALID);
    if(NULL == thi->current)
    {
        ATINY_LOG(LOG_ERR, "current null pointer");
        return ATINY_ERR;
    }

    ATINY_LOG(LOG_INFO, "excecute update");
    return thi->current->execute_update(thi->current);
}

int atiny_fota_manager_finish_download(atiny_fota_manager_s *thi, int result)
{
    ASSERT_THIS(return ATINY_ARG_INVALID);
    if(NULL == thi->current)
    {
        ATINY_LOG(LOG_ERR, "current null pointer");
        return ATINY_ERR;
    }

    ATINY_LOG(LOG_INFO, "finish download result %d", result);
    return thi->current->finish_download(thi->current, result);
}
int atiny_fota_manager_repot_result(atiny_fota_manager_s *thi)
{
    ASSERT_THIS(return ATINY_ARG_INVALID);
    if(NULL == thi->current)
    {
        ATINY_LOG(LOG_ERR, "current null pointer");
        return ATINY_ERR;
    }

    return thi->current->repot_result(thi->current);
}


int atiny_fota_manager_set_state(atiny_fota_manager_s *thi, atiny_fota_state_e state)
{
    lwm2m_uri_t uri;
    const char *uri_str = "/5/0/3";

    ASSERT_THIS(return ATINY_ARG_INVALID);

    if(state > ATINY_FOTA_UPDATING)
    {
        ATINY_LOG(LOG_ERR, "invalid download state %d", state);
        return ATINY_ARG_INVALID;
    }

    ATINY_LOG(LOG_INFO, "download stat from %d to %d", thi->state, state);
    thi->state = state;
    {
         atiny_fota_state_s *states[] = {ATINY_GET_STATE(thi->idle_state),
                                ATINY_GET_STATE(thi->downloading_state),
                                ATINY_GET_STATE(thi->downloaded_state),
                                ATINY_GET_STATE(thi->updating_state)};
        thi->current = states[state];
    }
    memset((void*)&uri, 0, sizeof(uri));
    lwm2m_stringToUri(uri_str, strlen(uri_str), &uri);
    lwm2m_resource_value_changed(thi->lwm2m_context, &uri);
    return ATINY_OK;
}

int atiny_fota_manager_set_storage_device(atiny_fota_manager_s *thi, atiny_fota_storage_device_s *device)
{
    ASSERT_THIS(return ATINY_ARG_INVALID);
    thi->device = device;
    return atiny_update_info_set(atiny_update_info_get_instance(), device);
}

atiny_fota_storage_device_s *atiny_fota_manager_get_storage_device(atiny_fota_manager_s *thi)
{
    ASSERT_THIS(return NULL);
    return thi->device;
}


void atiny_fota_manager_update_notify(firmware_update_rst_e rst, void *param)
{
    atiny_fota_manager_s *thi = ( atiny_fota_manager_s *)param;
   atiny_fota_manager_finish_download(thi, rst);
}
void atiny_fota_manager_init(atiny_fota_manager_s *thi)
{
    memset(thi, 0, sizeof(*thi));
    atiny_fota_idle_state_init(&thi->idle_state, thi);
    atiny_fota_downloading_state_init(&thi->downloading_state, thi);
    atiny_fota_downloaded_state_init(&thi->downloaded_state, thi);
    atiny_fota_updating_state_init(&thi->updating_state, thi);
    thi->current = ATINY_GET_STATE(thi->idle_state);
    set_firmware_update_notify(atiny_fota_manager_update_notify, thi);
    thi->init_flag = true;
}

void atiny_fota_manager_destroy(atiny_fota_manager_s *thi)
{
    ASSERT_THIS(return);

    if(thi->pkg_uri)
    {
        atiny_free(thi->pkg_uri);
    }
    memset(thi, 0, sizeof(*thi));
}

int atiny_fota_manager_set_lwm2m_context(atiny_fota_manager_s *thi, lwm2m_context_t*  lwm2m_context)
{
     ASSERT_THIS(return ATINY_ARG_INVALID);
     thi->lwm2m_context = lwm2m_context;
     return ATINY_OK;
}
lwm2m_context_t* atiny_fota_manager_get_lwm2m_context(atiny_fota_manager_s *thi)
{
    ASSERT_THIS(return NULL);
    return thi->lwm2m_context;
}


static atiny_fota_manager_s g_fota_manager;
atiny_fota_manager_s * atiny_fota_manager_get_instance(void)
{
    atiny_fota_manager_s *manager = &g_fota_manager;
    if(manager->init_flag)
    {
        return manager;
    }

    atiny_fota_manager_init(manager);
    return manager;
}



