#include "mainwindow.h"
#include "inspectionengine.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {

// Runs the same inspection engine as the GUI without opening a window.  This
// stable command interface is used by scripts and by the local MCP server.
int runAutomatedInspection(const QCommandLineParser &parser)
{
    const QString referencePath = parser.value(QStringLiteral("reference"));
    const QString candidatePath = parser.value(QStringLiteral("inspect"));
    const QString reportPath = parser.value(QStringLiteral("report"));
    const QString visualizationPath = parser.value(QStringLiteral("visualization"));

    QJsonObject report;
    report.insert(QStringLiteral("reference"), QFileInfo(referencePath).absoluteFilePath());
    report.insert(QStringLiteral("candidate"), QFileInfo(candidatePath).absoluteFilePath());

    const QImage reference(referencePath);
    const QImage candidate(candidatePath);
    InspectionResult result;
    if (reference.isNull() || candidate.isNull()) {
        report.insert(QStringLiteral("status"), QStringLiteral("ERROR"));
        report.insert(QStringLiteral("error"), QStringLiteral("Reference or candidate image could not be loaded."));
    } else {
        result = InspectionEngine().inspect(reference, candidate);
        report.insert(QStringLiteral("status"),
                      result.valid ? (result.findings.isEmpty() ? QStringLiteral("OK")
                                                                : QStringLiteral("NO_OK"))
                                   : QStringLiteral("ERROR"));
        report.insert(QStringLiteral("feature_matches"), result.featureMatches);
        report.insert(QStringLiteral("finding_count"), result.findings.size());
        if (!result.error.isEmpty())
            report.insert(QStringLiteral("error"), result.error);

        QJsonArray findings;
        for (int index = 0; index < result.findings.size(); ++index) {
            const InspectionFinding &finding = result.findings.at(index);
            QJsonObject item;
            item.insert(QStringLiteral("id"), index + 1);
            item.insert(QStringLiteral("x"), finding.bounds.x());
            item.insert(QStringLiteral("y"), finding.bounds.y());
            item.insert(QStringLiteral("width"), finding.bounds.width());
            item.insert(QStringLiteral("height"), finding.bounds.height());
            item.insert(QStringLiteral("changed_pixels"), finding.changedPixels);
            item.insert(QStringLiteral("mean_difference"), finding.meanDifference);
            findings.append(item);
        }
        report.insert(QStringLiteral("findings"), findings);

        if (!visualizationPath.isEmpty()) {
            const bool saved = result.visualization.save(visualizationPath);
            report.insert(QStringLiteral("visualization"),
                          saved ? QFileInfo(visualizationPath).absoluteFilePath() : QString());
        }
    }

    const QByteArray json = QJsonDocument(report).toJson(QJsonDocument::Indented);
    if (!reportPath.isEmpty()) {
        QFile file(reportPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return 3;
        file.write(json);
    }
    QTextStream(stdout) << json;
    return report.value(QStringLiteral("status")).toString() == QStringLiteral("ERROR") ? 2 : 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("AOI Vision Lab Qt"));
    QCoreApplication::setOrganizationName(QStringLiteral("Tomasiky"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("PCB comparison and AOI demonstration"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("reference"), QStringLiteral("Reference PCB image."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("inspect"), QStringLiteral("Candidate PCB image."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("report"), QStringLiteral("Write the JSON report to this path."), QStringLiteral("path")});
    parser.addOption({QStringLiteral("visualization"), QStringLiteral("Write the marked result image to this path."), QStringLiteral("path")});
    parser.process(application);

    const bool automated = parser.isSet(QStringLiteral("reference")) ||
                           parser.isSet(QStringLiteral("inspect"));
    if (automated) {
        if (!parser.isSet(QStringLiteral("reference")) ||
            !parser.isSet(QStringLiteral("inspect")))
            parser.showHelp(2);
        return runAutomatedInspection(parser);
    }

    MainWindow window;
    window.showMaximized();
    return application.exec();
}
