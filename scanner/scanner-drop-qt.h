#include <QtGui>
#include <QSvgRenderer>

extern "C" {
#include "input.h"

// work around error when including mingw-w64 1.0 float.h header
#ifdef __MINGW32__
#define _FLOAT_H___
#endif

#include "filetree.h"
#include "scanner-tag.h"
#include "scanner-common.h"
}

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
signals:
    void showResultList(GSList *files, void *tree);
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
    void showResultList(GSList *files, void *tree);
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

class ResultData : public QAbstractTableModel {
    Q_OBJECT
public:
    ResultData(GSList *files);
    int rowCount(QModelIndex const& parent = QModelIndex()) const;
    int columnCount(QModelIndex const& parent = QModelIndex()) const;
    QVariant data(QModelIndex const& index, int role = Qt::DisplayRole) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
private:
    std::vector<struct filename_list_node *> files_;
};

class ResultWindow : public QWidget {
    Q_OBJECT
public:
    ResultWindow(QWidget *parent, GSList *files, Filetree tree);
    ~ResultWindow();
    QSize sizeHint() const;
private slots:
    void tag_files();
private:
    ResultData data;
    QTreeView *view;
    QSortFilterProxyModel *proxyModel;
    QPushButton *tag_button;
    GSList *files_;
    Filetree tree_;
};

class IconDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    IconDelegate(QWidget *parent = NULL);
    void paint(QPainter *painter, QStyleOptionViewItem const& option,
               QModelIndex const& index) const;
};
