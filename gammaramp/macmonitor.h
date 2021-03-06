#ifndef MACMONITOR_H
#define MACMONITOR_H

// Application:
#include "monitor.h"
#include <stdint.h>
#include "gammaramp_global.h"

class GAMMARAMPSHARED_EXPORT MacMonitor : public Monitor
{
public:
    // Constructor:
    MacMonitor();

    // Init:
    virtual bool init();

    // Start:
    virtual bool start();

    // Restore:
    virtual void restore();

    // Set temperature:
    virtual bool setTemperature(int iTemperature, bool bForce=false);

protected:
    // Saved ramps:
    float *m_pSavedRamps;

private:
    // Color ramp fill:
    void colorRampFill(float *gamma_r, float *gamma_g, float *gamma_b,
        int iSize, int iTemperature);
};

#endif // MACMONITOR_H
