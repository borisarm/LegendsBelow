// Copyright (c) Microsoft Corporation.  All rights reserved

#if !defined(__cplusplus)
    #error C++11 required
#endif

#pragma once

#include <XTaskQueue.h>

extern "C" 
{

typedef void CALLBACK XGameInviteEventCallback(
    _In_opt_ void* context,
    _In_ const char* inviteUri
    );

STDAPI XGameInviteRegisterForEvent(
    _In_opt_ XTaskQueueHandle queue,
    _In_opt_ void* context,
    _In_ XGameInviteEventCallback* callback,
    _Out_ XTaskQueueRegistrationToken* token
    ) noexcept;

STDAPI_(bool) XGameInviteUnregisterForEvent(
    _In_ XTaskQueueRegistrationToken token,
    _In_ bool wait
    ) noexcept;

STDAPI XGameInviteRegisterForPendingEvent(
    _In_opt_ XTaskQueueHandle queue,
    _In_opt_ void* context,
    _In_ XGameInviteEventCallback* callback,
    _Out_ XTaskQueueRegistrationToken* token
    ) noexcept;

STDAPI_(bool) XGameInviteUnregisterForPendingEvent(
    _In_ XTaskQueueRegistrationToken token,
    _In_ bool wait
    ) noexcept;

STDAPI XGameInviteAcceptPendingInvite(
    _In_z_ const char* inviteUri
    ) noexcept;

}
