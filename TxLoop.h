#ifndef TXLOOP_HPP_

#include <QDateTime>
#include <QTimer>

#include "TwoPhaseSignal.h"
#include "varicode.h"

/**
 * Class to organize transmit loops.
 *
 * A transmit loop either regularly triggers either a CQ or a HB transmissions.
 *
 * One object of this class organizes one such loop.
 **/
class TxLoop : public TwoPhaseSignal {
    Q_OBJECT

private:
    const QString m_name;
    // When do we need to press the TX button the next time?
    QDateTime m_next_activity;
    // A time that is supposed to wake us up when it is time.
    QTimer m_next_activity_timer;
    // The currently active JS8 m_submode.
    Varicode::SubmodeType m_submode;
    // How much tx_delay, in milliseconds, we need to be early.
    qint64 m_tx_delay_ms;
    // Whether this loop is m_active (or else does nothing).
    bool m_active;
    // After how many milliseconds to repeat sending.
    qint64 m_loop_period_ms;
    
public:
    // The "name" argument is only used for logging.
    TxLoop(const QString& name);

    ~TxLoop();

    inline bool isActive() const { return m_active; }

    /**
     * Return the timestamp when the payload signal is supposed to start.
     * This timestamp will always point to a full second.
     * The actual triggering happens tx_delay earlier.
     *
     * May return an arbitrary or invalid timestamp when isActive() returns false.
     */
    inline const QDateTime & nextActivity() const {
        return m_next_activity;
    }

signals:
    void triggerTxNow();
    void nextActivityChanged(const QDateTime &);
    void canceled();

private slots:
    void onTimer();

public slots:
    void onPlumbingCompleted();
    void onDriftChange(qint64 new_drift);
    void onModeChange(Varicode::SubmodeType new_submode);
    void onTxDelayChange(qint64 tx_delay_ms);

    // This starts the loop:
    void onTxLoopPeriodChangeStart(qint64 loop_period_ms);
    void onLoopCancel();
};

#endif
