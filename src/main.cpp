#include <QGuiApplication>
#include <QRasterWindow>
#include <QCommandLineParser>
#include <QPainter>
#include <QImage>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QScreen>
#include <QStandardPaths>
#include <QMargins>
#include <QSurfaceFormat>
#include <QDebug>

#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include <sys/file.h>

namespace {


    QString lockFilePath()
    {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (dir.isEmpty())
            dir = QDir::tempPath();
        return dir + QStringLiteral("/wlcrosshair.pid");
    }


    int acquireSingletonOrToggleOff()
    {
        const QByteArray path = QFile::encodeName(lockFilePath());
        int fd = ::open(path.constData(), O_RDWR | O_CREAT, 0600);
        if (fd < 0) {
            qWarning().noquote() << "wlcrosshair: could not open lock file, continuing without toggle behaviour";
            return -2;
        }

        if (::flock(fd, LOCK_EX | LOCK_NB) == 0) {
            if (::ftruncate(fd, 0) != 0) { /* best effort */ }
            const QByteArray pid = QByteArray::number(static_cast<qint64>(::getpid())) + '\n';
            if (::write(fd, pid.constData(), pid.size()) != pid.size()) { /* best effort */ }
            return fd;
        }

        char buf[32] = {};
        ::lseek(fd, 0, SEEK_SET);
        const ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
        ::close(fd);
        if (n > 0) {
            const pid_t other = static_cast<pid_t>(::atol(buf));
            if (other > 0)
                ::kill(other, SIGTERM);
        }
        return -1;
    }


    bool argvHasFlag(int argc, char **argv, const char *shortOpt, const char *longOpt)
    {
        for (int i = 1; i < argc; ++i) {
            if ((shortOpt && std::strcmp(argv[i], shortOpt) == 0) ||
                (longOpt && std::strcmp(argv[i], longOpt) == 0))
                return true;
        }
        return false;
    }


    void detachFromTerminal(int lockFd)
    {
        const QString logPath = QDir(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation))
        .filePath(QStringLiteral("wlcrosshair.log"));
        if (!std::freopen("/dev/null", "r", stdin)) { /* stdin is never read; nothing to fall back to */ }
        if (!std::freopen(QFile::encodeName(logPath).constData(), "w", stdout)) {
            [[maybe_unused]] FILE *fallback = std::freopen("/dev/null", "w", stdout);
        }
        if (!std::freopen(QFile::encodeName(logPath).constData(), "w", stderr)) {
            [[maybe_unused]] FILE *fallback = std::freopen("/dev/null", "w", stderr);
        }

        if (::daemon(/*nochdir=*/1, /*noclose=*/1) != 0)
            return;

            if (lockFd >= 0) {
                if (::ftruncate(lockFd, 0) != 0) { /* best effort: atol() below stops at the first digit either way */ }
                ::lseek(lockFd, 0, SEEK_SET);
                const QByteArray pid = QByteArray::number(static_cast<qint64>(::getpid())) + '\n';
                if (::write(lockFd, pid.constData(), pid.size()) != pid.size()) { /* best effort */ }
            }
    }


    QString configDirPath()
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/wlcrosshair");
        QDir().mkpath(dir);
        return dir;
    }

    QImage builtinCrosshair(int size = 24)
    {
        QImage img(size, size, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor(0, 255, 70, 230), 2));
        const int c = size / 2;
        const int gap = qMax(2, size / 6);
        const int len = qMax(2, size / 2 - gap);
        p.drawLine(c, gap, c, gap + len);
        p.drawLine(c, size - gap - len, c, size - gap);
        p.drawLine(gap, c, gap + len, c);
        p.drawLine(size - gap - len, c, size - gap, c);
        return img;
    }

    class CrosshairWindow : public QRasterWindow
    {
    public:
        explicit CrosshairWindow(QImage image) : m_image(std::move(image)) {}

    protected:
        void paintEvent(QPaintEvent *) override
        {
            QPainter p(this);
            p.setCompositionMode(QPainter::CompositionMode_Source);
            p.fillRect(QRect(QPoint(0, 0), size()), Qt::transparent);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawImage(QRect(QPoint(0, 0), size()), m_image);
        }

    private:
        QImage m_image;
    };

} // namespace

int main(int argc, char *argv[])
{
    #if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    LayerShellQt::Shell::useLayerShell();
    #endif

    const bool interactive = argvHasFlag(argc, argv, "-h", "--help")
    || argvHasFlag(argc, argv, nullptr, "--list-outputs");

    int lockFd = -2;
    if (!interactive) {
        lockFd = acquireSingletonOrToggleOff();
        if (lockFd == -1)
            return 0;
    }
    if (!interactive)
        detachFromTerminal(lockFd);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("wlcrosshair"));
    app.setOrganizationName(QStringLiteral("wlcrosshair"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Minimal layer-shell crosshair overlay for KDE Plasma Wayland.\n"
        "Run once to show, run again to hide (toggle)."));
    parser.addHelpOption();

    QCommandLineOption imageOpt({QStringLiteral("i"), QStringLiteral("image")},
                                QStringLiteral("Path to a crosshair PNG (transparent background)."), QStringLiteral("file"));
    QCommandLineOption scaleOpt({QStringLiteral("s"), QStringLiteral("scale")},
                                QStringLiteral("Scale factor applied to the image."), QStringLiteral("factor"));
    QCommandLineOption outputOpt({QStringLiteral("o"), QStringLiteral("output")},
                                 QStringLiteral("Output/monitor name to show on (see --list-outputs)."), QStringLiteral("name"));
    QCommandLineOption offxOpt(QStringLiteral("offset-x"),
                               QStringLiteral("Horizontal offset in px from dead-center."), QStringLiteral("px"));
    QCommandLineOption offyOpt(QStringLiteral("offset-y"),
                               QStringLiteral("Vertical offset in px from dead-center."), QStringLiteral("px"));
    QCommandLineOption listOpt(QStringLiteral("list-outputs"),
                               QStringLiteral("Print available output names and exit."));

    parser.addOption(imageOpt);
    parser.addOption(scaleOpt);
    parser.addOption(outputOpt);
    parser.addOption(offxOpt);
    parser.addOption(offyOpt);
    parser.addOption(listOpt);
    parser.process(app);

    if (parser.isSet(listOpt)) {
        for (QScreen *s : QGuiApplication::screens())
            qInfo().noquote() << s->name() << QStringLiteral("(%1x%2)").arg(s->geometry().width()).arg(s->geometry().height());
        return 0;
    }

    const QString configDir = configDirPath();
    QSettings settings(configDir + QStringLiteral("/config.ini"), QSettings::IniFormat);

    QString imagePath = parser.value(imageOpt);
    if (imagePath.isEmpty())
        imagePath = settings.value(QStringLiteral("General/image")).toString();
    if (imagePath.isEmpty()) {
        const QString byName = configDir + QStringLiteral("/crosshair.png");
        if (QFile::exists(byName)) {
            imagePath = byName;
        } else {
            const QStringList pngs = QDir(configDir).entryList({QStringLiteral("*.png")}, QDir::Files, QDir::Name);
            if (!pngs.isEmpty())
                imagePath = configDir + QLatin1Char('/') + pngs.first();
        }
    }

    QImage img;
    if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
        img = QImage(imagePath);
        if (img.isNull())
            qWarning().noquote() << "wlcrosshair: failed to load" << imagePath << "- using built-in placeholder.";
    } else {
        qInfo().noquote() << "wlcrosshair: no PNG in" << configDir
        << "- drop crosshair.png there (or pass --image). Using built-in placeholder for now.";
    }
    if (img.isNull())
        img = builtinCrosshair();
    img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    bool scaleOk = false;
    double scale = parser.isSet(scaleOpt)
    ? parser.value(scaleOpt).toDouble(&scaleOk)
    : settings.value(QStringLiteral("General/scale"), 1.0).toDouble(&scaleOk);
    if (!scaleOk || scale <= 0.0)
        scale = 1.0;
    if (!qFuzzyCompare(scale, 1.0))
        img = img.scaled(qRound(img.width() * scale), qRound(img.height() * scale),
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);

        const int offX = parser.isSet(offxOpt) ? parser.value(offxOpt).toInt()
        : settings.value(QStringLiteral("General/offset_x"), 0).toInt();
    const int offY = parser.isSet(offyOpt) ? parser.value(offyOpt).toInt()
    : settings.value(QStringLiteral("General/offset_y"), 0).toInt();

    const QString outputName = parser.isSet(outputOpt)
    ? parser.value(outputOpt)
    : settings.value(QStringLiteral("General/output")).toString();

    QScreen *screen = QGuiApplication::primaryScreen();
    if (!outputName.isEmpty()) {
        for (QScreen *s : QGuiApplication::screens()) {
            if (s->name() == outputName) { screen = s; break; }
        }
    }

    auto *window = new CrosshairWindow(img);
    window->setScreen(screen);
    window->setFlag(Qt::FramelessWindowHint);
    window->setFlag(Qt::WindowTransparentForInput);
    window->setFlag(Qt::WindowDoesNotAcceptFocus);
    window->resize(img.width(), img.height());

    QSurfaceFormat fmt = window->format();
    fmt.setAlphaBufferSize(8);
    window->setFormat(fmt);

    window->create();

    auto *layerWindow = LayerShellQt::Window::get(window);
    if (!layerWindow) {
        qWarning().noquote() << "wlcrosshair: compositor did not offer wlr-layer-shell — "
        "confirm you're on Wayland with a current KWin (Plasma 5.27+).";
        QFile::remove(lockFilePath());
        return 1;
    }

    layerWindow->setScope(QStringLiteral("crosshair"));
    layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layerWindow->setExclusiveZone(-1);

    const QRect geo = screen->geometry();
    const int marginLeft = (geo.width() - img.width()) / 2 + offX;
    const int marginTop = (geo.height() - img.height()) / 2 + offY;
    layerWindow->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)
    | LayerShellQt::Window::AnchorLeft);
    layerWindow->setMargins(QMargins(marginLeft, marginTop, 0, 0));

    window->show();
    return app.exec();
}
