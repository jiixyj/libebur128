#include <QtGui>
#include <QSvgRenderer>

class RenderArea : public QWidget {
    Q_OBJECT
public:
    RenderArea(QWidget *parent = NULL);
    qreal rotation_state;
protected:
    void paintEvent(QPaintEvent *event);
private:
    QSvgRenderer *svg_renderer_;
};

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = NULL);
    QSize sizeHint() const;
protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void resizeEvent(QResizeEvent *event);
private:
    QPoint dragPosition;
    RenderArea *render_area_;
    QProgressBar *progress_bar_;
};
