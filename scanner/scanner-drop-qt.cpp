#include "scanner-drop-qt.moc"

#include <iostream>
#include <glib.h>

#include "logo.h"

gboolean verbose = TRUE;

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowSystemMenuHint),
      dragPosition()    
{
    setMinimumSize(130, 130);
    setMaximumSize(0, 0);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    QAction *quitAction = new QAction(tr("E&xit"), this);
    quitAction->setShortcut(tr("Ctrl+Q"));
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    addAction(quitAction);

    setContextMenuPolicy(Qt::ActionsContextMenu);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    render_area_ = new RenderArea;
    progress_bar_ = new QProgressBar;
    progress_bar_->setFixedHeight(15);
    progress_bar_->setTextVisible(false);

    layout->addWidget(render_area_);
    layout->addWidget(progress_bar_);
    setLayout(layout);
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragPosition = event->globalPos() - frameGeometry().topLeft();
        // setCursor(Qt::CursorShape(20));
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - dragPosition);
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setCursor(Qt::ArrowCursor);
        event->accept();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    event->accept();
}

QSize MainWindow::sizeHint() const
{
    return QSize(130, 130);
}

RenderArea::RenderArea(QWidget *parent)
    : QWidget(parent),
      rotation_state(0),
      svg_renderer_()
{
    QByteArray logo((const char *) test_svg, test_svg_len);
    svg_renderer_ = new QSvgRenderer(logo, this);
}

void RenderArea::paintEvent(QPaintEvent *event)
{
    static const qreal scale_factor = 0.8f;
    QPainter painter(this);
    QSize s = svg_renderer_->defaultSize();
    QSizeF ws(size());
    qreal svg_aspect = qreal(s.width()) / qreal(s.height());
    qreal canvas_aspect = ws.width() / ws.height();

    painter.translate( ws.width() / 2.0,  ws.height() / 2.0);
    painter.rotate(rotation_state);
    painter.translate(-ws.width() / 2.0, -ws.height() / 2.0);
    QSizeF ns = ws;
    ns.setWidth(ws.width() * svg_aspect / canvas_aspect * scale_factor);
    ns.setHeight(ws.height() * scale_factor);
    painter.translate((ws.width() - ns.width()) / 2.0,
                      (ws.height() - ns.height()) / 2.0);

    painter.scale(svg_aspect / canvas_aspect * scale_factor, scale_factor);

    svg_renderer_->render(&painter);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
