#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void btnSuchen_clicked();
    void onAnyNetworkReply(QNetworkReply *reply);
    void updateInputFields();


private:
    Ui::MainWindow *ui;

    QNetworkAccessManager* nwManager;

    QString cityName;
    QString iconCode;

    QString configFilePath() const;
    void initializeConfigFile() const;
    QString loadApiKey() const;

    void getWeatherByCity();
    void getWeatherByGeoCoords(double latitude, double longitude);
    void getWeatherIcon();
    void processWeatherApiCall(QNetworkReply *reply);
    void processIconApiCall(QNetworkReply *reply);

    int wSpeedToBeaufort(double speed);
    QString wDirectionToString(int deg);
};
#endif // MAINWINDOW_H
