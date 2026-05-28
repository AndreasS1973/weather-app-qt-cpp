#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <cmath>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    initializeConfigFile();

    // NetworkAccessManager
    nwManager = new QNetworkAccessManager(this);
    connect(nwManager, &QNetworkAccessManager::finished, this, &MainWindow::onAnyNetworkReply);

    // RadioButtons
    connect(ui->rbStadtText, &QRadioButton::toggled, this, &MainWindow::updateInputFields);
    connect(ui->rbLatLon, &QRadioButton::toggled, this, &MainWindow::updateInputFields);

    // Search
    connect(ui->btnSuchen, &QPushButton::clicked, this, &MainWindow::btnSuchen_clicked);
    connect(ui->leStadteNamen, &QLineEdit::returnPressed, this, &MainWindow::btnSuchen_clicked);

    // --- Restore last settings ---
    QSettings settings(configFilePath(), QSettings::IniFormat);

    QString inputMode = settings.value("Search/inputMode", "city").toString();

    const QString lastCity = settings.value("Search/lastCity").toString();
    const double lastlat = settings.value("Search/lastLat", 0.0).toDouble();
    const double lastlon = settings.value("Search/lastLon", 0.0).toDouble();

    bool shouldLoadWeather = false;
    bool loadByCoordinates = false;

    if (inputMode == "city")
    {
        ui->rbStadtText->setChecked(true);

        if (!lastCity.isEmpty()) {
            ui->leStadteNamen->setText(lastCity);
            ui->lbStaedteName->setText("🏙️ Lade letztes Wetter für " + lastCity + "...");
            cityName = lastCity;

            shouldLoadWeather = true;
            loadByCoordinates = false;
        }
    }
    else if (inputMode == "latlon")
    {
        ui->rbLatLon->setChecked(true);

        ui->leLat->setText(QString::number(lastlat));
        ui->leLon->setText(QString::number(lastlon));

        if (lastlat != 0.0 || lastlon != 0.0)
        {
            ui->lbStaedteName->setText(
                QString("Letztes Wetter für lat: %1, lon: %2")
                    .arg(lastlat)
                    .arg(lastlon)
                );

            shouldLoadWeather = true;
            loadByCoordinates = true;
        }
    }
    else
    {
        inputMode = "city";
        ui->rbStadtText->setChecked(true);
    }

    updateInputFields();

    if (shouldLoadWeather)
    {
        if (loadByCoordinates) {
            getWeatherByGeoCoords(lastlat, lastlon);
        } else {
            getWeatherByCity();
        }
    }

    ui->lbStatus->setTextFormat(Qt::RichText);


    this->setStyleSheet(R"(
    QWidget {
        background-color: #f7fbff;
        font-family: 'Segoe UI', 'Open Sans', sans-serif;
        color: #222;
    }

    QLabel {
        font-size: 15px;
        color: #222;
    }

    QLabel#lbTitle {
        font-size: 26px;
        font-weight: bold;
        color: #0078d7;
        margin-bottom: 8px;
    }

    QLabel#lbStaedteName {
        font-size: 26px;
        font-weight: 600;
        color: #0078d7;
        margin-top: 10px;
        qproperty-alignment: 'AlignCenter';
    }

    QLabel#lbTemp {
        font-size: 54px;
        font-weight: bold;
        color: #333;
        margin-top: 8px;
        margin-bottom: 10px;
        qproperty-alignment: 'AlignCenter';
    }

    QLabel#lbWetterBeschreibung {
        font-size: 18px;
        color: #333;
        font-weight: 500;
        margin-left: 10px;
    }

    QLabel#lbWind, QLabel#lbHumidity {
        font-size: 20px;
        font-weight: 600;
        color: #222;
        background: #eaf4ff;
        border-radius: 8px;
        padding: 10px 16px;
        margin: 6px 40px;
        qproperty-alignment: 'AlignCenter';
    }

    QLabel#lbStatus {
        font-style: italic;
        color: #555;
        background-color: #e6f0fa;
        padding: 4px 6px;
        border-top: 1px solid #cdd9e5;
    }

    QPushButton {
        background-color: #0078d7;
        color: white;
        border: none;
        border-radius: 6px;
        padding: 8px 16px;
        font-weight: bold;
    }
    QPushButton:hover {
        background-color: #005fa3;
    }

    QLineEdit {
        background: white;
        border: 1px solid #ccc;
        border-radius: 4px;
        padding: 6px 8px;
        min-width: 100px;
        font-size: 14px;
    }

    QRadioButton {
        font-size: 14px;
        color: #333;
        margin-right: 6px;
    }

    QFrame {
        background-color: #d0e3f3;
        height: 1px;
        margin: 6px 0;
    }
)");
}

MainWindow::~MainWindow()
{
    delete ui;
}

QString MainWindow::configFilePath() const
{
    return QCoreApplication::applicationDirPath() + "/config.ini";
}

void MainWindow::initializeConfigFile() const
{
    const QString path = configFilePath();

    if (QFile::exists(path)) {
        return;
    }

    QSettings settings(path, QSettings::IniFormat);
    settings.setValue("OpenWeather/apiKey", "");
    settings.setValue("Search/inputMode", "city");
    settings.sync();
}

QString MainWindow::loadApiKey() const
{
    QSettings settings(configFilePath(), QSettings::IniFormat);
    return settings.value("OpenWeather/apiKey").toString().trimmed();
}


void MainWindow::btnSuchen_clicked()
{
    // Store the selected search mode and the corresponding input values.
    // Then trigger the matching weather request.
    QSettings settings(configFilePath(), QSettings::IniFormat);

    if (ui->rbStadtText->isChecked())
    {
        cityName = ui->leStadteNamen->text();
        settings.setValue("Search/lastCity", cityName);
        settings.setValue("Search/inputMode", "city");

        ui->lbStaedteName->setText("🏙️ Suche Wetterdaten für " + cityName + "...");
        ui->lbStatus->setStyleSheet("");   // Reset to normal style
        ui->lbStatus->clear();
        getWeatherByCity();
    }
    else
    {
        double lati = ui->leLat->text().toDouble();
        double longi = ui->leLon->text().toDouble();

        settings.setValue("Search/lastLat", lati);
        settings.setValue("Search/lastLon", longi);
        settings.setValue("Search/inputMode", "latlon");

        ui->lbStaedteName->setText(QString("Suche Wetterdaten für lat: %1, long: %2 ...").arg(lati).arg(longi));
        ui->lbStatus->setStyleSheet("");   // Reset to normal style
        ui->lbStatus->clear();
        getWeatherByGeoCoords(lati, longi);
    }
}

void MainWindow::onAnyNetworkReply(QNetworkReply *reply)
{
    int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString replyUrl = reply->url().toString();

    /*
    https://doc.qt.io/qt-6/qnetworkreply.html#error
    QNetworkReply::NetworkError QNetworkReply::error() const
    Returns the error that was found during the processing of this request.
    If no error was found, returns NoError.
    */
    if (reply->error() != QNetworkReply::NoError)
    {
        // If server doesn't reply (DNS, SSL, Timeout etc.)
        if (reply->error() == QNetworkReply::HostNotFoundError ||
            reply->error() == QNetworkReply::TimeoutError ||
            reply->error() == QNetworkReply::SslHandshakeFailedError) {
            ui->lbStatus->setText("Netzwerkfehler: " + reply->errorString());
            reply->deleteLater();// Delete QNetworkReply object if scope is left
            return;
        }
        // HTTP errors, e.g. 404, 401, 500
        if (httpStatus >= 400) {
            ui->lbStatus->setTextFormat(Qt::RichText);
            ui->lbStatus->setWordWrap(true);
            ui->lbStatus->setTextInteractionFlags(Qt::TextSelectableByMouse); // Make error text selectable.
            ui->lbStatus->setText(
                QString("<div style='color:black; font-weight:bold; white-space:normal;'>HTTP Fehler %1:<br>%2</div>")
                    .arg(httpStatus)
                    .arg(reply->errorString().toHtmlEscaped()));
            qDebug() << "HTTP Fehler:" << httpStatus << reply->errorString();
            // The server may provide additional error details in JSON format.
            // For weather requests, processWeatherApiCall() parses that response.
            if (replyUrl.contains("/data/2.5/weather"))
            {
                processWeatherApiCall(reply);
            }

            else {
                ui->lbStatus->setText(QString("Fehler %1: %2").arg(httpStatus).arg(reply->errorString()));
            }
            reply->deleteLater();
            return;
        }
    }
    else
    {
        // From this point forward: handling of successful API calls, which need two different parsing methods.
        if (replyUrl.contains("/data/2.5/weather"))
        {
            processWeatherApiCall(reply);
        }
        else if (replyUrl.contains("/img/wn/"))
        {
            processIconApiCall(reply);
        }
        else
            qDebug() << "Unknown Request";
    }
    reply->deleteLater();

}

void MainWindow::updateInputFields()
{
    // Set the enabled state of the input fields according to the selected search mode.
    bool radioButtonState = ui->rbStadtText->isChecked();

    ui->leStadteNamen->setEnabled(radioButtonState);
    ui->leLat->setEnabled(!radioButtonState);
    ui->leLon->setEnabled(!radioButtonState);

    if(radioButtonState)
    {
        ui->leLat->clear();
        ui->leLon->clear();
    }
    else
    {
        ui->leStadteNamen->clear();
    }
}

void MainWindow::getWeatherByCity()
{

    const QString apiKey = loadApiKey();

    if (apiKey.isEmpty()) {
        QMessageBox::warning(
            this,
            "Missing API key",
            "Please enter your OpenWeather API key in config.ini."
            );
        return;
    }
    QUrl apiURL = QUrl(QString("https://api.openweathermap.org/data/2.5/weather?q=%1&appid=%2&units=metric&lang=de")
                           .arg(cityName)
                           .arg(apiKey));

    QNetworkRequest request(apiURL);
    nwManager->get(request);
}

void MainWindow::getWeatherByGeoCoords(double latitude, double longitude)
{
    const QString apiKey = loadApiKey();

    if (apiKey.isEmpty()) {
        QMessageBox::warning(
            this,
            "Missing API key",
            "Please enter your OpenWeather API key in config.ini."
            );
        return;
    }
    QUrl apiURL = QUrl(QString("https://api.openweathermap.org/data/2.5/weather?lat=%1&lon=%2&appid=%3&units=metric&lang=de")
                           .arg(latitude)
                           .arg(longitude)
                           .arg(apiKey));

    QNetworkRequest request(apiURL);
    nwManager->get(request);

}

void MainWindow::getWeatherIcon()
{
    // Request the weather icon after the weather response has been parsed.
    QUrl iconApiURL = QUrl(QString("https://openweathermap.org/img/wn/%1@2x.png").arg(iconCode));
    QNetworkRequest requestIcon(iconApiURL);
    nwManager->get(requestIcon);
}

void MainWindow::processWeatherApiCall(QNetworkReply *reply)
{
    QJsonDocument replyJsonDoc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject replyJsonObject = replyJsonDoc.object();

    // --- Read additional diagnostic data ---
    // OpenWeather may return API-level errors as JSON, e.g.:
    // { "cod": "404", "message": "city not found" }
    if (replyJsonObject.contains("cod") && replyJsonObject["cod"].toInt() != 200)
    {
        QString msg = replyJsonObject["message"].toString();
        ui->lbStaedteName->setText("API-Fehler: " + msg.toHtmlEscaped());
        ui->lbTemp->clear();
        ui->lbWetterBeschreibung->clear();
        ui->lbIcon->clear();
        ui->lbWind->clear();
        ui->lbHumidity->clear();
        qDebug() << "API-Fehler:" << msg;
        return;
    }

    ui->lbStatus->setText("<div style='color:green;'>Wetterdaten erfolgreich geladen.</div>");
    QTimer::singleShot(3000, this, [this](){ui->lbStatus->clear();});

    // Save the latest API response to the user's Documents folder.
    // This is not required for the UI, but useful for debugging and inspecting API data.
    QFile myReplyJsonFile(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                          + "/WetterApp_reply.json");
    if (myReplyJsonFile.open(QFile::WriteOnly | QFile::Truncate)) {
        myReplyJsonFile.write(replyJsonDoc.toJson());
        myReplyJsonFile.close();
    } else {
        qDebug() << "Could not write debug JSON file:" << myReplyJsonFile.errorString();
    }
    // -------- Parse JSON objects ---------
    double temp = replyJsonObject["main"].toObject()["temp"].toDouble();
    double humidity = replyJsonObject["main"].toObject()["humidity"].toDouble();
    int wDirection = replyJsonObject["wind"].toObject()["deg"].toInt();
    double wSpeed = replyJsonObject["wind"].toObject()["speed"].toDouble();
    double latJson = replyJsonObject["coord"].toObject()["lat"].toDouble();
    double lonJson = replyJsonObject["coord"].toObject()["lon"].toDouble();
    cityName = replyJsonObject["name"].toString();
    QString desc = replyJsonObject["weather"].toArray()[0].toObject()["description"].toString();

    ui->leLat->setText(QString("%1").arg(latJson));
    ui->leLon->setText(QString("%1").arg(lonJson));
    ui->leStadteNamen->setText(QString("%1").arg(cityName));
    ui->lbStaedteName->setText(QString("%1").arg(cityName));
    ui->lbTemp->setText(QString("🌡️ %1°C").arg(temp, 0, 'f', 1));
    ui->lbWetterBeschreibung->setText(desc);
    ui->lbHumidity->setText(QString("💧 Luftfeuchtigkeit: %1 %").arg(humidity));
    ui->lbWind->setText(
        QString("🧭️ Wind aus %1 (%3°), Windstärke %2 (%4 km/h)")
            .arg(wDirectionToString(wDirection))
            .arg(wSpeedToBeaufort(wSpeed))
            .arg(wDirection)
            .arg(wSpeed * 3.6, 0, 'f', 0)
        );

    // ------- Read Icon code from JsonObject & call getWeatherIcon() -----------------
    iconCode = replyJsonObject["weather"].toArray()[0].toObject()["icon"].toString();
    getWeatherIcon();
}

void MainWindow::processIconApiCall(QNetworkReply *reply)
{
    QByteArray iconData = reply->readAll();
    QPixmap iconPixmap;

    if (!iconPixmap.loadFromData(iconData)) {
        qDebug() << "Fehler beim Laden des Icons!";
        return;
    }

    ui->lbIcon->setPixmap(iconPixmap.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

int MainWindow::wSpeedToBeaufort(double speed)
{
    double bSpeedDouble;

    if(speed <= 0.0)
        return 0;
    else
        bSpeedDouble = std::pow((double)speed/0.836,2.0/3.0);

    return std::round(bSpeedDouble);
}

QString MainWindow::wDirectionToString(int deg)
{
    static const QList<QString> degLabel = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW", "N" };

    return degLabel[(int)((deg + 11.25)/22.5)];
}
