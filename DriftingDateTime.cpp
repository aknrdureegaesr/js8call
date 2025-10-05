#include <QLoggingCategory>
#include "DriftingDateTime.h"

Q_DECLARE_LOGGING_CATEGORY(driftingdatetime_js8)

namespace
{
    qint64 driftMS = 0;
}

namespace DriftingDateTime
{
    qint64
    drift()
    {
        return driftMS;
    }

    void
    setDrift(qint64 const ms)
    {
        qCDebug(driftingdatetime_js8) << "Setting drift to" << ms;
        driftMS = ms;
    }

    qint64
    incrementDrift(qint64 const msDelta)
    {
        setDrift(driftMS + msDelta);
        return drift();
    }

    QDateTime
    currentDateTime()
    {
        return QDateTime::currentDateTime().addMSecs(driftMS);
    }

    QDateTime
    currentDateTimeUtc()
    {
        return QDateTime::currentDateTimeUtc().addMSecs(driftMS);
    }

    qint64
    currentMSecsSinceEpoch()
    {
        return QDateTime::currentMSecsSinceEpoch() + driftMS;
    }

    qint64
    currentSecsSinceEpoch()
    {
        return currentMSecsSinceEpoch() / 1000;
    }
}

Q_LOGGING_CATEGORY(driftingdatetime_js8, "driftingdatetime.js8", QtWarningMsg)
