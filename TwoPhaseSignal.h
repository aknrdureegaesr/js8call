#ifndef TWOPHASESIGNAL_H
#define TWOPHASESIGNAL_H
#include <QObject>

/**
 * This is a humble helper class for Qt signals and slots which are the only
 * means to transfer data.  In that case, we first do the plumbing case,
 * where signals and slots are connected.  Only after that has been completed,
 * signals are to be sent that convey the initial values.
 */
class TwoPhaseSignal : public QObject {
    Q_OBJECT
public:
    TwoPhaseSignal();
    ~TwoPhaseSignal();

public slots:
    /**
     * Called when the plumbing has been completed.
     * The object should react by fireing its signals.
    */
    virtual void onPlumbingCompleted() = 0;
};

#endif // TWOPHASESIGNAL_H
