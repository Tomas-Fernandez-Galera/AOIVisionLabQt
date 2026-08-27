#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "zoomimagelabel.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QTransform>
#include <QVBoxLayout>

#include <opencv2/core/version.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(tr("AOI Vision Lab Qt - DEMO"));
    setMinimumSize(1100, 700);

    connect(ui->referenceButton, &QPushButton::clicked,
            this, &MainWindow::chooseReferenceImage);
    connect(ui->inspectionButton, &QPushButton::clicked,
            this, &MainWindow::chooseInspectionImage);
    connect(ui->loadDemoButton, &QPushButton::clicked,
            this, &MainWindow::loadSelectedDemo);
    connect(ui->inspectButton, &QPushButton::clicked,
            this, &MainWindow::inspectImages);
    connect(ui->darkThemeCheck, &QCheckBox::toggled,
            this, &MainWindow::setLightTheme);
    connect(ui->aboutButton, &QPushButton::clicked,
            this, &MainWindow::showAbout);
    connect(ui->findingsTable, &QTableWidget::cellClicked,
            this, &MainWindow::focusFinding);

    ui->findingsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    setupLanguages();
    applyTheme();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::prepareScreenshot(const QString &demoId, bool lightTheme)
{
    const int index = ui->demoCombo->findData(demoId);
    if (index >= 0) {
        ui->demoCombo->setCurrentIndex(index);
        loadSelectedDemo();
        inspectImages();
    }
    ui->darkThemeCheck->setChecked(lightTheme);
}

QString MainWindow::uiText(const char *english, const char *spanish) const
{
    return QString::fromUtf8(spanishLanguage ? spanish : english);
}

void MainWindow::setupLanguages()
{
    // The visible flags make the selector understandable without occupying a
    // separate toolbar. The saved choice wins; otherwise Windows decides.
    ui->languageCombo->addItem(QString::fromUtf8("🇬🇧  English"), QStringLiteral("en"));
    ui->languageCombo->addItem(QString::fromUtf8("🇪🇸  Español"), QStringLiteral("es"));

    QSettings settings;
    QString code = settings.value(QStringLiteral("interface/language")).toString();
    if (code.isEmpty())
        code = QLocale::system().language() == QLocale::Spanish
            ? QStringLiteral("es") : QStringLiteral("en");
    int index = ui->languageCombo->findData(code);
    if (index < 0) index = 0;
    ui->languageCombo->setCurrentIndex(index);
    spanishLanguage = ui->languageCombo->currentData().toString() == QStringLiteral("es");

    connect(ui->languageCombo, &QComboBox::activated,
            this, &MainWindow::changeLanguage);
    applyLanguage();
}

void MainWindow::changeLanguage(int index)
{
    spanishLanguage = ui->languageCombo->itemData(index).toString() == QStringLiteral("es");
    QSettings().setValue(QStringLiteral("interface/language"),
                         spanishLanguage ? QStringLiteral("es") : QStringLiteral("en"));
    applyLanguage();
}

void MainWindow::populateDemoExamples()
{
    const QString selectedId = ui->demoCombo->currentData().toString();
    ui->demoCombo->clear();
    auto add = [this](const char *id, const char *english, const char *spanish) {
        ui->demoCombo->addItem(uiText(english, spanish), QString::fromLatin1(id));
    };
    add("original-defects", "Original board - component defects",
        "Placa original - defectos de componentes");
    add("original-alignment", "Original board - alignment only",
        "Placa original - solo desalineación");
    add("original-combined", "Original board - alignment and defects",
        "Placa original - desalineación y defectos");
    ui->demoCombo->insertSeparator(ui->demoCombo->count());
    add("led-ok", "LED controller - OK", "Controlador LED - OK");
    add("led-missing", "LED controller - missing LEDs D5/D6",
        "Controlador LED - faltan D5/D6");
    add("led-connector", "LED controller - reversed connector J2",
        "Controlador LED - conector J2 invertido");
    add("led-capacitor", "LED controller - missing capacitor C13",
        "Controlador LED - falta condensador C13");
    add("led-resistor", "LED controller - missing resistor R17",
        "Controlador LED - falta resistencia R17");
    add("led-color", "LED controller - wrong LED D8",
        "Controlador LED - LED D8 incorrecto");
    add("led-ic", "LED controller - missing IC U5",
        "Controlador LED - falta circuito U5");
    add("led-solder-u3", "LED controller - solder bridge on U3",
        "Controlador LED - puente de soldadura en U3");
    add("led-solder-u1", "LED controller - solder bridge on U1",
        "Controlador LED - puente de soldadura en U1");
    add("led-solder-u5", "LED controller - excess solder on U5",
        "Controlador LED - exceso de soldadura en U5");
    const int selectedIndex = ui->demoCombo->findData(selectedId);
    if (selectedIndex >= 0) ui->demoCombo->setCurrentIndex(selectedIndex);
}

void MainWindow::applyLanguage()
{
    populateDemoExamples();
    ui->loadDemoButton->setText(uiText("Load example", "Cargar ejemplo"));
    ui->darkThemeCheck->setText(uiText("Light theme", "Tema claro"));
    ui->aboutButton->setText(uiText("About", "Acerca de"));
    ui->referenceGroup->setTitle(uiText("Known-good reference", "Referencia correcta"));
    ui->inspectionGroup->setTitle(uiText("Board under inspection", "Placa inspeccionada"));
    ui->resultGroup->setTitle(uiText("Inspection result", "Resultado de inspección"));
    ui->findingsGroup->setTitle(uiText("Findings", "Hallazgos"));
    ui->referenceButton->setText(uiText("Load reference PCB…", "Cargar PCB de referencia…"));
    ui->inspectionButton->setText(uiText("Load PCB to analyze…", "Cargar PCB para analizar…"));
    ui->inspectButton->setText(uiText("Inspect images", "Inspeccionar imágenes"));
    if (referenceImage.isNull())
        ui->referenceImage->setText(uiText("Load the reference PCB image",
                                           "Carga la imagen PCB de referencia"));
    if (inspectionImage.isNull())
        ui->inspectionImage->setText(uiText("Load the PCB image to inspect",
                                            "Carga la imagen PCB que se inspeccionará"));
    if (resultImage.isNull())
        ui->resultImage->setText(uiText("Differences and findings will appear here",
                                        "Aquí aparecerán las diferencias y los hallazgos"));
    if (ui->decisionLabel->text() == QStringLiteral("PENDING") ||
        ui->decisionLabel->text() == QStringLiteral("PENDIENTE"))
        ui->decisionLabel->setText(uiText("PENDING", "PENDIENTE"));
    ui->findingsTable->setHorizontalHeaderLabels({QStringLiteral("#"),
        uiText("Region", "Región"), uiText("Difference", "Diferencia"),
        uiText("Status", "Estado")});
    for (int row = 0; row < ui->findingsTable->rowCount(); ++row)
        if (QTableWidgetItem *item = ui->findingsTable->item(row, 3))
            item->setText(uiText("REVIEW", "REVISAR"));
    statusBar()->showMessage(uiText(
        "Ready - OpenCV %1 - load a reference and an inspection image",
        "Preparado - OpenCV %1 - carga una referencia y una imagen de inspección")
        .arg(QString::fromLatin1(CV_VERSION)));
}

void MainWindow::showAbout()
{
    QDialog dialog(this);
    dialog.setWindowTitle(uiText("About AOI Vision Lab Qt", "Acerca de AOI Vision Lab Qt"));
    dialog.resize(720, 620);
    auto *layout = new QVBoxLayout(&dialog);
    auto *content = new QTextBrowser(&dialog);
    content->setOpenExternalLinks(true);
    const QString english = QStringLiteral(R"(
<h1>AOI Vision Lab Qt — DEMO</h1>
<p><b>Version 0.1.0</b><br>Copyright © 2026 Tomás Fernández Galera</p>
<h2>Purpose of this demonstration</h2>
<p>This application is a technical demonstration of how open-source libraries can be used to build the foundations of an <b>Automated Optical Inspection (AOI)</b> system for an electronic PCB assembly line.</p>
<p><b>Qt</b> provides the responsive Windows desktop interface, image viewers and operator workflow. <b>OpenCV</b> provides feature detection, geometric registration, perspective correction, image comparison and defect-region extraction.</p>
<h2>What it demonstrates</h2>
<ul><li>Comparison against a known-good reference board.</li><li>Automatic correction of rotation, translation and perspective.</li><li>Detection of missing or wrong components, reversed connectors, solder bridges and excess solder.</li><li>Visual review with zoom, pan, highlighted regions and an OK / NOT OK decision.</li></ul>
<h2>AI and local automation</h2>
<p>The inspection engine can also operate without the graphical interface. Its command-line mode accepts a reference board and a candidate image, then produces a structured JSON report and an optional marked result image.</p>
<p>A bundled <b>local MCP server</b> exposes this operation to compatible AI assistants. An operator can request, for example: <i>“Load board-model-A.png as the reference and inspect board-Pascual.jpg.”</i> The assistant receives the OK / NOT OK result, detected regions and their measured coordinates.</p>
<p>The MCP integration runs on the local computer and delegates processing to the same C++/OpenCV engine as the desktop application. AOI Vision Lab Qt does not upload production images to the Internet.</p>
<h2>From demo to production line</h2>
<p>The same architecture can be extended with industrial cameras, controlled lighting, triggers, PLC communication, product recipes, calibrated measurements, traceability, statistics and machine-learning classifiers. A real deployment would require validation with production images, controlled tolerances and quantified false-positive and false-negative rates.</p>
<p><b>This demo is not a certified inspection system and must not be used by itself to accept or reject manufactured products.</b></p>
<h2>License</h2>
<p>The source code is distributed under the <b>GNU General Public License version 3</b>, without warranty. The AOI Vision Lab Qt name, icon and visual identity are not granted under the GPL.</p>
<p>Qt and OpenCV are independent projects and retain their respective licenses and trademarks.</p>)");
    const QString spanish = QStringLiteral(R"(
<h1>AOI Vision Lab Qt — DEMO</h1>
<p><b>Versión 0.1.0</b><br>Copyright © 2026 Tomás Fernández Galera</p>
<h2>Objetivo de esta demostración</h2>
<p>Esta aplicación es una demostración técnica de cómo pueden utilizarse librerías de código abierto para construir la base de un sistema de <b>inspección óptica automatizada (AOI)</b> destinado a una línea de montaje de placas electrónicas PCB.</p>
<p><b>Qt</b> proporciona la interfaz de escritorio para Windows, los visualizadores y el flujo de trabajo del operario. <b>OpenCV</b> aporta detección de características, registro geométrico, corrección de perspectiva, comparación de imágenes y extracción de regiones defectuosas.</p>
<h2>Qué demuestra</h2>
<ul><li>Comparación con una placa de referencia conocida como correcta.</li><li>Corrección automática de rotación, traslación y perspectiva.</li><li>Detección de componentes ausentes o incorrectos, conectores invertidos, puentes y excesos de soldadura.</li><li>Revisión visual con zoom, desplazamiento, regiones resaltadas y decisión OK / NO OK.</li></ul>
<h2>IA y automatización local</h2>
<p>El motor de inspección también puede utilizarse sin abrir la interfaz gráfica. El modo de línea de comandos recibe una placa de referencia y una imagen candidata, y genera un informe JSON estructurado y, opcionalmente, una imagen con los hallazgos señalados.</p>
<p>El <b>servidor MCP local</b> incluido expone esta función a asistentes de IA compatibles. El operario puede solicitar, por ejemplo: <i>«Carga placa-modelo-A.png como referencia y analiza placa-Pascual.jpg».</i> El asistente recibe el resultado OK / NO OK, las regiones detectadas y sus coordenadas medidas.</p>
<p>La integración MCP se ejecuta en el propio ordenador y delega el procesamiento en el mismo motor C++/OpenCV que utiliza la aplicación. AOI Vision Lab Qt no sube las imágenes de producción a Internet.</p>
<h2>De la demo a una línea de producción</h2>
<p>La misma arquitectura puede ampliarse con cámaras industriales, iluminación controlada, disparadores, comunicación con PLC, recetas de producto, medidas calibradas, trazabilidad, estadísticas y clasificadores de aprendizaje automático. Una implantación real exigiría validación con imágenes de producción, tolerancias controladas y tasas cuantificadas de falsos positivos y falsos negativos.</p>
<p><b>Esta demo no es un sistema de inspección certificado y no debe utilizarse por sí sola para aceptar o rechazar productos fabricados.</b></p>
<h2>Licencia</h2>
<p>El código fuente se distribuye bajo la <b>GNU General Public License versión 3</b>, sin garantía. El nombre AOI Vision Lab Qt, su icono y su identidad visual no se conceden bajo la GPL.</p>
<p>Qt y OpenCV son proyectos independientes y conservan sus respectivas licencias y marcas.</p>)");
    content->setHtml(spanishLanguage ? spanish : english);
    layout->addWidget(content);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    buttons->button(QDialogButtonBox::Close)->setText(uiText("Close", "Cerrar"));
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::chooseReferenceImage()
{
    chooseImageForLabel(uiText("Choose the known-good reference image",
                               "Elegir la imagen de referencia correcta"),
                        ui->referenceImage, referenceImage);
}

void MainWindow::chooseInspectionImage()
{
    chooseImageForLabel(uiText("Choose the image to inspect",
                               "Elegir la imagen que se va a inspeccionar"),
                        ui->inspectionImage, inspectionImage);
}

void MainWindow::chooseImageForLabel(const QString &caption, QLabel *target,
                                     QImage &storage)
{
    const QString path = QFileDialog::getOpenFileName(
        this, caption, QString(), uiText("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)",
                                        "Imágenes (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
    if (!path.isEmpty())
        loadImage(path, target, storage);
}

bool MainWindow::loadImage(const QString &path, QLabel *target, QImage &storage)
{
    const QImage image(path);
    if (image.isNull()) {
        statusBar()->showMessage(uiText("The selected image could not be opened",
                                        "No se pudo abrir la imagen seleccionada"), 5000);
        return false;
    }
    storage = image;
    findingRegions.clear();
    showImage(target, storage);
    target->setToolTip(path);
    ui->decisionLabel->setText(uiText("PENDING", "PENDIENTE"));
    ui->decisionLabel->setStyleSheet(QStringLiteral(
        "font-size: 14px; font-weight: 800; color: #526276; "
        "background: #e8edf4; border-radius: 5px;"));
    statusBar()->showMessage(uiText("Image loaded: %1", "Imagen cargada: %1").arg(path), 5000);
    updateInspectionAvailability();
    return true;
}

void MainWindow::showImage(QLabel *target, const QImage &image)
{
    if (auto *viewer = qobject_cast<ZoomImageLabel *>(target)) {
        viewer->setImage(image);
        return;
    }
    target->setPixmap(QPixmap::fromImage(image).scaled(
        target->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::updateInspectionAvailability()
{
    ui->inspectButton->setEnabled(!referenceImage.isNull() && !inspectionImage.isNull());
}

void MainWindow::loadSelectedDemo()
{
    const QString id = ui->demoCombo->currentData().toString();
    if (id == QStringLiteral("original-defects"))
        loadProjectDemo(QStringLiteral("pcb-reference-good.png"), QStringLiteral("pcb-inspection-defective.png"));
    else if (id == QStringLiteral("original-alignment"))
        loadProjectDemo(QStringLiteral("pcb-reference-good.png"), QStringLiteral("pcb-reference-good.png"), true);
    else if (id == QStringLiteral("original-combined"))
        loadProjectDemo(QStringLiteral("pcb-reference-good.png"), QStringLiteral("pcb-inspection-defective.png"), true);
    else if (id.startsWith(QStringLiteral("led-"))) {
        const QString base = QStringLiteral("led-controller-set/");
        const QString reference = base + QStringLiteral("led-controller-reference.png");
        if (id == QStringLiteral("led-ok"))
            loadProjectDemo(reference, base + QStringLiteral("led-controller-ok.png"), true);
        else if (id == QStringLiteral("led-missing"))
            loadProjectDemo(reference, base + QStringLiteral("led-controller-not-ok-missing-leds-d5-d6.png"), true);
        else if (id == QStringLiteral("led-connector"))
            loadProjectDemo(reference, base + QStringLiteral("led-controller-not-ok-connector-j2-reversed.png"), true);
        else if (id == QStringLiteral("led-capacitor"))
            loadProjectDemo(reference, base + QStringLiteral("led-controller-not-ok-capacitor-c13-missing.png"), true);
        else if (id == QStringLiteral("led-resistor"))
            loadProjectDemo(reference, base + QStringLiteral("led-controller-not-ok-resistor-r17-missing.png"), true);
        else if (id == QStringLiteral("led-color"))
            loadProjectDemo(reference, base + QStringLiteral("led-controller-not-ok-led-d8-wrong-color.png"), true);
        else if (id == QStringLiteral("led-ic"))
            loadProjectDemo(reference, base + QStringLiteral("led-controller-not-ok-ic-u5-missing.png"), true);
        else if (id == QStringLiteral("led-solder-u3"))
            loadProjectDemo(reference, base + QStringLiteral("led-controller-not-ok-solder-bridge-u3.png"), true);
        else if (id == QStringLiteral("led-solder-u1"))
            loadProjectDemo(reference, base + QStringLiteral("led-controller-not-ok-solder-bridge-u1.png"), true);
        else if (id == QStringLiteral("led-solder-u5"))
            loadProjectDemo(reference, base + QStringLiteral("led-controller-not-ok-excess-solder-u5.png"), true);
    }
}

void MainWindow::loadProjectDemo(const QString &referenceFileName,
                                 const QString &inspectionFileName,
                                 bool applyGeometricMisalignment)
{
    QDir directory(QCoreApplication::applicationDirPath());
    QString referencePath;
    QString inspectionPath;

    // Qt Creator normally places the executable several levels below source.
    for (int level = 0; level < 7; ++level) {
        const QString referenceCandidate =
            directory.filePath("test-images/" + referenceFileName);
        const QString inspectionCandidate =
            directory.filePath("test-images/" + inspectionFileName);
        if (QFileInfo::exists(referenceCandidate) && QFileInfo::exists(inspectionCandidate)) {
            referencePath = referenceCandidate;
            inspectionPath = inspectionCandidate;
            break;
        }
        directory.cdUp();
    }

    if (referencePath.isEmpty() ||
        !loadImage(referencePath, ui->referenceImage, referenceImage) ||
        !loadImage(inspectionPath, ui->inspectionImage, inspectionImage)) {
        statusBar()->showMessage(uiText("Synthetic demo images could not be located",
                                        "No se encontraron las imágenes de demostración"), 6000);
        return;
    }
    if (applyGeometricMisalignment) {
        inspectionImage = createMisalignedCapture(inspectionImage);
        showImage(ui->inspectionImage, inspectionImage);
        ui->inspectionImage->setToolTip(
            uiText("Deterministic rotation, translation and perspective transform",
                   "Transformación determinista de rotación, traslación y perspectiva"));
    }
    inspectImages();
}

QImage MainWindow::createMisalignedCapture(const QImage &source) const
{
    // This is an exact projective transformation, not a regenerated picture.
    // Therefore every component pixel remains tied to the original board and
    // any post-registration difference is attributable to interpolation or to
    // a real anomaly already present in the source image.
    const QSize size = source.size();
    const QPolygonF sourceQuad({QPointF(0, 0),
                                QPointF(size.width(), 0),
                                QPointF(size.width(), size.height()),
                                QPointF(0, size.height())});
    const QPolygonF destinationQuad({
        QPointF(size.width() * 0.055, size.height() * 0.095),
        QPointF(size.width() * 0.965, size.height() * 0.025),
        QPointF(size.width() * 0.985, size.height() * 0.925),
        QPointF(size.width() * 0.025, size.height() * 0.985)});

    QTransform transform;
    QTransform::quadToQuad(sourceQuad, destinationQuad, transform);
    QImage transformed(size, QImage::Format_RGB32);
    transformed.fill(QColor(225, 225, 225));
    QPainter painter(&transformed);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setTransform(transform);
    painter.drawImage(QPointF(0, 0), source);
    painter.end();
    return transformed;
}

void MainWindow::inspectImages()
{
    statusBar()->showMessage(uiText("Aligning and inspecting images…",
                                    "Alineando e inspeccionando imágenes…"));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const InspectionResult result = inspectionEngine.inspect(referenceImage, inspectionImage);
    QApplication::restoreOverrideCursor();

    if (!result.valid) {
        statusBar()->showMessage(result.error, 7000);
        return;
    }

    resultImage = result.visualization;
    findingRegions.clear();
    findingRegions.reserve(result.findings.size());
    showImage(ui->resultImage, resultImage);
    if (result.findings.isEmpty()) {
        ui->decisionLabel->setText(QStringLiteral("✓  OK"));
        ui->decisionLabel->setStyleSheet(QStringLiteral(
            "font-size: 15px; font-weight: 900; color: white; "
            "background: #16834a; border: 1px solid #4ade80; border-radius: 5px;"));
    } else {
        ui->decisionLabel->setText(uiText("✕  NOT OK", "✕  NO OK"));
        ui->decisionLabel->setStyleSheet(QStringLiteral(
            "font-size: 15px; font-weight: 900; color: white; "
            "background: #bd2636; border: 1px solid #fb7185; border-radius: 5px;"));
    }
    ui->findingsTable->setRowCount(result.findings.size());
    for (int row = 0; row < result.findings.size(); ++row) {
        const InspectionFinding &finding = result.findings.at(row);
        findingRegions.append(finding.bounds);
        ui->findingsTable->setItem(row, 0,
                                  new QTableWidgetItem(QString::number(row + 1)));
        ui->findingsTable->setItem(row, 1, new QTableWidgetItem(
            QStringLiteral("x=%1, y=%2, %3 × %4")
                .arg(finding.bounds.x()).arg(finding.bounds.y())
                .arg(finding.bounds.width()).arg(finding.bounds.height())));
        ui->findingsTable->setItem(row, 2, new QTableWidgetItem(
            uiText("%1 pixels; score %2", "%1 píxeles; puntuación %2")
                .arg(finding.changedPixels)
                .arg(finding.meanDifference, 0, 'f', 1)));
        ui->findingsTable->setItem(row, 3, new QTableWidgetItem(uiText("REVIEW", "REVISAR")));
    }
    statusBar()->showMessage(
        uiText("Inspection complete - %1 finding(s), %2 alignment matches",
               "Inspección terminada - %1 hallazgo(s), %2 coincidencias de alineación")
            .arg(result.findings.size()).arg(result.featureMatches));
}

void MainWindow::focusFinding(int row, int column)
{
    Q_UNUSED(column);
    if (row < 0 || row >= findingRegions.size())
        return;
    ui->resultImage->focusRegion(findingRegions.at(row));
}

void MainWindow::setLightTheme(bool enabled)
{
    // Dark is the default inspection environment. The checkbox deliberately
    // enables the optional light appearance instead of repeating that default.
    darkTheme = !enabled;
    applyTheme();
}

void MainWindow::applyTheme()
{
    if (!darkTheme) {
        setStyleSheet(QString());
        return;
    }
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget { background: #111827; color: #e5edf8; }
        QGroupBox { border: 1px solid #334763; border-radius: 8px; margin-top: 10px; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; }
        QPushButton { background: #1769d2; border: 1px solid #3b82f6; border-radius: 6px;
                      padding: 7px 14px; color: white; }
        QPushButton:hover { background: #2381ef; }
        QPushButton:disabled { background: #334155; color: #94a3b8; }
        QLabel#referenceImage, QLabel#inspectionImage, QLabel#resultImage {
            background: #0b1220; border: 1px dashed #456080; border-radius: 6px;
        }
        QTableWidget { background: #0b1220; gridline-color: #334763; }
        QHeaderView::section { background: #1d2c42; padding: 5px; border: 0; }
        QStatusBar { background: #0b1220; }
    )"));
}
