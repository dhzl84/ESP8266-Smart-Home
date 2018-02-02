#include "SystemState.h"

SystemState::SystemState()
{
   SystemState::init();
}

SystemState::~SystemState()
{
   /* do nothing */
}

void SystemState::init()
{
   systemState          = systemState_init;
   connectDebounceTime  = debounceTime; /* 30s */
   restartRequest       = false;
}

void SystemState::setSystemState(systemState_t state)
{
   /* reinitialize debounce timer if transition from offline to online is detected */
   if ( (systemState == systemState_offline) && (state == systemState_online) )
   {
      connectDebounceTime = debounceTime;
   }
   systemState = state;
}

systemState_t SystemState::getSystemState(void)
{
   return systemState;
}

int SystemState::getConnectDebounceTimer(void)
{
   if (connectDebounceTime > 0)
   {
      connectDebounceTime--;
   }
   return (connectDebounceTime);
}

void SystemState::setSystemRestartRequest(bool request)
{
   restartRequest = request;
}
bool SystemState::getSystemRestartRequest(void)
{
   return restartRequest;
}
