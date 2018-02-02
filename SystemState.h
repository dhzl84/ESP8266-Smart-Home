#ifndef SYSTEMSTATE_H_
#define SYSTEMSTATE_H_

#define debounceTime 30000

typedef enum systemState_e
{
   systemState_init = 0,
   systemState_offline,
   systemState_online
} systemState_t;

class SystemState
{
private:
   systemState_t  systemState;
   int            connectDebounceTime; /* avoid reconnecting each loop with this timer */
   bool           restartRequest;

public:
                  SystemState(void);
                  ~SystemState(void);
   void           init(void);
   void           setSystemState(systemState_t state);
   systemState_t  getSystemState(void);
   int            getConnectDebounceTimer(void);
   void           setSystemRestartRequest(bool request);
   bool           getSystemRestartRequest(void);
};



#endif /* SYSTEMSTATE_H_ */
