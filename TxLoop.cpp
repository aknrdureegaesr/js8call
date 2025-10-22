#include <QLoggingCategory>
#include "DriftingDateTime.h"
#include "TxLoop.h"
#include "JS8Submode.hpp"

Q_DECLARE_LOGGING_CATEGORY(txloop_js8)

TxLoop::TxLoop(const QString & name_) :
    // We found out the hard way that it is wise
    // to initialize this with something UTC-based.
    m_name{name_},
    m_next_activity{DriftingDateTime::currentDateTimeUtc()},
    m_submode{Varicode::JS8CallSlow},
    m_tx_delay_ms{100}, // arbitrary
    m_active{false},
    m_loop_period_ms{60000} // arbitrary
{
    m_next_activity_timer.setSingleShot(true);
    m_next_activity_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_next_activity_timer, &QTimer::timeout, this, &TxLoop::onTimer);
}

TxLoop::~TxLoop() {
    qCDebug(txloop_js8) << m_name << "Destruction of TX loop";
    onLoopCancel();
}

void TxLoop::onTimer() {
    m_next_activity_timer.stop();
    if(m_active) {
        // Calculate the next start point.
        // Doing it as milliseconds is fast and straightforward.
        qint64 now_ms = DriftingDateTime::currentMSecsSinceEpoch();
        qint64 next_activity_ms_raw = m_next_activity.toMSecsSinceEpoch() + m_loop_period_ms;
        qint64 mode_period_ms = ((qint64)1000) * JS8::Submode::period(m_submode);

        // Be a bit paranoid and make sure that we start when a node slot starts:
        qint64 into_mode_slot =  next_activity_ms_raw % mode_period_ms; // should be 0
        qint64 next_activity_ms = next_activity_ms_raw - into_mode_slot;

        // Be a bit paranoid and make sure we wait for the future, not for the past:
        if(next_activity_ms - m_tx_delay_ms < now_ms) {
            qint64 loop_periods_missing = (now_ms - (next_activity_ms - m_tx_delay_ms)) / m_loop_period_ms + 1;
            qint64 additional_increase = loop_periods_missing * m_loop_period_ms;
            qCWarning(txloop_js8)
                << m_name << "originally planed to repeat at" << next_activity_ms
                << "but now is already" << now_ms << "with tx_delay_ms" << m_tx_delay_ms
                << ", so will advance another" << additional_increase << "ms into the future.";
            next_activity_ms += additional_increase;
        }

        qint64 wait_for_next_trigger = next_activity_ms - now_ms - m_tx_delay_ms;
        m_next_activity_timer.start(wait_for_next_trigger);
        m_next_activity.setMSecsSinceEpoch(next_activity_ms);
        qCDebug(txloop_js8)
            << m_name << "triggers TX, plans future signal to happen at" << m_next_activity
            << ", with tx_delay_ms" << m_tx_delay_ms << "timer wakes us in" << wait_for_next_trigger << "ms";
        emit nextActivityChanged(m_next_activity);
        emit triggerTxNow();
    } else {
        qCDebug(txloop_js8) << m_name << "timer triggered, but loop is inactive, so ignoring.";
    }
}

void TxLoop::onPlumbingCompleted() {
    if(m_active) {
        qCDebug(txloop_js8) << m_name << "Plumbing completed and TX loop already active, next transmission at" << m_next_activity;
        emit nextActivityChanged(m_next_activity);
    } else {
        qCDebug(txloop_js8) << m_name << "Plumbing completed and TX loop not active.";
        emit canceled();
    }
}

void TxLoop::onDriftChange(qint64 /* new_drift */) {
    if (m_active) {
        m_next_activity_timer.stop();
        QDateTime now = DriftingDateTime::currentDateTimeUtc();
        QDateTime need_to_push_ptt = m_next_activity.addMSecs(-m_tx_delay_ms);
        if (now < need_to_push_ptt &&
            need_to_push_ptt < now.addMSecs(m_loop_period_ms)) {
            // Small drift change which does not affect
            // when the next event is to happen.
            // Just adjust the timer (slightly).
            qint64 fire_in_ms = now.msecsTo(need_to_push_ptt);
            qCDebug(txloop_js8) << m_name << "Small drift change, pushing PTT in" << fire_in_ms << "ms";
            m_next_activity_timer.start(fire_in_ms);
        } else {
            qCDebug(txloop_js8) << m_name << "Big drift change, recalculating next PTT moment.";
            // Big drift change. Calculate next event anew:
            onTxLoopPeriodChangeStart(m_loop_period_ms);
        }
    }
}

void TxLoop::onModeChange(Varicode::SubmodeType new_submode) {
    if (m_submode != new_submode) {
        qCDebug(txloop_js8)
            << m_name << "Submode change from" << JS8::Submode::name(m_submode)
            << "to" << JS8::Submode::name(new_submode);
        m_submode = new_submode;
        if(m_active)
            onTxLoopPeriodChangeStart(m_loop_period_ms);
    }
}

void TxLoop::onTxDelayChange(qint64 tx_delay_ms) {
    qCDebug(txloop_js8)
        << m_name << "TX delay change from" << this->m_tx_delay_ms
        << "to" << tx_delay_ms;
    this->m_tx_delay_ms = tx_delay_ms;
    if(m_loop_period_ms < tx_delay_ms) {
        // This is extreme...
        qCDebug(txloop_js8) << m_name << "Extreme long TX delay, need to adjust period.";
        onTxLoopPeriodChangeStart(tx_delay_ms);
    }
    if(m_active)
        onTxLoopPeriodChangeStart(m_loop_period_ms);
}

void TxLoop::onLoopCancel() {
    if(m_active) {
        qCDebug(txloop_js8) << m_name << "Canceling activity and emitting pertinent signal.";
        m_next_activity_timer.stop();
        m_active = false;
        emit canceled();
    } else {
        qCDebug(txloop_js8) << m_name << "Canceling activity, but I was idle anyway.";
    }
}

void TxLoop::onTxLoopPeriodChangeStart(qint64 loop_period_ms) {
    // This is also sometimes used to adjust the timer when members changed.
    m_next_activity_timer.stop(); // Probably redundant.
    QDateTime now = DriftingDateTime::currentDateTimeUtc();
    qint64 now_ms = now.currentMSecsSinceEpoch();
    qint64 earliest = now_ms + m_loop_period_ms;
    qint64 mode_period_ms = ((qint64)1000) * JS8::Submode::period(m_submode);
    qint64 remainder = earliest % mode_period_ms;
    qint64 next_start =
        0 == remainder ? earliest : earliest + mode_period_ms - remainder;
    bool need_to_send_signal =
        !m_active || next_start != m_next_activity.currentMSecsSinceEpoch();
    qint64 need_to_wait = next_start-now_ms-m_tx_delay_ms;
    m_active = true;
    m_next_activity.setMSecsSinceEpoch(next_start);
    m_next_activity_timer.start(need_to_wait);
    qCDebug(txloop_js8)
        << m_name
        << "Active tx loop, signal to start at" << m_next_activity
        << "need to emit signal:" << need_to_send_signal
        << "mode_period_ms:" << mode_period_ms
        << "loop_period_ms:" << m_loop_period_ms
        << "tx_delay_ms:" << m_tx_delay_ms
        << "now_ms:" << now_ms
        << "earliest:" << earliest
        << "remainder:" << remainder
        << "next_start:" << next_start
        << "meed to wait:" << need_to_wait;

    if(need_to_send_signal)
        emit nextActivityChanged(m_next_activity);
}

Q_LOGGING_CATEGORY(txloop_js8, "txloop.js8", QtWarningMsg)
