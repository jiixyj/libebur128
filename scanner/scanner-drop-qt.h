#include <QtGui>
#include <QSvgRenderer>

class RenderArea : public QWidget {
    Q_OBJECT
public:
    RenderArea(QWidget *parent = NULL);
public slots:
    void updateLogo();
    void resetLogo();
protected:
    void paintEvent(QPaintEvent *event);
private:
    QSvgRenderer *svg_renderer_;
    qreal rotation_state;
};

class WorkerThread : public QThread {
    Q_OBJECT
public:
    WorkerThread(QList<QUrl> const& files);
protected:
    void run();
private:
    QList<QUrl> urls_;
};

class GUIUpdateThread : public QThread {
    Q_OBJECT
public:
    GUIUpdateThread(QWidget *parent);
public slots:
    void stopThread();
protected:
    void run();
signals:
    void rotateLogo();
    void resetLogo();
    void setProgressBar(int value);
private:
    int old_progress_bar_value_;
    bool rotation_active_;
    bool stop_thread_;
};

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = NULL);
    ~MainWindow();
    QSize sizeHint() const;
protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
private slots:
    void cleanUpThread();
    void setProgressBar(int);
    void rotateLogo();
    void resetLogo();
signals:
    void stopGUIThread();
private:
    QPoint dragPosition;
    RenderArea *render_area_;
    QProgressBar *progress_bar_;
    WorkerThread *worker_thread_;
    GUIUpdateThread gui_update_thread_;
    QTimer *logo_rotation_timer;
};
