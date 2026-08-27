#pragma once

#include "inspectionengine.h"

#include <QImage>
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void prepareScreenshot(const QString &demoId, bool lightTheme);

private slots:
    void chooseReferenceImage();
    void chooseInspectionImage();
    void loadSelectedDemo();
    void inspectImages();
    void setLightTheme(bool enabled);
    void changeLanguage(int index);
    void showAbout();
    void focusFinding(int row, int column);

private:
    void chooseImageForLabel(const QString &caption, class QLabel *target, QImage &storage);
    bool loadImage(const QString &path, class QLabel *target, QImage &storage);
    void showImage(class QLabel *target, const QImage &image);
    void updateInspectionAvailability();
    void loadProjectDemo(const QString &referenceFileName,
                         const QString &inspectionFileName,
                         bool applyGeometricMisalignment = false);
    QImage createMisalignedCapture(const QImage &source) const;
    void applyTheme();
    void setupLanguages();
    void applyLanguage();
    void populateDemoExamples();
    QString uiText(const char *english, const char *spanish) const;

    Ui::MainWindow *ui;
    bool darkTheme = true;
    bool spanishLanguage = false;
    QImage referenceImage;
    QImage inspectionImage;
    QImage resultImage;
    QVector<QRect> findingRegions;
    InspectionEngine inspectionEngine;
};
